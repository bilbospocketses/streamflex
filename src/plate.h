// An app's own icon made into a tile icon like the library's (spec "Icons", choice 2): centered on the
// library's square plate, saved as a PNG in the icons folder beside config.ini (Linux:
// ~/.config/streamflex/icons), named from the app's name. StreamFlex only ever writes these files, and
// never over one that is there.
#ifndef PLATE_H
#define PLATE_H

#include <stdbool.h>
#include <stddef.h>
#include <SDL3/SDL.h>

#define PLATE_SIZE 512         // The side a plated icon is saved at: the library's canvas (build-library.py CANVAS)
#define PLATE_RADIUS 0.22      // The corners' radius, a share of the side (libtools.py OUTLINE_RADIUS)
#define PLATE_GLYPH_BOX 0.60   // The icon's box, a share of the side (build-library.py GLYPH_BOX)
#define ICONS_NAME_MAX 48      // The longest file name icons_file_name() gives, before ".png"

SDL_Surface *plate_icon(SDL_Surface *icon, int size);          // Centered on the library's square plate
bool plate_save(SDL_Surface *plated, const char *path);         // IMG_SavePNG
bool icons_dir(char *out, size_t size);                         // Beside config.ini; Linux ~/.config/streamflex/icons
void icons_file_name(const char *app_name, char *out, size_t size);   // Safe file name, ".png"
// icons_dir()'s rule over the paths it is given (util.c's icons_dir() gives it config.config_path and the
// home folder), made as it is found; and a new file's path in a folder, a clash numbered -2, -3 and on
bool icons_dir_at(const char *config_path, const char *home, char *out, size_t size);
bool icons_new_path(const char *dir, const char *app_name, char *out, size_t size);

#endif
