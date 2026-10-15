// The settings screen (settings_screen.c): opened over the menu on show with :settings
#ifndef SETTINGS_SCREEN_H
#define SETTINGS_SCREEN_H

#include <stdbool.h>

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

#endif
