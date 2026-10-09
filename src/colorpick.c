#include <math.h>
#include <string.h>
#include "colorpick.h"

#define SAMPLES 64   // The most points across and down an image's mean luminance reads

// The swatches, row by row: 3a's ten presets (COLORPICK_PRESETS), four neutrals, then ten accents
static const struct {
    const char *name;
    SettingColor color;
} SWATCHES[COLORPICK_SWATCHES] = {
    { "Black",      { 0x00, 0x00, 0x00 } },
    { "Charcoal",   { 0x1E, 0x1E, 0x1E } },
    { "Graphite",   { 0x33, 0x38, 0x3D } },
    { "Slate",      { 0x2E, 0x34, 0x40 } },
    { "Midnight",   { 0x12, 0x1A, 0x2E } },
    { "Navy",       { 0x0B, 0x1F, 0x3A } },
    { "Teal",       { 0x07, 0x60, 0x6C } },
    { "Forest",     { 0x1E, 0x3B, 0x2F } },
    { "Plum",       { 0x3B, 0x1F, 0x3A } },
    { "Burgundy",   { 0x4A, 0x15, 0x20 } },
    { "White",      { 0xFF, 0xFF, 0xFF } },
    { "Light gray", { 0xC8, 0xC8, 0xC8 } },
    { "Gray",       { 0x80, 0x80, 0x80 } },
    { "Dark gray",  { 0x4A, 0x4A, 0x4A } },
    { "Red",        { 0xD0, 0x30, 0x30 } },
    { "Orange",     { 0xE0, 0x70, 0x20 } },
    { "Amber",      { 0xF0, 0xB0, 0x00 } },
    { "Yellow",     { 0xF0, 0xE0, 0x40 } },
    { "Lime",       { 0x80, 0xC0, 0x40 } },
    { "Green",      { 0x30, 0xA0, 0x50 } },
    { "Cyan",       { 0x20, 0xB0, 0xC0 } },
    { "Blue",       { 0x30, 0x70, 0xD0 } },
    { "Indigo",     { 0x50, 0x48, 0xC0 } },
    { "Pink",       { 0xD0, 0x48, 0x90 } }
};

// A function to get a swatch's name
const char *colorpick_name(int index)
{
    return index >= 0 && index < COLORPICK_SWATCHES ? SWATCHES[index].name : "";
}

// A function to get a swatch's color
SettingColor colorpick_swatch(int index)
{
    SettingColor black = { 0, 0, 0 };
    return index >= 0 && index < COLORPICK_SWATCHES ? SWATCHES[index].color : black;
}

// A function to find a color among the swatches; -1 when it is none of them
int colorpick_find(SettingColor color)
{
    for (int i = 0; i < COLORPICK_SWATCHES; i++) {
        const SettingColor *s = &SWATCHES[i].color;
        if (s->r == color.r && s->g == color.g && s->b == color.b)
            return i;
    }
    return -1;
}

// A function to open the picker on a color: the cursor on its swatch, else on the Custom row
void colorpick_open(ColorPick *pick, SettingColor current)
{
    memset(pick, 0, sizeof(*pick));
    pick->original = current;
    pick->hex = current;
    pick->chosen = current;
    int found = colorpick_find(current);
    pick->cursor = found >= 0 ? found : COLORPICK_CUSTOM;
    pick->column = found >= 0 ? found % COLORPICK_COLUMNS : 0;
}

// A function to step one hex digit of a color (0 the red's first, 5 the blue's second) up or
// down, wrapping F to 0 and 0 to F
static SettingColor step_digit(SettingColor color, int digit, int direction)
{
    unsigned char *channel = digit < 2 ? &color.r : digit < 4 ? &color.g : &color.b;
    int shift = digit % 2 == 0 ? 4 : 0;
    int nibble = (*channel >> shift) & 0x0F;
    nibble = (nibble + (direction > 0 ? 1 : 15)) % 16;
    *channel = (unsigned char) ((*channel & ~(0x0F << shift)) | (nibble << shift));
    return color;
}

// A function to act on a key in the hex editor
static ColorPickResult edit(ColorPick *pick, ColorPickCommand command)
{
    switch (command) {
        case COLORPICK_LEFT:
            if (pick->digit == 0)
                return COLORPICK_NONE;
            pick->digit--;
            return COLORPICK_MOVED;
        case COLORPICK_RIGHT:
            if (pick->digit == 5)
                return COLORPICK_NONE;
            pick->digit++;
            return COLORPICK_MOVED;
        case COLORPICK_UP:
        case COLORPICK_DOWN:
            pick->hex = step_digit(pick->hex, pick->digit, command == COLORPICK_UP ? 1 : -1);
            return COLORPICK_MOVED;
        case COLORPICK_OK:
            pick->chosen = pick->hex;
            return COLORPICK_CHOSEN;
        case COLORPICK_BACK:
            pick->editing = false;
            return COLORPICK_MOVED;
    }
    return COLORPICK_NONE;
}

