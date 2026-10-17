#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "applist.h"
#include "alloc.h"
#include "fileio.h"

#define FIRST_CAPACITY 64
#define LOCALES 4          // ll_CC@MODIFIER, ll_CC, ll@MODIFIER, ll
#define LOCALE_BYTES 64
#define PART_BYTES 16      // The longest language, country or modifier a locale's name is read with
#define WORD_BYTES 32
#define DIR_BYTES 4096

struct AppList {
    AppItem *items;
    int count;
    int capacity;
    bool failed;   // An add ran out of memory: an app is missing
};

// A run of a .desktop file's text, not yet copied: a key, or its value
typedef struct {
    const char *text;   // NULL when the key was not there
    size_t length;
} Slice;

// The data folders being gathered by applist_data_dirs()
typedef struct {
    char *buffer;
    size_t size;
    size_t used;
    const char **dirs;
    int max;
    int count;
} DirList;

// The words that mark an entry as no app to launch from a TV: an uninstaller, or a help or readme
// shortcut (spec "Installed apps"). A name holding one of them as a word is left out, and "read me" too.
static const char *const SKIP_WORDS[] = { "uninstall", "uninstaller", "uninstallation", "readme", "help", "documentation", NULL };

// The ends of a command's last name that open a document, not an app: a Start menu shortcut to a readme,
// a license or a manual
static const char *const DOCUMENT_ENDS[] = { ".txt", ".pdf", ".htm", ".html", ".chm", ".rtf", ".md", ".url", NULL };

