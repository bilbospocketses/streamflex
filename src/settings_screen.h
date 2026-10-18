// The settings screen (settings_screen.c): opened over the menu on show with :settings
#ifndef SETTINGS_SCREEN_H
#define SETTINGS_SCREEN_H

#include <stdbool.h>
#include <SDL3/SDL.h>
#include "applist.h"

bool settings_is_open(void);
void settings_open(void);
void settings_handle_command(const char *command);
void settings_draw(void);
void settings_close_now(void);
void settings_pads_changed(void);
void settings_refresh_all(void);          // Every refresh group in REFRESH_ORDER
void settings_refresh_for_reload(void);   // REFRESH_ORDER for a reload: the title font open, the gamepad running, the screensaver's dim kept
void settings_reflow(void);               // A new screen size: fonts, text cache, preview, layout; page and edits kept
int settings_drop_textures(void);         // Before the renderer goes: the text cache, the preview, the font samples, the browsed image; how many
void settings_remake_textures(void);      // On a reset or new renderer: drop, then make them again; page, cursor, picker and edits kept
bool settings_raw_key(int code, bool repeat);  // A key while settings are open: true when capture or the 10 s took it
void settings_raw_release(int code);
bool settings_raw_pad(int label);              // Each frame: the gamepad label held, -1 for none; true when taken
bool settings_text_key(SDL_Scancode scancode, SDL_Keymod mod, bool repeat);   // From KEY_DOWN, before handle_keypress; true when the keyboard took it
// The installed apps, for Add > App (spec "Installed apps"). settings_apps_start() starts this visit's
// scan on its own thread, and does nothing once this visit has started one; Task 10b's App page calls it,
// so a visit that never opens App lists nothing. Closing settings stops the scan and frees the list, and
// the next visit lists the apps anew. settings_apps() gives what it listed: NULL while it runs (*found:
// so far; also before the start) and when it failed (*why); an empty list, with its *why, when it found
// none.
void settings_apps_start(void);
const AppList *settings_apps(int *found, const char **why);
// After a change to the menus model: the launcher's menus are rebuilt from it, and when a menu came, went,
// was renamed or moved, the settings model, the screen's menus and the pickers' are made again over them.
// False, with the launcher's menus untouched, when out of memory, or when the model holds no menu (the
// launcher cannot show none): the change stays in the menus model alone. No keyboard is open for a menu's
// coming, going, rename or move; a tile's change made with the keyboard open (a title) remakes nothing but
// the menus, so the keyboard stays.
bool settings_menus_changed(void);

#endif
