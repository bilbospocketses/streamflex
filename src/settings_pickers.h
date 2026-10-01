// The settings screen's pickers: the list picker (commands, the default menu, the device) and the
// colour picker, drawn in the settings column in place of a page, and the contrast warning. The
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

#endif
