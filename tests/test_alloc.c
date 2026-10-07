// The allocation-failure proof for the pure modules: each operation runs again and again, with
// its first allocation failing, then its second, and so on, until a run in which none failed. Every
// run must end without a crash or a leak, and a run that failed must leave what it worked on as it
// was.
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "alloc.h"
#include "fileio.h"
#include "inidoc.h"
#include "config_save.h"
#include "browser.h"
#include "settings.h"
#include "listpick.h"
#include "fontlist.h"
#include "bindings.h"

#define DIR "alloc-fixture"
#define CONFIG DIR "/config.ini"

static int calls = 0;        // Allocations asked for since the count was started
static int fail_at = 0;      // The one that fails; 0 for none
static bool failed = false;  // Whether it has, in the run under way
static int live = 0;         // Blocks allocated and not yet freed
static int runs = 0;         // The runs the last proof made: the last one had no failure

// A function to say how many allocations a proof made fail, one run each
static void report(const char *what)
{
    printf("%s: allocations 1 to %d each failed in turn\n", what, runs - 1);
}

// A function to allocate as realloc() does, except for the one allocation chosen to fail
static void *test_reallocate(void *memory, size_t size)
{
    calls++;
    if (calls == fail_at) {
        failed = true;
        return NULL;
    }
    void *result = realloc(memory, size);
    if (result != NULL && memory == NULL)
        live++;
    return result;
}

// A function to free, counting the block as gone
static void test_release(void *memory)
{
    live--;
    free(memory);
}

// A function to start a run in which allocation `n` fails
static void arm(int n)
{
    calls = 0;
    fail_at = n;
    failed = false;
}

// A function to end the failing part of a run: no allocation after this fails
static void disarm(void)
{
    fail_at = 0;
}

// A function to check a run's end: nothing left allocated. False (and a failed check) when
// something was, so the proof stops rather than repeating the report for every run after it.
static bool no_leak(int line, int n)
{
    check_count++;
    if (live != 0) {
        check_failures++;
        fprintf(stderr, "test_alloc.c:%d: %d block(s) left allocated with allocation %d failing\n", line, live, n);
        live = 0;
        return false;
    }
    return true;
}

// A function to check a condition that names the run it failed in
#define CHECK_RUN(condition, n) do { \
    check_count++; \
    if (!(condition)) { \
        check_failures++; \
        fprintf(stderr, "%s:%d: with allocation %d failing: %s\n", __FILE__, __LINE__, (n), #condition); \
    } \
} while (0)

// A function to write a document out, for comparing it with what it was (caller frees)
static char *text_of(const IniDoc *doc)
{
    return inidoc_serialize(doc, NULL);
}

static const char *const CONFIG_TEXT =
    "; my launcher\n"
    "[General]\n"
    "DefaultMenu=Main\n"
    "\n"
    "[Layout]\n"
    "Rows=1\n"
    "  continued\n"
    "MaxButtons=4 ; the old name\n"
    "\n"
    "[Main]\n"
    "Entry1=Kodi;kodi.png;kodi\n"
    "Entry2=Plex;plex.png;plex\n";

// A function to prove that reading a file into lines fails cleanly: NULL, and nothing left allocated
static void prove_parse(void)
{
    for (int n = 1;; n++) {
        arm(n);
        IniDoc *doc = inidoc_parse(CONFIG_TEXT, strlen(CONFIG_TEXT));
        disarm();
        CHECK_RUN(failed ? doc == NULL : doc != NULL, n);
        if (doc != NULL) {
            char *out = text_of(doc);
            CHECK_RUN(out != NULL && strcmp(out, CONFIG_TEXT) == 0, n);
            alloc_free(out);
        }
        inidoc_free(doc);
        runs = n;
        if (!no_leak(__LINE__, n) || !failed)
            break;
    }
    report("reading a file into lines");
}

// A function to prove that one set fails cleanly: refused, saying "out of memory", with the
// document as it was; or done, giving `expected`
static void prove_set(const char *section, const char *key, const char *value, IniDocPlacement placement,
                      const char *expected)
{
    for (int n = 1;; n++) {
        IniDoc *doc = inidoc_parse(CONFIG_TEXT, strlen(CONFIG_TEXT));
        arm(n);
        bool ok = inidoc_set(doc, section, key, value, placement);
        disarm();
        char *out = text_of(doc);
        if (ok)
            CHECK_RUN(out != NULL && strcmp(out, expected) == 0, n);
        else {
            CHECK_RUN(failed && strcmp(inidoc_why(doc), "out of memory") == 0, n);
            CHECK_RUN(out != NULL && strcmp(out, CONFIG_TEXT) == 0, n);
        }
        alloc_free(out);
        inidoc_free(doc);
        runs = n;
        if (!no_leak(__LINE__, n) || !failed)
            break;
    }
    printf("setting %s: allocations 1 to %d each failed in turn\n", key, runs - 1);
}

