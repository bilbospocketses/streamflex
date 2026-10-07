#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "browser.h"
#include "fileio.h"
#include "alloc.h"

static const char *const IMAGE_EXTENSIONS[] = { ".jpg", ".jpeg", ".png", ".webp" };
static const char *const TOO_FEW = "a slideshow needs 2 or more images";
static const char *const TOO_LONG = "the path is too long for the settings to hold (1023 bytes at most)";
static const char *const OUT_OF_MEMORY = "out of memory";

typedef enum {
    LOAD_DONE,
    LOAD_UNLISTED,       // The folder cannot be listed
    LOAD_NO_MEMORY
} LoadResult;

struct Browser {
    BrowserMode mode;
    BrowserList list;
    BrowserCheck check;
    void *context;
    BrowserPlace *places;   // Copies of the places given
    int place_count;
    char *folder;           // The folder on show; NULL while the places are shown
    BrowserRow *rows;
    int row_count;
    int cursor;
    char chosen[BROWSER_PATH_MAX];
    const char *why;        // Why the last command did nothing because of the browser itself; NULL otherwise
};

// A function to compare names without regard to ASCII case, then exactly
static int compare_names(const char *a, const char *b)
{
    const char *x = a;
    const char *y = b;
    while (*x != '\0' && fileio_lower(*x) == fileio_lower(*y)) {
        x++;
        y++;
    }
    int difference = fileio_lower(*x) - fileio_lower(*y);
    return difference != 0 ? difference : strcmp(a, b);
}

// A function to sort entries by name, for qsort
static int compare_entries(const void *a, const void *b)
{
    const FileioEntry *x = *(const FileioEntry *const *) a;
    const FileioEntry *y = *(const FileioEntry *const *) b;
    return compare_names(x->name, y->name);
}

// A function to tell an image file by its extension, whatever its case
bool browser_is_image(const char *name)
{
    size_t length = strlen(name);
    for (size_t i = 0; i < sizeof(IMAGE_EXTENSIONS) / sizeof(IMAGE_EXTENSIONS[0]); i++) {
        size_t extension_length = strlen(IMAGE_EXTENSIONS[i]);
        if (length <= extension_length)
            continue;
        const char *tail = name + length - extension_length;
        size_t k = 0;
        while (k < extension_length && fileio_lower(tail[k]) == IMAGE_EXTENSIONS[i][k])
            k++;
        if (k == extension_length)
            return true;
    }
    return false;
}

// A function to tell a listed file that counts as an image: not a folder, not hidden, and with an
// image's extension, whatever its case. The browser, the Folder row's count and both platforms'
// slideshow scans all use it, so they agree on every folder.
bool browser_is_image_file(const FileioEntry *entry)
{
    return !entry->is_dir && !entry->hidden && browser_is_image(entry->name);
}

// A function to tell a root, which has no parent: "/", "C:", "C:\" or "\\server\share"
static bool is_root(const char *path, size_t length)
{
    if (length == 1 && fileio_is_separator(path[0]))
        return true;
    if ((length == 2 || length == 3) && path[1] == ':' && (length == 2 || fileio_is_separator(path[2])))
        return true;
    if (length >= 2 && fileio_is_separator(path[0]) && fileio_is_separator(path[1])) {
        int separators = 0;
        for (size_t i = 2; i < length; i++) {
            if (fileio_is_separator(path[i]))
                separators++;
        }
        return separators <= 1;
    }
    return false;
}

// A function to find a path's parent folder; false for a root or a bare name
bool browser_parent(const char *path, char *out, size_t size)
{
    size_t length = strlen(path);
    while (length > 1 && fileio_is_separator(path[length - 1]) && !is_root(path, length))
        length--;
    if (length == 0 || is_root(path, length))
        return false;
    size_t cut = length;
    while (cut > 0 && !fileio_is_separator(path[cut - 1]))
        cut--;
    if (cut == 0)
        return false;

    // Keep the separator when the parent is a root ("/", "C:\"), drop it otherwise
    if (!is_root(path, cut))
        cut--;
    if (cut + 1 > size)
        return false;
    memcpy(out, path, cut);
    out[cut] = '\0';
    return true;
}

// A function to join a folder and a name into a new path, with the separator the folder already
// uses: a backslash only when the folder has one, or has none of either kind after a drive ("C:").
// So "C:/Users/me/Pictures" keeps forward slashes, and a path saved from it matches the one it
// started from.
static char *join_path(const char *folder, const char *name)
{
    size_t length = strlen(folder);
    bool backslash = strchr(folder, '\\') != NULL || (strchr(folder, '/') == NULL && length >= 2 && folder[1] == ':');
    const char *between = length > 0 && fileio_is_separator(folder[length - 1]) ? "" : (backslash ? "\\" : "/");
    size_t size = length + strlen(between) + strlen(name) + 1;
    char *path = alloc_malloc(size);
    if (path != NULL)
        snprintf(path, size, "%s%s%s", folder, between, name);
    return path;
}

