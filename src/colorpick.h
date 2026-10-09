// The color picker's model: a 6 x 4 grid of named swatches moved with all four arrows, and below it
// a Custom #RRGGBB row whose hex editor steps one digit at a time; and the contrast warning's math
// (WCAG relative luminance and contrast, an image's mean luminance, a color laid over another).
// Pure: no SDL, no globals, no allocation. settings_pickers.c draws it.
#ifndef COLORPICK_H
#define COLORPICK_H

#include <stdbool.h>
#include "settings.h"

#define COLORPICK_COLUMNS 6
#define COLORPICK_ROWS 4
#define COLORPICK_SWATCHES 24
#define COLORPICK_PRESETS 10              // The first ten swatches: 3a's presets, which Left and Right step
#define COLORPICK_CUSTOM 24               // The cursor on the Custom #RRGGBB row
#define COLORPICK_MIN_CONTRAST 3.0        // WCAG's 3:1: below it the caption warns

typedef enum {
    COLORPICK_UP,
    COLORPICK_DOWN,
    COLORPICK_LEFT,
    COLORPICK_RIGHT,
    COLORPICK_OK,
    COLORPICK_BACK
} ColorPickCommand;

typedef enum {
    COLORPICK_NONE,
    COLORPICK_MOVED,      // What the preview shows may have changed
    COLORPICK_CHOSEN,     // `chosen` holds the color
    COLORPICK_CANCELED
} ColorPickResult;

typedef struct {
    int cursor;              // 0-23 a swatch, row by row; COLORPICK_CUSTOM the Custom row
    int column;              // The column Up goes back to from the Custom row
    bool editing;            // The hex editor is open
    int digit;               // 0-5: the digit of #RRGGBB that Up and Down step
    SettingColor original;   // The color the picker opened with
    SettingColor hex;        // The hex editor's color
    SettingColor chosen;     // The color OK chose
} ColorPick;

void colorpick_open(ColorPick *pick, SettingColor current);
ColorPickResult colorpick_command(ColorPick *pick, ColorPickCommand command);
SettingColor colorpick_shown(const ColorPick *pick);
const char *colorpick_name(int index);
SettingColor colorpick_swatch(int index);
int colorpick_find(SettingColor color);
double color_luminance(SettingColor color);
double color_contrast(double a, double b);
// rgba: RGBA32 bytes, R at byte 0 (SDL_PIXELFORMAT_RGBA32), alpha ignored; pitch is bytes per row, at
// least width * 4; rgba is non-NULL whenever width and height are both positive
double color_mean_luminance(const unsigned char *rgba, int width, int height, int pitch);
double color_over(double below, SettingColor over, int alpha);

#endif
