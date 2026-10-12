#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "winplan.h"

// A function to tell whether two names of the same length are one name, in any case, as SDL compares
// renderer names
static bool same_name(const char *a, const char *b, size_t length)
{
    for (size_t i = 0; i < length; i++)
        if (tolower((unsigned char) a[i]) != tolower((unsigned char) b[i]))
            return false;
    return true;
}

// A function to tell whether a name (its first length characters) is one of a comma-separated list's
// entries, whole
static bool in_list(const char *list, const char *name, size_t length)
{
    for (const char *at = list; *at != '\0'; ) {
        size_t entry = strcspn(at, ",");
        if (entry == length && same_name(at, name, length))
            return true;
        at += entry;
        if (*at == ',')
            at++;
    }
    return false;
}

// A function to tell whether every name of a comma-separated override is in a list. An empty entry
// names no renderer, so it is passed over, as SDL passes it over
static bool all_in_list(const char *list, const char *names)
{
    for (const char *at = names; *at != '\0'; ) {
        size_t entry = strcspn(at, ",");
        if (entry > 0 && !in_list(list, at, entry))
            return false;
        at += entry;
        if (*at == ',')
            at++;
    }
    return true;
}

// A function to work out the window for a background and the renderers that may draw it. Only
// Transparent on Windows needs a window of its own: per-pixel alpha, one row short of the display so
// Windows composes it, marked fullscreen for the taskbar, on the two renderers that can draw it.
// SDL_RENDER_DRIVER, when set, picks the renderer: it takes the list away and changes nothing else,
// unless it names one outside the transparent list, which is refused by its names alone (a renderer
// that is made can still draw no alpha), for the opaque window. It may be a list, as SDL reads it, and
// is taken only when every name in it is in the transparent list, in any case.
WindowPlan window_plan(int background, bool windows, const char *render_driver_env)
{
    WindowPlan plan;
    memset(&plan, 0, sizeof(plan));
    bool forced = render_driver_env != NULL && render_driver_env[0] != '\0';
    plan.kind = WINDOW_FULLSCREEN;
    plan.render_list = forced ? NULL : windows ? RENDER_LIST_WINDOWS : RENDER_LIST_UNIX;
    if (!windows || background != WINPLAN_BACKGROUND_TRANSPARENT)
        return plan;
    if (forced && !all_in_list(RENDER_LIST_TRANSPARENT, render_driver_env)) {
        plan.refused = true;
        snprintf(plan.reason, sizeof(plan.reason), "SDL_RENDER_DRIVER=%s is not in the transparent list", render_driver_env);
        return plan;
    }
    plan.kind = WINDOW_TRANSPARENT_SHORT;
    plan.render_list = forced ? NULL : RENDER_LIST_TRANSPARENT;
    plan.mark_fullscreen = true;
    return plan;
}

// A function to word the log's Renderer: line: the renderer SDL chose, and the list's first entry
// named as unavailable when it was not that one, or SDL_RENDER_DRIVER named when it chose (no list)
void winplan_renderer_line(const char *render_list, const char *chosen, char *out, size_t size)
{
    if (render_list == NULL) {
        snprintf(out, size, "Renderer: %s (from SDL_RENDER_DRIVER)", chosen);
        return;
    }
    size_t first = strcspn(render_list, ",");
    if (strlen(chosen) == first && strncmp(render_list, chosen, first) == 0)
        snprintf(out, size, "Renderer: %s", chosen);
    else
        snprintf(out, size, "Renderer: %s (%.*s was unavailable)", chosen, (int) first, render_list);
}