// A function to prove that writing a document out fails cleanly
static void prove_serialize(void)
{
    IniDoc *doc = inidoc_parse(CONFIG_TEXT, strlen(CONFIG_TEXT));
    for (int n = 1;; n++) {
        arm(n);
        char *out = inidoc_serialize(doc, NULL);
        disarm();
        CHECK_RUN(failed ? out == NULL : (out != NULL && strcmp(out, CONFIG_TEXT) == 0), n);
        alloc_free(out);
        if (!failed)
            break;
    }
    inidoc_free(doc);
    no_leak(__LINE__, 0);
}

// A function to test whether a file holds exactly a text
static bool holds(const char *path, const char *text)
{
    char *content = fileio_read_all(path, NULL);
    bool same = content != NULL && strcmp(content, text) == 0;
    alloc_free(content);
    return same;
}

// A function to prove that a save fails cleanly: refused with a reason, with the config, its older
// backup and the folder all as they were; or done, with the old file kept as the backup
static void prove_config_save(void)
{
    static const char *const older = "; an older backup\n";
    static const char *const saved =
        "; my launcher\n"
        "[General]\n"
        "DefaultMenu=Main\n"
        "\n"
        "[Layout]\n"
        "Rows=2\n"
        "MaxButtons=5 ; the old name\n"
        "\n"
        "[Main]\n"
        "Rows=3\n"
        "Entry1=Kodi;kodi.png;kodi\n"
        "Entry2=Plex;plex.png;plex\n"
        "\n"
        "[Background]\n"
        "Mode=Image\n";
    ConfigEdit edits[] = {
        { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY },
        { "Layout", "Columns", "MaxButtons", "5", INIDOC_AFTER_LAST_KEY },
        { "Main", "Rows", NULL, "3", INIDOC_UNDER_HEADER },
        { "Background", "Mode", NULL, "Image", INIDOC_AFTER_LAST_KEY }
    };
    CHECK(fileio_make_dirs(DIR));
    for (int n = 1;; n++) {
        fileio_remove(CONFIG ".tmp");
        fileio_remove(CONFIG ".bak.tmp");
        CHECK(fileio_write_all(CONFIG, CONFIG_TEXT, strlen(CONFIG_TEXT)));
        CHECK(fileio_write_all(CONFIG ".bak", older, strlen(older)));
        ConfigSaveResult result;
        arm(n);
        bool ok = config_save(CONFIG, NULL, NULL, edits, 4, &result);
        disarm();
        if (ok) {
            CHECK_RUN(holds(CONFIG, saved), n);
            CHECK_RUN(holds(CONFIG ".bak", CONFIG_TEXT), n);
        }
        else {
            CHECK_RUN(failed && result.why[0] != '\0', n);
            CHECK_RUN(holds(CONFIG, CONFIG_TEXT), n);
            CHECK_RUN(holds(CONFIG ".bak", older) || holds(CONFIG ".bak", CONFIG_TEXT), n);   // Kept, or a new copy of the same file
        }
        CHECK_RUN(!fileio_exists(CONFIG ".tmp"), n);
        CHECK_RUN(!fileio_exists(CONFIG ".bak.tmp"), n);
        runs = n;
        if (!no_leak(__LINE__, n) || !failed)
            break;
    }
    report("saving");
}

// A function to leave a reason behind that no proof expects, so each proof's "out of memory" must
// come from the call under test
static void set_other_reason(void)
{
    CHECK(fileio_read_all(DIR "/no-such-file", NULL) == NULL);
}

// A function to prove that listing a folder fails cleanly: -1 with "out of memory" and no list,
// never a shorter list. The folder holds more names than the list's first 32 places, so the list
// grows during it.
static void prove_list(void)
{
    char path[64];
    CHECK(fileio_make_dirs(DIR "/many"));
    for (int i = 0; i < 40; i++) {
        snprintf(path, sizeof(path), DIR "/many/file-%02d.png", i);
        CHECK(fileio_write_all(path, "x", 1));
    }
    for (int n = 1;; n++) {
        FileioEntry *entries = NULL;
        set_other_reason();
        arm(n);
        int count = fileio_list(DIR "/many", &entries);
        disarm();
        if (failed) {
            CHECK_RUN(count == -1 && entries == NULL, n);
            CHECK_RUN(strcmp(fileio_last_error(), "out of memory") == 0, n);
        }
        else
            CHECK_RUN(count == 40, n);
        fileio_free_list(entries, count > 0 ? count : 0);
        runs = n;
        if (!no_leak(__LINE__, n) || !failed)
            break;
    }
    report("listing a folder");
}

