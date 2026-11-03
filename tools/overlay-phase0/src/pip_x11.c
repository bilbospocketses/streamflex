/* probe_spawn pip, Linux. The session kind comes from the environment. On X11, and on Wayland through XWayland when
   DISPLAY is set, the window manager's _NET_CLIENT_LIST is the window list, each window's _NET_WM_PID ties it to the
   browser, _NET_CLOSE_WINDOW closes a picture-in-picture window, and _NET_ACTIVE_WINDOW both raises the main window
   and says which window has the focus. A Wayland session has no portable window list: a browser drawing on Wayland itself is out of reach, and the pip_session line
   says so instead of failing quietly. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "pip.h"
#include "spawn.h"

#ifdef PROBE_HAVE_X11
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

static Display *g_dpy;
static Atom a_client_list, a_pid, a_name, a_utf8, a_role, a_state, a_above, a_active, a_close;

/* A window can close between the list and a read of its properties; Xlib's default handler would end the process. */
static int ignore_x_error(Display *d, XErrorEvent *e)
{
    (void)d;
    (void)e;
    return 0;
}

/* A property's items (n of them), or NULL when the window has no such property of that type. XFree the result. */
static unsigned char *get_prop(Window w, Atom prop, Atom type, unsigned long *n)
{
    Atom actual = None;
    int format = 0;
    unsigned long after = 0;
    unsigned char *data = NULL;
    *n = 0;
    if (XGetWindowProperty(g_dpy, w, prop, 0, 4096, False, type, &actual, &format, n, &after, &data) != Success
        || actual == None || !data) {
        if (data)
            XFree(data);
        *n = 0;
        return NULL;
    }
    return data;
}

/* Drops a UTF-8 sequence that a cut left unfinished at the end of s. */
static void trim_utf8(char *s)
{
    size_t n = strlen(s), k = n;
    while (k > 0 && ((unsigned char)s[k - 1] & 0xC0) == 0x80)
        k--;
    if (k > 0 && (unsigned char)s[k - 1] >= 0xC0) {
        unsigned char lead = (unsigned char)s[k - 1];
        size_t want = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : 2;
        if (n - (k - 1) < want)
            s[k - 1] = '\0';
    }
}

static void copy_prop(Window w, Atom prop, Atom type, char *out, size_t cap)
{
    unsigned long n = 0;
    unsigned char *d = get_prop(w, prop, type, &n);
    out[0] = '\0';
    if (!d)
        return;
    size_t len = n < cap - 1 ? (size_t)n : cap - 1;
    memcpy(out, d, len);
    out[len] = '\0';
    XFree(d);
    trim_utf8(out);
}

static int has_state(Window w, Atom state)
{
    unsigned long n = 0;
    Atom *atoms = (Atom *)get_prop(w, a_state, XA_ATOM, &n);
    int found = 0;
    for (unsigned long i = 0; atoms && i < n; i++)
        if (atoms[i] == state)
            found = 1;
    if (atoms)
        XFree(atoms);
    return found;
}

static const char *open_x11(const char *display)
{
    if (!display || !*display)
        return "no_display";
    XSetErrorHandler(ignore_x_error);
    g_dpy = XOpenDisplay(display);
    if (!g_dpy)
        return "open_failed";
    a_client_list = XInternAtom(g_dpy, "_NET_CLIENT_LIST", False);
    a_pid = XInternAtom(g_dpy, "_NET_WM_PID", False);
    a_name = XInternAtom(g_dpy, "_NET_WM_NAME", False);
    a_utf8 = XInternAtom(g_dpy, "UTF8_STRING", False);
    a_role = XInternAtom(g_dpy, "WM_WINDOW_ROLE", False);
    a_state = XInternAtom(g_dpy, "_NET_WM_STATE", False);
    a_above = XInternAtom(g_dpy, "_NET_WM_STATE_ABOVE", False);
    a_active = XInternAtom(g_dpy, "_NET_ACTIVE_WINDOW", False);
    a_close = XInternAtom(g_dpy, "_NET_CLOSE_WINDOW", False);
    /* A window manager without EWMH keeps no client list, and then there is nothing to watch. */
    unsigned long n = 0;
    unsigned char *list = get_prop(DefaultRootWindow(g_dpy), a_client_list, XA_WINDOW, &n);
    if (!list) {
        XCloseDisplay(g_dpy);
        g_dpy = NULL;
        return "no_client_list";
    }
    XFree(list);
    return "open";
}

