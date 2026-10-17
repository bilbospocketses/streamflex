#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "appscan.h"
#include "applist.h"
#include "fileio.h"
#include "alloc.h"
#include "test_hooks.h"

#define MAX_DEPTH 4               // An applications folder may hold folders (kde4/); no deeper than this
#define MAX_FOLDERS 32            // The applications folders a scan reads
#define FOLDER_TEXT_BYTES 8192    // Their paths, one after another
#define PATH_BYTES 4096
#define WHY_BYTES 1024
#define LANG_BYTES 64
#define DELAY_STEP_MS 10u         // The harness's delay looks for a stop this often

typedef enum { SCAN_HOOK_NONE, SCAN_HOOK_FAIL, SCAN_HOOK_EMPTY } ScanHook;

struct AppScan {
    SDL_Thread *thread;                 // NULL once it has been waited for
    SDL_AtomicInt done;
    SDL_AtomicInt found;                // Apps listed so far, for "Finding apps... (N)"
    SDL_AtomicInt stop;                 // appscan_free() asks the thread to stop at its next file
    AppList *list;                      // NULL once taken, or when the scan failed
    char why[WHY_BYTES];                // Why there are no apps; "" when there are
    char lang[LANG_BYTES];              // Linux: the locale the names are read in, found on the main thread
    char folder_text[FOLDER_TEXT_BYTES];
    size_t folder_used;
    const char *folders[MAX_FOLDERS];   // Linux: the applications folders, an earlier one's file shadowing a later one's
    int folder_count;
    char **ids;                         // Linux: the desktop file IDs read so far, the thread's own
    int id_count;
    int id_capacity;
    bool short_of_memory;               // An ID or a file's entry could not be kept: the list may be short
    Uint32 delay_ms;                    // Harness only: STREAMFLEX_TEST_APP_SCAN's delay...
    ScanHook hook;                      // ...or its outcome, both read on the main thread
};

// A function to tell whether appscan_free() has asked the thread to stop
static bool stopping(AppScan *scan)
{
    return SDL_GetAtomicInt(&scan->stop) != 0;
}

#ifndef _WIN32
// A function to add an applications folder to the scan's list, `tail` after it; one that does not fit is
// left out
static void add_folder(AppScan *scan, const char *folder, size_t length, const char *tail)
{
    size_t tail_length = strlen(tail);
    size_t need = length + tail_length + 1;
    if (length == 0 || scan->folder_count >= MAX_FOLDERS || scan->folder_used + need > sizeof(scan->folder_text))
        return;
    char *at = scan->folder_text + scan->folder_used;
    memcpy(at, folder, length);
    memcpy(at + length, tail, tail_length + 1);
    scan->folders[scan->folder_count++] = at;
    scan->folder_used += need;
}

// A function to find the applications folders on the main thread, from the environment: each data
// folder's applications/ (applist_data_dirs()). The harness can name them instead.
static void list_folders(AppScan *scan)
{
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: STREAMFLEX_TEST_APP_DIRS names the applications folders
    // (colon-separated, an empty one skipped) in place of the data folders'
    const char *test_dirs = getenv("STREAMFLEX_TEST_APP_DIRS");
    if (test_dirs != NULL) {
        for (const char *at = test_dirs; *at != '\0';) {
            size_t length = strcspn(at, ":");
            add_folder(scan, at, length, "");
            at += length;
            if (*at == ':')
                at++;
        }
        return;
    }
#endif
    char data_text[FOLDER_TEXT_BYTES];
    const char *dirs[MAX_FOLDERS];
    int count = applist_data_dirs(getenv("XDG_DATA_HOME"), getenv("XDG_DATA_DIRS"), getenv("HOME"), data_text,
                                  sizeof(data_text), dirs, MAX_FOLDERS);
    for (int i = 0; i < count; i++)
        add_folder(scan, dirs[i], strlen(dirs[i]), "/applications");
}

// A function to find the locale the apps' names are read in, on the main thread: the first of LC_ALL,
// LC_MESSAGES and LANG that is set and not empty, as POSIX picks a program's messages' language
static void read_locale(AppScan *scan)
{
    static const char *const NAMES[] = { "LC_ALL", "LC_MESSAGES", "LANG" };
    for (size_t i = 0; i < sizeof(NAMES) / sizeof(NAMES[0]); i++) {
        const char *value = getenv(NAMES[i]);
        if (value == NULL || value[0] == '\0')
            continue;
        if (strlen(value) < sizeof(scan->lang))
            memcpy(scan->lang, value, strlen(value) + 1);
        return;
    }
}