// A function to prove that reading a file that has to grow its buffer (over 4096 bytes) fails
// cleanly: NULL with "out of memory", or the whole file
static void prove_read_all(void)
{
    static char big[10000];
    memset(big, 'x', sizeof(big) - 1);
    CHECK(fileio_write_all(DIR "/big.txt", big, sizeof(big) - 1));
    for (int n = 1;; n++) {
        size_t length = 0;
        set_other_reason();
        arm(n);
        char *text = fileio_read_all(DIR "/big.txt", &length);
        disarm();
        if (failed)
            CHECK_RUN(text == NULL && strcmp(fileio_last_error(), "out of memory") == 0, n);
        else
            CHECK_RUN(text != NULL && length == sizeof(big) - 1 && memcmp(text, big, length) == 0, n);
        alloc_free(text);
        runs = n;
        if (!no_leak(__LINE__, n) || !failed)
            break;
    }
    report("reading a file that outgrows its first buffer");
}

// A function to prove that making folders fails cleanly: false with "out of memory", or made
static void prove_make_dirs(void)
{
    char path[64];
    for (int n = 1;; n++) {
        snprintf(path, sizeof(path), DIR "/made/%d/a/b", n);   // A new path each run, so there is always one to make
        set_other_reason();
        arm(n);
        bool made = fileio_make_dirs(path);
        disarm();
        if (failed)
            CHECK_RUN(!made && strcmp(fileio_last_error(), "out of memory") == 0, n);
        else
            CHECK_RUN(made && fileio_is_dir(path), n);
        runs = n;
        if (!no_leak(__LINE__, n) || !failed)
            break;
    }
    report("making folders");
}

// A function to prove that finding a file's full path fails cleanly: false with "out of memory",
// or the path (Linux allocates nothing for it)
static void prove_full_path(void)
{
    CHECK(fileio_write_all(DIR "/full.txt", "x", 1));
    char expected[1024];
    CHECK(fileio_full_path(DIR "/full.txt", expected, sizeof(expected)));
    for (int n = 1;; n++) {
        char path[1024] = "";
        set_other_reason();
        arm(n);
        bool found = fileio_full_path(DIR "/full.txt", path, sizeof(path));
        disarm();
        if (failed)
            CHECK_RUN(!found && path[0] == '\0' && strcmp(fileio_last_error(), "out of memory") == 0, n);
        else
            CHECK_RUN(found && strcmp(path, expected) == 0, n);
        runs = n;
        if (!no_leak(__LINE__, n) || !failed)
            break;
    }
    report("finding a file's full path");
}

// A function to tell whether two lists of places are the same, place by place
static bool same_places(const FileioPlace *a, int a_count, const FileioPlace *b, int b_count)
{
    if (a_count != b_count)
        return false;
    for (int i = 0; i < a_count; i++) {
        if (strcmp(a[i].label, b[i].label) != 0 || strcmp(a[i].path, b[i].path) != 0 || a[i].network != b[i].network)
            return false;
    }
    return true;
}

// A function to prove that listing the places fails cleanly: -1 with "out of memory" and no list,
// never a list with a place missing. `under`, when set, lists that folder the way /media is listed.
static void prove_places(const char *what, const char *under)
{
    FileioPlace *expected = NULL;
#ifndef _WIN32
    int expected_count = under != NULL ? fileio_places_under(under, &expected) : fileio_places(&expected);
#else
    (void) under;
    int expected_count = fileio_places(&expected);
#endif
    CHECK(expected_count >= 1);
    for (int n = 1;; n++) {
        FileioPlace *places = NULL;
        set_other_reason();
        arm(n);
#ifndef _WIN32
        int count = under != NULL ? fileio_places_under(under, &places) : fileio_places(&places);
#else
        int count = fileio_places(&places);
#endif
        disarm();
        if (failed) {
            CHECK_RUN(count == -1 && places == NULL, n);
            CHECK_RUN(strcmp(fileio_last_error(), "out of memory") == 0, n);
        }
        else
            CHECK_RUN(same_places(places, count, expected, expected_count), n);
        fileio_free_places(places, count > 0 ? count : 0);
        runs = n;
        if (!failed)
            break;
    }
    fileio_free_places(expected, expected_count);
    no_leak(__LINE__, runs);
    report(what);
}

