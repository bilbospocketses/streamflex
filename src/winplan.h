// The window a background needs and the renderers that may draw it (spec section 3): pure, no SDL, so
// tests/test_winplan.c builds it on its own, as layout.c is built
#ifndef WINPLAN_H
#define WINPLAN_H

#include <stdbool.h>
#include <stddef.h>

#define RENDER_LIST_WINDOWS "vulkan,direct3d12,direct3d11,opengl"
#define RENDER_LIST_UNIX "vulkan,opengl"
#define RENDER_LIST_TRANSPARENT "vulkan,direct3d11"
#define WINPLAN_BACKGROUND_TRANSPARENT 3   // ModeBackground's BACKGROUND_TRANSPARENT; launcher.c checks they agree
#ifdef _WIN32
#define WINPLAN_WINDOWS true               // What a caller passes as window_plan()'s `windows`
#else
#define WINPLAN_WINDOWS false
#endif

typedef enum { WINDOW_FULLSCREEN, WINDOW_TRANSPARENT_SHORT } WindowKind;

typedef struct {
    WindowKind kind;
    const char *render_list;   // NULL: pass no list (SDL_RENDER_DRIVER chose)
    bool mark_fullscreen;      // ITaskbarList2::MarkFullscreenWindow on the HWND
    bool refused;              // An override outside the transparent list: use the opaque window
    char reason[128];          // Set when refused: "SDL_RENDER_DRIVER=<value> is not in the transparent list"
} WindowPlan;

// background: the ModeBackground wanted; windows: built for Windows; render_driver_env: SDL_getenv("SDL_RENDER_DRIVER"), NULL or "" for unset
WindowPlan window_plan(int background, bool windows, const char *render_driver_env);

// A rectangle in a display's coordinates (pixels on Windows, where StreamFlex is per-monitor v2 aware)
typedef struct { int x, y, w, h; } WindowRect;

// The Transparent window's place: the display's top-left corner and one pixel row short of its bounds.
// A window that exactly covers the display is promoted to independent flip, which skips the
// compositor and with it the window's alpha (spec section 3, the spike).
WindowRect window_short_rect(WindowRect display);

// Whether the window in force (`now`) must give way to one of the kind a background wants: only when
// the kinds differ, and never to a Transparent window again once no renderer could draw one
// (`transparent_failed`, cleared when another Mode is chosen)
bool window_must_change(WindowKind now, WindowKind wanted, bool transparent_failed);

// Failed presents in a row on a Transparent window that make a lost device: the Vulkan fix fails a
// swapchain the surface cannot give alpha whenever it is rebuilt (a resize, a VSync toggle, another
// display), and then every frame fails with no render event (spec section 3)
#define PRESENT_FAILURES_LOST 3

// Counts one present into *failures; true once PRESENT_FAILURES_LOST in a row have failed on a
// Transparent window, and the count then starts again. A present that worked, or any present on
// another window, sets the count to 0.
bool present_failed_lost(int *failures, bool presented, bool transparent);

// The log's Renderer: line for the renderer SDL chose from render_list (NULL: SDL_RENDER_DRIVER chose);
// chosen must not be NULL: it is SDL_GetRendererName's result for a valid renderer
void winplan_renderer_line(const char *render_list, const char *chosen, char *out, size_t size);

#endif
