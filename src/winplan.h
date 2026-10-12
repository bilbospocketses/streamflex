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
    char reason[128];          // Set when refused: "SDL_RENDER_DRIVER=<name> is not in the transparent list"
} WindowPlan;

// background: the ModeBackground wanted; windows: built for Windows; render_driver_env: SDL_getenv("SDL_RENDER_DRIVER"), NULL or "" for unset
WindowPlan window_plan(int background, bool windows, const char *render_driver_env);
// The log's Renderer: line for the renderer SDL chose from render_list (NULL: SDL_RENDER_DRIVER chose)
void winplan_renderer_line(const char *render_list, const char *chosen, char *out, size_t size);

#endif
