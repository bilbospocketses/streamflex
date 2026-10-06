#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "config_save.h"
#include "fileio.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define DIR "config-save-fixture"
#define CONFIG DIR "/config.ini"

static const char *const ORIGINAL =
    "; my launcher\n"
    "[Layout]\n"
    "Rows=1\n"
    "MaxButtons=4 ; the old name\n"
    "\n"
    "[Games]\n"
    "IconSize=128\n"
    "Entry1=One;apps;:quit\n";

// A function to make a file read-only or writable again
static void set_read_only(const char *path, bool read_only)
{
#ifdef _WIN32
    SetFileAttributesA(path, read_only ? FILE_ATTRIBUTE_READONLY : FILE_ATTRIBUTE_NORMAL);
#else
    chmod(path, read_only ? 0444 : 0644);
#endif
}

// A function to tell whether read-only files can be tested: root writes them anyway
static bool can_test_read_only(void)
{
#ifndef _WIN32
    if (geteuid() == 0) {
        printf("skipped a read-only check: running as root\n");
        return false;
    }
#endif
    return true;
}

// A function to remove an empty folder, if there is one
static void remove_folder(const char *path)
{
#ifdef _WIN32
    RemoveDirectoryA(path);
#else
    rmdir(path);
#endif
}

// A function to start a check from a known file, with no backup or temporary file lying about
static void reset(const char *path, const char *text)
{
    char other[256];
    set_read_only(path, false);
    fileio_remove(path);
    snprintf(other, sizeof(other), "%s.bak", path);
    fileio_remove(other);
    snprintf(other, sizeof(other), "%s.tmp", path);
    fileio_remove(other);
    snprintf(other, sizeof(other), "%s.bak.tmp", path);
    fileio_remove(other);
    remove_folder(other);   // test_older_backup_survives_a_failed_backup() puts a folder there
    CHECK(fileio_make_dirs(DIR));
    if (text != NULL)
        CHECK(fileio_write_all(path, text, strlen(text)));
}

// A function to test whether a file holds exactly a text
static bool holds(const char *path, const char *text)
{
    char *content = fileio_read_all(path, NULL);
    bool same = content != NULL && strcmp(content, text) == 0;
    free(content);
    return same;
}

// A function to test the edits: a changed value, an alias edited in place, a removed key and a
// new key under a menu header, with the old file kept as .bak and no .tmp left behind
static void test_saves_only_the_edits(void)
{
    reset(CONFIG, ORIGINAL);
    ConfigEdit edits[] = {
        { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY },
        { "Layout", "Columns", "MaxButtons", "5", INIDOC_AFTER_LAST_KEY },
        { "Games", "IconSize", NULL, NULL, INIDOC_UNDER_HEADER },
        { "Games", "Rows", NULL, "3", INIDOC_UNDER_HEADER }
    };
    ConfigSaveResult result;
    CHECK(config_save(CONFIG, NULL, NULL, edits, 4, &result));
    CHECK(holds(CONFIG,
        "; my launcher\n"
        "[Layout]\n"
        "Rows=2\n"
        "MaxButtons=5 ; the old name\n"
        "\n"
        "[Games]\n"
        "Rows=3\n"
        "Entry1=One;apps;:quit\n"));
    CHECK(strstr(result.path, "config-save-fixture") != NULL);
    CHECK(strstr(result.backup, "config.ini.bak") != NULL);
    CHECK(holds(CONFIG ".bak", ORIGINAL));
    CHECK(!fileio_exists(CONFIG ".tmp"));
    CHECK(!fileio_exists(CONFIG ".bak.tmp"));
    CHECK_STR(result.warning, "");
}