// A function to tell whether a byte belongs to a word: an ASCII letter or digit
static bool is_word_byte(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

// A function to compare two texts as the list sorts and matches names: ASCII letters in either case alike,
// every other byte by its value, so UTF-8 sorts by code point and no character is split
static int compare_text(const char *a, const char *b)
{
    for (;; a++, b++) {
        int x = fileio_lower(*a);
        int y = fileio_lower(*b);
        if (x != y || x == 0)
            return x - y;
    }
}

// A function to tell whether a name holds one of SKIP_WORDS as a word (a run of ASCII letters and digits,
// matched in either case), or "read" then "me"
static bool has_skip_word(const char *name)
{
    char previous[WORD_BYTES] = "";
    const char *p = name;
    for (;;) {
        while (*p != '\0' && !is_word_byte(*p))
            p++;
        if (*p == '\0')
            return false;
        char word[WORD_BYTES];
        size_t length = 0;
        for (; is_word_byte(*p); p++) {
            if (length + 1 < sizeof(word))
                word[length++] = (char) fileio_lower(*p);
        }
        word[length] = '\0';
        for (int i = 0; SKIP_WORDS[i] != NULL; i++) {
            if (strcmp(word, SKIP_WORDS[i]) == 0)
                return true;
        }
        if (strcmp(previous, "read") == 0 && strcmp(word, "me") == 0)
            return true;
        memcpy(previous, word, length + 1);
    }
}

// A function to tell whether a command opens no app: a web link, or a last name (after its last '/' or
// '\', a closing quote dropped) that is an uninstaller's (unins000.exe, uninstall-foo.desktop) or a
// document's
static bool opens_no_app(const char *command)
{
    if (strstr(command, "://") != NULL)
        return true;
    const char *last = command;
    for (const char *p = command; *p != '\0'; p++) {
        if (fileio_is_separator(*p))
            last = p + 1;
    }
    char name[256];
    size_t length = 0;
    for (; last[length] != '\0' && length + 1 < sizeof(name); length++)
        name[length] = (char) fileio_lower(last[length]);
    if (length > 0 && name[length - 1] == '"')
        length--;
    name[length] = '\0';
    if (strncmp(name, "unins", 5) == 0)
        return true;
    for (int i = 0; DOCUMENT_ENDS[i] != NULL; i++) {
        size_t end = strlen(DOCUMENT_ENDS[i]);
        if (length > end && strcmp(name + length - end, DOCUMENT_ENDS[i]) == 0)
            return true;
    }
    return false;
}

// A function to make an empty list of apps; NULL when out of memory
AppList *applist_create(void)
{
    return alloc_calloc(1, sizeof(AppList));
}

// A function to free what an item holds, leaving it empty
void applist_item_clear(AppItem *item)
{
    alloc_free(item->name);
    alloc_free(item->command);
    alloc_free(item->icon);
    item->name = NULL;
    item->command = NULL;
    item->icon = NULL;
}

// A function to free a list and its apps
void applist_free(AppList *list)
{
    if (list == NULL)
        return;
    for (int i = 0; i < list->count; i++)
        applist_item_clear(&list->items[i]);
    alloc_free(list->items);
    alloc_free(list);
}

// A function to add an app: its name, the command its tile runs, and its icon (Linux: the Icon= value;
// Windows: the app ID). False for an app left out (see SKIP_WORDS and opens_no_app()), one already listed
// (the same name in either case, the same command), and one that ran out of memory, which leaves the
// list as it was and marks it as failed.
bool applist_add(AppList *list, const char *name, const char *command, const char *icon)
{
    if (name == NULL || name[0] == '\0' || command == NULL || command[0] == '\0' || has_skip_word(name) ||
        opens_no_app(command))
        return false;
    for (int i = 0; i < list->count; i++) {
        if (compare_text(list->items[i].name, name) == 0 && strcmp(list->items[i].command, command) == 0)
            return false;
    }
    if (list->count == list->capacity) {
        int capacity = list->capacity > 0 ? list->capacity * 2 : FIRST_CAPACITY;
        AppItem *items = alloc_realloc(list->items, (size_t) capacity * sizeof(AppItem));
        if (items == NULL) {
            list->failed = true;
            return false;
        }
        list->items = items;
        list->capacity = capacity;
    }
    AppItem item = { alloc_strdup(name), alloc_strdup(command), icon != NULL ? alloc_strdup(icon) : NULL };
    if (item.name == NULL || item.command == NULL || (icon != NULL && item.icon == NULL)) {
        applist_item_clear(&item);
        list->failed = true;
        return false;
    }
    list->items[list->count++] = item;
    return true;
}

// A function to tell whether an add ran out of memory, so an app is missing
bool applist_failed(const AppList *list)
{
    return list->failed;
}

// A function to order two apps: by name as compare_text() reads it, then its exact bytes, then the command
static int compare_items(const void *a, const void *b)
{
    const AppItem *x = a;
    const AppItem *y = b;
    int order = compare_text(x->name, y->name);
    if (order == 0)
        order = strcmp(x->name, y->name);
    if (order == 0)
        order = strcmp(x->command, y->command);
    return order;
}

// A function to put the apps in order: alphabetical, the same order on every run
void applist_sort(AppList *list)
{
    if (list->count > 1)
        qsort(list->items, (size_t) list->count, sizeof(AppItem), compare_items);
}

// A function to count the apps
int applist_count(const AppList *list)
{
    return list->count;
}

// A function to get an app; NULL past either end of the list
const AppItem *applist_item(const AppList *list, int index)
{
    return index >= 0 && index < list->count ? &list->items[index] : NULL;
}

// A function to tell whether a text holds a query, ASCII letters in either case alike
static bool contains_folded(const char *text, const char *query)
{
    size_t length = strlen(query);
    for (const char *at = text; *at != '\0'; at++) {
        size_t k = 0;
        while (k < length && at[k] != '\0' && fileio_lower(at[k]) == fileio_lower(query[k]))
            k++;
        if (k == length)
            return true;
    }
    return false;
}

// A function to find the apps whose name holds a query (the keyboard's filter), in the list's order: it
// fills at most `max` indexes and returns how many match. An empty or NULL query matches every app.
int applist_filter(const AppList *list, const char *query, int *indexes, int max)
{
    int total = 0;
    for (int i = 0; i < list->count; i++) {
        if (query != NULL && query[0] != '\0' && !contains_folded(list->items[i].name, query))
            continue;
        if (total < max)
            indexes[total] = i;
        total++;
    }
    return total;
}

// A function to tell whether a run of text is exactly a string
static bool slice_is(Slice slice, const char *text)
{
    return slice.text != NULL && slice.length == strlen(text) && memcmp(slice.text, text, slice.length) == 0;
}

// A function to read a boolean as the Desktop Entry Specification writes one: "true" ("1" read as well,
// as GLib reads it)
static bool slice_true(Slice slice)
{
    return slice_is(slice, "true") || slice_is(slice, "1");
}

// A function to keep a key's value unless an earlier line had the key: the first counts
static void keep_first(Slice *kept, Slice value)
{
    if (kept->text == NULL)
        *kept = value;
}

// A function to list the locales a Name[...] key is matched against, best first, from a POSIX locale name
// (ll_CC.ENCODING@MODIFIER): ll_CC@MODIFIER, ll_CC, ll@MODIFIER, ll, each only when the name has its
// parts, as the Desktop Entry Specification orders them. C and POSIX, and a name with a part too long to
// be one, have none.
static int locale_candidates(const char *lang, char out[][LOCALE_BYTES])
{
    if (lang == NULL)
        return 0;
    size_t language = strcspn(lang, "_.@");
    const char *country = lang[language] == '_' ? lang + language + 1 : NULL;
    size_t country_length = country != NULL ? strcspn(country, ".@") : 0;
    const char *at = strchr(lang, '@');
    const char *modifier = at != NULL && at[1] != '\0' ? at + 1 : NULL;
    if (language == 0 || language >= PART_BYTES || country_length >= PART_BYTES ||
        (modifier != NULL && strlen(modifier) >= PART_BYTES))
        return 0;
    if ((language == 1 && lang[0] == 'C') || (language == 5 && strncmp(lang, "POSIX", 5) == 0))
        return 0;
    int count = 0;
    int l = (int) language;
    int c = (int) country_length;
    if (country_length > 0 && modifier != NULL)
        snprintf(out[count++], LOCALE_BYTES, "%.*s_%.*s@%s", l, lang, c, country, modifier);
    if (country_length > 0)
        snprintf(out[count++], LOCALE_BYTES, "%.*s_%.*s", l, lang, c, country);
    if (modifier != NULL)
        snprintf(out[count++], LOCALE_BYTES, "%.*s@%s", l, lang, modifier);
    snprintf(out[count++], LOCALE_BYTES, "%.*s", l, lang);
    return count;
}

// A function to rank a key against the locales: its place among them for a Name[<locale>] key that names
// one, else `count`, which ranks below every one
static int locale_rank(Slice key, char locales[][LOCALE_BYTES], int count)
{
    if (key.length < 7 || memcmp(key.text, "Name[", 5) != 0 || key.text[key.length - 1] != ']')
        return count;
    Slice locale = { key.text + 5, key.length - 6 };
    for (int i = 0; i < count; i++) {
        if (slice_is(locale, locales[i]))
            return i;
    }
    return count;
}

// A function to copy a string value with the Desktop Entry Specification's escapes read (\s \n \t \r and
// \\; the three that would break a tile's one line read as a space), and the spaces at either end
// dropped; NULL when out of memory
static char *unescaped(Slice value)
{
    char *out = alloc_malloc(value.length + 1);
    if (out == NULL)
        return NULL;
    size_t n = 0;
    for (size_t i = 0; i < value.length; i++) {
        char c = value.text[i];
        if (c == '\\' && i + 1 < value.length) {
            char next = value.text[i + 1];
            if (next == 's' || next == 'n' || next == 't' || next == 'r') {
                c = ' ';
                i++;
            }
            else if (next == '\\')
                i++;
        }
        out[n++] = c;
    }
    while (n > 0 && out[n - 1] == ' ')
        n--;
    size_t start = 0;
    while (start < n && out[start] == ' ')
        start++;
    memmove(out, out + start, n - start);
    out[n - start] = '\0';
    return out;
}

// A function to read a .desktop file's entry as an app for the list (spec "Installed apps"). It is one
// when its [Desktop Entry] group has Type=Application, a non-empty Exec (unix.c's start_process() runs
// nothing without one), a name, and none of NoDisplay, Hidden and Terminal true (start_process() runs
// Exec with no terminal, so a terminal app would show nothing). Its name is the Name[] of the best
// locale `lang` gives (LC_ALL, LC_MESSAGES or LANG's value), else Name; its command is the file's path,
// which StreamFlex launches; its icon is the Icon= value, NULL when none. A path holding ';' is refused:
// unix.c cuts the command there (DELIMITER_ACTION). 1 for an app; 0, with `out` empty, for a file that
// is no app; -1, with `out` empty, when out of memory, so the scan can tell a short read from a skip.
int applist_parse_desktop(const char *path, const char *text, size_t length, const char *lang, AppItem *out)
{
    memset(out, 0, sizeof(*out));
    if (path == NULL || path[0] == '\0' || strchr(path, ';') != NULL || text == NULL)
        return 0;
    char locales[LOCALES][LOCALE_BYTES];
    int locale_count = locale_candidates(lang, locales);
    Slice type = { NULL, 0 };
    Slice name = { NULL, 0 };
    Slice localized = { NULL, 0 };
    Slice icon = { NULL, 0 };
    Slice exec = { NULL, 0 };
    Slice no_display = { NULL, 0 };
    Slice hidden = { NULL, 0 };
    Slice terminal = { NULL, 0 };
    int localized_rank = locale_count;
    bool in_entry = false;
    for (size_t at = 0; at < length;) {
        size_t end = at;
        while (end < length && text[end] != '\n')
            end++;
        Slice line = { text + at, end - at };
        at = end + 1;
        if (line.length > 0 && line.text[line.length - 1] == '\r')
            line.length--;
        if (line.length == 0 || line.text[0] == '#')
            continue;
        if (line.text[0] == '[') {
            in_entry = slice_is(line, "[Desktop Entry]");
            continue;
        }
        const char *equals = memchr(line.text, '=', line.length);
        if (!in_entry || equals == NULL)
            continue;
        Slice key = { line.text, (size_t) (equals - line.text) };
        while (key.length > 0 && key.text[key.length - 1] == ' ')
            key.length--;
        Slice value = { equals + 1, (size_t) (line.text + line.length - (equals + 1)) };
        while (value.length > 0 && value.text[0] == ' ') {
            value.text++;
            value.length--;
        }
        if (slice_is(key, "Type"))
            keep_first(&type, value);
        else if (slice_is(key, "Name"))
            keep_first(&name, value);
        else if (slice_is(key, "Icon"))
            keep_first(&icon, value);
        else if (slice_is(key, "Exec"))
            keep_first(&exec, value);
        else if (slice_is(key, "NoDisplay"))
            keep_first(&no_display, value);
        else if (slice_is(key, "Hidden"))
            keep_first(&hidden, value);
        else if (slice_is(key, "Terminal"))
            keep_first(&terminal, value);
        else {
            int rank = locale_rank(key, locales, locale_count);
            if (rank < localized_rank && value.length > 0) {
                localized = value;
                localized_rank = rank;
            }
        }
    }
    if (!slice_is(type, "Application") || slice_true(terminal) || slice_true(no_display) || slice_true(hidden) || exec.length == 0)
        return 0;
    Slice chosen = localized.text != NULL ? localized : name;
    if (chosen.text == NULL)
        return 0;
    out->name = unescaped(chosen);
    out->command = alloc_strdup(path);
    out->icon = icon.length > 0 ? unescaped(icon) : NULL;
    bool short_of_memory = out->name == NULL || out->command == NULL || (icon.length > 0 && out->icon == NULL);
    if (short_of_memory || out->name[0] == '\0') {
        applist_item_clear(out);
        return short_of_memory ? -1 : 0;
    }
    if (out->icon != NULL && out->icon[0] == '\0') {
        alloc_free(out->icon);
        out->icon = NULL;
    }
    return 1;
}

// A function to add a data folder: absolute only, as the XDG Base Directory Specification asks (a relative
// one is ignored), with `tail` after it, a trailing '/' dropped, once each. One that does not fit is left out.
static void add_data_dir(DirList *list, const char *dir, size_t length, const char *tail)
{
    while (length > 1 && dir[length - 1] == '/')
        length--;
    if (length == 0 || dir[0] != '/')
        return;
    char path[DIR_BYTES];
    int written = snprintf(path, sizeof(path), "%.*s%s", (int) length, dir, tail);
    if (written < 0 || (size_t) written >= sizeof(path))
        return;
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->dirs[i], path) == 0)
            return;
    }
    size_t need = (size_t) written + 1;
    if (list->count >= list->max || list->used + need > list->size)
        return;
    memcpy(list->buffer + list->used, path, need);
    list->dirs[list->count++] = list->buffer + list->used;
    list->used += need;
}

