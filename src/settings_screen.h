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
bool settings_raw_key(int code, bool repeat);  // A key while settings are open: true when capture or the 10 s took it
void settings_raw_release(int code);
bool settings_raw_pad(int label);              // Each frame: the gamepad label held, -1 for none; true when taken

#endif
