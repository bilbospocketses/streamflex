// Unit test 14 (spec section 9): the window a background needs and the renderers that may draw it.
// winplan.c is pure, so this links it alone, as test_layout links layout.c.
#include <stdbool.h>
#include <string.h>
#include "check.h"
#include "winplan.h"

// ModeBackground's values (launcher.h, which needs SDL): Color, Image, Slideshow, Transparent
#define BG_COLOR 0
#define BG_IMAGE 1
#define BG_SLIDESHOW 2
#define BG_TRANSPARENT WINPLAN_BACKGROUND_TRANSPARENT

// A plan is checked field by field, as a macro so a failure names the caller's line: its kind, its
// list (NULL for none), whether it marks the window, and whether it was refused
#define CHECK_PLAN(plan, kind_, list_, mark_, refused_) do { \
    WindowPlan plan_ = (plan); \
    const char *list_wanted_ = (list_); \
    CHECK_INT((int) plan_.kind, (int) (kind_)); \
    if (list_wanted_ == NULL) \
        CHECK(plan_.render_list == NULL); \
    else \
        CHECK_STR(plan_.render_list, list_wanted_); \
    CHECK(plan_.mark_fullscreen == (mark_)); \
    CHECK(plan_.refused == (refused_)); \
} while (0)

// Transparent on Windows: one row short, the transparent list, and the taskbar mark; an empty
// SDL_RENDER_DRIVER counts as unset
static void test_transparent_on_windows(void)
{
    CHECK_PLAN(window_plan(BG_TRANSPARENT, true, NULL), WINDOW_TRANSPARENT_SHORT, RENDER_LIST_TRANSPARENT, true, false);
    CHECK_PLAN(window_plan(BG_TRANSPARENT, true, ""), WINDOW_TRANSPARENT_SHORT, RENDER_LIST_TRANSPARENT, true, false);
}

// Every opaque background on Windows: true fullscreen on the Vulkan-first list, never marked
static void test_opaque_on_windows(void)
{
    CHECK_PLAN(window_plan(BG_COLOR, true, NULL), WINDOW_FULLSCREEN, RENDER_LIST_WINDOWS, false, false);
    CHECK_PLAN(window_plan(BG_IMAGE, true, NULL), WINDOW_FULLSCREEN, RENDER_LIST_WINDOWS, false, false);
    CHECK_PLAN(window_plan(BG_SLIDESHOW, true, NULL), WINDOW_FULLSCREEN, RENDER_LIST_WINDOWS, false, false);
}

// Every background on Linux, Transparent included (the chroma key stays there): fullscreen on
// Linux's list, never marked
static void test_linux(void)
{
    CHECK_PLAN(window_plan(BG_COLOR, false, NULL), WINDOW_FULLSCREEN, RENDER_LIST_UNIX, false, false);
    CHECK_PLAN(window_plan(BG_IMAGE, false, NULL), WINDOW_FULLSCREEN, RENDER_LIST_UNIX, false, false);
    CHECK_PLAN(window_plan(BG_SLIDESHOW, false, NULL), WINDOW_FULLSCREEN, RENDER_LIST_UNIX, false, false);
    CHECK_PLAN(window_plan(BG_TRANSPARENT, false, NULL), WINDOW_FULLSCREEN, RENDER_LIST_UNIX, false, false);
}