// A function to list the data folders apps and their icons are found in (spec "Installed apps"), in the
// order a file in one shadows the same file in a later one: XDG_DATA_HOME when it is absolute, else
// ~/.local/share; each absolute entry of XDG_DATA_DIRS, else /usr/local/share and /usr/share when it is
// unset or empty; then the user's and the system's Flatpak exports, and Snap's. Each folder once, so one
// XDG_DATA_DIRS already names keeps its place there. The paths are packed into `buffer`; it fills at most
// `max` and returns how many it filled.
int applist_data_dirs(const char *data_home, const char *data_dirs, const char *home, char *buffer, size_t size,
                      const char **dirs, int max)
{
    static const char *const FLATPAK = "/var/lib/flatpak/exports/share";
    static const char *const SNAP = "/var/lib/snapd/desktop";
    DirList list = { buffer, size, 0, dirs, max, 0 };
    bool has_home = home != NULL && home[0] == '/';
    if (data_home != NULL && data_home[0] == '/')
        add_data_dir(&list, data_home, strlen(data_home), "");
    else if (has_home)
        add_data_dir(&list, home, strlen(home), "/.local/share");
    if (data_dirs == NULL || data_dirs[0] == '\0')
        data_dirs = "/usr/local/share:/usr/share";
    for (const char *at = data_dirs; *at != '\0';) {
        size_t length = strcspn(at, ":");
        add_data_dir(&list, at, length, "");
        at += length;
        if (*at == ':')
            at++;
    }
    if (has_home)
        add_data_dir(&list, home, strlen(home), "/.local/share/flatpak/exports/share");
    add_data_dir(&list, FLATPAK, strlen(FLATPAK), "");
    add_data_dir(&list, SNAP, strlen(SNAP), "");
    return list.count;
}