// A function to free a list of rows
static void free_row_list(BrowserRow *rows, int count)
{
    for (int i = 0; i < count; i++) {
        alloc_free(rows[i].name);
        alloc_free(rows[i].path);
    }
    alloc_free(rows);
}

// A function to free the rows on show
static void free_rows(Browser *browser)
{
    free_row_list(browser->rows, browser->row_count);
    browser->rows = NULL;
    browser->row_count = 0;
}

// A function to add a row to a list with room for it. The row takes over `path`; false when out of
// memory, with nothing added.
static bool add_row(BrowserRow *rows, int *count, BrowserRowKind kind, const char *name, char *path,
                    bool enabled, const char *why)
{
    char *name_copy = alloc_strdup(name);
    if (name_copy == NULL || path == NULL) {
        alloc_free(name_copy);
        alloc_free(path);
        return false;
    }
    BrowserRow *row = &rows[(*count)++];
    row->kind = kind;
    row->name = name_copy;
    row->path = path;
    row->enabled = enabled;
    row->why = why;
    row->image_count = 0;
    return true;
}

// A function to put the cursor on the row for a path, or on the first row
static void select_path(Browser *browser, const char *path)
{
    browser->cursor = 0;
    for (int i = 0; path != NULL && i < browser->row_count; i++) {
        if (strcmp(browser->rows[i].path, path) == 0) {
            browser->cursor = i;
            return;
        }
    }
}

// A function to show the places, with the cursor on `selected` when it is one of them. False when
// out of memory, with what was on show left as it was.
static bool load_places(Browser *browser, const char *selected)
{
    BrowserRow *rows = alloc_calloc((size_t) (browser->place_count > 0 ? browser->place_count : 1), sizeof(BrowserRow));
    int count = 0;
    bool ok = rows != NULL;
    for (int i = 0; ok && i < browser->place_count; i++)
        ok = add_row(rows, &count, BROWSER_ROW_PLACE, browser->places[i].label, alloc_strdup(browser->places[i].path), true, NULL);
    if (!ok) {
        free_row_list(rows, count);
        browser->why = OUT_OF_MEMORY;
        return false;
    }
    free_rows(browser);
    alloc_free(browser->folder);
    browser->folder = NULL;
    browser->rows = rows;
    browser->row_count = count;
    select_path(browser, selected);
    return true;
}

// A function to say why a path cannot be chosen: config.ini's limits (the check), else the
// settings' own; NULL when it can
static const char *why_not(const Browser *browser, const char *path)
{
    const char *why = browser->check != NULL ? browser->check(path, browser->context) : NULL;
    if (why == NULL && strlen(path) >= BROWSER_PATH_MAX)
        why = TOO_LONG;
    return why;
}

// A function to show a folder: folders first, then images (in file mode, every file), each sorted by
// name, hidden files left out; in folder mode "Use this folder" comes first. A path too long to
// choose is shown, and refused with the reason. When the folder cannot be listed, or memory runs
// out, what was on show is left as it was.
static LoadResult load_folder(Browser *browser, const char *folder, const char *selected)
{
    FileioEntry *entries = NULL;
    int count = browser->list(folder, &entries, browser->context);
    if (count < 0)
        return LOAD_UNLISTED;
    FileioEntry **folders = alloc_calloc((size_t) (count > 0 ? count : 1), sizeof(FileioEntry*));
    FileioEntry **images = alloc_calloc((size_t) (count > 0 ? count : 1), sizeof(FileioEntry*));
    char *copy = alloc_strdup(folder);   // `folder` may be a row's path, which is about to be freed
    int folder_count = 0;
    int image_count = 0;
    bool ok = folders != NULL && images != NULL && copy != NULL;
    for (int i = 0; ok && i < count; i++) {
        if (entries[i].hidden)
            continue;
        if (entries[i].is_dir)
            folders[folder_count++] = &entries[i];
        else if (browser->mode == BROWSER_FILE || browser_is_image_file(&entries[i]))
            images[image_count++] = &entries[i];
    }
    BrowserRow *rows = NULL;
    int row_count = 0;
    if (ok) {
        qsort(folders, (size_t) folder_count, sizeof(FileioEntry*), compare_entries);
        qsort(images, (size_t) image_count, sizeof(FileioEntry*), compare_entries);
        rows = alloc_calloc((size_t) (folder_count + image_count + 1), sizeof(BrowserRow));
        ok = rows != NULL;
    }
    if (ok && browser->mode == BROWSER_FOLDER) {
        const char *why = image_count < 2 ? TOO_FEW : why_not(browser, copy);
        ok = add_row(rows, &row_count, BROWSER_ROW_USE_FOLDER, "Use this folder", alloc_strdup(copy), why == NULL, why);
        if (ok)
            rows[0].image_count = image_count;
    }
    for (int i = 0; ok && i < folder_count; i++)
        ok = add_row(rows, &row_count, BROWSER_ROW_FOLDER, folders[i]->name, join_path(copy, folders[i]->name), true, NULL);
    for (int i = 0; ok && i < image_count; i++) {
        char *path = join_path(copy, images[i]->name);
        bool choosable = browser->mode == BROWSER_IMAGE || browser->mode == BROWSER_FILE;
        const char *why = path != NULL && choosable ? why_not(browser, path) : NULL;
        ok = add_row(rows, &row_count, browser->mode == BROWSER_FILE ? BROWSER_ROW_FILE : BROWSER_ROW_IMAGE,
                     images[i]->name, path, choosable && why == NULL, why);
    }
    alloc_free(folders);
    alloc_free(images);
    fileio_free_list(entries, count);
    if (!ok) {
        free_row_list(rows, row_count);
        alloc_free(copy);
        browser->why = OUT_OF_MEMORY;
        return LOAD_NO_MEMORY;
    }
    free_rows(browser);
    alloc_free(browser->folder);
    browser->folder = copy;
    browser->rows = rows;
    browser->row_count = row_count;
    select_path(browser, selected);
    return LOAD_DONE;
}

