// Key codes as config.ini stores them (spec section 9, test 1): every code a user can bind or a config
// can hold reads as the same key as on SDL2, from the code that key sends on SDL3, and names as that
// key. The keys that moved are read by physical key from SDL2 2.26.5's and SDL3 3.2.10's scancode
// tables (scancodes_linux.h, Windows' scan codes and SDL2's VKeytoScancode).
#include <stdbool.h>
#include <stdint.h>
#include "check.h"
#include "keycodes.h"

// Each physical key whose SDL3 code is not SDL2's: the code it sends on SDL3, and the one SDL2 sent
static const struct { uint32_t sdl3, sdl2; } MOVED[] = {
    { 0x4000010B, 0x40000102 },   // Next track (KEY_NEXTSONG, VK_MEDIA_NEXT_TRACK): SDL2's AudioNext
    { 0x4000010C, 0x40000103 },   // Previous track: AudioPrev
    { 0x4000010D, 0x40000104 },   // Stop (KEY_STOPCD, VK_MEDIA_STOP): AudioStop
    { 0x4000010F, 0x40000105 },   // Play/pause (KEY_PLAYPAUSE, VK_MEDIA_PLAY_PAUSE): AudioPlay
    { 0x40000106, 0x40000105 },   // Play (KEY_PLAY): AudioPlay as well, which SDL2 gave both
    { 0x40000110, 0x40000107 },   // Media select (KEY_MEDIA, VK_LAUNCH_MEDIA_SELECT): MediaSelect
    { 0x40000118, 0x4000010C },   // AC Search
    { 0x40000119, 0x4000010D },   // AC Home
    { 0x4000011A, 0x4000010E },   // AC Back
    { 0x4000011B, 0x4000010F },   // AC Forward
    { 0x4000011C, 0x40000110 },   // AC Stop
    { 0x4000011D, 0x40000111 },   // AC Refresh
    { 0x4000011E, 0x40000112 },   // AC Bookmarks
    { 0x4000010E, 0x40000119 },   // Eject (KEY_EJECTCD): Eject
    { 0x40000102, 0x4000011A },   // Sleep (KEY_SLEEP): Sleep
    { 0x4000010A, 0x4000011D },   // Rewind (KEY_REWIND): AudioRewind
    { 0x40000109, 0x4000011E },   // Fast forward (KEY_FASTFORWARD): AudioFastForward
};

// SDL3's keys SDL2 never had whose codes SDL2 used for other keys: stored 0x200 higher
static const uint32_t SDL3_ONLY[] = { 0x40000103, 0x40000104, 0x40000105, 0x40000107, 0x40000108, 0x40000111,
                                      0x40000112, 0x40000113, 0x40000114, 0x40000115, 0x40000116, 0x40000117 };

// SDL2's keys SDL3 never sends: Mail, WWW, Calculator, Computer, the brightness keys, DisplaySwitch,
// the keyboard-light keys, App1 and App2
static const uint32_t SDL2_ONLY[] = { 0x40000108, 0x40000109, 0x4000010A, 0x4000010B, 0x40000113, 0x40000114,
                                      0x40000115, 0x40000116, 0x40000117, 0x40000118, 0x4000011B, 0x4000011C };

// The F keys, F1 to F24: Windows' :exit registers F1-F11 and F13-F24 (platform/keycode_convert.h)
static const uint32_t F_KEYS[] = {
    0x4000003A, 0x4000003B, 0x4000003C, 0x4000003D, 0x4000003E, 0x4000003F, 0x40000040, 0x40000041, 0x40000042,
    0x40000043, 0x40000044, 0x40000045, 0x40000068, 0x40000069, 0x4000006A, 0x4000006B, 0x4000006C, 0x4000006D,
    0x4000006E, 0x4000006F, 0x40000070, 0x40000071, 0x40000072, 0x40000073
};

#define COUNT(a) ((int) (sizeof(a) / sizeof((a)[0])))

// A function to tell whether a code is stored and named as itself, on both platforms
static bool unmoved(uint32_t code, bool skip_windows_mute)
{
    return keycode_to_stored(code, false) == code && (skip_windows_mute || keycode_to_stored(code, true) == code)
        && keycode_from_stored(code) == code;
}

