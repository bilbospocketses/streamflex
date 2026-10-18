// Finding a Linux app's own icon from its .desktop file's Icon= value (spec "Icons", choice 2): an
// absolute path as it is; a name in the hicolor theme (the largest PNG, or the scalable SVG when the
// largest PNG is under ICONFIND_GOOD_SIZE), then in the pixmaps folders. `dirs` are the data folders
// (applist_data_dirs(): the user's, $XDG_DATA_DIRS', the Flatpak exports' and Snap's), so the Flatpak
// export icon folders are searched as theirs; a test passes a fixture tree. Pure: no SDL; it looks at the
// file system through fileio.c.
#ifndef ICONFIND_H
#define ICONFIND_H

#include <stdbool.h>
#include <stddef.h>

#define ICONFIND_GOOD_SIZE 128   // A PNG this size or larger plates sharp; a smaller one gives way to an SVG

bool iconfind_path(const char *icon_value, const char *const *dirs, int dir_count, char *out, size_t size);   // pure

#endif
