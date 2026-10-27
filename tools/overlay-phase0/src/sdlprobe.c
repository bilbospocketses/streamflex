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

static int set_backend(const char *backend)
{
    if (strcmp(backend, "default") == 0)
        return 0;
    if (strcmp(backend, "xinput") == 0) {
        SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");
        SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "0");
        SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "1");
        return 0;
    }
    if (strcmp(backend, "rawinput") == 0) {
        SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "0");
        SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "0");
        return 0;
    }
    if (strcmp(backend, "wgi") == 0) {
        SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");
        SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "0");
        return 0;
    }
    return -1;
}

int probe_pad(int argc, char **argv)
{
    long seconds;
    if (argc < 2 || probe_parse_count(argv[0], PROBE_PAD_MAX_S, &seconds) < 0 || set_backend(argv[1]) < 0) {
        fputs("usage: probe_spawn pad <seconds> <default|xinput|rawinput|wgi>\n", stderr);
        return 2;
    }
    char fields[512], text[256];
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
        return emit_sdl_error("sdl_failed");
    SDL_Window *w = SDL_CreateWindow("CPTK probe pad", 320, 240, 0);
    int focused = w && (SDL_GetWindowFlags(w) & SDL_WINDOW_INPUT_FOCUS) != 0;
    Uint64 end = SDL_GetTicks() + (Uint64)seconds * 1000u;
    while (SDL_GetTicks() < end) {
        SDL_Event e;
        if (!SDL_WaitEventTimeout(&e, 50))
            continue;
        switch (e.type) {
        case SDL_EVENT_GAMEPAD_ADDED: {
            SDL_Gamepad *pad = SDL_OpenGamepad(e.gdevice.which);
            probe_json_string(text, sizeof text, pad ? SDL_GetGamepadName(pad) : "?");
            snprintf(fields, sizeof fields, "\"event\":\"pad_added\",\"name\":%s", text);
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
            snprintf(fields, sizeof fields, "\"event\":\"focus\",\"focused\":%s", focused ? "true" : "false");
            probe_emit(fields);
            break;
        case SDL_EVENT_QUIT:
            end = 0;
            break;
        default:
            break;
        }
    }
    probe_emit("\"event\":\"pad_end\"");
    SDL_DestroyWindow(w);
    SDL_Quit();
    return 0;
}

int probe_focus(int argc, char **argv)
{
    if (argc < 1) {
        fputs("usage: probe_spawn focus <program> [args...]\n", stderr);
        return 2;
    }
    char fields[256];
    if (!SDL_Init(SDL_INIT_VIDEO))
        return emit_sdl_error("sdl_failed");
    SDL_Window *w = SDL_CreateWindow("CPTK probe focus", 0, 0, SDL_WINDOW_FULLSCREEN);
    SDL_Renderer *r = w ? SDL_CreateRenderer(w, NULL) : NULL;
    if (!r) {
        /* Report it rather than print focus results nothing measured. */
        int rc = emit_sdl_error("sdl_failed");
        if (w)
            SDL_DestroyWindow(w);
        SDL_Quit();
        return rc;
    }
    SDL_SetRenderDrawColor(r, 32, 32, 32, 255);
    SDL_RenderClear(r);
    SDL_RenderPresent(r);

    /* argv is the tail of main's argv, so argv[argc] is the NULL that SDL_CreateProcess needs. */
    SDL_Process *p = SDL_CreateProcess((const char *const *)argv, false);
    if (!p)
        return emit_sdl_error("spawn_failed");
    probe_emit("\"event\":\"started\"");
    int code = 0;
    while (!SDL_WaitProcess(p, false, &code)) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
        }
        SDL_RenderClear(r);
        SDL_RenderPresent(r);
        SDL_Delay(20);
    }
    int64_t t_exit = probe_now_ms();
    snprintf(fields, sizeof fields, "\"event\":\"exited\",\"code\":%d", code);
    probe_emit(fields);
    SDL_DestroyProcess(p);

    SDL_RaiseWindow(w);
    long long gained_ms = (SDL_GetWindowFlags(w) & SDL_WINDOW_INPUT_FOCUS) ? 0 : -1;
    long long key_ms = -1;
    while (probe_now_ms() - t_exit < 10000 && key_ms < 0) {
        SDL_Event e;
        if (!SDL_WaitEventTimeout(&e, 20))
            continue;
        if (e.type == SDL_EVENT_WINDOW_FOCUS_GAINED && gained_ms < 0)
            gained_ms = probe_now_ms() - t_exit;
        else if (e.type == SDL_EVENT_KEY_DOWN)
            key_ms = probe_now_ms() - t_exit;
    }
    snprintf(fields, sizeof fields, "\"event\":\"focus_after_exit\",\"gained\":%s,\"ms\":%lld",
             (gained_ms >= 0 && gained_ms <= 1000) ? "true" : "false", gained_ms);
    probe_emit(fields);
    snprintf(fields, sizeof fields, "\"event\":\"key_after_exit\",\"ms\":%lld", key_ms);
    probe_emit(fields);
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(w);
    SDL_Quit();
    return 0;
}
