/* probe_spawn pad and focus: SDL3 gamepad input while another window has focus, and focus after a browser exits. */
#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "keys.h"
#include "spawn.h"

#define PROBE_PAD_MAX_S 86400L /* a day; seconds * 1000 then cannot wrap SDL's Uint64 ticks */

/* Prints {"event":<event>,"error":<SDL's last error>} and returns 1. */
static int emit_sdl_error(const char *event)
{
    char error[256], fields[320];
    probe_json_string(error, sizeof error, SDL_GetError());
    snprintf(fields, sizeof fields, "\"event\":\"%s\",\"error\":%s", event, error);
    probe_emit(fields);
    return 1;
}

static void emit_focus(int focused)
{
    probe_emit(focused ? "\"event\":\"focus\",\"focused\":true" : "\"event\":\"focus\",\"focused\":false");
}

/* The Windows backends a pad run can be pinned to, each with the hint that turns it on. */
static const struct {
    const char *name;
    const char *hint;
} backends[] = {
    { "xinput", SDL_HINT_XINPUT_ENABLED },
    { "rawinput", SDL_HINT_JOYSTICK_RAWINPUT },
    { "wgi", SDL_HINT_JOYSTICK_WGI },
};

/* "default" keeps SDL's own choice. A named backend is turned on and every other Windows backend off, DirectInput
   and HIDAPI included, so each reading belongs to that backend alone. */
static int set_backend(const char *backend)
{
    if (strcmp(backend, "default") == 0)
        return 0;
    size_t n = sizeof backends / sizeof backends[0], named = n;
    for (size_t i = 0; i < n; i++)
        if (strcmp(backend, backends[i].name) == 0)
            named = i;
    if (named == n)
        return -1;
    for (size_t i = 0; i < n; i++)
        SDL_SetHint(backends[i].hint, i == named ? "1" : "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_DIRECTINPUT, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "0");
    return 0;
}

/* Opens the probe's window with a renderer and draws one frame. On failure prints sdl_failed, shuts SDL down and
   returns 1, so a probe never prints results from a window that is not there. The focus changes that opening the
   window made are dropped (other queued events, such as a pad found at start, stay), so a probe starts from the
   window's settled focus flag rather than reporting its own window's churn. */
static int open_window(const char *title, int width, int height, SDL_WindowFlags flags, SDL_Window **w,
                       SDL_Renderer **r)
{
    *w = SDL_CreateWindow(title, width, height, flags);
    *r = *w ? SDL_CreateRenderer(*w, NULL) : NULL;
    if (!*r) {
        int rc = emit_sdl_error("sdl_failed");
        if (*w)
            SDL_DestroyWindow(*w);
        SDL_Quit();
        return rc;
    }
    SDL_SetRenderDrawColor(*r, 32, 32, 32, 255);
    SDL_RenderClear(*r);
    SDL_RenderPresent(*r);
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_EVENT_WINDOW_FOCUS_GAINED, SDL_EVENT_WINDOW_FOCUS_LOST);
    return 0;
}

static void close_window(SDL_Window *w, SDL_Renderer *r)
{
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(w);
    SDL_Quit();
}

int probe_pad(int argc, char **argv)
{
    long seconds;
    if (argc < 2 || probe_parse_count(argv[0], PROBE_PAD_MAX_S, &seconds) < 0 || set_backend(argv[1]) < 0) {
        fputs("usage: probe_spawn pad <seconds> <default|xinput|rawinput|wgi>\n", stderr);
        return 2;
    }
    char fields[512], text[256], guid[33];
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
        return emit_sdl_error("sdl_failed");
    SDL_Window *w;
    SDL_Renderer *r;
    if (open_window("CPTK probe pad", 320, 240, 0, &w, &r))
        return 1;
    int focused = (SDL_GetWindowFlags(w) & SDL_WINDOW_INPUT_FOCUS) != 0;
    emit_focus(focused);
    Uint64 end = SDL_GetTicks() + (Uint64)seconds * 1000u;
    while (SDL_GetTicks() < end) {
        SDL_Event e;
        if (!SDL_WaitEventTimeout(&e, 50))
            continue;
        switch (e.type) {
        case SDL_EVENT_GAMEPAD_ADDED: {
            /* The GUID names the backend that found the pad, so a reading can be tied to it. */
            SDL_GUIDToString(SDL_GetGamepadGUIDForID(e.gdevice.which), guid, sizeof guid);
            SDL_Gamepad *pad = SDL_OpenGamepad(e.gdevice.which);
            if (pad) {
                probe_json_string(text, sizeof text, SDL_GetGamepadName(pad));
                snprintf(fields, sizeof fields, "\"event\":\"pad_added\",\"name\":%s,\"guid\":\"%s\"", text, guid);
            } else {
                probe_json_string(text, sizeof text, SDL_GetError());
                snprintf(fields, sizeof fields, "\"event\":\"pad_open_failed\",\"guid\":\"%s\",\"error\":%s", guid,
                         text);
            }
            probe_emit(fields);
            break;
        }
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            probe_json_string(text, sizeof text, SDL_GetGamepadStringForButton((SDL_GamepadButton)e.gbutton.button));
            snprintf(fields, sizeof fields, "\"event\":\"button\",\"button\":%s,\"focused\":%s", text,
                     focused ? "true" : "false");
            probe_emit(fields);
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            focused = e.type == SDL_EVENT_WINDOW_FOCUS_GAINED;
            emit_focus(focused);
            break;
        case SDL_EVENT_QUIT:
            end = 0;
            break;
        default:
            break;
        }
    }
    probe_emit("\"event\":\"pad_end\"");
    close_window(w, r);
    return 0;
}

