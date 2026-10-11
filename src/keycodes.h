// Key codes as config.ini stores them: SDL2's numbering, which every config written so far holds.
// SDL3 renumbered the media and browser keys (SDL2's AudioNext ... AudioFastForward, #40000102 to
// #4000011E), so a key event's SDL3 code is turned into the stored one as it arrives, and a stored
// code into SDL3's only to name its key. A key SDL2 never had, in that block, is stored 0x200 higher,
// clear of every SDL2 code. Pure: no SDL, no globals.
#ifndef KEYCODES_H
#define KEYCODES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32
#define KEYCODES_WINDOWS true   // SDL2 named Windows' mute key AudioMute (VK_VOLUME_MUTE); Linux's was Mute
#else
#define KEYCODES_WINDOWS false
#endif

uint32_t keycode_to_stored(uint32_t sdl3, bool windows);   // A key event's SDL3 code, as config.ini stores it
uint32_t keycode_from_stored(uint32_t stored);              // The SDL3 code naming a stored code; 0 for a key SDL3 never sends

#endif
