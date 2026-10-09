#include <math.h>
#include <stdbool.h>
#include <string.h>
#include "check.h"
#include "colorpick.h"

// A function to make a colour
static SettingColor rgb(unsigned char r, unsigned char g, unsigned char b)
{
    SettingColor color = { r, g, b };
    return color;
}

// A function to compare two colours
static bool same(SettingColor a, SettingColor b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

// A function to compare two numbers to 3 places
static bool near(double a, double b)
{
    return fabs(a - b) < 0.001;
}

// A function to test the swatches: 24, in the spec's order, with names
static void test_swatches(void)
{
    CHECK_STR(colorpick_name(0), "Black");
    CHECK(same(colorpick_swatch(0), rgb(0x00, 0x00, 0x00)));
    CHECK_STR(colorpick_name(9), "Burgundy");
    CHECK(same(colorpick_swatch(9), rgb(0x4A, 0x15, 0x20)));
    CHECK_STR(colorpick_name(10), "White");
    CHECK_STR(colorpick_name(13), "Dark grey");
    CHECK_STR(colorpick_name(14), "Red");
    CHECK(same(colorpick_swatch(14), rgb(0xD0, 0x30, 0x30)));
    CHECK_STR(colorpick_name(23), "Pink");
    CHECK(same(colorpick_swatch(23), rgb(0xD0, 0x48, 0x90)));
    CHECK_INT(colorpick_find(rgb(0x07, 0x60, 0x6C)), 6);        // Teal
    CHECK_INT(colorpick_find(rgb(0x07, 0x60, 0x6D)), -1);
}

// A function to test moving through the grid, onto the Custom row and back
static void test_grid(void)
{
    ColourPick pick;
    colorpick_open(&pick, rgb(0x0B, 0x1F, 0x3A));               // Navy, the sixth swatch
    CHECK_INT(pick.cursor, 5);
    CHECK_INT(colorpick_command(&pick, COLORPICK_RIGHT), COLORPICK_NONE);   // The row's end
    CHECK_INT(colorpick_command(&pick, COLORPICK_UP), COLORPICK_NONE);      // The grid's top
    CHECK_INT(colorpick_command(&pick, COLORPICK_LEFT), COLORPICK_MOVED);
    CHECK_INT(pick.cursor, 4);
    CHECK(same(colorpick_shown(&pick), rgb(0x12, 0x1A, 0x2E)));   // The preview follows: Midnight
    for (int i = 0; i < 3; i++)
        CHECK_INT(colorpick_command(&pick, COLORPICK_DOWN), COLORPICK_MOVED);
    CHECK_INT(pick.cursor, 22);                                    // Row 4, column 5: Indigo
    CHECK_INT(colorpick_command(&pick, COLORPICK_DOWN), COLORPICK_MOVED);
    CHECK_INT(pick.cursor, COLORPICK_CUSTOM);
    CHECK(same(colorpick_shown(&pick), rgb(0x0B, 0x1F, 0x3A)));   // Custom shows the colour it opened with
    CHECK_INT(colorpick_command(&pick, COLORPICK_DOWN), COLORPICK_NONE);
    CHECK_INT(colorpick_command(&pick, COLORPICK_LEFT), COLORPICK_NONE);   // One wide row
    CHECK_INT(colorpick_command(&pick, COLORPICK_UP), COLORPICK_MOVED);
    CHECK_INT(pick.cursor, 22);                                    // Back to the column it left
    CHECK_INT(colorpick_command(&pick, COLORPICK_OK), COLORPICK_CHOSEN);
    CHECK(same(pick.chosen, rgb(0x50, 0x48, 0xC0)));

    // Back on the grid cancels, and the preview goes back
    colorpick_open(&pick, rgb(0x0B, 0x1F, 0x3A));
    colorpick_command(&pick, COLORPICK_LEFT);                  // Column 5, so Back must restore it too
    colorpick_command(&pick, COLORPICK_DOWN);
    CHECK_INT(colorpick_command(&pick, COLORPICK_BACK), COLORPICK_CANCELLED);
    CHECK(same(colorpick_shown(&pick), rgb(0x0B, 0x1F, 0x3A)));
    CHECK_INT(pick.cursor, 5);                                     // Back on Navy's swatch, not on Custom
    CHECK_INT(pick.column, 5);
}

// A function to test the hex editor: digits chosen, stepped with wrapping, kept or left
static void test_hex(void)
{
    ColourPick pick;
    colorpick_open(&pick, rgb(0x12, 0x34, 0x5F));               // Not a swatch: the cursor on Custom
    CHECK_INT(pick.cursor, COLORPICK_CUSTOM);
    CHECK_INT(colorpick_command(&pick, COLORPICK_OK), COLORPICK_MOVED);
    CHECK(pick.editing);
    CHECK_INT(pick.digit, 0);
    CHECK_INT(colorpick_command(&pick, COLORPICK_LEFT), COLORPICK_NONE);    // The first digit
    CHECK_INT(colorpick_command(&pick, COLORPICK_UP), COLORPICK_MOVED);
    CHECK(same(colorpick_shown(&pick), rgb(0x22, 0x34, 0x5F)));   // 1 -> 2 in the red's first digit
    for (int i = 0; i < 5; i++)
        colorpick_command(&pick, COLORPICK_RIGHT);
    CHECK_INT(pick.digit, 5);
    CHECK_INT(colorpick_command(&pick, COLORPICK_RIGHT), COLORPICK_NONE);   // The last digit
    CHECK_INT(colorpick_command(&pick, COLORPICK_UP), COLORPICK_MOVED);
    CHECK(same(colorpick_shown(&pick), rgb(0x22, 0x34, 0x50)));   // F wraps to 0
    colorpick_command(&pick, COLORPICK_DOWN);
    colorpick_command(&pick, COLORPICK_DOWN);
    CHECK(same(colorpick_shown(&pick), rgb(0x22, 0x34, 0x5E)));   // 0 wraps to F, then E

    // Back leaves the editor, not the picker; OK in it keeps the colour
    CHECK_INT(colorpick_command(&pick, COLORPICK_BACK), COLORPICK_MOVED);
    CHECK(!pick.editing);
    CHECK_INT(pick.cursor, COLORPICK_CUSTOM);
    CHECK_INT(colorpick_command(&pick, COLORPICK_OK), COLORPICK_MOVED);   // Opens on the edited colour
    CHECK(same(colorpick_shown(&pick), rgb(0x22, 0x34, 0x5E)));
    CHECK_INT(colorpick_command(&pick, COLORPICK_OK), COLORPICK_CHOSEN);
    CHECK(same(pick.chosen, rgb(0x22, 0x34, 0x5E)));
}

// A function to test luminance and contrast against WCAG's own figures
static void test_contrast(void)
{
    CHECK(near(colour_luminance(rgb(0, 0, 0)), 0.0));
    CHECK(near(colour_luminance(rgb(255, 255, 255)), 1.0));
    CHECK(near(colour_luminance(rgb(0x80, 0x80, 0x80)), 0.2159));
    CHECK(near(colour_contrast(1.0, 0.0), 21.0));
    CHECK(near(colour_contrast(0.0, 1.0), 21.0));           // Either way round
    double grey = colour_luminance(rgb(0x76, 0x76, 0x76));   // WCAG's #767676 on white is 4.54:1
    CHECK(fabs(colour_contrast(1.0, grey) - 4.54) < 0.01);

    // A black overlay at half opacity over white; a colour at full opacity is the colour
    CHECK(near(colour_over(1.0, rgb(0, 0, 0), 128), 1.0 - 128.0 / 255.0));
    CHECK(near(colour_over(0.3, rgb(255, 255, 255), 255), 1.0));
    CHECK(near(colour_over(0.3, rgb(255, 255, 255), 0), 0.3));
}

// A function to test an image's mean luminance, read from its pixels
static void test_mean(void)
{
    unsigned char pixels[2 * 2 * 4];
    memset(pixels, 0, sizeof(pixels));
    for (int i = 0; i < 4; i++)
        pixels[i * 4 + 3] = 0xFF;
    memset(pixels, 0xFF, 4);                                  // One white pixel, three black
    CHECK(near(colour_mean_luminance(pixels, 2, 2, 8), 0.25));

    // An image larger than the sampling grid (test_mean_sampling gives a pitch wider than the row)
    static unsigned char big[300 * 200 * 4];
    for (int i = 0; i < 300 * 200; i++) {
        big[i * 4] = big[i * 4 + 1] = big[i * 4 + 2] = 0xFF;
        big[i * 4 + 3] = 0xFF;
    }
    CHECK(near(colour_mean_luminance(big, 300, 200, 300 * 4), 1.0));
    CHECK(near(colour_mean_luminance(big, 0, 0, 0), 0.0));    // Nothing to read
}

// The spec's 24 swatches, in order, to check the whole table against
static const struct {
    const char *name;
    SettingColor color;
} SPEC[] = {
    { "Black", { 0x00, 0x00, 0x00 } }, { "Charcoal", { 0x1E, 0x1E, 0x1E } }, { "Graphite", { 0x33, 0x38, 0x3D } },
    { "Slate", { 0x2E, 0x34, 0x40 } }, { "Midnight", { 0x12, 0x1A, 0x2E } }, { "Navy", { 0x0B, 0x1F, 0x3A } },
    { "Teal", { 0x07, 0x60, 0x6C } }, { "Forest", { 0x1E, 0x3B, 0x2F } }, { "Plum", { 0x3B, 0x1F, 0x3A } },
    { "Burgundy", { 0x4A, 0x15, 0x20 } }, { "White", { 0xFF, 0xFF, 0xFF } }, { "Light grey", { 0xC8, 0xC8, 0xC8 } },
    { "Grey", { 0x80, 0x80, 0x80 } }, { "Dark grey", { 0x4A, 0x4A, 0x4A } }, { "Red", { 0xD0, 0x30, 0x30 } },
    { "Orange", { 0xE0, 0x70, 0x20 } }, { "Amber", { 0xF0, 0xB0, 0x00 } }, { "Yellow", { 0xF0, 0xE0, 0x40 } },
    { "Lime", { 0x80, 0xC0, 0x40 } }, { "Green", { 0x30, 0xA0, 0x50 } }, { "Cyan", { 0x20, 0xB0, 0xC0 } },
    { "Blue", { 0x30, 0x70, 0xD0 } }, { "Indigo", { 0x50, 0x48, 0xC0 } }, { "Pink", { 0xD0, 0x48, 0x90 } }
};

// A function to test every swatch against the spec, the ends of the table, and the header's values
static void test_swatch_table(void)
{
    CHECK_INT((int) (sizeof(SPEC) / sizeof(SPEC[0])), COLORPICK_SWATCHES);
    for (int i = 0; i < COLORPICK_SWATCHES; i++) {
        CHECK_STR(colorpick_name(i), SPEC[i].name);
        CHECK(same(colorpick_swatch(i), SPEC[i].color));
        CHECK_INT(colorpick_find(SPEC[i].color), i);   // Each found at its own place: no two alike
    }

    // Outside the table: no name, and black
    CHECK_STR(colorpick_name(-1), "");
    CHECK_STR(colorpick_name(COLORPICK_SWATCHES), "");
    CHECK(same(colorpick_swatch(-1), rgb(0, 0, 0)));
    CHECK(same(colorpick_swatch(COLORPICK_SWATCHES), rgb(0, 0, 0)));

    CHECK_INT(COLORPICK_COLUMNS, 6);
    CHECK_INT(COLORPICK_ROWS, 4);
    CHECK_INT(COLORPICK_SWATCHES, 24);
    CHECK_INT(COLORPICK_CUSTOM, 24);
    double threshold = COLORPICK_MIN_CONTRAST;
    CHECK(near(threshold, 3.0));                          // WCAG's 3:1
}

// A function to test the grid's moves the first test does not make: Right and Up moving, Left at a
// row's start, Right on the Custom row, the column kept, and Up from Custom when it opened there
static void test_grid_moves(void)
{
    ColourPick pick;
    colorpick_open(&pick, rgb(0x0B, 0x1F, 0x3A));               // Navy: row 1, column 6
    CHECK_INT(pick.column, 5);
    CHECK_INT(colorpick_command(&pick, COLORPICK_LEFT), COLORPICK_MOVED);
    CHECK_INT(pick.column, 4);                                     // The column follows the cursor
    CHECK_INT(colorpick_command(&pick, COLORPICK_RIGHT), COLORPICK_MOVED);
    CHECK_INT(pick.cursor, 5);
    CHECK_INT(colorpick_command(&pick, COLORPICK_DOWN), COLORPICK_MOVED);
    CHECK_INT(colorpick_command(&pick, COLORPICK_DOWN), COLORPICK_MOVED);
    CHECK_INT(pick.cursor, 17);                                    // Yellow
    CHECK_INT(colorpick_command(&pick, COLORPICK_UP), COLORPICK_MOVED);
    CHECK_INT(pick.cursor, 11);                                    // Light grey
    CHECK(same(colorpick_shown(&pick), rgb(0xC8, 0xC8, 0xC8)));

    // Left at a row's start stays: it does not wrap to the row above
    colorpick_open(&pick, rgb(0x07, 0x60, 0x6C));               // Teal: row 2, column 1
    CHECK_INT(colorpick_command(&pick, COLORPICK_LEFT), COLORPICK_NONE);
    CHECK_INT(pick.cursor, 6);

    // A colour that is not a swatch: the cursor on Custom, where Right does nothing and Up goes to
    // the first column of the last row
    colorpick_open(&pick, rgb(0x12, 0x34, 0x56));
    CHECK_INT(colorpick_command(&pick, COLORPICK_RIGHT), COLORPICK_NONE);
    CHECK_INT(pick.cursor, COLORPICK_CUSTOM);
    CHECK_INT(colorpick_command(&pick, COLORPICK_UP), COLORPICK_MOVED);
    CHECK_INT(pick.cursor, 18);
    CHECK(same(colorpick_shown(&pick), rgb(0x80, 0xC0, 0x40)));   // Lime
}

// A function to test Back on the Custom row after an edit: the picker cancels and the preview goes
// back to the colour it opened with, not the edited one
static void test_cancel_edit(void)
{
    ColourPick pick;
    colorpick_open(&pick, rgb(0x12, 0x34, 0x5F));
    colorpick_command(&pick, COLORPICK_OK);
    colorpick_command(&pick, COLORPICK_UP);
    CHECK(same(colorpick_shown(&pick), rgb(0x22, 0x34, 0x5F)));
    CHECK_INT(colorpick_command(&pick, COLORPICK_BACK), COLORPICK_MOVED);   // Out of the editor
    CHECK_INT(colorpick_command(&pick, COLORPICK_BACK), COLORPICK_CANCELLED);
    CHECK(!pick.editing);
    CHECK_INT(pick.cursor, COLORPICK_CUSTOM);
    CHECK(same(colorpick_shown(&pick), rgb(0x12, 0x34, 0x5F)));
}

// A function to test the hex editor on every digit, Left moving back, a reopened editor starting on
// the first digit, and a command it does not know
static void test_hex_digits(void)
{
    ColourPick pick;
    colorpick_open(&pick, rgb(0x12, 0x34, 0x56));
    colorpick_command(&pick, COLORPICK_OK);
    const SettingColor after[6] = {
        { 0x22, 0x34, 0x56 }, { 0x23, 0x34, 0x56 }, { 0x23, 0x44, 0x56 },
        { 0x23, 0x45, 0x56 }, { 0x23, 0x45, 0x66 }, { 0x23, 0x45, 0x67 }
    };
    for (int digit = 0; digit < 6; digit++) {
        if (digit > 0)
            CHECK_INT(colorpick_command(&pick, COLORPICK_RIGHT), COLORPICK_MOVED);
        CHECK_INT(pick.digit, digit);
        CHECK_INT(colorpick_command(&pick, COLORPICK_UP), COLORPICK_MOVED);
        CHECK(same(colorpick_shown(&pick), after[digit]));
    }
    CHECK_INT(colorpick_command(&pick, COLORPICK_LEFT), COLORPICK_MOVED);
    CHECK_INT(pick.digit, 4);
    CHECK_INT(colorpick_command(&pick, COLORPICK_DOWN), COLORPICK_MOVED);
    CHECK(same(colorpick_shown(&pick), rgb(0x23, 0x45, 0x57)));

    // An unknown command changes nothing, in the editor or on the grid
    CHECK_INT(colorpick_command(&pick, (ColourPickCommand) 99), COLORPICK_NONE);
    CHECK(pick.editing);
    CHECK_INT(pick.digit, 4);

    // Leaving the editor and opening it again starts on the first digit
    colorpick_command(&pick, COLORPICK_BACK);
    CHECK_INT(colorpick_command(&pick, (ColourPickCommand) 99), COLORPICK_NONE);
    CHECK_INT(pick.cursor, COLORPICK_CUSTOM);
    colorpick_command(&pick, COLORPICK_OK);
    CHECK_INT(pick.digit, 0);
}

// A function to test each channel's weight and the dark end of the sRGB curve, which the grey figures
// cannot tell apart
static void test_luminance_channels(void)
{
    CHECK(near(colour_luminance(rgb(255, 0, 0)), 0.2126));
    CHECK(near(colour_luminance(rgb(0, 255, 0)), 0.7152));
    CHECK(near(colour_luminance(rgb(0, 0, 255)), 0.0722));
    CHECK(fabs(colour_luminance(rgb(1, 1, 1)) - 1.0 / 255.0 / 12.92) < 1e-7);   // The linear part
    CHECK(near(colour_contrast(0.5, 0.2), 2.2));
    CHECK(near(colour_contrast(0.2, 0.5), 2.2));
}

// A function to test which pixels the mean reads: rows found by the pitch, points spread over a large
// image, every pixel of a small one, and an image with no rows or no columns
static void test_mean_sampling(void)
{
    // A 2 x 2 image in rows 16 bytes apart, the padding white: one white pixel of four
    unsigned char padded[2 * 16];
    memset(padded, 0xFF, sizeof(padded));
    memset(padded, 0, 8);
    memset(padded + 16, 0, 8);
    memset(padded, 0xFF, 3);
    CHECK(near(colour_mean_luminance(padded, 2, 2, 16), 0.25));

    // Larger than the sampling grid: the left half black, then the top half black
    static unsigned char big[300 * 200 * 4];
    for (int y = 0; y < 200; y++)
        for (int x = 0; x < 300; x++)
            memset(big + (y * 300 + x) * 4, x < 150 ? 0x00 : 0xFF, 4);
    CHECK(near(colour_mean_luminance(big, 300, 200, 300 * 4), 0.5));
    for (int y = 0; y < 200; y++)
        for (int x = 0; x < 300; x++)
            memset(big + (y * 300 + x) * 4, y < 100 ? 0x00 : 0xFF, 4);
    CHECK(near(colour_mean_luminance(big, 300, 200, 300 * 4), 0.5));

    // A 3 x 3 image is read pixel by pixel: one white pixel of nine
    unsigned char small[3 * 3 * 4];
    memset(small, 0, sizeof(small));
    memset(small, 0xFF, 3);
    CHECK(near(colour_mean_luminance(small, 3, 3, 12), 1.0 / 9.0));

    // No rows, or no columns: nothing to read
    CHECK(near(colour_mean_luminance(small, 3, 0, 12), 0.0));
    CHECK(near(colour_mean_luminance(small, 0, 3, 12), 0.0));
}

int main(void)
{
    test_swatches();
    test_grid();
    test_hex();
    test_contrast();
    test_mean();
    test_swatch_table();
    test_grid_moves();
    test_cancel_edit();
    test_hex_digits();
    test_luminance_channels();
    test_mean_sampling();
    return check_report();
}