// A function to tell whether this is the first file of its desktop file ID: the first is the one in force,
// so a user's copy that hides an app (Hidden=true, NoDisplay=true) hides the system's (Desktop Entry
// Specification, "Desktop File ID"). A new ID is kept; out of memory, the file is skipped and the scan
// marked short.
static bool first_of_id(AppScan *scan, const char *id)
{
    for (int i = 0; i < scan->id_count; i++) {
        if (strcmp(scan->ids[i], id) == 0)
            return false;
    }
    if (scan->id_count == scan->id_capacity) {
        int capacity = scan->id_capacity > 0 ? scan->id_capacity * 2 : 256;
        char **ids = alloc_realloc(scan->ids, (size_t) capacity * sizeof(char*));
        if (ids == NULL) {
            scan->short_of_memory = true;
            return false;
        }
        scan->ids = ids;
        scan->id_capacity = capacity;
    }
    char *copy = alloc_strdup(id);
    if (copy == NULL) {
        scan->short_of_memory = true;
        return false;
    }
    scan->ids[scan->id_count++] = copy;
    return true;
}

// A function to read one .desktop file into the list, unless an earlier file had its ID. A file that
// cannot be read, or is no app (applist_parse_desktop()'s 0), adds nothing; one read short of memory
// (its -1) marks the scan short, so finish() lets the list go, as it does for an add short of memory.
static void read_desktop_file(AppScan *scan, const char *path, const char *id)
{
    if (!first_of_id(scan, id))
        return;
    size_t length = 0;
    char *text = fileio_read_all(path, &length);
    if (text == NULL)
        return;
    AppItem item;
    int got = applist_parse_desktop(path, text, length, scan->lang, &item);
    if (got < 0)
        scan->short_of_memory = true;
    else if (got == 1) {
        if (applist_add(scan->list, item.name, item.command, item.icon))
            SDL_AddAtomicInt(&scan->found, 1);
        applist_item_clear(&item);
    }
    alloc_free(text);
}

// A function to tell whether a file's name is a .desktop file's
static bool is_desktop_file(const char *name)
{
    size_t length = strlen(name);
    return length > 8 && strcmp(name + length - 8, ".desktop") == 0;
}

// A function to note a folder that could not be listed because memory ran out (fileio's own reason, as
// fileio.c tells it): the list is short, so finish() lets it go, as it does for a file read short of
// memory. One that could not be listed for another reason (no permission) holds no apps, as a top folder
// that cannot be listed does while another can.
static void note_unlisted(AppScan *scan)
{
    if (strcmp(fileio_last_error(), "out of memory") == 0)
        scan->short_of_memory = true;
}

// A function to read the .desktop files in a folder and the folders under it. `prefix` is the desktop
// file ID's start for this folder: "" at the top, "kde4-" in kde4/. Regular files only, so a pipe named
// like one never holds the thread in its read; hidden ones skipped. False when the folder cannot be listed;
// a folder under it that cannot be listed for want of memory marks the scan short (note_unlisted()).
static bool scan_folder(AppScan *scan, const char *folder, const char *prefix, int depth)
{
    FileioEntry *entries = NULL;
    int count = fileio_list(folder, &entries);
    if (count < 0)
        return false;
    char path[PATH_BYTES];
    char id[PATH_BYTES];
    for (int i = 0; i < count && !stopping(scan); i++) {
        const FileioEntry *entry = &entries[i];
        if (entry->hidden)
            continue;
        int path_length = snprintf(path, sizeof(path), "%s/%s", folder, entry->name);
        int id_length = snprintf(id, sizeof(id), "%s%s%s", prefix, entry->name, entry->is_dir ? "-" : "");
        if (path_length < 0 || (size_t) path_length >= sizeof(path) || id_length < 0 || (size_t) id_length >= sizeof(id))
            continue;
        if (entry->is_dir && depth < MAX_DEPTH) {
            if (!scan_folder(scan, path, id, depth + 1))
                note_unlisted(scan);
        }
        else if (entry->is_file && is_desktop_file(entry->name))
            read_desktop_file(scan, path, id);
    }
    fileio_free_list(entries, count);
    return true;
}

// A function to read every applications folder, in order. A folder that is not there holds no apps; the
// scan fails only when none could be listed and one that is there could not be, and is short (let go)
// when one could not be listed for want of memory.
static void scan_folders(AppScan *scan)
{
    bool listed = false;
    char problem[512] = "";
    for (int i = 0; i < scan->folder_count && !stopping(scan); i++) {
        if (scan_folder(scan, scan->folders[i], "", 0)) {
            listed = true;
            continue;
        }
        char reason[256];
        snprintf(reason, sizeof(reason), "%s", fileio_last_error());   // fileio_present() replaces it
        note_unlisted(scan);
        if (fileio_present(scan->folders[i]))
            snprintf(problem, sizeof(problem), "%s: %s", scan->folders[i], reason);
    }
    if (!listed && problem[0] != '\0')
        snprintf(scan->why, sizeof(scan->why), "The apps could not be listed: %s", problem);
}
#endif

// A function to list the apps on the scan's thread, the platform's way. Windows' AppsFolder scan is
// Task 8's; until it lands, a Windows build lists none and says so.
static void list_apps(AppScan *scan)
{
#ifdef _WIN32
    snprintf(scan->why, sizeof(scan->why), "The apps could not be listed: this build does not list Windows apps yet");
#else
    scan_folders(scan);
#endif
}

