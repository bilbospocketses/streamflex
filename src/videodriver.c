#include <stdio.h>
#include <string.h>
#include "videodriver.h"

// A function to tell whether an environment value is set: NULL and "" are not, as SDL reads them
static bool is_set(const char *value)
{
    return value != NULL && value[0] != '\0';
}

// A function to write the note for SDL's choice of video driver: an override first, by the variable
// SDL reads first (SDL3's name; SDL2's only when SDL3's is unset, not when it is empty, as SDL falls
// back to it), else X11 chosen with a compositor running
bool video_driver_note(const char *driver, const char *sdl_video_driver, const char *sdl_videodriver,
                       const char *wayland_display, char *out, size_t size)
{
    if (size > 0)
        out[0] = '\0';
    if (is_set(sdl_video_driver))
        snprintf(out, size, "Video: the driver list comes from SDL_VIDEO_DRIVER=%s", sdl_video_driver);
    else if (sdl_video_driver == NULL && is_set(sdl_videodriver))
        snprintf(out, size, "Video: the driver list comes from SDL_VIDEODRIVER=%s", sdl_videodriver);
    else if (strcmp(driver, "x11") == 0 && is_set(wayland_display))
        snprintf(out, size, "Video: x11, though a Wayland compositor is running: it lacks fifo-v1, "
                            "which SDL needs before it prefers Wayland");
    else
        return false;
    return true;
}
