/* probe_spawn pip, Linux: the session kind from the environment. This version has no X11 support, so the watch lists
   no windows and only supervises, and its pip_session line says so. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "pip.h"

static int session(void)
{
    const char *kind = probe_session_kind(getenv("WAYLAND_DISPLAY"), getenv("XDG_SESSION_TYPE"), getenv("DISPLAY"));
    const char *display = getenv("DISPLAY");
    char disp[160], fields[512];
    probe_json_string(disp, sizeof disp, display ? display : "");
    snprintf(fields, sizeof fields,
             "\"event\":\"pip_session\",\"session\":\"%s\",\"display\":%s,\"x11\":\"not_built\",\"can_list\":false,"
             "\"can_raise\":false,\"note\":\"built without libX11: no windows to list\"",
             kind, disp);
    probe_emit(fields);
    return 0;
}

static int snapshot(probe_win *out, int cap)
{
    (void)out;
    (void)cap;
    return 0;
}

static int close_window(uint64_t id, char *method, size_t cap)
{
    (void)id;
    snprintf(method, cap, "none");
    return 0;
}

static int raise_window(uint64_t id, char *method, size_t cap)
{
    (void)id;
    snprintf(method, cap, "none");
    return 0;
}

static uint64_t foreground(void)
{
    return 0;
}

const probe_pip_platform probe_pip_platform_impl = { session, snapshot, close_window, raise_window, foreground };
