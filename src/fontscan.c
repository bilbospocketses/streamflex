#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include "fontscan.h"
#include "fontlist.h"
#include "fileio.h"
#include "alloc.h"
#include "test_hooks.h"
#ifdef _WIN32
#include <windows.h>
#define SEPARATOR "\\"       // As join_paths() writes it, so a bundled font's path reads as the launcher's
#else
#define SEPARATOR "/"
#endif

#define MAX_DEPTH 8          // Font folders nest (/usr/share/fonts/truetype/dejavu); no deeper than this
#define PATH_BYTES 2048

struct FontScan {
    SDL_Thread *thread;     // NULL once it has been waited for
    SDL_atomic_t done;
    char *bundled_folder;
    char **files;
    bool *bundled;
    int count;
    int capacity;
    bool failed;            // A file could not be added (out of memory): the list is short
    bool fail_adds;         // Harness only: every add fails as out of memory would (set before the thread starts)
};

// A function to make room for one more file; false when out of memory
static bool grow_files(FontScan *scan)
{
    if (scan->count < scan->capacity)
        return true;
    int capacity = scan->capacity ? scan->capacity * 2 : 256;
    char **files = alloc_realloc(scan->files, (size_t) capacity * sizeof(char*));
    if (files == NULL)
        return false;
    scan->files = files;
    bool *flags = alloc_realloc(scan->bundled, (size_t) capacity * sizeof(bool));
    if (flags == NULL)
        return false;
    scan->bundled = flags;
    scan->capacity = capacity;
    return true;
}

// A function to add a file to the list; a file already listed (by path) is left as it is. Out of
// memory, the file is left out and the list marked as failed: the thread cannot say so, and the
// main thread lets a short list go (fontscan_failed()).
static void add_file(FontScan *scan, const char *path, bool bundled)
{
    for (int i = 0; i < scan->count; i++) {
        if (strcmp(scan->files[i], path) == 0)
            return;
    }
    char *copy = !scan->fail_adds && grow_files(scan) ? alloc_strdup(path) : NULL;
    if (copy == NULL) {
        scan->failed = true;
        return;
    }
    scan->files[scan->count] = copy;
    scan->bundled[scan->count] = bundled;
    scan->count++;
}

// A function to list the font files in a folder and the folders under it: regular files only, so a
// pipe named like a font never holds the font list in its read. A folder that cannot be listed is
// skipped, and so is anything hidden.
static void scan_folder(FontScan *scan, const char *folder, bool bundled, int depth)
{
    FileioEntry *entries = NULL;
    int count = fileio_list(folder, &entries);
    char path[PATH_BYTES];
    size_t length = strlen(folder);
    bool slash = length > 0 && fileio_is_separator(folder[length - 1]);
    for (int i = 0; i < count; i++) {
        if (entries[i].hidden)
            continue;
        snprintf(path, sizeof(path), "%s%s%s", folder, slash ? "" : SEPARATOR, entries[i].name);
        if (entries[i].is_dir && depth < MAX_DEPTH)
            scan_folder(scan, path, bundled, depth + 1);
        else if (fontlist_is_font_file(&entries[i]))
            add_file(scan, path, bundled);
    }
    fileio_free_list(entries, count > 0 ? count : 0);
}

#ifdef _WIN32
// A function to turn a UTF-16 string into UTF-8, into a buffer; false when it does not fit
static bool to_utf8(const wchar_t *wide, char *out, int size)
{
    return WideCharToMultiByte(CP_UTF8, 0, wide, -1, out, size, NULL, NULL) > 0;
}

// A function to list the fonts one Fonts registry key names. A value's data is a file: a bare name
// is in Windows' own Fonts folder, and a full path (a font installed for one user) is used as it is.
static void scan_registry(FontScan *scan, HKEY root, const char *fonts_folder)
{
    HKEY key;
    if (RegOpenKeyExW(root, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0, KEY_READ, &key) != ERROR_SUCCESS)
        return;
    wchar_t name[512];
    wchar_t data[1024];
    char file[PATH_BYTES];
    char path[PATH_BYTES];
    for (DWORD i = 0;; i++) {
        DWORD name_size = (DWORD) (sizeof(name) / sizeof(name[0]));
        DWORD data_size = (DWORD) (sizeof(data) - sizeof(wchar_t));
        DWORD type = 0;
        LONG result = RegEnumValueW(key, i, name, &name_size, NULL, &type, (LPBYTE) data, &data_size);
        if (result == ERROR_NO_MORE_ITEMS)
            break;
        if (result != ERROR_SUCCESS || type != REG_SZ)
            continue;
        data[data_size / sizeof(wchar_t)] = L'\0';
        if (!to_utf8(data, file, (int) sizeof(file)) || !fontlist_is_font_name(file))
            continue;
        bool full = (file[0] != '\0' && file[1] == ':') || (file[0] == '\\' && file[1] == '\\');
        if (full)
            snprintf(path, sizeof(path), "%s", file);
        else
            snprintf(path, sizeof(path), "%s\\%s", fonts_folder, file);
        add_file(scan, path, false);
    }
    RegCloseKey(key);
}
#endif