#ifndef _WIN32
// A function to set an environment variable, or clear it when `value` is NULL
static void set_variable(const char *name, const char *value)
{
    if (value != NULL)
        setenv(name, value, 1);
    else
        unsetenv(name);
}

// A function to prove the Linux places with a Home that has a user-dirs.dirs (so Pictures is read
// from it) and a /media-like folder holding the user's own folder of drives
static void prove_linux_places(void)
{
    const char *variables[] = { "HOME", "XDG_PICTURES_DIR", "XDG_CONFIG_HOME", "USER" };
    char *saved[4];
    for (int i = 0; i < 4; i++) {
        const char *value = getenv(variables[i]);
        saved[i] = value != NULL ? strdup(value) : NULL;
    }
    CHECK(fileio_make_dirs(DIR "/home/.config"));
    CHECK(fileio_make_dirs(DIR "/home/Bilder"));
    const char *dirs = "XDG_PICTURES_DIR=\"$HOME/Bilder\"\n";
    CHECK(fileio_write_all(DIR "/home/.config/user-dirs.dirs", dirs, strlen(dirs)));
    CHECK(fileio_make_dirs(DIR "/media/streamflex-tester/USB STICK"));
    CHECK(fileio_make_dirs(DIR "/media/someone-else"));
    char home[1024];
    char media[1024];
    CHECK(fileio_real_path(DIR "/home", home, sizeof(home)));
    CHECK(fileio_real_path(DIR "/media", media, sizeof(media)));
    set_variable("HOME", home);
    set_variable("XDG_PICTURES_DIR", NULL);
    set_variable("XDG_CONFIG_HOME", NULL);
    set_variable("USER", "streamflex-tester");
    prove_places("listing the places, Pictures from user-dirs.dirs", NULL);
    prove_places("listing a /media folder with the user's drives in it", media);
    for (int i = 0; i < 4; i++) {
        set_variable(variables[i], saved[i]);
        free(saved[i]);
    }
}
#endif

// Whether the pretend list function was the allocation that failed, in the run under way
static bool list_failed = false;

// A pretend file system for the browser, allocating as fileio_list() does
static int fake_list(const char *folder, FileioEntry **entries, void *context)
{
    (void) context;
    static const char *const pictures[] = { "Autumn/", "beach.png", "zebra.jpg", ".hidden.png", "notes.txt" };
    static const char *const autumn[] = { "a.jpg", "b.jpg" };
    const char *const *names;
    int count;
    if (strcmp(folder, "/home/me/Pictures") == 0) {
        names = pictures;
        count = 5;
    }
    else if (strcmp(folder, "/home/me/Pictures/Autumn") == 0) {
        names = autumn;
        count = 2;
    }
    else if (strcmp(folder, "/home/me") == 0 || strcmp(folder, "/home") == 0) {
        static const char *const one[] = { "me/" };
        static const char *const pictures_only[] = { "Pictures/" };
        names = strcmp(folder, "/home") == 0 ? one : pictures_only;
        count = 1;
    }
    else
        return -1;
    *entries = alloc_calloc((size_t) count, sizeof(FileioEntry));
    if (*entries == NULL) {
        list_failed = true;
        return -1;
    }
    for (int i = 0; i < count; i++) {
        size_t length = strlen(names[i]);
        bool is_dir = names[i][length - 1] == '/';
        char *name = alloc_strdup(names[i]);
        if (name == NULL) {
            fileio_free_list(*entries, i);
            *entries = NULL;
            list_failed = true;
            return -1;
        }
        if (is_dir)
            name[length - 1] = '\0';
        (*entries)[i] = (FileioEntry) { .name = name, .is_dir = is_dir, .is_file = !is_dir, .hidden = name[0] == '.' };
    }
    return count;
}

// A function to describe what the browser shows, for comparing it before and after a command
static void describe_browser(const Browser *browser, char *out, size_t size)
{
    int used = snprintf(out, size, "%s|%d|", browser_folder(browser) != NULL ? browser_folder(browser) : "(places)",
                        browser_cursor(browser));
    for (int i = 0; i < browser_row_count(browser) && used > 0 && (size_t) used < size; i++) {
        const BrowserRow *row = browser_row(browser, i);
        used += snprintf(out + used, size - (size_t) used, "%s=%s,%d;", row->name, row->path, row->enabled);
    }
}