/* Reads every queued event. The first focus loss sets *lost_ms, and the first focus gain after it sets *back_ms,
   each in ms since t0. Every other event, a key included, is dropped. */
static void drain_focus(Uint64 t0, long long *lost_ms, long long *back_ms)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST && *lost_ms < 0)
            *lost_ms = (long long)(SDL_GetTicks() - t0);
        else if (e.type == SDL_EVENT_WINDOW_FOCUS_GAINED && *lost_ms >= 0 && *back_ms < 0)
            *back_ms = (long long)(SDL_GetTicks() - t0);
    }
}

int probe_focus(int argc, char **argv)
{
    if (argc < 1) {
        fputs("usage: probe_spawn focus <program> [args...]\n", stderr);
        return 2;
    }
    char fields[320];
    if (!SDL_Init(SDL_INIT_VIDEO))
        return emit_sdl_error("sdl_failed");
    SDL_Window *w;
    SDL_Renderer *r;
    if (open_window("CPTK probe focus", 0, 0, SDL_WINDOW_FULLSCREEN, &w, &r))
        return 1;

    /* argv is the tail of main's argv, so argv[argc] is the NULL the args property needs. The child's stdout goes
       nowhere, so only JSON lines reach ours; its stderr is ours, as run's is. */
    SDL_Process *p = NULL;
    SDL_PropertiesID props = SDL_CreateProperties();
    if (props) {
        SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv);
        SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
        SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_NULL);
        SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_INHERITED);
        p = SDL_CreateProcessWithProperties(props);
    }
    if (!p) {
        int rc = emit_sdl_error("spawn_failed");
        SDL_DestroyProperties(props);
        close_window(w, r);
        return rc;
    }
    SDL_DestroyProperties(props);
    probe_emit("\"event\":\"started\"");
    Uint64 t_start = SDL_GetTicks();
    long long lost_ms = -1, back_ms = -1;
    int code = 0;
    while (!SDL_WaitProcess(p, false, &code)) {
        drain_focus(t_start, &lost_ms, &back_ms);
        SDL_RenderClear(r);
        SDL_RenderPresent(r);
        SDL_Delay(20);
    }
    Uint64 t_exit = SDL_GetTicks();
    /* What is queued now came before the raise, so it is not the raise's doing. The drain runs before exited is
       printed: a key sent once exited has been read then arrives after it and is always counted below. */
    drain_focus(t_start, &lost_ms, &back_ms);
    snprintf(fields, sizeof fields, "\"event\":\"exited\",\"code\":%d", code);
    probe_emit(fields);
    SDL_DestroyProcess(p);
    bool raised = SDL_RaiseWindow(w);
    long long gained_ms = -1, key_ms = -1;
    while (SDL_GetTicks() - t_exit < 10000 && key_ms < 0) {
        SDL_Event e;
        if (!SDL_WaitEventTimeout(&e, 20))
            continue;
        if (e.type == SDL_EVENT_WINDOW_FOCUS_GAINED && gained_ms < 0)
            gained_ms = (long long)(SDL_GetTicks() - t_exit);
        else if (e.type == SDL_EVENT_KEY_DOWN)
            key_ms = (long long)(SDL_GetTicks() - t_exit);
    }
    snprintf(fields, sizeof fields,
             "\"event\":\"focus_after_exit\",\"raised\":%s,\"lost\":%s,\"lost_ms\":%lld,\"back_before_raise\":%s,"
             "\"gained\":%s,\"ms\":%lld",
             raised ? "true" : "false", lost_ms >= 0 ? "true" : "false", lost_ms, back_ms >= 0 ? "true" : "false",
             (gained_ms >= 0 && gained_ms <= 1000) ? "true" : "false", gained_ms);
    probe_emit(fields);
    snprintf(fields, sizeof fields, "\"event\":\"key_after_exit\",\"ms\":%lld", key_ms);
    probe_emit(fields);
    close_window(w, r);
    return 0;
}