// A function to end the scan on its thread: the IDs go; a list the scan could not keep whole is let go (a
// short list would misstate the apps, and the next visit lists them anew), as is one that failed; an empty
// one says so; the rest is sorted
static void finish(AppScan *scan)
{
    for (int i = 0; i < scan->id_count; i++)
        alloc_free(scan->ids[i]);
    alloc_free(scan->ids);
    scan->ids = NULL;
    scan->id_count = 0;
    scan->id_capacity = 0;
    if (scan->short_of_memory || applist_failed(scan->list))
        snprintf(scan->why, sizeof(scan->why), "Out of memory while listing the apps");
    if (scan->why[0] != '\0') {
        applist_free(scan->list);
        scan->list = NULL;
        return;
    }
    if (applist_count(scan->list) == 0)
        snprintf(scan->why, sizeof(scan->why), "No apps were found");
    applist_sort(scan->list);
}

// A function run on the scan's thread: list the apps, then say it is done. It logs nothing (output_log()
// is the main thread's).
static int scan_thread(void *data)
{
    AppScan *scan = data;
    bool decided = false;
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: STREAMFLEX_TEST_APP_SCAN holds the scan back (delay:<ms>,
    // which a stop ends at once), fails it (fail), or has it find nothing (empty), whatever the folders hold
    for (Uint32 waited = 0; waited < scan->delay_ms && !stopping(scan); waited += DELAY_STEP_MS)
        SDL_Delay(DELAY_STEP_MS);
    if (scan->hook == SCAN_HOOK_FAIL)
        snprintf(scan->why, sizeof(scan->why), "The apps could not be listed: the test hook failed it");
    decided = scan->hook != SCAN_HOOK_NONE;
#endif
    if (!decided && !stopping(scan))
        list_apps(scan);
    finish(scan);
    SDL_SetAtomicInt(&scan->done, 1);
    return 0;
}

// A function to start listing the installed apps on a thread. Everything the thread reads from the
// environment is read here, on the main thread. NULL when it cannot start: out of memory, or no thread
// (the harness's STREAMFLEX_TEST_FAIL=appthread).
AppScan *appscan_start(void)
{
    AppScan *scan = alloc_calloc(1, sizeof(AppScan));
    AppList *list = scan != NULL ? applist_create() : NULL;
    if (list == NULL) {
        alloc_free(scan);
        return NULL;
    }
    scan->list = list;
#ifndef _WIN32
    read_locale(scan);
    list_folders(scan);
#endif
#ifdef STREAMFLEX_TEST_HOOKS
    const char *asked = getenv("STREAMFLEX_TEST_APP_SCAN");
    if (asked != NULL && strncmp(asked, "delay:", 6) == 0)
        scan->delay_ms = (Uint32) strtoul(asked + 6, NULL, 10);
    else if (asked != NULL && strcmp(asked, "fail") == 0)
        scan->hook = SCAN_HOOK_FAIL;
    else if (asked != NULL && strcmp(asked, "empty") == 0)
        scan->hook = SCAN_HOOK_EMPTY;
#endif
    SDL_SetAtomicInt(&scan->done, 0);
    SDL_SetAtomicInt(&scan->found, 0);
    SDL_SetAtomicInt(&scan->stop, 0);
    scan->thread = test_failing("appthread") ? NULL : SDL_CreateThread(scan_thread, "App scan", scan);
    if (scan->thread == NULL) {
        appscan_free(scan);
        return NULL;
    }
    return scan;
}

// A function to tell whether the scan is done, and how many apps it has listed so far. Once the thread
// says so it is waited for, which costs nothing (it has finished) and makes everything it wrote safe to
// read here, on any CPU: SDL_SetAtomicInt() alone does not order the thread's earlier writes on a weakly
// ordered one.
bool appscan_poll(AppScan *scan, int *found)
{
    if (found != NULL)
        *found = SDL_GetAtomicInt(&scan->found);
    if (scan->thread == NULL)
        return true;
    if (SDL_GetAtomicInt(&scan->done) == 0)
        return false;
    SDL_WaitThread(scan->thread, NULL);
    scan->thread = NULL;
    return true;
}

// A function to take the list once the scan is done, sorted: the caller frees it. NULL before then, once
// taken, and when the scan failed.
AppList *appscan_take(AppScan *scan)
{
    if (scan->thread != NULL)
        return NULL;
    AppList *list = scan->list;
    scan->list = NULL;
    return list;
}

// A function to say why the scan failed or found nothing, for the caption; NULL while it runs and when it
// found apps
const char *appscan_why(const AppScan *scan)
{
    return scan->thread == NULL && scan->why[0] != '\0' ? scan->why : NULL;
}

// A function to free the scan. A thread still running is asked to stop and waited for first, so nothing
// it fills is freed under it.
void appscan_free(AppScan *scan)
{
    if (scan == NULL)
        return;
    if (scan->thread != NULL) {
        SDL_SetAtomicInt(&scan->stop, 1);
        SDL_WaitThread(scan->thread, NULL);
    }
    applist_free(scan->list);
    alloc_free(scan);
}