static const BrowserPlace PLACES[] = {
    { "Pictures", "/home/me/Pictures", false },
    { "Home", "/home/me", false }
};

// A function to prove that the browser fails cleanly: opening gives NULL, and "out of memory" as
// its reason, when its own memory runs out (a folder whose list failed is one that cannot be listed, so the next place opens), and a
// command that fails leaves what is on show as it was and says why
static void prove_browser(void)
{
    for (int n = 1;; n++) {
        list_failed = false;
        const char *why = "not set";
        arm(n);
        Browser *browser = browser_open(BROWSER_IMAGE, "/home/me/Pictures/zebra.jpg", PLACES, 2, fake_list, NULL, NULL, &why);
        disarm();
        if (!failed)
            CHECK_RUN(browser != NULL && browser_row_count(browser) == 3 && browser_cursor(browser) == 2 && why == NULL, n);
        else if (!list_failed)
            CHECK_RUN(browser == NULL && why != NULL && strcmp(why, "out of memory") == 0, n);
        else
            CHECK_RUN(browser != NULL && strcmp(browser_folder(browser), "/home/me/Pictures") == 0, n);
        browser_free(browser);
        runs = n;
        if (!no_leak(__LINE__, n) || !failed)
            break;
    }
    report("opening the browser");

    // Into a folder (OK), and back out of it (Back)
    static const BrowserCommand commands[] = { BROWSER_OK, BROWSER_BACK };
    static const char *const after[] = { "/home/me/Pictures/Autumn", "/home/me" };
    for (int c = 0; c < 2; c++) {
        for (int n = 1;; n++) {
            Browser *browser = browser_open(BROWSER_IMAGE, "/home/me/Pictures", PLACES, 2, fake_list, NULL, NULL, NULL);
            char before[2048];
            char now[2048];
            describe_browser(browser, before, sizeof(before));
            list_failed = false;
            arm(n);
            BrowserResult result = browser_command(browser, commands[c], 10);
            disarm();
            describe_browser(browser, now, sizeof(now));
            // Back from a folder whose parent cannot be listed (here, because the list ran out of
            // memory) goes to the places, as it does for any parent that cannot be listed
            bool places = commands[c] == BROWSER_BACK && list_failed && browser_folder(browser) == NULL;
            if (result == BROWSER_MOVED)
                CHECK_RUN(places || (browser_folder(browser) != NULL && strcmp(browser_folder(browser), after[c]) == 0), n);
            else {
                CHECK_RUN(failed && result == BROWSER_NONE, n);
                CHECK_RUN(strcmp(before, now) == 0, n);
                const char *why = browser_why(browser);
                CHECK_RUN(list_failed ? why == NULL : (why != NULL && strcmp(why, "out of memory") == 0), n);
            }
            browser_free(browser);
            runs = n;
            if (!no_leak(__LINE__, n) || !failed)
                break;
        }
        report(commands[c] == BROWSER_OK ? "going into a folder" : "going back out of it");
    }

    // A folder's first image, for its preview: the right one, or none
    Browser *browser = browser_open(BROWSER_FOLDER, "/home/me", PLACES, 2, fake_list, NULL, NULL, NULL);
    for (int n = 1;; n++) {
        char out[BROWSER_PATH_MAX] = "";
        arm(n);
        bool found = browser_first_image(browser, "/home/me/Pictures", out, sizeof(out));
        disarm();
        if (failed)
            CHECK_RUN(!found, n);
        else
            CHECK_RUN(found && strcmp(out, "/home/me/Pictures/beach.png") == 0, n);
        runs = n;
        if (!failed)
            break;
    }
    browser_free(browser);
    no_leak(__LINE__, runs);
    report("finding a folder's first image");
}

// A function to prove that removing a key allocates nothing, so it cannot fail for want of memory
static void prove_remove(void)
{
    IniDoc *doc = inidoc_parse(CONFIG_TEXT, strlen(CONFIG_TEXT));
    arm(1);
    bool removed = inidoc_remove(doc, "Layout", "Rows");
    disarm();
    CHECK(removed && !failed && calls == 0);
    inidoc_free(doc);
    no_leak(__LINE__, 1);
}