// A function to test the moved keys: the SDL3 code is stored as SDL2's, on both platforms, and names back
static void test_moved(void)
{
    for (int i = 0; i < COUNT(MOVED); i++) {
        CHECK(keycode_to_stored(MOVED[i].sdl3, false) == MOVED[i].sdl2);
        CHECK(keycode_to_stored(MOVED[i].sdl3, true) == MOVED[i].sdl2);
        CHECK(keycode_to_stored(keycode_from_stored(MOVED[i].sdl2), false) == MOVED[i].sdl2);
    }
    // AudioPlay's code names the play/pause key, the one a remote has
    CHECK(keycode_from_stored(0x40000105) == 0x4000010F);
    CHECK(keycode_from_stored(0x40000102) == 0x4000010B);
}

// A function to test the mute key: AudioMute on Windows under SDL2, Mute on Linux
static void test_mute(void)
{
    CHECK(keycode_to_stored(0x4000007F, true) == 0x40000106);
    CHECK(keycode_to_stored(0x4000007F, false) == 0x4000007F);
    CHECK(keycode_from_stored(0x40000106) == 0x4000007F);
    CHECK(keycode_from_stored(0x4000007F) == 0x4000007F);
}

// A function to test SDL3's own keys: stored clear of every code SDL2 could send, and named back
static void test_sdl3_only(void)
{
    for (int i = 0; i < COUNT(SDL3_ONLY); i++) {
        uint32_t stored = keycode_to_stored(SDL3_ONLY[i], false);
        CHECK(stored == SDL3_ONLY[i] + 0x200);
        CHECK(keycode_to_stored(SDL3_ONLY[i], true) == stored);
        CHECK(stored > 0x400001FF);
        CHECK(keycode_from_stored(stored) == SDL3_ONLY[i]);
    }
}

// A function to test the other codes 0x200 above the renumbered block, which no SDL3-only key is stored
// as: each is stored and named as itself, never as the key 0x200 below it (#4000030F is not play/pause)
static void test_above_block(void)
{
    int wrong = 0;
    for (uint32_t code = 0x40000302; code <= 0x4000031E; code++) {
        bool sdl3_only = false;
        for (int i = 0; i < COUNT(SDL3_ONLY); i++)
            sdl3_only = sdl3_only || code == SDL3_ONLY[i] + 0x200;
        wrong += !sdl3_only && !unmoved(code, false);
    }
    CHECK_INT(wrong, 0);
    CHECK(keycode_from_stored(0x40000302) == 0x40000302);
    CHECK(keycode_from_stored(0x4000030F) == 0x4000030F);
    CHECK(keycode_from_stored(0x4000031E) == 0x4000031E);
}

// A function to test SDL2's keys SDL3 never sends: no SDL3 key names them
static void test_sdl2_only(void)
{
    for (int i = 0; i < COUNT(SDL2_ONLY); i++)
        CHECK(keycode_from_stored(SDL2_ONLY[i]) == 0);
}

// A function to test every code no key moved: Unicode key codes, the scancode keys before and after
// the renumbered block, and SDL3's extended keys, each stored and named as itself
static void test_unmoved(void)
{
    int wrong = 0;
    for (uint32_t code = 0; code <= 0x10FFFF; code++)
        wrong += !unmoved(code, false);
    for (uint32_t code = 0x40000000; code <= 0x40000101; code++)
        wrong += !unmoved(code, code == 0x4000007F);
    for (uint32_t code = 0x4000011F; code <= 0x400001FF; code++)
        wrong += !unmoved(code, false);
    for (uint32_t code = 0x20000001; code <= 0x20000007; code++)
        wrong += !unmoved(code, false);
    CHECK_INT(wrong, 0);
}

// A function to test the F keys behind Windows' :exit
static void test_f_keys(void)
{
    for (int i = 0; i < COUNT(F_KEYS); i++)
        CHECK(unmoved(F_KEYS[i], false));
}

int main(void)
{
    test_moved();
    test_mute();
    test_sdl3_only();
    test_above_block();
    test_sdl2_only();
    test_unmoved();
    test_f_keys();
    return check_report();
}
