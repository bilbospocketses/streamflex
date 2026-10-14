#include <string.h>
#include "check.h"
#include "videodriver.h"

#define FIFO_NOTE "Video: x11, though a Wayland compositor is running: it lacks fifo-v1, which SDL needs before it prefers Wayland"

// A function to test the drivers that need no note: SDL's wayland, x11 with no compositor running,
// and any other platform's driver
static void test_no_note(void)
{
    char out[160] = "stale";
    CHECK(!video_driver_note("wayland", NULL, NULL, "wayland-0", out, sizeof(out)));
    CHECK_STR(out, "");
    CHECK(!video_driver_note("x11", NULL, NULL, NULL, out, sizeof(out)));
    CHECK(!video_driver_note("x11", NULL, NULL, "", out, sizeof(out)));     // Set but empty is unset
    CHECK(!video_driver_note("windows", NULL, NULL, NULL, out, sizeof(out)));
    CHECK_STR(out, "");
}

// A function to test SDL keeping to X11 while a compositor runs, with nothing overriding it
static void test_no_fifo(void)
{
    char out[160];
    CHECK(video_driver_note("x11", NULL, NULL, "wayland-0", out, sizeof(out)));
    CHECK_STR(out, FIFO_NOTE);
    CHECK(video_driver_note("x11", "", "", "wayland-1", out, sizeof(out)));   // Empty overrides are unset
    CHECK_STR(out, FIFO_NOTE);
}

// A function to test an override, named by the variable that set it: SDL3's own name first, as SDL
// reads it, then SDL2's, which SDL3 still reads; an override is the reason whatever SDL chose
static void test_override(void)
{
    char out[160];
    CHECK(video_driver_note("x11", "x11", NULL, "wayland-0", out, sizeof(out)));
    CHECK_STR(out, "Video: the driver list comes from SDL_VIDEO_DRIVER=x11");
    CHECK(video_driver_note("wayland", NULL, "wayland", NULL, out, sizeof(out)));
    CHECK_STR(out, "Video: the driver list comes from SDL_VIDEODRIVER=wayland");
    CHECK(video_driver_note("x11", "x11,wayland", "wayland", "wayland-0", out, sizeof(out)));
    CHECK_STR(out, "Video: the driver list comes from SDL_VIDEO_DRIVER=x11,wayland");
    CHECK(video_driver_note("x11", "", "x11", "wayland-0", out, sizeof(out)));
    CHECK_STR(out, "Video: the driver list comes from SDL_VIDEODRIVER=x11");
}

// A function to test that a short buffer is cut, never overrun
static void test_short_buffer(void)
{
    char out[8];
    CHECK(video_driver_note("x11", NULL, NULL, "wayland-0", out, sizeof(out)));
    CHECK_STR(out, "Video: ");
}

int main(void)
{
    test_no_note();
    test_no_fifo();
    test_override();
    test_short_buffer();
    return check_report();
}
