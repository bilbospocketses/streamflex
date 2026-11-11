/* probe_spawn pip: run's supervision plus a watch on the browser's top-level windows. */
#ifndef PROBE_PIP_H
#define PROBE_PIP_H

#include <stddef.h>
#include <stdint.h>

#define PROBE_PIP_MAX_WINDOWS 256

/* One top-level window of the browser, as a platform lists it (Windows: a visible one, minimized included; X11: one
   in the window manager's client list, minimized or on another workspace included). Strings are UTF-8. */
typedef struct {
    uint64_t id;        /* the HWND, or the X11 window id */
    long pid;           /* the process that owns it */
    char title[512];
    char cls[128];      /* Windows: the window class; X11: WM_CLASS's class part */
    char instance[128]; /* X11: WM_CLASS's instance part; Windows: "" */
    char role[64];      /* X11: WM_WINDOW_ROLE; Windows: "" */
    int topmost;        /* WS_EX_TOPMOST, or _NET_WM_STATE_ABOVE */
    long long area;     /* width times height, in pixels */
} probe_win;

/* "title" when title, lowercased with hyphens read as spaces and outer spaces trimmed, is exactly
   "picture in picture"; "role" when role is "PictureInPicture"; else NULL. */
const char *probe_pip_rule(const char *title, const char *role);

/* "wayland" when wayland_display is set or session_type is "wayland"; else "x11" when display is set; else "none". */
const char *probe_session_kind(const char *wayland_display, const char *session_type, const char *display);

typedef struct {
    /* Prints the pip_session line; returns 1 when it can list windows, else 0 (the watch then only supervises). */
    int (*session)(void);
    /* Fills out with the browser's top-level windows, as probe_win describes them; returns how many. */
    int (*snapshot)(probe_win *out, int cap);
    /* Asks the browser to close window id, as its own close button would; writes the method into method; returns 1
       when the request was sent. The window going away shows in a later snapshot. */
    int (*close)(uint64_t id, char *method, size_t cap);
    /* Brings window id to the front; writes the method used into method; returns 1 when the call reported success. */
    int (*raise)(uint64_t id, char *method, size_t cap);
    /* The window that has the keyboard focus now, or 0. */
    uint64_t (*foreground)(void);
} probe_pip_platform;

extern const probe_pip_platform probe_pip_platform_impl;

int probe_pip_main(int argc, char **argv, const probe_pip_platform *p);

#endif
