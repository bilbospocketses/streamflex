// Keeping images off the chroma key. On Windows, Transparent mode makes every pixel of the window
// that is exactly the key color see-through, so an icon's opaque pixel of that color, or one the
// renderer's scaling blends onto it from neighbors next to the key, would be a hole in it. Pure:
// no SDL, so tests/test_chroma.c builds it on its own; the caller hands it the pixels as RGBA bytes.
#ifndef CHROMA_H
#define CHROMA_H

// Moves each fully opaque pixel within one step of the key in every channel to two steps off it;
// returns how many moved
int chroma_keep_off(unsigned char *rgba, int width, int height, int pitch,
                    unsigned char key_r, unsigned char key_g, unsigned char key_b);

#endif
