// The settings screen's pickers: the list picker (commands, the default menu, the device), the font
// picker (a list picker of the installed font families, each drawn in its own face) and the colour
// picker, drawn in the settings column in place of a page, and the contrast warning. The
// screen hands them what they draw with and how to apply a value (PickerHost). launcher.h, which has
// no include guard, must come first (Menu).
#ifndef SETTINGS_PICKERS_H
#define SETTINGS_PICKERS_H

#include <stdbool.h>
#include <stddef.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include "settings.h"

typedef struct {
    SettingsState *model;
    Menu **menus;
    int menu_count;
    void (*apply)(const SettingSlot *slot, bool refresh);          // Put a value into the launcher
    void (*event)(const SettingsEvent *event);                    // Hand an event to the pages
    void (*quiet)(const SettingsEvent *event);                    // Log and store a change, without refreshing
    void (*text)(TTF_Font *font, const char *text, int x, int y, int max_width, Uint8 alpha, bool right);
    int (*row)(const SettingsRow *row, bool highlighted, int x, int y, int width, int note_room);
    TTF_Font *font_row;
    TTF_Font *font_small;
    int row_height;
    int column_width;
    int margin;
} PickerHost;

void pickers_begin(const PickerHost *host);
void pickers_end(void);
bool pickers_active(void);
void pickers_open(SettingSlot *slot);
void pickers_command(const char *command);
void pickers_pads_changed(void);   // The pads present changed: an open Device list is made again
void pickers_draw(int x, int top, int bottom);
void pickers_path(char *out, size_t size);
const char *pickers_hint(void);
const char *pickers_note(void);
void contrast_warning(SettingId id, SettingColor color, char *out, size_t size);
void pickers_tick(void);    // Each frame: read more faces while the fonts load; run the capture's and the 10 s's clocks
void pickers_quit(void);    // At quit: the font list, kept for the session until then

// The key and gamepad bindings: capture, the 10 s confirmation, the binding's command picker, and the
// launcher's lists rebuilt from the model
void pickers_key_name(int device, int code, char *out, size_t size);   // A SettingsKeyNamer
void pickers_capture(int device);
void pickers_probation(int device, int code, const char *command);
void pickers_settle(void);           // Settings close: a change not yet confirmed goes back first
void pickers_end_probation(void);    // Discard: the 10 s end, with nothing to put back
bool pickers_raw_key(int code, bool repeat);
void pickers_raw_release(int code);
bool pickers_raw_pad(int label);
bool pickers_busy(void);             // A capture or the 10 s are running
void apply_bindings(const Bindings *bindings);
void pickers_open_binding_command(void);

#endif