// A function to prove that the settings model fails cleanly when it cannot be made
static void prove_settings(void)
{
    static const char *const names[] = { "Main", "Games" };
    for (int n = 1;; n++) {
        arm(n);
        SettingsState *state = settings_create(names, 2);
        disarm();
        CHECK_RUN(failed ? state == NULL : state != NULL, n);
        settings_free(state);
        runs = n;
        if (!no_leak(__LINE__, n) || !failed)
            break;
    }
    report("making the settings model");
}

// A function to prove that naming the pads fails cleanly: a pad whose name could not be copied has
// none, and nothing is left allocated once the model is freed
static void prove_pads(void)
{
    static const char *const names[] = { "Main" };
    static const char *const pads[] = { "Xbox Controller", "8BitDo Pro 2" };
    for (int n = 1;; n++) {
        SettingsState *state = settings_create(names, 1);
        arm(n);
        settings_set_pads(state, pads, 2);
        disarm();
        CHECK_RUN(settings_pad_count(state) == 2, n);
        for (int i = 0; i < 2; i++) {
            // Allocation n copies pads[n - 1]: that pad, when it failed, falls back to "Pad N"
            char fallback[16];
            snprintf(fallback, sizeof(fallback), "Pad %d", i);
            const char *name = settings_pad_name(state, i);
            CHECK_RUN(name != NULL && strcmp(name, failed && i == n - 1 ? fallback : pads[i]) == 0, n);
        }
        settings_free(state);
        runs = n;
        if (!no_leak(__LINE__, n) || !failed)
            break;
    }
    report("naming the pads");
}

// A function to prove that the list picker fails cleanly: a row that could not be added is not
// there, a Custom row that could not be pinned is not either, and nothing is left allocated
static void prove_listpick(void)
{
    for (int n = 1;; n++) {
        arm(n);
        ListPick *pick = listpick_create();
        bool added = pick != NULL && listpick_add(pick, "Left", ":left", true, NULL) &&
                     listpick_add(pick, "Right", ":right", true, NULL);
        bool pinned = added && listpick_select(pick, "custom command", "Custom: custom command");
        disarm();
        CHECK_RUN(failed || (added && pinned), n);
        if (!failed)
            CHECK_RUN(listpick_count(pick) == 3 && listpick_row(pick, 0)->custom, n);
        else if (pick != NULL) {
            // Allocations 2-4 make "Left" (label, value, the rows), 5-6 "Right", 7-8 the Custom row:
            // the one that failed is not there, and those before it are
            int kept = n <= 4 ? 0 : n <= 6 ? 1 : 2;
            CHECK_RUN(!added || !pinned, n);
            CHECK_RUN(listpick_count(pick) == kept, n);
            CHECK_RUN(listpick_count(pick) == 0 || !listpick_row(pick, 0)->custom, n);
        }
        listpick_free(pick);
        runs = n;
        if (!no_leak(__LINE__, n) || !failed)
            break;
    }
    report("the list picker");
}

// A function to prove that the font list fails cleanly: a face that could not be added is not
// there, and nothing is left allocated
static void prove_fontlist(void)
{
    for (int n = 1;; n++) {
        arm(n);
        FontList *list = fontlist_create();
        bool added = list != NULL && fontlist_add(list, "/f/a.ttf", 0, "A", "Bold", false) &&
                     fontlist_add(list, "/f/a.ttf", 1, "A", "Regular", false) &&
                     fontlist_add(list, "/f/b.ttf", 0, "B", "Regular", true);
        disarm();
        if (list != NULL)
            fontlist_finish(list);
        CHECK_RUN(failed || (added && fontlist_count(list) == 2 && fontlist_face(list, 1) == 1), n);
        fontlist_free(list);
        runs = n;
        if (!no_leak(__LINE__, n) || !failed)
            break;
    }
    report("the font list");
}

// A function to prove that sorting the font list fails cleanly: out of memory, the families keep
// the order they were found in, and each face still finds its family
static void prove_fontlist_finish(void)
{
    for (int n = 1;; n++) {
        FontList *list = fontlist_create();
        CHECK(list != NULL && fontlist_add(list, "/f/b.ttf", 0, "B", "Regular", false) &&
              fontlist_add(list, "/f/a.ttf", 0, "A", "Regular", false));
        arm(n);
        fontlist_finish(list);
        disarm();
        const char *first = failed ? "B" : "A";
        CHECK_RUN(fontlist_count(list) == 2 && strcmp(fontlist_family(list, 0), first) == 0, n);
        CHECK_RUN(fontlist_find(list, "/f/a.ttf", 0) == (failed ? 1 : 0), n);
        fontlist_free(list);
        runs = n;
        if (!no_leak(__LINE__, n) || !failed)
            break;
    }
    report("sorting the font list");
}