static int snapshot(probe_win *out, int cap)
{
    unsigned long n = 0;
    Window *list = (Window *)get_prop(DefaultRootWindow(g_dpy), a_client_list, XA_WINDOW, &n);
    int count = 0;
    for (unsigned long i = 0; list && i < n && count < cap; i++) {
        unsigned long k = 0;
        unsigned long *pid = (unsigned long *)get_prop(list[i], a_pid, XA_CARDINAL, &k);
        long p = pid && k ? (long)pid[0] : -1;
        if (pid)
            XFree(pid);
        if (p <= 0 || !probe_child_has_pid(p))
            continue;
        XWindowAttributes wa;
        if (!XGetWindowAttributes(g_dpy, list[i], &wa) || wa.map_state != IsViewable)
            continue;
        probe_win *w = &out[count++];
        memset(w, 0, sizeof *w);
        w->id = (uint64_t)list[i];
        w->pid = p;
        w->area = (long long)wa.width * wa.height;
        copy_prop(list[i], a_name, a_utf8, w->title, sizeof w->title);
        if (!w->title[0])
            copy_prop(list[i], XA_WM_NAME, AnyPropertyType, w->title, sizeof w->title);
        XClassHint hint;
        if (XGetClassHint(g_dpy, list[i], &hint)) {
            snprintf(w->instance, sizeof w->instance, "%s", hint.res_name ? hint.res_name : "");
            snprintf(w->cls, sizeof w->cls, "%s", hint.res_class ? hint.res_class : "");
            if (hint.res_name)
                XFree(hint.res_name);
            if (hint.res_class)
                XFree(hint.res_class);
        }
        copy_prop(list[i], a_role, XA_STRING, w->role, sizeof w->role);
        w->topmost = has_state(list[i], a_above);
    }
    if (list)
        XFree(list);
    return count;
}

/* EWMH's close request: the window manager closes the window as its own close button would (WM_DELETE_WINDOW to
   the browser), and the browser puts the video back in its page. Source 2, as for the raise below. */
static int close_window(uint64_t id, char *method, size_t cap)
{
    XEvent e;
    memset(&e, 0, sizeof e);
    e.xclient.type = ClientMessage;
    e.xclient.window = (Window)id;
    e.xclient.message_type = a_close;
    e.xclient.format = 32;
    e.xclient.data.l[0] = CurrentTime;
    e.xclient.data.l[1] = 2;
    Status s = XSendEvent(g_dpy, DefaultRootWindow(g_dpy), False, SubstructureRedirectMask | SubstructureNotifyMask,
                          &e);
    XFlush(g_dpy);
    snprintf(method, cap, "_NET_CLOSE_WINDOW");
    return s != 0;
}

/* EWMH's activation request. Source 2 ("a pager or other direct user action") is the one window managers do not hold
   back as focus stealing; the launcher acts for the user's remote, so it is the honest one. */
static int raise_window(uint64_t id, char *method, size_t cap)
{
    XEvent e;
    memset(&e, 0, sizeof e);
    e.xclient.type = ClientMessage;
    e.xclient.window = (Window)id;
    e.xclient.message_type = a_active;
    e.xclient.format = 32;
    e.xclient.data.l[0] = 2;
    e.xclient.data.l[1] = CurrentTime;
    e.xclient.data.l[2] = 0;
    Status s = XSendEvent(g_dpy, DefaultRootWindow(g_dpy), False, SubstructureRedirectMask | SubstructureNotifyMask,
                          &e);
    XFlush(g_dpy);
    snprintf(method, cap, "_NET_ACTIVE_WINDOW");
    return s != 0;
}

static uint64_t foreground(void)
{
    unsigned long n = 0;
    Window *w = (Window *)get_prop(DefaultRootWindow(g_dpy), a_active, XA_WINDOW, &n);
    uint64_t r = w && n ? (uint64_t)w[0] : 0;
    if (w)
        XFree(w);
    return r;
}
#else
static const char *open_x11(const char *display)
{
    (void)display;
    return "not_built";
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
#endif

static int session(void)
{
    const char *kind = probe_session_kind(getenv("WAYLAND_DISPLAY"), getenv("XDG_SESSION_TYPE"), getenv("DISPLAY"));
    const char *display = getenv("DISPLAY");
    const char *x11 = open_x11(display);
    int can = strcmp(x11, "open") == 0;
    const char *note;
    if (strcmp(kind, "wayland") == 0)
        note = can ? "XWayland lists X11 clients only: a Firefox drawing on Wayland itself is not in the list"
                   : "Wayland has no portable window list: the watch cannot see or raise the browser's windows";
    else if (strcmp(x11, "not_built") == 0)
        note = "built without libX11: no windows to list";
    else if (strcmp(kind, "x11") == 0)
        note = can ? "the window manager's client list" : "no usable X11 connection";
    else
        note = "no display: no windows to list";
    char disp[160], fields[640];
    probe_json_string(disp, sizeof disp, display ? display : "");
    snprintf(fields, sizeof fields,
             "\"event\":\"pip_session\",\"session\":\"%s\",\"display\":%s,\"x11\":\"%s\",\"can_list\":%s,"
             "\"can_raise\":%s,\"note\":\"%s\"",
             kind, disp, x11, can ? "true" : "false", can ? "true" : "false", note);
    probe_emit(fields);
    return can;
}

const probe_pip_platform probe_pip_platform_impl = { session, snapshot, close_window, raise_window, foreground };
