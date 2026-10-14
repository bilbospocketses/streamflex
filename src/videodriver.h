// The note StreamFlex logs once SDL has chosen its video driver (spec section 5): where the driver
// list came from when the environment set it, or why SDL chose X11 while a Wayland compositor runs
// (SDL 3.2.10 and later prefer Wayland only when the compositor offers fifo-v1). Pure: no SDL, so
// tests/test_videodriver.c builds it on its own; launcher.c hands it SDL's choice and the environment.
#ifndef VIDEODRIVER_H
#define VIDEODRIVER_H

#include <stdbool.h>
#include <stddef.h>

// driver: SDL_GetCurrentVideoDriver(); the others the environment's SDL_VIDEO_DRIVER, SDL_VIDEODRIVER
// and WAYLAND_DISPLAY, NULL or "" when unset. Writes the line into out, cut to fit; false, with out
// empty, when there is nothing to say.
bool video_driver_note(const char *driver, const char *sdl_video_driver, const char *sdl_videodriver,
                       const char *wayland_display, char *out, size_t size);

#endif