// A function to prove that list mode's add and set fail cleanly: refused, saying "out of memory",
// with the document as it was, and no later add refused for it
static void prove_list_mode(void)
{
    static const char *const text = "[Hotkeys]\nHotkey1=#4000003A;:quit\n\n[Main]\nEntry1=One;apps;:quit\n";
    static const char *const after_set = "[Hotkeys]\nHotkey1=#40000045;:home\n\n[Main]\nEntry1=One;apps;:quit\n";
    for (int n = 1;; n++) {
        IniDoc *doc = inidoc_parse(text, strlen(text));
        IniDocItem items[2];
        inidoc_list(doc, "Hotkeys", NULL, items, 2);
        arm(n);
        bool set = inidoc_list_set(doc, items[0].line, "Hotkey1", "#40000045;:home");
        bool added = set && inidoc_list_add(doc, "Hotkeys", "Hotkey2", "#4000003A;:up");
        disarm();
        char *out = text_of(doc);
        if (!failed)
            CHECK_RUN(set && added && strstr(out, "Hotkey1=#40000045;:home\nHotkey2=#4000003A;:up\n") != NULL, n);
        else {
            CHECK_RUN(strcmp(inidoc_why(doc), "out of memory") == 0, n);
            CHECK_RUN(out != NULL && strcmp(out, set ? after_set : text) == 0, n);   // As it was before the call that failed
            CHECK_RUN(inidoc_list_add(doc, "Hotkeys", "Hotkey3", ":x"), n);           // A refusal for memory does not stick
        }
        alloc_free(out);
        inidoc_free(doc);
        runs = n;
        if (!no_leak(__LINE__, n) || !failed)
            break;
    }
    report("list mode");
}

// A function to prove that a save with list edits fails cleanly: refused with a reason (memory's) and
// the config as it was, or done in full
static void prove_config_save_lists(void)
{
    static const char *const before = "[Hotkeys]\nHotkey1=#4000003A;:quit\nHotkey2=#4000003B;:home\n";
    static const char *const after = "[Hotkeys]\nHotkey1=#4000003A;:settings\nHotkey2=#4000003C;:back\n";
    ConfigListEdit lists[] = {
        { CONFIG_LIST_SET, "Hotkeys", "Hotkey1=#4000003A;:quit", "Hotkey", true, "#4000003A;:settings" },
        { CONFIG_LIST_REMOVE, "Hotkeys", "Hotkey2=#4000003B;:home", NULL, false, NULL },
        { CONFIG_LIST_ADD, "Hotkeys", NULL, "Hotkey", true, "#4000003C;:back" }
    };
    CHECK(fileio_make_dirs(DIR));
    for (int n = 1;; n++) {
        fileio_remove(CONFIG ".tmp");
        fileio_remove(CONFIG ".bak.tmp");
        CHECK(fileio_write_all(CONFIG, before, strlen(before)));
        ConfigSaveResult result;
        arm(n);
        bool ok = config_save_all(CONFIG, NULL, NULL, NULL, 0, lists, 3, &result);
        disarm();
        if (ok)
            CHECK_RUN(holds(CONFIG, after), n);
        else {
            CHECK_RUN(failed && result.why[0] != '\0' && holds(CONFIG, before), n);
            CHECK_RUN(strstr(result.why, "out of memory") != NULL, n);   // Memory's reason, never another's
        }
        CHECK_RUN(!fileio_exists(CONFIG ".tmp"), n);
        runs = n;
        if (!no_leak(__LINE__, n) || !failed)
            break;
    }
    report("saving list edits");
}