// A function to open the browser: at `start` (an image, or in file mode any start, opens its folder
// with it highlighted), else at the first place that can be listed (Pictures, then Home, ...) and is
// not on a network share, which could keep the browser waiting on the network, else at the places.
// NULL when out of memory (the reason goes in *why), rather than a browser opened somewhere other
// than asked. A folder whose list function fails (out of memory there included) cannot be listed,
// and the next place is tried.
Browser *browser_open(BrowserMode mode, const char *start, const BrowserPlace *places, int place_count,
                      BrowserList list, BrowserCheck check, void *context, const char **why)
{
    if (why != NULL)
        *why = NULL;
    Browser *browser = alloc_calloc(1, sizeof(Browser));
    if (browser == NULL) {
        if (why != NULL)
            *why = OUT_OF_MEMORY;
        return NULL;
    }
    browser->mode = mode;
    browser->list = list;
    browser->check = check;
    browser->context = context;
    browser->places = alloc_calloc((size_t) (place_count > 0 ? place_count : 1), sizeof(BrowserPlace));
    bool ok = browser->places != NULL;
    for (int i = 0; ok && i < place_count; i++) {
        char *label = alloc_strdup(places[i].label);
        char *path = alloc_strdup(places[i].path);
        ok = label != NULL && path != NULL;
        if (!ok) {
            alloc_free(label);
            alloc_free(path);
            break;
        }
        browser->places[i] = (BrowserPlace) { .label = label, .path = path, .network = places[i].network };
        browser->place_count++;
    }

    // A start folder's trailing separators are dropped ("/home/me/Pictures/"), except a root's
    char *folder = NULL;
    if (ok && start != NULL && start[0] != '\0') {
        folder = alloc_strdup(start);
        ok = folder != NULL;
    }
    LoadResult loaded = LOAD_UNLISTED;
    if (folder != NULL) {
        size_t length = strlen(folder);
        while (length > 1 && fileio_is_separator(folder[length - 1]) && !is_root(folder, length))
            folder[--length] = '\0';
        if ((mode == BROWSER_IMAGE && browser_is_image(folder)) || mode == BROWSER_FILE) {
            char *parent = alloc_malloc(length + 1);
            ok = parent != NULL;
            if (ok && browser_parent(folder, parent, length + 1))
                loaded = load_folder(browser, parent, folder);
            alloc_free(parent);
        }
        else
            loaded = load_folder(browser, folder, NULL);
        alloc_free(folder);
    }
    for (int i = 0; ok && loaded == LOAD_UNLISTED && i < browser->place_count; i++) {
        if (!browser->places[i].network)
            loaded = load_folder(browser, browser->places[i].path, NULL);
    }
    ok = ok && loaded != LOAD_NO_MEMORY;
    bool opened = loaded == LOAD_DONE;
    if (!ok || (!opened && !load_places(browser, NULL))) {
        browser_free(browser);
        if (why != NULL)
            *why = OUT_OF_MEMORY;   // The only way to fail: every other failure opens elsewhere
        return NULL;
    }
    return browser;
}

