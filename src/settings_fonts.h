// The font picker's fonts, which settings_pickers.c shows and chooses from: the installed font files
// listed on a thread (fontscan.c), their faces read on the main thread a few files a frame into
// families (fontlist.c) and kept for the session once read, and each family's name drawn in its own
// face, from a small cache. Main thread only: the listing's own thread is fontscan.c's.
#ifndef SETTINGS_FONTS_H
#define SETTINGS_FONTS_H

#include <stdbool.h>
#include <stddef.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include "settings.h"
#include "listpick.h"

#define FONT_VALUE_MAX (SETTING_TEXT_MAX + 16)   // A font row's value: "<face>|<path>"

bool fonts_ready(void);           // The fonts were read, and are kept for the session
bool fonts_start(void);           // Start listing them, unless that is under way; false (logged) when it cannot start
int fonts_files_read(void);       // The files read so far, for "Loading fonts... (N)"
bool fonts_tick(void);            // Read more of the files; true on the frame the reading ended (read, or let go)
bool fonts_fill(ListPick *to);    // One row a family; false when out of memory
void fonts_value_in_use(SettingId id, char *out, size_t size);   // The font the titles or the clock use, as a row's value
const char *fonts_value_path(const char *value);                 // The file in a row's value
SDL_Texture *fonts_sample(TTF_Font *row_font, const char *value, const char *name, int *w, int *h);
void fonts_clear_samples(void);
void fonts_quit(void);            // At quit: wait for a listing, let the list go, and say so

#endif
