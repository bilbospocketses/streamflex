// The colour picker's model: a 6 x 4 grid of named swatches moved with all four arrows, and below it
// a Custom #RRGGBB row whose hex editor steps one digit at a time; and the contrast warning's maths
// (WCAG relative luminance and contrast, an image's mean luminance, a colour laid over another).
// Pure: no SDL, no globals, no allocation. settings_pickers.c draws it.
#ifndef COLOURPICK_H
#define COLOURPICK_H

#include <stdbool.h>
#include "settings.h"

#define COLOURPICK_COLUMNS 6
#define COLOURPICK_ROWS 4
#define COLOURPICK_SWATCHES 24
#define COLOURPICK_PRESETS 10              // The first ten swatches: 3a's presets, which Left and Right step
#define COLOURPICK_CUSTOM 24               // The cursor on the Custom #RRGGBB row
#define COLOURPICK_MIN_CONTRAST 3.0        // WCAG's 3:1: below it the caption warns

typedef enum {
    COLOURPICK_UP,
    COLOURPICK_DOWN,
    COLOURPICK_LEFT,
    COLOURPICK_RIGHT,
    COLOURPICK_OK,
    COLOURPICK_BACK
} ColourPickCommand;

typedef enum {
    COLOURPICK_NONE,
    COLOURPICK_MOVED,      // What the preview shows may have changed
    COLOURPICK_CHOSEN,     // `chosen` holds the colour
    COLOURPICK_CANCELLED
} ColourPickResult;

typedef struct {
    int cursor;              // 0-23 a swatch, row by row; COLOURPICK_CUSTOM the Custom row
    int column;              // The column Up goes back to from the Custom row
    bool editing;            // The hex editor is open
    int digit;               // 0-5: the digit of #RRGGBB that Up and Down step
    SettingColor original;   // The colour the picker opened with
    SettingColor hex;        // The hex editor's colour
    SettingColor chosen;     // The colour OK chose
} ColourPick;

void colourpick_open(ColourPick *pick, SettingColor current);
ColourPickResult colourpick_command(ColourPick *pick, ColourPickCommand command);
SettingColor colourpick_shown(const ColourPick *pick);
const char *colourpick_name(int index);
SettingColor colourpick_swatch(int index);
int colourpick_find(SettingColor color);
double colour_luminance(SettingColor color);
double colour_contrast(double a, double b);
// rgba: RGBA32 bytes, R at byte 0 (SDL_PIXELFORMAT_RGBA32), alpha ignored; pitch is bytes per row, at
// least width * 4; rgba is non-NULL whenever width and height are both positive
double colour_mean_luminance(const unsigned char *rgba, int width, int height, int pitch);
double colour_over(double below, SettingColor over, int alpha);

#endif
