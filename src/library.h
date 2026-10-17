// The built-in icon library: a manifest, icons.ini, whose sections name icons and whose 'file' keys give
// each one's path inside the library folder, with each icon's title, group and match = aliases for the
// icon pickers. Pure: inih and the C library only, so it is unit-tested without SDL.
#ifndef LIBRARY_H
#define LIBRARY_H

#include <stdbool.h>

#define LIBRARY_MANIFEST "icons.ini"
#define LIBRARY_NAME_MAX 32
#define LIBRARY_FALLBACK_ICON "apps"

// Receives one message per problem found while loading
typedef void (*LibraryWarn)(const char *message);

// One icon as the manifest describes it: its name, its title (the name when the manifest gives none), its
// group ("" when none), and its match = aliases, the other names an app it stands for goes by
typedef struct { const char *name; const char *title; const char *group; const char *const *match; int match_count; } LibraryInfo;

void        library_set_warn(LibraryWarn function);
bool        library_is_name(const char *field);
int         library_load(const char *root);
const char *library_lookup(const char *name);
const char *library_legacy_name(const char *path);
const char *library_rescue(const char *field, const char **name);
void        library_free(void);
int         library_count(void);                  // The icons loaded
const LibraryInfo *library_info(int index);       // Manifest order; library_info(0) is the first of library_count() in one array

#endif
