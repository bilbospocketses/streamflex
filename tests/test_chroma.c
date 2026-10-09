#include <string.h>
#include "check.h"
#include "chroma.h"

// A function to tell whether a pixel of an RGBA buffer holds a color
static int pixel_is(const unsigned char *pixel, int r, int g, int b, int a)
{
    return pixel[0] == r && pixel[1] == g && pixel[2] == b && pixel[3] == a;
}

// A function to test the default key, #010101: an opaque pixel within one step of it in every
// channel becomes #030303; one two steps off in any channel, or with any transparency, stays
static void test_default_key(void)
{
    unsigned char rgba[] = {
        1, 1, 1, 255,     // the key: moves
        0, 0, 0, 255,     // one step off in every channel: moves
        2, 2, 2, 255,     // one step off the other way: moves
        0, 2, 1, 255,     // a mix, each channel one step off at most: moves
        3, 1, 1, 255,     // two steps off in red: stays
        1, 1, 3, 255,     // two steps off in blue: stays
        1, 1, 1, 254,     // not fully opaque: stays
        0, 0, 0, 0        // transparent: stays
    };
    CHECK_INT(chroma_keep_off(rgba, 8, 1, (int) sizeof(rgba), 1, 1, 1), 4);
    CHECK(pixel_is(rgba, 3, 3, 3, 255));
    CHECK(pixel_is(rgba + 4, 3, 3, 3, 255));
    CHECK(pixel_is(rgba + 8, 3, 3, 3, 255));
    CHECK(pixel_is(rgba + 12, 3, 3, 3, 255));
    CHECK(pixel_is(rgba + 16, 3, 1, 1, 255));
    CHECK(pixel_is(rgba + 20, 1, 1, 3, 255));
    CHECK(pixel_is(rgba + 24, 1, 1, 1, 254));
    CHECK(pixel_is(rgba + 28, 0, 0, 0, 0));
}

// A function to test keys at the ends of the range, where two steps off must stay inside it: a key
// channel below 128 moves away from 0, one from 128 up moves towards 0
static void test_other_keys(void)
{
    unsigned char black[] = { 0, 0, 0, 255, 1, 1, 1, 255, 2, 0, 0, 255 };
    CHECK_INT(chroma_keep_off(black, 3, 1, 12, 0, 0, 0), 2);
    CHECK(pixel_is(black, 2, 2, 2, 255));
    CHECK(pixel_is(black + 4, 2, 2, 2, 255));
    CHECK(pixel_is(black + 8, 2, 0, 0, 255));      // two steps off in red: stays

    unsigned char white[] = { 255, 255, 255, 255, 254, 254, 254, 255, 253, 255, 255, 255 };
    CHECK_INT(chroma_keep_off(white, 3, 1, 12, 255, 255, 255), 2);
    CHECK(pixel_is(white, 253, 253, 253, 255));
    CHECK(pixel_is(white + 4, 253, 253, 253, 255));
    CHECK(pixel_is(white + 8, 253, 255, 255, 255));  // two steps off in red: stays

    unsigned char magenta[] = { 255, 0, 255, 255, 254, 1, 254, 255, 255, 2, 255, 255 };
    CHECK_INT(chroma_keep_off(magenta, 3, 1, 12, 255, 0, 255), 2);
    CHECK(pixel_is(magenta, 253, 2, 253, 255));
    CHECK(pixel_is(magenta + 4, 253, 2, 253, 255));
    CHECK(pixel_is(magenta + 8, 255, 2, 255, 255));  // two steps off in green: stays

    unsigned char middle[] = { 127, 128, 129, 255 };    // 127 moves up, 128 and 129 down
    CHECK_INT(chroma_keep_off(middle, 1, 1, 4, 127, 128, 128), 1);
    CHECK(pixel_is(middle, 129, 126, 126, 255));
}

// A function to test what the rule is for: after it, a 2 x 2 average of dark opaque pixels (as a
// renderer's linear filter takes when it halves an image) is never the key. Black and #020202
// columns average exactly to #010101 before it.
static void test_blend_cannot_reach_the_key(void)
{
    unsigned char rgba[] = { 0, 0, 0, 255, 2, 2, 2, 255, 0, 0, 0, 255, 2, 2, 2, 255 };
    int before = (rgba[0] + rgba[4] + rgba[8] + rgba[12]) / 4;
    CHECK_INT(before, 1);
    CHECK_INT(chroma_keep_off(rgba, 2, 2, 8, 1, 1, 1), 4);
    for (int c = 0; c < 3; c++)
        CHECK_INT((rgba[c] + rgba[4 + c] + rgba[8 + c] + rgba[12 + c]) / 4, 3);
}

// A function to test that the rows' padding (pitch past the width) is never read as a pixel or
// written, and that every row is visited
static void test_pitch(void)
{
    // Two rows of two pixels, each row padded to 12 bytes with key-colored bytes
    unsigned char rgba[24];
    for (size_t i = 0; i < sizeof(rgba); i++)
        rgba[i] = (i % 4 == 3) ? 255 : 1;
    CHECK_INT(chroma_keep_off(rgba, 2, 2, 12, 1, 1, 1), 4);
    CHECK(pixel_is(rgba, 3, 3, 3, 255));
    CHECK(pixel_is(rgba + 4, 3, 3, 3, 255));
    CHECK(pixel_is(rgba + 8, 1, 1, 1, 255));    // padding, untouched
    CHECK(pixel_is(rgba + 12, 3, 3, 3, 255));
    CHECK(pixel_is(rgba + 16, 3, 3, 3, 255));
    CHECK(pixel_is(rgba + 20, 1, 1, 1, 255));   // padding, untouched
}

// A function to test that an image with no pixel near the key, or no pixels at all, is left as it was
static void test_nothing_to_move(void)
{
    unsigned char rgba[] = { 10, 20, 30, 255, 1, 1, 1, 128 };
    unsigned char before[sizeof(rgba)];
    memcpy(before, rgba, sizeof(rgba));
    CHECK_INT(chroma_keep_off(rgba, 2, 1, (int) sizeof(rgba), 1, 1, 1), 0);
    CHECK(memcmp(rgba, before, sizeof(rgba)) == 0);
    CHECK_INT(chroma_keep_off(rgba, 0, 0, 0, 1, 1, 1), 0);
    CHECK(memcmp(rgba, before, sizeof(rgba)) == 0);
}

// A function to test that a second pass moves nothing: a moved pixel is off the key for good
static void test_idempotent(void)
{
    unsigned char rgba[] = { 1, 1, 1, 255, 0, 2, 0, 255 };
    CHECK_INT(chroma_keep_off(rgba, 2, 1, 8, 1, 1, 1), 2);
    CHECK_INT(chroma_keep_off(rgba, 2, 1, 8, 1, 1, 1), 0);
}

int main(void)
{
    test_default_key();
    test_other_keys();
    test_blend_cannot_reach_the_key();
    test_pitch();
    test_nothing_to_move();
    test_idempotent();
    return check_report();
}
