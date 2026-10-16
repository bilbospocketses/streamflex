// Unit test 14 (spec section 9): the window a background needs and the renderers that may draw it.
// winplan.c is pure, so this links it alone, as test_layout links layout.c.
#include <stdbool.h>
#include <stdint.h>
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

// A function to test the Transparent window's place: the display's own top-left corner, its full
// width, and one pixel row short of its height, on the first display and on one left of it and lower
static void test_window_short_rect(void)
{
    WindowRect rect = window_short_rect((WindowRect) { 0, 0, 3840, 2160 });
    CHECK_INT(rect.x, 0);
    CHECK_INT(rect.y, 0);
    CHECK_INT(rect.w, 3840);
    CHECK_INT(rect.h, 2159);
    rect = window_short_rect((WindowRect) { -1920, 120, 1920, 1080 });
    CHECK_INT(rect.x, -1920);
    CHECK_INT(rect.y, 120);
    CHECK_INT(rect.w, 1920);
    CHECK_INT(rect.h, 1079);
}

// A function to test when a window gives way to one of another kind: only when the kinds differ, and
// never to a Transparent window again once no renderer could draw one, while leaving Transparent
// always goes ahead, a Transparent window still in force included: the whole table of the window in
// force, the kind wanted and the failure. On Linux every background plans a fullscreen window, so
// nothing ever changes.
static void test_window_must_change(void)
{
    CHECK(!window_must_change(WINDOW_FULLSCREEN, WINDOW_FULLSCREEN, false));
    CHECK(!window_must_change(WINDOW_TRANSPARENT_SHORT, WINDOW_TRANSPARENT_SHORT, false));
    CHECK(window_must_change(WINDOW_FULLSCREEN, WINDOW_TRANSPARENT_SHORT, false));
    CHECK(window_must_change(WINDOW_TRANSPARENT_SHORT, WINDOW_FULLSCREEN, false));
    CHECK(!window_must_change(WINDOW_FULLSCREEN, WINDOW_TRANSPARENT_SHORT, true));
    CHECK(window_must_change(WINDOW_TRANSPARENT_SHORT, WINDOW_FULLSCREEN, true));
    CHECK(window_must_change(WINDOW_TRANSPARENT_SHORT, WINDOW_TRANSPARENT_SHORT, true));
    CHECK(!window_must_change(WINDOW_FULLSCREEN, WINDOW_FULLSCREEN, true));
    for (int background = BG_COLOR; background <= BG_TRANSPARENT; background++)
        CHECK(!window_must_change(WINDOW_FULLSCREEN, window_plan(background, false, NULL).kind, false));
    CHECK(window_must_change(WINDOW_FULLSCREEN, window_plan(BG_TRANSPARENT, true, NULL).kind, false));
}

// A function to test the count of failed presents on a Transparent window: the third in a row is a
// lost device, and the count starts again for the renderer that follows; a present that works ends a
// run, and an opaque window's failures are never counted
static void test_present_failures(void)
{
    int failures = 0;
    CHECK(!present_failed_lost(&failures, false, true));
    CHECK(!present_failed_lost(&failures, false, true));
    CHECK(present_failed_lost(&failures, false, true));
    CHECK_INT(failures, 0);
    CHECK(!present_failed_lost(&failures, false, true));
    CHECK(!present_failed_lost(&failures, false, true));
    CHECK(!present_failed_lost(&failures, true, true));
    CHECK(!present_failed_lost(&failures, false, true));
    CHECK(!present_failed_lost(&failures, false, true));
    CHECK(present_failed_lost(&failures, false, true));
    for (int i = 0; i < 5; i++)
        CHECK(!present_failed_lost(&failures, false, false));
    CHECK_INT(failures, 0);
}

// A function to test the count of lost renderers: it counts on one window, and starts again at 1 for a
// loss on another, as a new window starts with none; a window it has never seen (0 to start) is
// another window too
static void test_renderer_loss_count(void)
{
    int losses = 0;
    uint32_t seen = 0;
    renderer_loss_count(&losses, &seen, 7);
    CHECK_INT(losses, 1);
    CHECK_INT((int) seen, 7);
    renderer_loss_count(&losses, &seen, 7);
    renderer_loss_count(&losses, &seen, 7);
    CHECK_INT(losses, 3);
    renderer_loss_count(&losses, &seen, 8);
    CHECK_INT(losses, 1);
    CHECK_INT((int) seen, 8);
    renderer_loss_count(&losses, &seen, 8);
    CHECK_INT(losses, 2);
}

// A function to test when lost renderers give a window up: a Transparent window at the third loss and
// after, never before, and an opaque window never, however many
static void test_renderer_losses_spent(void)
{
    CHECK_INT(RENDERER_LOSSES_TRANSPARENT, 3);
    CHECK(!renderer_losses_spent(WINDOW_TRANSPARENT_SHORT, 0));
    CHECK(!renderer_losses_spent(WINDOW_TRANSPARENT_SHORT, 1));
    CHECK(!renderer_losses_spent(WINDOW_TRANSPARENT_SHORT, 2));
    CHECK(renderer_losses_spent(WINDOW_TRANSPARENT_SHORT, 3));
    CHECK(renderer_losses_spent(WINDOW_TRANSPARENT_SHORT, 4));
    for (int losses = 0; losses <= 10; losses++)
        CHECK(!renderer_losses_spent(WINDOW_FULLSCREEN, losses));
}

int main(void)
{
    test_transparent_on_windows();
    test_opaque_on_windows();
    test_linux();
    test_override_takes_only_the_list();
    test_override_outside_the_transparent_list();
    test_renderer_line();
    test_window_short_rect();
    test_window_must_change();
    test_present_failures();
    test_renderer_loss_count();
    test_renderer_losses_spent();
    return check_report();
}