// A function to act on one key: move through the grid and onto the Custom row, open the hex
// editor, choose, or cancel
ColorPickResult colorpick_command(ColorPick *pick, ColorPickCommand command)
{
    if (pick->editing)
        return edit(pick, command);
    int before = pick->cursor;
    int row = pick->cursor / COLORPICK_COLUMNS;
    int column = pick->cursor % COLORPICK_COLUMNS;
    switch (command) {
        case COLORPICK_LEFT:
            if (pick->cursor != COLORPICK_CUSTOM && column > 0)
                pick->cursor--;
            break;
        case COLORPICK_RIGHT:
            if (pick->cursor != COLORPICK_CUSTOM && column < COLORPICK_COLUMNS - 1)
                pick->cursor++;
            break;
        case COLORPICK_UP:
            if (pick->cursor == COLORPICK_CUSTOM)
                pick->cursor = (COLORPICK_ROWS - 1) * COLORPICK_COLUMNS + pick->column;
            else if (row > 0)
                pick->cursor -= COLORPICK_COLUMNS;
            break;
        case COLORPICK_DOWN:
            if (pick->cursor == COLORPICK_CUSTOM)
                break;
            if (row < COLORPICK_ROWS - 1)
                pick->cursor += COLORPICK_COLUMNS;
            else {
                pick->column = column;
                pick->cursor = COLORPICK_CUSTOM;
            }
            break;
        case COLORPICK_OK:
            if (pick->cursor == COLORPICK_CUSTOM) {
                pick->editing = true;
                pick->digit = 0;
                return COLORPICK_MOVED;
            }
            pick->chosen = SWATCHES[pick->cursor].color;
            return COLORPICK_CHOSEN;
        case COLORPICK_BACK:
            // Cancel: the picker as it opened, so the preview goes back to the original color
            colorpick_open(pick, pick->original);
            return COLORPICK_CANCELED;
    }
    if (pick->cursor != COLORPICK_CUSTOM)
        pick->column = pick->cursor % COLORPICK_COLUMNS;
    return pick->cursor != before ? COLORPICK_MOVED : COLORPICK_NONE;
}

// A function to say what the preview shows: the swatch under the cursor; on the Custom row, the hex
// editor's color (the color the picker opened with, until it is edited)
SettingColor colorpick_shown(const ColorPick *pick)
{
    if (pick->cursor == COLORPICK_CUSTOM)
        return pick->hex;
    return SWATCHES[pick->cursor].color;
}

// A function to turn an sRGB channel into linear light, as WCAG defines it
static double linear(unsigned char channel)
{
    double c = (double) channel / 255.0;
    return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

// A function to give a color's relative luminance: 0 for black, 1 for white (WCAG 2)
double color_luminance(SettingColor color)
{
    return 0.2126 * linear(color.r) + 0.7152 * linear(color.g) + 0.0722 * linear(color.b);
}

// A function to give the contrast ratio of two luminances, 1 to 21, whichever is lighter
double color_contrast(double a, double b)
{
    double light = a > b ? a : b;
    double dark = a > b ? b : a;
    return (light + 0.05) / (dark + 0.05);
}

// A function to give an image's mean relative luminance, read at most SAMPLES points across and down;
// 0 for an empty image. The contract is colorpick.h's.
double color_mean_luminance(const unsigned char *rgba, int width, int height, int pitch)
{
    if (width <= 0 || height <= 0)
        return 0.0;
    int across = width < SAMPLES ? width : SAMPLES;
    int down = height < SAMPLES ? height : SAMPLES;
    double sum = 0.0;
    for (int j = 0; j < down; j++) {
        int y = (int) ((long long) j * height / down);
        for (int i = 0; i < across; i++) {
            int x = (int) ((long long) i * width / across);
            const unsigned char *p = rgba + (long long) y * pitch + (long long) x * 4;
            SettingColor color = { p[0], p[1], p[2] };
            sum += color_luminance(color);
        }
    }
    return sum / (double) (across * down);
}

// A function to give the luminance of a color laid at an alpha (0-255) over a background of a given
// luminance. It mixes the luminances, not the colors: close enough for an advisory warning.
double color_over(double below, SettingColor over, int alpha)
{
    double a = (double) alpha / 255.0;
    return a * color_luminance(over) + (1.0 - a) * below;
}