// A function to free the browser
void browser_free(Browser *browser)
{
    if (browser == NULL)
        return;
    free_rows(browser);
    alloc_free(browser->folder);
    for (int i = 0; i < browser->place_count; i++) {
        alloc_free((char*) browser->places[i].label);
        alloc_free((char*) browser->places[i].path);
    }
    alloc_free(browser->places);
    alloc_free(browser);
}

// A function to act on one key: move, page, open a folder or place, choose, or go back up
BrowserResult browser_command(Browser *browser, BrowserCommand command, int page_rows)
{
    int last = browser->row_count - 1;
    int before = browser->cursor;
    browser->why = NULL;
    if (page_rows < 1)
        page_rows = 1;
    switch (command) {
        case BROWSER_UP:
            if (browser->cursor > 0)
                browser->cursor--;
            break;
        case BROWSER_DOWN:
            if (browser->cursor < last)
                browser->cursor++;
            break;
        case BROWSER_PAGE_UP:
            browser->cursor = browser->cursor - page_rows < 0 ? 0 : browser->cursor - page_rows;
            break;
        case BROWSER_PAGE_DOWN:
            browser->cursor = browser->cursor + page_rows > last ? (last > 0 ? last : 0) : browser->cursor + page_rows;
            break;
        case BROWSER_OK: {
            if (browser->cursor < 0 || browser->cursor > last)
                return BROWSER_NONE;
            const BrowserRow *row = &browser->rows[browser->cursor];
            if (row->kind == BROWSER_ROW_PLACE || row->kind == BROWSER_ROW_FOLDER) {
                char *path = alloc_strdup(row->path);   // The row goes when the folder opens
                bool opened = path != NULL && load_folder(browser, path, NULL) == LOAD_DONE;
                if (path == NULL)
                    browser->why = OUT_OF_MEMORY;
                alloc_free(path);
                return opened ? BROWSER_MOVED : BROWSER_NONE;
            }
            if (!row->enabled)
                return BROWSER_NONE;
            snprintf(browser->chosen, sizeof(browser->chosen), "%s", row->path);
            return BROWSER_CHOSEN;
        }
        case BROWSER_BACK: {
            if (browser->folder == NULL)
                return BROWSER_CLOSED;
            size_t size = strlen(browser->folder) + 1;
            char *from = alloc_strdup(browser->folder);
            char *parent = alloc_malloc(size);
            // Out of a folder whose parent cannot be listed (or that has none), to the places; out
            // of memory, nowhere
            bool moved = from != NULL && parent != NULL;
            if (!moved)
                browser->why = OUT_OF_MEMORY;
            else {
                LoadResult loaded = browser_parent(from, parent, size) ? load_folder(browser, parent, from) : LOAD_UNLISTED;
                moved = loaded == LOAD_UNLISTED ? load_places(browser, from) : loaded == LOAD_DONE;
            }
            alloc_free(from);
            alloc_free(parent);
            return moved ? BROWSER_MOVED : BROWSER_NONE;
        }
    }
    return browser->cursor != before ? BROWSER_MOVED : BROWSER_NONE;
}

// A function to count the rows on show
int browser_row_count(const Browser *browser)
{
    return browser->row_count;
}

// A function to get a row on show
const BrowserRow *browser_row(const Browser *browser, int index)
{
    return index >= 0 && index < browser->row_count ? &browser->rows[index] : NULL;
}

// A function to get the cursor
int browser_cursor(const Browser *browser)
{
    return browser->cursor;
}

// A function to get the folder on show; NULL while the places are shown
const char *browser_folder(const Browser *browser)
{
    return browser->folder;
}

// A function to say why the last command did nothing because of the browser itself ("out of
// memory"); NULL when it did what it could, or when the folder's own list failed, whose reason is
// the list function's (fileio_last_error() for fileio_list)
const char *browser_why(const Browser *browser)
{
    return browser->why;
}

// A function to get the path just chosen
const char *browser_chosen(const Browser *browser)
{
    return browser->chosen;
}

// A function to find a folder's first image by name, for previewing a folder; false when it has
// none, it cannot be listed, its path does not fit in `out`, or memory runs out: a preview only
bool browser_first_image(const Browser *browser, const char *folder, char *out, size_t size)
{
    FileioEntry *entries = NULL;
    int count = browser->list(folder, &entries, browser->context);
    const FileioEntry *first = NULL;
    for (int i = 0; i < count; i++) {
        if (!browser_is_image_file(&entries[i]))
            continue;
        if (first == NULL || compare_names(entries[i].name, first->name) < 0)
            first = &entries[i];
    }
    char *path = first != NULL ? join_path(folder, first->name) : NULL;
    bool found = path != NULL && strlen(path) < size;
    if (found)
        snprintf(out, size, "%s", path);
    alloc_free(path);
    fileio_free_list(entries, count > 0 ? count : 0);
    return found;
}