// SDL_RENDER_DRIVER takes the list away and changes nothing else: not the window's kind, not the mark.
// Its names match in any case, and a list of them is taken when every one is in the transparent list,
// as SDL reads the hint
static void test_override_takes_only_the_list(void)
{
    CHECK_PLAN(window_plan(BG_COLOR, true, "opengl"), WINDOW_FULLSCREEN, NULL, false, false);
    CHECK_PLAN(window_plan(BG_TRANSPARENT, false, "opengl"), WINDOW_FULLSCREEN, NULL, false, false);
    CHECK_PLAN(window_plan(BG_TRANSPARENT, true, "direct3d11"), WINDOW_TRANSPARENT_SHORT, NULL, true, false);
    CHECK_PLAN(window_plan(BG_TRANSPARENT, true, "vulkan"), WINDOW_TRANSPARENT_SHORT, NULL, true, false);
    CHECK_PLAN(window_plan(BG_TRANSPARENT, true, "Vulkan"), WINDOW_TRANSPARENT_SHORT, NULL, true, false);
    CHECK_PLAN(window_plan(BG_TRANSPARENT, true, "vulkan,direct3d11"), WINDOW_TRANSPARENT_SHORT, NULL, true, false);
}

// An override outside the transparent list is refused by its names alone, for the opaque window,
// with the reason the Video: line gives; a name is matched whole, never as a prefix, and a list with
// any name outside is refused whole
static void test_override_outside_the_transparent_list(void)
{
    WindowPlan plan = window_plan(BG_TRANSPARENT, true, "direct3d12");
    CHECK_PLAN(plan, WINDOW_FULLSCREEN, NULL, false, true);
    CHECK_STR(plan.reason, "SDL_RENDER_DRIVER=direct3d12 is not in the transparent list");
    plan = window_plan(BG_TRANSPARENT, true, "direct3d");
    CHECK_PLAN(plan, WINDOW_FULLSCREEN, NULL, false, true);
    CHECK_STR(plan.reason, "SDL_RENDER_DRIVER=direct3d is not in the transparent list");
    plan = window_plan(BG_TRANSPARENT, true, "opengl");
    CHECK_PLAN(plan, WINDOW_FULLSCREEN, NULL, false, true);
    CHECK_STR(plan.reason, "SDL_RENDER_DRIVER=opengl is not in the transparent list");
    plan = window_plan(BG_TRANSPARENT, true, "direct3d1");
    CHECK_PLAN(plan, WINDOW_FULLSCREEN, NULL, false, true);
    CHECK_STR(plan.reason, "SDL_RENDER_DRIVER=direct3d1 is not in the transparent list");
    plan = window_plan(BG_TRANSPARENT, true, "vulkan,opengl");
    CHECK_PLAN(plan, WINDOW_FULLSCREEN, NULL, false, true);
    CHECK_STR(plan.reason, "SDL_RENDER_DRIVER=vulkan,opengl is not in the transparent list");
}

// The Renderer: line (spec section 3, Logging): the list's first choice plain, any other with the
// first named as unavailable, and an override's said to come from SDL_RENDER_DRIVER
static void test_renderer_line(void)
{
    char line[160];
    winplan_renderer_line(RENDER_LIST_UNIX, "vulkan", line, sizeof(line));
    CHECK_STR(line, "Renderer: vulkan");
    winplan_renderer_line(RENDER_LIST_UNIX, "opengl", line, sizeof(line));
    CHECK_STR(line, "Renderer: opengl (vulkan was unavailable)");
    winplan_renderer_line(RENDER_LIST_WINDOWS, "direct3d11", line, sizeof(line));
    CHECK_STR(line, "Renderer: direct3d11 (vulkan was unavailable)");
    winplan_renderer_line(RENDER_LIST_TRANSPARENT, "direct3d11", line, sizeof(line));
    CHECK_STR(line, "Renderer: direct3d11 (vulkan was unavailable)");
    winplan_renderer_line(NULL, "opengl", line, sizeof(line));
    CHECK_STR(line, "Renderer: opengl (from SDL_RENDER_DRIVER)");
    winplan_renderer_line(RENDER_LIST_UNIX, "vulk", line, sizeof(line));   // A prefix of the first is not it
    CHECK_STR(line, "Renderer: vulk (vulkan was unavailable)");
}

int main(void)
{
    test_transparent_on_windows();
    test_opaque_on_windows();
    test_linux();
    test_override_takes_only_the_list();
    test_override_outside_the_transparent_list();
    test_renderer_line();
    return check_report();
}