#ifdef STREAMFLEX_TEST_HOOKS
// Only the headless harness builds this: a function to list the folders STREAMFLEX_TEST_FONT_DIRS
// names (colon-separated, an empty one skipped) in place of the system's. strtok() is not used: its
// state is shared with every other thread.
static void scan_test_folders(FontScan *scan, const char *dirs)
{
    char folder[PATH_BYTES];
    while (*dirs != '\0') {
        const char *end = strchr(dirs, ':');
        size_t length = end != NULL ? (size_t) (end - dirs) : strlen(dirs);
        if (length > 0 && length < sizeof(folder)) {
            memcpy(folder, dirs, length);
            folder[length] = '\0';
            scan_folder(scan, folder, false, 0);
        }
        dirs += length;
        if (*dirs == ':')
            dirs++;
    }
}
#endif

// A function run on the scan's thread: list every font file. It logs nothing (output_log() is the
// main thread's) and opens no font (SDL_ttf's FreeType library is the main thread's too).
static int scan_thread(void *data)
{
    FontScan *scan = data;
    if (scan->bundled_folder != NULL)
        scan_folder(scan, scan->bundled_folder, true, 0);
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: STREAMFLEX_TEST_FONT_DIRS stands in for the system's
    // font folders, and STREAMFLEX_TEST_FONT_DELAY_MS holds the list back, as a slow disk would
    const char *delay = getenv("STREAMFLEX_TEST_FONT_DELAY_MS");
    if (delay != NULL)
        SDL_Delay((Uint32) atoi(delay));
    const char *dirs = getenv("STREAMFLEX_TEST_FONT_DIRS");
    if (dirs != NULL) {
        scan_test_folders(scan, dirs);
        SDL_AtomicSet(&scan->done, 1);
        return 0;
    }
#endif
#ifdef _WIN32
    char windows[MAX_PATH + 1];
    UINT length = GetWindowsDirectoryA(windows, (UINT) sizeof(windows));
    char fonts_folder[PATH_BYTES];
    snprintf(fonts_folder, sizeof(fonts_folder), "%s\\Fonts", length > 0 && length < sizeof(windows) ? windows : "C:\\Windows");
    scan_registry(scan, HKEY_LOCAL_MACHINE, fonts_folder);
    scan_registry(scan, HKEY_CURRENT_USER, fonts_folder);
#else
    scan_folder(scan, "/usr/share/fonts", false, 0);
    scan_folder(scan, "/usr/local/share/fonts", false, 0);
    const char *home = getenv("HOME");
    if (home != NULL) {
        char path[PATH_BYTES];
        snprintf(path, sizeof(path), "%s/.local/share/fonts", home);
        scan_folder(scan, path, false, 0);
        snprintf(path, sizeof(path), "%s/.fonts", home);
        scan_folder(scan, path, false, 0);
    }
#endif
    SDL_AtomicSet(&scan->done, 1);
    return 0;
}

// A function to start listing the font files on a thread; NULL when it cannot start (out of memory,
// a list without the bundled folder would leave the bundled fonts out). The harness can fail the
// bundled folder's copy (STREAMFLEX_TEST_FAIL=fontfolder), the thread's start (fontthread) and every
// file's add (fontadd); each is decided here, on the main thread, so nothing changes under the
// thread's feet.
FontScan *fontscan_start(const char *bundled_folder)
{
    FontScan *scan = alloc_calloc(1, sizeof(FontScan));
    test_fail("fontfolder", true);
    char *folder = bundled_folder != NULL ? alloc_strdup(bundled_folder) : NULL;
    test_fail("fontfolder", false);
    if (scan == NULL || (bundled_folder != NULL && folder == NULL)) {
        alloc_free(scan);
        alloc_free(folder);
        return NULL;
    }
    scan->bundled_folder = folder;
    scan->fail_adds = test_failing("fontadd");
    SDL_AtomicSet(&scan->done, 0);
    scan->thread = test_failing("fontthread") ? NULL : SDL_CreateThread(scan_thread, "Font scan", scan);
    if (scan->thread == NULL) {
        fontscan_free(scan);
        return NULL;
    }
    return scan;
}

// A function to tell whether the list is complete. Once the thread says so it is waited for, which
// costs nothing (it has finished) and makes everything it wrote safe to read here, on any CPU:
// SDL_AtomicSet() alone does not order the thread's earlier writes on a weakly ordered one.
bool fontscan_done(FontScan *scan)
{
    if (scan->thread == NULL)
        return true;
    if (SDL_AtomicGet(&scan->done) == 0)
        return false;
    SDL_WaitThread(scan->thread, NULL);
    scan->thread = NULL;
    return true;
}

// A function to tell whether a file could not be added (out of memory), so the list is short; only
// once the scan is done
bool fontscan_failed(const FontScan *scan)
{
    return scan->failed;
}

// A function to count the files found; only once the scan is done
int fontscan_count(const FontScan *scan)
{
    return scan->count;
}

// A function to get a file found
const char *fontscan_file(const FontScan *scan, int index)
{
    return scan->files[index];
}

// A function to tell whether a file is one of the bundled fonts
bool fontscan_bundled(const FontScan *scan, int index)
{
    return scan->bundled[index];
}

// A function to free the scan, waiting for its thread
void fontscan_free(FontScan *scan)
{
    if (scan == NULL)
        return;
    if (scan->thread != NULL)
        SDL_WaitThread(scan->thread, NULL);
    for (int i = 0; i < scan->count; i++)
        alloc_free(scan->files[i]);
    alloc_free(scan->files);
    alloc_free(scan->bundled);
    alloc_free(scan->bundled_folder);
    alloc_free(scan);
}
