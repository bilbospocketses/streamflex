#include <stddef.h>
#include "chroma.h"

// A function to tell whether a channel is within one step of the key's
static int near_channel(unsigned char value, unsigned char key)
{
    return value + 1 >= key && value <= key + 1;
}

// A function to give a channel of the key two steps off it: away from 0 for a key channel below
// 128, towards 0 from 128 up, so it never leaves the range
static unsigned char two_off(unsigned char key)
{
    return (unsigned char) (key < 128 ? key + 2 : key - 2);
}

// A function to move each fully opaque pixel of an RGBA image within one step of the key in every
// channel (the key itself included) to two steps off it in every channel. For #010101 that is
// #030303. The renderer's scaling averages neighboring pixels, and pixels at #030303 or lighter
// cannot average back to #010101; art with other colors as far off still could (#000003, #000300
// and #030000 together average to it). A pixel with any transparency is left alone. `pitch` is the bytes from one row to
// the next. Returns how many pixels moved.
int chroma_keep_off(unsigned char *rgba, int width, int height, int pitch,
                    unsigned char key_r, unsigned char key_g, unsigned char key_b)
{
    unsigned char off_r = two_off(key_r);
    unsigned char off_g = two_off(key_g);
    unsigned char off_b = two_off(key_b);
    int moved = 0;
    for (int y = 0; y < height; y++) {
        unsigned char *pixel = rgba + (size_t) y * (size_t) pitch;
        for (int x = 0; x < width; x++, pixel += 4) {
            if (pixel[3] == 255 && near_channel(pixel[0], key_r) && near_channel(pixel[1], key_g)
                && near_channel(pixel[2], key_b)) {
                pixel[0] = off_r;
                pixel[1] = off_g;
                pixel[2] = off_b;
                moved++;
            }
        }
    }
    return moved;
}
