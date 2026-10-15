#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "keycodes.h"

#define FIRST_MOVED 0x40000102u      // SDL2's AudioNext, where the block SDL3 renumbered starts...
#define LAST_MOVED 0x4000011Eu       // ...to SDL2's AudioFastForward, where it ends
#define SDL3_ONLY 0x200u             // How far up a key SDL2 never had is stored, clear of every SDL2 code
#define SDL3_MUTE 0x4000007Fu        // Mute: SDL_SCANCODE_MUTE's key code in both
#define SDL2_AUDIOMUTE 0x40000106u   // Windows' mute key under SDL2 (VK_VOLUME_MUTE); Linux's was Mute

// Each physical key SDL3 sends another code for than SDL2 did: SDL3's code, and SDL2's, which config.ini
// stores (SDL2 2.26.5 and SDL3 3.2.10, by scancodes_linux.h's evdev names, which X11 and Wayland map
// through, and Windows' scan codes). Where SDL2 gave two keys one code, the key a remote sends comes
// first: play/pause before play.
static const struct { uint32_t sdl3, stored; } MOVED[] = {
    { 0x4000010Bu, 0x40000102u },   // Media next track: SDL2's AudioNext
    { 0x4000010Cu, 0x40000103u },   // Media previous track: AudioPrev
    { 0x4000010Du, 0x40000104u },   // Media stop: AudioStop
    { 0x4000010Fu, 0x40000105u },   // Media play/pause: AudioPlay
    { 0x40000106u, 0x40000105u },   // Media play (KEY_PLAY): AudioPlay as well
    { 0x40000110u, 0x40000107u },   // Media select: MediaSelect
    { 0x40000118u, 0x4000010Cu },   // AC search
    { 0x40000119u, 0x4000010Du },   // AC home
    { 0x4000011Au, 0x4000010Eu },   // AC back
    { 0x4000011Bu, 0x4000010Fu },   // AC forward
    { 0x4000011Cu, 0x40000110u },   // AC stop
    { 0x4000011Du, 0x40000111u },   // AC refresh
    { 0x4000011Eu, 0x40000112u },   // AC bookmarks
    { 0x4000010Eu, 0x40000119u },   // Media eject: Eject
    { 0x40000102u, 0x4000011Au },   // Sleep
    { 0x4000010Au, 0x4000011Du },   // Media rewind: AudioRewind
    { 0x40000109u, 0x4000011Eu },   // Media fast forward: AudioFastForward
};
#define MOVED_COUNT (sizeof(MOVED) / sizeof(MOVED[0]))

// A function to give the code config.ini stores for a key event's SDL3 key code
uint32_t keycode_to_stored(uint32_t sdl3, bool windows)
{
    if (windows && sdl3 == SDL3_MUTE)
        return SDL2_AUDIOMUTE;
    for (size_t i = 0; i < MOVED_COUNT; i++) {
        if (MOVED[i].sdl3 == sdl3)
            return MOVED[i].stored;
    }
    if (sdl3 >= FIRST_MOVED && sdl3 <= LAST_MOVED)
        return sdl3 + SDL3_ONLY;     // A key SDL2 never had: Wake, Channel up, Media pause, AC New and the rest
    return sdl3;
}

// A function to give the SDL3 key code that names a stored code; 0 for a key SDL3 never sends
uint32_t keycode_from_stored(uint32_t stored)
{
    if (stored == SDL2_AUDIOMUTE)
        return SDL3_MUTE;
    for (size_t i = 0; i < MOVED_COUNT; i++) {
        if (MOVED[i].stored == stored)
            return MOVED[i].sdl3;    // The first of two: play/pause names AudioPlay's code
    }
    if (stored >= FIRST_MOVED + SDL3_ONLY && stored <= LAST_MOVED + SDL3_ONLY &&
        keycode_to_stored(stored - SDL3_ONLY, false) == stored)
        return stored - SDL3_ONLY;
    if (stored >= FIRST_MOVED && stored <= LAST_MOVED)
        return 0;                    // SDL2's Mail, WWW, Calculator and the rest: SDL3 sends none of them
    return stored;
}
