// The installed apps the settings' Add > App page lists (spec "Installed apps"): a pure list the platform
// scans fill on their thread (appscan.c), and the rules every platform shares: which entries are no app
// to launch (uninstallers, help and readme shortcuts, documents, web links), which are one app found
// twice, the order, and the filter the keyboard types into. For Linux it also reads a .desktop file's
// entry and names the data folders apps and their icons are found in. Pure: no SDL, no globals; memory
// comes from alloc.h.
#ifndef APPLIST_H
#define APPLIST_H

#include <stdbool.h>
#include <stddef.h>

typedef struct { char *name; char *command; char *icon; } AppItem;   // icon: Linux Icon= value; Windows the app ID
typedef struct AppList AppList;

AppList *applist_create(void);
void applist_free(AppList *list);
bool applist_add(AppList *list, const char *name, const char *command, const char *icon);   // false when filtered or a duplicate
void applist_sort(AppList *list);
int applist_count(const AppList *list);
const AppItem *applist_item(const AppList *list, int index);
int applist_filter(const AppList *list, const char *query, int *indexes, int max);
int applist_parse_desktop(const char *path, const char *text, size_t length, const char *lang, AppItem *out);  // 1 an app, 0 skipped, -1 out of memory
bool applist_failed(const AppList *list);   // An add ran out of memory, so the list is short
void applist_item_clear(AppItem *item);     // Frees what applist_parse_desktop() filled in
// The data folders apps (in applications/) and their icons are found in, in the order a file in one
// shadows the same file in a later one: XDG_DATA_HOME, XDG_DATA_DIRS, the Flatpak exports, Snap's. They
// are packed into `buffer`; fills at most `max` and returns how many it filled.
int applist_data_dirs(const char *data_home, const char *data_dirs, const char *home, char *buffer, size_t size,
                      const char **dirs, int max);

#endif