// A function to test that what a save could not keep reaches the result: the replace is made to
// fail to keep the file's permissions (Linux) or attributes (Windows), which no real file can be
// made to do to the .tmp the save has just written, and the save still succeeds
static void test_warning_is_passed_on(void)
{
    reset(CONFIG, ORIGINAL);
    ConfigEdit edit = { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    fileio_set_fault(FILEIO_FAULT_KEEP, 0, 0);
    bool saved = config_save(CONFIG, NULL, NULL, &edit, 1, &result);
    fileio_set_fault(FILEIO_FAULT_NONE, 0, 0);
    CHECK(saved);
    CHECK(strstr(result.warning, "could not be kept") != NULL);
    char *content = fileio_read_all(CONFIG, NULL);
    CHECK(content != NULL && strstr(content, "Rows=2\n") != NULL);
    free(content);
}

// A function to test a key beside its older alias: the save leaves the key alone, whether it is
// set or removed, so the file never holds two lines for one setting
static void test_alias_beside_the_key_is_removed(void)
{
    const char *both =
        "[Layout]\n"
        "MaxButtons=3 ; the old name\n"
        "Columns=4\n"
        "Rows=1\n";
    reset(CONFIG, both);
    ConfigEdit edit = { "Layout", "Columns", "MaxButtons", "5", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(config_save(CONFIG, NULL, NULL, &edit, 1, &result));
    CHECK(holds(CONFIG,
        "[Layout]\n"
        "Columns=5\n"
        "Rows=1\n"));

    reset(CONFIG, both);
    edit.value = NULL;
    CHECK(config_save(CONFIG, NULL, NULL, &edit, 1, &result));
    CHECK(holds(CONFIG,
        "[Layout]\n"
        "Rows=1\n"));
}

// A function to test that a change made on disk meanwhile, by hand, survives the save
static void test_keeps_a_hand_edit_made_meanwhile(void)
{
    reset(CONFIG, ORIGINAL);
    const char *edited_meanwhile =
        "; my launcher\n"
        "[Layout]\n"
        "Rows=1\n"
        "MaxButtons=4 ; the old name\n"
        "\n"
        "[Games]\n"
        "Columns=6\n"
        "IconSize=128\n"
        "Entry1=One;apps;:quit\n";
    CHECK(fileio_write_all(CONFIG, edited_meanwhile, strlen(edited_meanwhile)));
    ConfigEdit edit = { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(config_save(CONFIG, NULL, NULL, &edit, 1, &result));
    char *content = fileio_read_all(CONFIG, NULL);
    CHECK(content != NULL && strstr(content, "Columns=6\n") != NULL && strstr(content, "Rows=2\n") != NULL);
    free(content);
}

// A function to test that a value config.ini cannot hold fails the save and leaves the file alone
static void test_refused_value_changes_nothing(void)
{
    reset(CONFIG, ORIGINAL);
    ConfigEdit edit = { "Background", "Image", NULL, "/a ;b.png", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(!config_save(CONFIG, NULL, NULL, &edit, 1, &result));
    CHECK(strstr(result.why, "comment") != NULL);
    CHECK(holds(CONFIG, ORIGINAL));
    CHECK(!fileio_exists(CONFIG ".bak"));
    CHECK(!fileio_exists(CONFIG ".tmp"));
    CHECK(!fileio_exists(CONFIG ".bak.tmp"));
}

// A function to test that a value that fits alone but not beside the comment on its line fails
// with that reason, and leaves the file alone
static void test_refused_beside_its_comment(void)
{
    const char *commented = "[Background]\nImage=x ; a long comment that takes up the room on this line\n";
    reset(CONFIG, commented);
    char value[256];
    memset(value, 'a', 150);
    value[150] = '\0';
    ConfigEdit edit = { "Background", "Image", NULL, value, INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(!config_save(CONFIG, NULL, NULL, &edit, 1, &result));
    CHECK(strstr(result.why, "with its comment") != NULL);
    CHECK(holds(CONFIG, commented));
    CHECK(!fileio_exists(CONFIG ".bak"));
}

// A function to test that a backup that cannot be written fails the save before anything else
// changes: the older backup and the config are both left as they were
static void test_older_backup_survives_a_failed_backup(void)
{
    reset(CONFIG, ORIGINAL);
    const char *older = "; an older backup\n";
    CHECK(fileio_write_all(CONFIG ".bak", older, strlen(older)));
    CHECK(fileio_make_dirs(CONFIG ".bak.tmp"));   // A folder in the way: the backup cannot be written
    ConfigEdit edit = { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(!config_save(CONFIG, NULL, NULL, &edit, 1, &result));
    CHECK(strstr(result.why, "could not write the backup") != NULL);
    CHECK(holds(CONFIG ".bak", older));
    CHECK(holds(CONFIG, ORIGINAL));
    CHECK(!fileio_exists(CONFIG ".tmp"));
    remove_folder(CONFIG ".bak.tmp");
}

// A function to test that a read-only file fails with the reason and is left alone
static void test_read_only_fails(void)
{
    if (!can_test_read_only())
        return;
    reset(CONFIG, ORIGINAL);
    set_read_only(CONFIG, true);
    ConfigEdit edit = { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(!config_save(CONFIG, NULL, NULL, &edit, 1, &result));
    CHECK(strstr(result.why, "permission denied") != NULL);
    CHECK(holds(CONFIG, ORIGINAL));
    set_read_only(CONFIG, false);
}

// A function to test the fallback: a read-only config under the system prefix is saved as the
// user's own copy, and the system copy is left alone
static void test_system_copy_falls_back_to_the_user_config(void)
{
    if (!can_test_read_only())
        return;
    const char *system_config = DIR "/system/config.ini";
    const char *user_config = DIR "/home/.config/streamflex/config.ini";
    CHECK(fileio_make_dirs(DIR "/system"));
    reset(system_config, ORIGINAL);
    reset(user_config, NULL);
    set_read_only(system_config, true);
    char prefix[CONFIG_SAVE_PATH_MAX];
    CHECK(fileio_real_path(DIR "/system", prefix, sizeof(prefix) - 1));
    size_t used = strlen(prefix);
    snprintf(prefix + used, sizeof(prefix) - used, "/");
    ConfigEdit edit = { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(config_save(system_config, prefix, user_config, &edit, 1, &result));
    CHECK_STR(result.path, user_config);
    CHECK_STR(result.backup, "");
    CHECK(holds(system_config, ORIGINAL));
    char *content = fileio_read_all(user_config, NULL);
    CHECK(content != NULL && strstr(content, "Rows=2\n") != NULL && strstr(content, "; my launcher\n") != NULL);
    free(content);
    set_read_only(system_config, false);

    // Without a system prefix there is no fallback
    set_read_only(system_config, true);
    CHECK(!config_save(system_config, NULL, NULL, &edit, 1, &result));
    set_read_only(system_config, false);
}

// A function to test the fallback when the user's own config exists already (an earlier save made
// it, say): the launcher reads that one next, so it is the one changed, keeping what it holds
static void test_fallback_changes_an_existing_user_config(void)
{
    if (!can_test_read_only())
        return;
    const char *system_config = DIR "/system/config.ini";
    const char *user_config = DIR "/home/.config/streamflex/config.ini";
    const char *user_text =
        "; mine\n"
        "[Layout]\n"
        "Rows=1\n"
        "IconSpacing=5%\n";
    CHECK(fileio_make_dirs(DIR "/system"));
    CHECK(fileio_make_dirs(DIR "/home/.config/streamflex"));
    reset(system_config, ORIGINAL);
    reset(user_config, user_text);
    set_read_only(system_config, true);
    char prefix[CONFIG_SAVE_PATH_MAX];
    CHECK(fileio_real_path(DIR "/system", prefix, sizeof(prefix) - 1));
    size_t used = strlen(prefix);
    snprintf(prefix + used, sizeof(prefix) - used, "/");
    ConfigEdit edit = { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(config_save(system_config, prefix, user_config, &edit, 1, &result));
    CHECK(holds(user_config,
        "; mine\n"
        "[Layout]\n"
        "Rows=2\n"
        "IconSpacing=5%\n"));
    CHECK(holds(DIR "/home/.config/streamflex/config.ini.bak", user_text));
    CHECK(holds(system_config, ORIGINAL));

    // One that cannot be written fails with the reason, rather than being replaced
    set_read_only(user_config, true);
    CHECK(!config_save(system_config, prefix, user_config, &edit, 1, &result));
    CHECK(strstr(result.why, "permission denied") != NULL);
    set_read_only(user_config, false);
    set_read_only(system_config, false);
}

// A function to test that a user config whose path does not fit fails, saying which path
static void test_user_config_too_long(void)
{
    if (!can_test_read_only())
        return;
    const char *system_config = DIR "/system/config.ini";
    CHECK(fileio_make_dirs(DIR "/system"));
    reset(system_config, ORIGINAL);
    set_read_only(system_config, true);
    char prefix[CONFIG_SAVE_PATH_MAX];
    CHECK(fileio_real_path(DIR "/system", prefix, sizeof(prefix) - 1));
    size_t used = strlen(prefix);
    snprintf(prefix + used, sizeof(prefix) - used, "/");
    char user_config[CONFIG_SAVE_PATH_MAX + 64];
    memset(user_config, 'u', sizeof(user_config) - 1);
    user_config[sizeof(user_config) - 1] = '\0';
    ConfigEdit edit = { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(!config_save(system_config, prefix, user_config, &edit, 1, &result));
    CHECK_STR(result.why, "the user config's path is too long");
    CHECK(holds(system_config, ORIGINAL));
    set_read_only(system_config, false);
}

#ifndef _WIN32
// A function to test that a user config that exists but cannot be read fails the save, saying
// why, rather than being replaced without a backup
static void test_unreadable_user_config_fails(void)
{
    if (!can_test_read_only())
        return;
    const char *system_config = DIR "/system/config.ini";
    const char *user_config = DIR "/home/.config/streamflex/config.ini";
    const char *user_text = "; mine\n[Layout]\nRows=1\n";
    CHECK(fileio_make_dirs(DIR "/system"));
    CHECK(fileio_make_dirs(DIR "/home/.config/streamflex"));
    reset(system_config, ORIGINAL);
    reset(user_config, user_text);
    set_read_only(system_config, true);
    CHECK(chmod(user_config, 0200) == 0);   // Writable, but not readable
    char prefix[CONFIG_SAVE_PATH_MAX];
    CHECK(fileio_real_path(DIR "/system", prefix, sizeof(prefix) - 1));
    size_t used = strlen(prefix);
    snprintf(prefix + used, sizeof(prefix) - used, "/");
    ConfigEdit edit = { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(!config_save(system_config, prefix, user_config, &edit, 1, &result));
    CHECK(strstr(result.why, "permission denied") != NULL);
    CHECK(chmod(user_config, 0644) == 0);
    CHECK(holds(user_config, user_text));
    CHECK(!fileio_exists(DIR "/home/.config/streamflex/config.ini.bak"));
    set_read_only(system_config, false);
}
#endif

// A function to test that a config that has vanished fails with a reason
static void test_missing_file_fails(void)
{
    reset(CONFIG, NULL);
    ConfigEdit edit = { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(!config_save(CONFIG, NULL, NULL, &edit, 1, &result));
    CHECK(strstr(result.why, "not found") != NULL);
    CHECK(!fileio_exists(CONFIG));
}

#ifndef _WIN32
// A function to test that a symbolic link is followed: the file it points to changes and keeps its
// permissions, the link stays
static void test_follows_a_symbolic_link(void)
{
    reset(DIR "/real.ini", ORIGINAL);
    CHECK(chmod(DIR "/real.ini", 0640) == 0);
    remove(DIR "/link.ini");
    CHECK(symlink("real.ini", DIR "/link.ini") == 0);
    ConfigEdit edit = { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(config_save(DIR "/link.ini", NULL, NULL, &edit, 1, &result));
    CHECK_STR(result.warning, "");
    struct stat info;
    CHECK(lstat(DIR "/link.ini", &info) == 0 && S_ISLNK(info.st_mode));
    CHECK(lstat(DIR "/real.ini", &info) == 0 && S_ISREG(info.st_mode) && (info.st_mode & 07777) == 0640);
    char *content = fileio_read_all(DIR "/real.ini", NULL);
    CHECK(content != NULL && strstr(content, "Rows=2\n") != NULL);
    free(content);
}

// A function to test that a config whose path leaves no room for its backup's name fails before
// anything is written, rather than keeping its backup under a name cut short
static void test_too_long_for_a_backup_fails(void)
{
    // Nest folders until the config's path is 1021 bytes: it fits, but "<path>.bak" does not
    const size_t target = CONFIG_SAVE_PATH_MAX - 3;
    const char *name = "/config.ini";
    char folder[CONFIG_SAVE_PATH_MAX];
    CHECK(fileio_make_dirs(DIR));
    CHECK(fileio_real_path(DIR, folder, sizeof(folder)));
    size_t length = strlen(folder);
    if (length + strlen(name) + 2 > target) {
        printf("skipped the long path check: the fixture's own path is %zu bytes already\n", length);
        return;
    }
    size_t missing = target - length - strlen(name);
    while (missing > 0) {
        // Each folder is a '/' and up to 100 letters, never leaving a single byte for the last one
        size_t letters = missing > 101 ? (missing == 102 ? 98 : 100) : missing - 1;
        folder[length] = '/';
        memset(folder + length + 1, 'd', letters);
        length += letters + 1;
        folder[length] = '\0';
        missing -= letters + 1;
    }
    CHECK(fileio_make_dirs(folder));

    // Start from the config alone, whatever an earlier run left beside it
    char other[2 * CONFIG_SAVE_PATH_MAX];
    FileioEntry *entries = NULL;
    int count = fileio_list(folder, &entries);
    for (int i = 0; i < count; i++) {
        snprintf(other, sizeof(other), "%s/%s", folder, entries[i].name);
        if (!entries[i].is_dir)
            fileio_remove(other);
    }
    fileio_free_list(entries, count);
    char config[2 * CONFIG_SAVE_PATH_MAX];
    snprintf(config, sizeof(config), "%s%s", folder, name);
    CHECK_INT((int) strlen(config), (int) target);
    CHECK(fileio_write_all(config, ORIGINAL, strlen(ORIGINAL)));

    ConfigEdit edit = { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    CHECK(!config_save(config, NULL, NULL, &edit, 1, &result));
    CHECK(strstr(result.why, "the path is too long") != NULL);
    CHECK(holds(config, ORIGINAL));
    count = fileio_list(folder, &entries);
    CHECK(count == 1 && strcmp(entries[0].name, "config.ini") == 0);
    fileio_free_list(entries, count);
}
#endif

#ifdef _WIN32
// A function to test that hidden files do not stop a save: a hidden backup is replaced, and a
// hidden config is still hidden afterwards
static void test_hidden_files(void)
{
    ConfigEdit edit = { "Layout", "Rows", NULL, "2", INIDOC_AFTER_LAST_KEY };
    ConfigSaveResult result;
    reset(CONFIG, ORIGINAL);
    const char *older = "; an older backup\n";
    CHECK(fileio_write_all(CONFIG ".bak", older, strlen(older)));
    CHECK(SetFileAttributesA(CONFIG ".bak", FILE_ATTRIBUTE_HIDDEN));
    CHECK(config_save(CONFIG, NULL, NULL, &edit, 1, &result));
    CHECK(holds(CONFIG ".bak", ORIGINAL));
    CHECK(!fileio_exists(CONFIG ".bak.tmp"));

    reset(CONFIG, ORIGINAL);
    CHECK(SetFileAttributesA(CONFIG, FILE_ATTRIBUTE_HIDDEN));
    CHECK(config_save(CONFIG, NULL, NULL, &edit, 1, &result));
    DWORD attributes = GetFileAttributesA(CONFIG);
    CHECK(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_HIDDEN) != 0);
    char *content = fileio_read_all(CONFIG, NULL);
    CHECK(content != NULL && strstr(content, "Rows=2\n") != NULL);
    free(content);
    SetFileAttributesA(CONFIG, FILE_ATTRIBUTE_NORMAL);
}
#endif

// A function to test list edits: a set, a removal and two additions. The additions are numbered
// after the highest HotkeyN the file holds once the removal is made.
static void test_list_edits(void)
{
    reset(CONFIG, "[General]\nDefaultMenu=Main\n\n[Hotkeys]\nHotkey1=#4000003A;:quit\nHotkey4=#4000003B;:home\n"
                  "; end\n\n[Main]\nEntry1=One;apps;:quit\n");
    ConfigListEdit lists[] = {
        { CONFIG_LIST_SET, "Hotkeys", "Hotkey1=#4000003A;:quit", "Hotkey", true, "#4000003A;:settings" },
        { CONFIG_LIST_REMOVE, "Hotkeys", "Hotkey4=#4000003B;:home", NULL, false, NULL },
        { CONFIG_LIST_ADD, "Hotkeys", NULL, "Hotkey", true, "#4000003C;:back" },
        { CONFIG_LIST_ADD, "Hotkeys", NULL, "Hotkey", true, "#4000003D;:up" }
    };
    ConfigSaveResult result;
    CHECK(config_save_all(CONFIG, NULL, NULL, NULL, 0, lists, 4, &result));
    CHECK(holds(CONFIG, "[General]\nDefaultMenu=Main\n\n[Hotkeys]\nHotkey1=#4000003A;:settings\nHotkey2=#4000003C;:back\n"
                        "Hotkey3=#4000003D;:up\n; end\n\n[Main]\nEntry1=One;apps;:quit\n"));
    CHECK_STR(result.notes, "");
}

// A function to test list edits after a hand edit made while settings were open (Review Focus 4).
// Settings read Hotkey1, Hotkey2 and Hotkey3; by the save, a hand edit changed Hotkey2, removed
// Hotkey3 and added Hotkey7. The change is written as a new line and the removal skipped, each with
// a note, and every new line is numbered after 7.
static void test_list_edits_after_hand_edit(void)
{
    reset(CONFIG, "[Hotkeys]\nHotkey1=#4000003A;:quit\nHotkey2=#4000003B;:back ; changed by hand\n"
                  "Hotkey7=#40000040;:sleep\n");
    ConfigListEdit lists[] = {
        { CONFIG_LIST_SET, "Hotkeys", "Hotkey2=#4000003B;:home", "Hotkey", true, "#4000003B;:up" },
        { CONFIG_LIST_REMOVE, "Hotkeys", "Hotkey3=#4000003C;:quit", NULL, false, NULL },
        { CONFIG_LIST_ADD, "Hotkeys", NULL, "Hotkey", true, "#4000003D;:down" }
    };
    ConfigSaveResult result;
    CHECK(config_save_all(CONFIG, NULL, NULL, NULL, 0, lists, 3, &result));
    CHECK(holds(CONFIG, "[Hotkeys]\nHotkey1=#4000003A;:quit\nHotkey2=#4000003B;:back ; changed by hand\n"
                        "Hotkey7=#40000040;:sleep\nHotkey8=#4000003B;:up\nHotkey9=#4000003D;:down\n"));
    CHECK(strstr(result.notes, "'Hotkey2=#4000003B;:home' changed meanwhile, so its change is written as a new line") != NULL);
    CHECK(strstr(result.notes, "'Hotkey3=#4000003C;:quit' is not there any more, so its removal is skipped") != NULL);

    // A gamepad control keeps its label, with no number; a line that cannot be written fails the
    // whole save, and nothing is written
    reset(CONFIG, "[Gamepad]\nButtonA=:select\n");
    ConfigListEdit pad[] = {
        { CONFIG_LIST_SET, "Gamepad", "ButtonA=:select", "ButtonB", false, ":back" },
        { CONFIG_LIST_ADD, "Gamepad", NULL, "ButtonA", false, "; not a command" }
    };
    CHECK(!config_save_all(CONFIG, NULL, NULL, NULL, 0, pad, 2, &result));
    CHECK(strstr(result.why, "cannot be written") != NULL);
    CHECK(holds(CONFIG, "[Gamepad]\nButtonA=:select\n"));
}

// A function to test how a new line is numbered: only <stem><digits> keys count, the highest wins
// wherever it stands, a section with no keys starts at 1, and a number too large to go above fails
// the save rather than wrapping round to one already taken
static void test_list_numbering(void)
{
    reset(CONFIG, "[Hotkeys]\nHotkey5=#40000001;:a\nHotkey2=#40000002;:b\nHotkez9=#40000003;:c\n"
                  "Hotkey9x=#40000004;:d\nHotkey=#40000005;:e\n");
    ConfigListEdit add = { CONFIG_LIST_ADD, "Hotkeys", NULL, "Hotkey", true, "#40000006;:f" };
    ConfigSaveResult result;
    CHECK(config_save_all(CONFIG, NULL, NULL, NULL, 0, &add, 1, &result));
    CHECK(holds(CONFIG, "[Hotkeys]\nHotkey5=#40000001;:a\nHotkey2=#40000002;:b\nHotkez9=#40000003;:c\n"
                        "Hotkey9x=#40000004;:d\nHotkey=#40000005;:e\nHotkey6=#40000006;:f\n"));

    reset(CONFIG, "[General]\nDefaultMenu=Main\n\n[Hotkeys]\n; none yet\n");
    CHECK(config_save_all(CONFIG, NULL, NULL, NULL, 0, &add, 1, &result));
    CHECK(holds(CONFIG, "[General]\nDefaultMenu=Main\n\n[Hotkeys]\nHotkey1=#40000006;:f\n; none yet\n"));

    static const char *const huge = "[Hotkeys]\nHotkey99999999999999999999=#40000001;:a\n";
    reset(CONFIG, huge);
    CHECK(!config_save_all(CONFIG, NULL, NULL, NULL, 0, &add, 1, &result));
    CHECK_STR(result.why, "a new Hotkey line in [Hotkeys] cannot be numbered: the highest number there is too large");
    CHECK(holds(CONFIG, huge));
    CHECK(!fileio_exists(CONFIG ".bak"));
}

// A function to test the list edits' other paths: a gamepad control renamed and added without a
// number; a key longer than a short buffer kept whole; single-key edits saved with list edits; and
// each refusal (a line break, a gone line's new line, an empty-named key's removal) failing the
// save with its reason and the file as it was
static void test_list_edit_edges(void)
{
    ConfigSaveResult result;
    reset(CONFIG, "[Gamepad]\nButtonA=:select\n");
    ConfigListEdit pad[] = {
        { CONFIG_LIST_SET, "Gamepad", "ButtonA=:select", "ButtonB", false, ":back" },
        { CONFIG_LIST_ADD, "Gamepad", NULL, "ButtonX", false, ":home" }
    };
    CHECK(config_save_all(CONFIG, NULL, NULL, NULL, 0, pad, 2, &result));
    CHECK(holds(CONFIG, "[Gamepad]\nButtonB=:back\nButtonX=:home\n"));

    // A key of 150 bytes, set by its own (numbered) key and added under a fixed one
    char long_key[160];
    memset(long_key, 'k', 150);
    long_key[150] = '\0';
    char text[400];
    snprintf(text, sizeof(text), "[Hotkeys]\n%s=#40000001;:a\n", long_key);
    char original[200];
    snprintf(original, sizeof(original), "%s=#40000001;:a", long_key);
    reset(CONFIG, text);
    ConfigListEdit longs[] = {
        { CONFIG_LIST_SET, "Hotkeys", original, "Hotkey", true, "#40000001;:b" },
        { CONFIG_LIST_ADD, "Hotkeys", NULL, long_key, false, "#40000002;:c" }
    };
    CHECK(config_save_all(CONFIG, NULL, NULL, NULL, 0, longs, 2, &result));
    snprintf(text, sizeof(text), "[Hotkeys]\n%s=#40000001;:b\n%s=#40000002;:c\n", long_key, long_key);
    CHECK(holds(CONFIG, text));

    // Single-key edits and list edits in one save
    reset(CONFIG, "[General]\nDefaultMenu=Main\n\n[Hotkeys]\nHotkey1=#4000003A;:quit\n");
    ConfigEdit edit = { "General", "DefaultMenu", NULL, "Games", INIDOC_AFTER_LAST_KEY };
    ConfigListEdit add = { CONFIG_LIST_ADD, "Hotkeys", NULL, "Hotkey", true, "#4000003B;:up" };
    CHECK(config_save_all(CONFIG, NULL, NULL, &edit, 1, &add, 1, &result));
    CHECK(holds(CONFIG, "[General]\nDefaultMenu=Games\n\n[Hotkeys]\nHotkey1=#4000003A;:quit\nHotkey2=#4000003B;:up\n"));

    // A line break typed into a binding is refused with its reason, for a set and for an add
    static const char *const hotkeys = "[Hotkeys]\nHotkey1=#4000003A;:quit\n";
    reset(CONFIG, hotkeys);
    ConfigListEdit broken_set = { CONFIG_LIST_SET, "Hotkeys", "Hotkey1=#4000003A;:quit", "Hotkey", true, "#4000003A;:a\nb" };
    CHECK(!config_save_all(CONFIG, NULL, NULL, NULL, 0, &broken_set, 1, &result));
    CHECK_STR(result.why, "the Hotkey1 value in [Hotkeys] cannot be written: it contains a line break");
    CHECK(holds(CONFIG, hotkeys));
    ConfigListEdit broken_add = { CONFIG_LIST_ADD, "Hotkeys", NULL, "Hotkey", true, "#4000003A;:a\r" };
    CHECK(!config_save_all(CONFIG, NULL, NULL, NULL, 0, &broken_add, 1, &result));
    CHECK_STR(result.why, "the Hotkey2 value in [Hotkeys] cannot be written: it contains a line break");
    CHECK(holds(CONFIG, hotkeys));

    // A gone line's change that cannot be written as a new line fails the save, and leaves no note
    // for a change that was never made
    ConfigListEdit gone = { CONFIG_LIST_SET, "Hotkeys", "Hotkey1=#4000003A;:home", "Hotkey", true, "#4000003A ;:a" };
    CHECK(!config_save_all(CONFIG, NULL, NULL, NULL, 0, &gone, 1, &result));
    CHECK(strstr(result.why, "the Hotkey2 value in [Hotkeys] cannot be written: ") != NULL);
    CHECK_STR(result.notes, "");
    CHECK(holds(CONFIG, hotkeys));

    // A key with an empty name cannot be removed: the indented line after it would join Hotkey1
    static const char *const empty_named = "[Hotkeys]\nHotkey1=#4000003A;:quit\n=#4000003B;:home\n  Hotkey2=#4000003C;:up\n";
    reset(CONFIG, empty_named);
    ConfigListEdit remove_empty = { CONFIG_LIST_REMOVE, "Hotkeys", "=#4000003B;:home", NULL, false, NULL };
    CHECK(!config_save_all(CONFIG, NULL, NULL, NULL, 0, &remove_empty, 1, &result));
    CHECK_STR(result.why, "the line '=#4000003B;:home' in [Hotkeys] cannot be removed: it would change how other lines read");
    CHECK(holds(CONFIG, empty_named));
    CHECK(!fileio_exists(CONFIG ".bak"));
}

// A function to test the notes: one line each, in order; cut short at the buffer's end, still ended;
// and none after a save that failed later on (the backup cannot be written), since nothing was made
static void test_list_notes(void)
{
    static const char *const hotkeys = "[Hotkeys]\nHotkey1=#4000003A;:quit\n";
    reset(CONFIG, hotkeys);
    ConfigListEdit two[] = {
        { CONFIG_LIST_REMOVE, "Hotkeys", "Hotkey3=#4000003C;:quit", NULL, false, NULL },
        { CONFIG_LIST_REMOVE, "Hotkeys", "Hotkey4=#4000003D;:quit", NULL, false, NULL }
    };
    ConfigSaveResult result;
    CHECK(config_save_all(CONFIG, NULL, NULL, NULL, 0, two, 2, &result));
    CHECK_STR(result.notes, "'Hotkey3=#4000003C;:quit' is not there any more, so its removal is skipped\n"
                            "'Hotkey4=#4000003D;:quit' is not there any more, so its removal is skipped");

    // Six notes of about 230 bytes each overrun the 1024 bytes
    char gone[180];
    memset(gone, 'g', 170);
    snprintf(gone + 170, sizeof(gone) - 170, "=#4000");
    ConfigListEdit many[6];
    for (int i = 0; i < 6; i++)
        many[i] = (ConfigListEdit) { CONFIG_LIST_REMOVE, "Hotkeys", gone, NULL, false, NULL };
    reset(CONFIG, hotkeys);
    CHECK(config_save_all(CONFIG, NULL, NULL, NULL, 0, many, 6, &result));
    CHECK(result.notes[sizeof(result.notes) - 1] == '\0');
    CHECK_INT((int) strlen(result.notes), (int) sizeof(result.notes) - 1);
    CHECK(strncmp(result.notes, "'ggg", 4) == 0);

    reset(CONFIG, hotkeys);
    CHECK(fileio_make_dirs(CONFIG ".bak.tmp"));   // A folder in the way: the backup cannot be written
    ConfigListEdit changed = { CONFIG_LIST_SET, "Hotkeys", "Hotkey1=#4000003A;:home", "Hotkey", true, "#4000003A;:up" };
    CHECK(!config_save_all(CONFIG, NULL, NULL, NULL, 0, &changed, 1, &result));
    CHECK(strstr(result.why, "could not write the backup") != NULL);
    CHECK_STR(result.notes, "");
    CHECK(holds(CONFIG, hotkeys));
    remove_folder(CONFIG ".bak.tmp");
}

int main(void)
{
    test_saves_only_the_edits();
    test_warning_is_passed_on();
    test_alias_beside_the_key_is_removed();
    test_keeps_a_hand_edit_made_meanwhile();
    test_refused_value_changes_nothing();
    test_refused_beside_its_comment();
    test_older_backup_survives_a_failed_backup();
    test_read_only_fails();
    test_system_copy_falls_back_to_the_user_config();
    test_fallback_changes_an_existing_user_config();
    test_user_config_too_long();
    test_missing_file_fails();
    test_list_edits();
    test_list_edits_after_hand_edit();
    test_list_numbering();
    test_list_edit_edges();
    test_list_notes();
#ifndef _WIN32
    test_unreadable_user_config_fails();
    test_follows_a_symbolic_link();
    test_too_long_for_a_backup_fails();
#else
    test_hidden_files();
#endif
    return check_report();
}