// A function to prove that loading and changing bindings fails cleanly: a list that could not be
// loaded is empty, and Discard leaves it so; a binding that could not be added is not there; and
// nothing is left allocated. Eight lines fill the list's first block, so the addition must grow it.
static void prove_bindings(void)
{
    static const char *const text = "[Hotkeys]\nHotkey1=#4000003A;:quit\nHotkey2=#4000003B;:home\n"
        "Hotkey3=#4000003D;:up\nHotkey4=#4000003E;:down\nHotkey5=#4000003F;:left\nHotkey6=#40000040;:right\n"
        "Hotkey7=#40000041;:select\nHotkey8=#40000042;:back\n";
    for (int n = 1;; n++) {
        IniDoc *doc = inidoc_parse(text, strlen(text));
        IniDocItem items[8];
        int count = inidoc_list(doc, "Hotkeys", NULL, items, 8);
        arm(n);
        Bindings *b = bindings_create(false, true);
        bool loaded = b != NULL && bindings_load(b, BINDINGS_KEYBOARD, items, count);
        bool ok = loaded && bindings_set(b, BINDINGS_KEYBOARD, -1, 0x4000003C, ":back") == 8;
        disarm();
        CHECK_RUN(failed || ok, n);
        if (b != NULL && !ok) {
            CHECK_RUN(bindings_count(b, BINDINGS_KEYBOARD) == (loaded ? 8 : 0), n);
            bindings_discard(b);
            CHECK_RUN(bindings_count(b, BINDINGS_KEYBOARD) == (loaded ? 8 : 0), n);
        }
        bindings_free(b);
        inidoc_free(doc);
        runs = n;
        if (!no_leak(__LINE__, n) || !failed)
            break;
    }
    report("the bindings");
}

// A function to prove that loading a list again fails cleanly: out of memory, the list is empty, and
// Discard does not bring back the list loaded before it. Nine lines make the list grow past its first
// block, so both of the load's allocations can fail.
static void prove_bindings_reload(void)
{
    static const char *const text = "[Hotkeys]\nHotkey1=#4000003A;:quit\nHotkey2=#4000003B;:home\n"
        "Hotkey3=#4000003D;:up\nHotkey4=#4000003E;:down\nHotkey5=#4000003F;:left\nHotkey6=#40000040;:right\n"
        "Hotkey7=#40000041;:select\nHotkey8=#40000042;:back\nHotkey9=#40000043;:settings\n";
    for (int n = 1;; n++) {
        IniDoc *doc = inidoc_parse(text, strlen(text));
        IniDocItem items[9];
        int count = inidoc_list(doc, "Hotkeys", NULL, items, 9);
        Bindings *b = bindings_create(false, true);
        CHECK(b != NULL && bindings_load(b, BINDINGS_KEYBOARD, items, 1));
        arm(n);
        bool ok = bindings_load(b, BINDINGS_KEYBOARD, items, count);
        disarm();
        CHECK_RUN(failed != ok, n);
        int expected = ok ? 9 : 0;
        CHECK_RUN(bindings_count(b, BINDINGS_KEYBOARD) == expected, n);
        bindings_discard(b);
        CHECK_RUN(bindings_count(b, BINDINGS_KEYBOARD) == expected, n);
        bindings_free(b);
        inidoc_free(doc);
        runs = n;
        if (!no_leak(__LINE__, n) || !failed)
            break;
    }
    report("loading the bindings again");
}

int main(void)
{
    AllocHooks hooks = { test_reallocate, test_release };
    alloc_set_hooks(&hooks);
    prove_parse();
    prove_set("Layout", "Rows", "2", INIDOC_AFTER_LAST_KEY,
        "; my launcher\n[General]\nDefaultMenu=Main\n\n[Layout]\nRows=2\nMaxButtons=4 ; the old name\n\n"
        "[Main]\nEntry1=Kodi;kodi.png;kodi\nEntry2=Plex;plex.png;plex\n");
    prove_set("Main", "Rows", "3", INIDOC_UNDER_HEADER,
        "; my launcher\n[General]\nDefaultMenu=Main\n\n[Layout]\nRows=1\n  continued\nMaxButtons=4 ; the old name\n\n"
        "[Main]\nRows=3\nEntry1=Kodi;kodi.png;kodi\nEntry2=Plex;plex.png;plex\n");
    prove_set("Background", "Mode", "Image", INIDOC_AFTER_LAST_KEY,
        "; my launcher\n[General]\nDefaultMenu=Main\n\n[Layout]\nRows=1\n  continued\nMaxButtons=4 ; the old name\n\n"
        "[Main]\nEntry1=Kodi;kodi.png;kodi\nEntry2=Plex;plex.png;plex\n\n[Background]\nMode=Image\n");
    prove_serialize();
    prove_config_save();
    prove_list();
    prove_read_all();
    prove_make_dirs();
    prove_full_path();
#ifdef _WIN32
    prove_places("listing the places", NULL);
#else
    prove_linux_places();
#endif
    prove_browser();
    prove_remove();
    prove_settings();
    prove_pads();
    prove_listpick();
    prove_fontlist();
    prove_fontlist_finish();
    prove_list_mode();
    prove_config_save_lists();
    prove_bindings();
    prove_bindings_reload();
    alloc_set_hooks(NULL);
    return check_report();
}
