/* probe_spawn text, Linux: each character as the key, and Shift or AltGr, that give it in the session's keyboard
   layout, sent through uinput. The layout comes from --layout, else from the session: X11's _XKB_RULES_NAMES,
   GNOME's org.gnome.desktop.input-sources, KDE's kxkbrc, then localectl's system default, each attempt reported. The
   chosen layout is compiled with xkbcommon, and each character is found by pressing nothing, Shift, AltGr, then
   Shift and AltGr on an xkb_state, keys in code order: the same reading the compositor makes of those presses. A
   character no key gives directly is looked up in xkbcommon's compose table for the session's locale: the shortest
   entry producing it whose every keysym the layout can type, a dead-key sequence before a Multi_key one, is typed
   key by key, and the compositor composes it. Only a character with no path is named untyped. */
#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "pip.h"
#include "spawn.h"
#include "text.h"
#include "uinput_linux.h"

#ifdef PROBE_HAVE_XKB
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon.h>
#endif
#ifdef PROBE_HAVE_X11
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif

#define MOD_SHIFT 1
#define MOD_ALTGR 2
#define STALL_MS 10000 /* a source with no output, no CPU use and no exit for this long is stalled */

static void sleep_ms(long ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/* Everything from here to press exists only to feed xkbcommon, so a build without it carries none of it. */
#ifdef PROBE_HAVE_XKB
typedef struct {
    char rules[64], model[64], layout[64], variant[64], options[256];
    const char *source;
} rmlvo;

/* value (NULL: "") into field, cut to cap - 1 bytes. */
static void set_field(char *field, size_t cap, const char *value)
{
    size_t n = value ? strlen(value) : 0;
    if (n >= cap)
        n = cap - 1;
    if (n)
        memcpy(field, value, n);
    field[n] = '\0';
}
#define SET(field, value) set_field(field, sizeof field, value)

static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n'))
        s[--n] = '\0';
    return s;
}

/* The n-th (from 0) comma-separated item of list, or "" when there is none. */
static void nth_item(const char *list, int n, char *out, size_t cap)
{
    out[0] = '\0';
    for (int i = 0; list && i < n; i++) {
        list = strchr(list, ',');
        if (list)
            list++;
    }
    if (!list)
        return;
    size_t len = strcspn(list, ",");
    if (len >= cap)
        len = cap - 1;
    memcpy(out, list, len);
    out[len] = '\0';
}

/* utime + stime summed over every process in the process group pgid, in clock ticks, from each /proc/<pid>/stat; -1
   when none is readable. A source that is a wrapper (a shell that runs its work as its own child) is judged by all
   of that work, not by the wrapper alone. */
static long long group_cpu_ticks(pid_t pgid)
{
    DIR *proc = opendir("/proc");
    if (!proc)
        return -1;
    long long total = -1;
    struct dirent *d;
    while ((d = readdir(proc)) != NULL) {
        if (d->d_name[0] < '0' || d->d_name[0] > '9')
            continue;
        char path[300], buf[1024];
        snprintf(path, sizeof path, "/proc/%s/stat", d->d_name);
        FILE *f = fopen(path, "r");
        if (!f)
            continue; /* it exited while the list was read */
        size_t n = fread(buf, 1, sizeof buf - 1, f);
        fclose(f);
        buf[n] = '\0';
        char *p = strrchr(buf, ')'); /* the command name may hold spaces; the fields after it do not */
        int pgrp = 0;
        unsigned long long ut = 0, st = 0;
        /* state, ppid, pgrp, session, tty_nr, tpgid, flags, minflt, cminflt, majflt, cmajflt, utime, stime */
        if (!p || sscanf(p + 2, "%*c %*d %d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu", &pgrp, &ut, &st) != 3
            || pgrp != (int)pgid)
            continue;
        total = (total < 0 ? 0 : total) + (long long)(ut + st);
    }
    closedir(proc);
    return total;
}

/* Runs argv from PATH in a process group of its own, with stdin and stderr on /dev/null and its stdout into out (the
   first cap - 1 bytes). Returns "ok", or why not: "not_found" (exit 127), "failed" (another failure), "stalled"
   (STALL_MS with no output, no CPU use anywhere in its process group and no exit; the whole group is killed). A
   source that keeps working is never cut off. */
static const char *capture(char *const argv[], char *out, size_t cap)
{
    int fds[2];
    out[0] = '\0';
    if (pipe(fds) == -1)
        return "failed";
    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return "failed";
    }
    if (pid == 0) {
        setpgid(0, 0); /* its own group, so a wrapper's children are watched and killed with it */
        int null_fd = open("/dev/null", O_RDWR);
        if (null_fd >= 0) {
            dup2(null_fd, STDIN_FILENO);
            dup2(null_fd, STDERR_FILENO);
        }
        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]);
        close(fds[1]);
        execvp(argv[0], argv);
        _exit(127);
    }
    setpgid(pid, pid); /* the same from this side, so the group exists before the first reading; once the child has
                          run exec this fails harmlessly, the child having set it itself */
    close(fds[1]);
    size_t len = 0;
    int pipe_open = 1, status = 0;
    long long cpu = group_cpu_ticks(pid);
    int64_t last = probe_mono_ms();
    for (;;) {
        if (pipe_open) {
            struct pollfd pfd = { fds[0], POLLIN, 0 };
            if (poll(&pfd, 1, 100) > 0) {
                char buf[512];
                ssize_t n = read(fds[0], buf, sizeof buf);
                if (n > 0) {
                    size_t keep = len + (size_t)n < cap - 1 ? (size_t)n : cap - 1 - len;
                    memcpy(out + len, buf, keep);
                    len += keep;
                    out[len] = '\0';
                    last = probe_mono_ms();
                } else if (n == 0 || errno != EINTR) {
                    pipe_open = 0;
                }
            }
        } else {
            poll(NULL, 0, 100);
        }
        if (waitpid(pid, &status, WNOHANG) == pid)
            break;
        long long now_cpu = group_cpu_ticks(pid);
        if (now_cpu != cpu) {
            cpu = now_cpu;
            last = probe_mono_ms();
        }
        if (probe_mono_ms() - last > STALL_MS) {
            kill(-pid, SIGKILL); /* the whole group: a wrapper's children too */
            waitpid(pid, NULL, 0);
            close(fds[0]);
            return "stalled";
        }
    }
    /* What it wrote just before it exited. */
    struct pollfd pfd = { fds[0], POLLIN, 0 };
    ssize_t n;
    char buf[512];
    while (pipe_open && poll(&pfd, 1, 0) > 0 && (n = read(fds[0], buf, sizeof buf)) > 0) {
        size_t keep = len + (size_t)n < cap - 1 ? (size_t)n : cap - 1 - len;
        memcpy(out + len, buf, keep);
        len += keep;
        out[len] = '\0';
    }
    close(fds[0]);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        return "ok";
    return WIFEXITED(status) && WEXITSTATUS(status) == 127 ? "not_found" : "failed";
}

/* "de" or "de(nodeadkeys)". */
static const char *from_argument(const char *arg, rmlvo *r)
{
    const char *open_paren = strchr(arg, '(');
    size_t len = open_paren ? (size_t)(open_paren - arg) : strlen(arg);
    if (!len || len >= sizeof r->layout)
        return "bad_argument";
    memcpy(r->layout, arg, len);
    r->layout[len] = '\0';
    if (open_paren) {
        const char *close_paren = strchr(open_paren, ')');
        size_t vlen = close_paren ? (size_t)(close_paren - open_paren - 1) : 0;
        if (!close_paren || close_paren[1] || !vlen || vlen >= sizeof r->variant)
            return "bad_argument";
        memcpy(r->variant, open_paren + 1, vlen);
        r->variant[vlen] = '\0';
    }
    return "ok";
}

/* The X server's own rules, model, layouts, variants and options, and the group it has active. */
static const char *from_x11(rmlvo *r)
{
#ifndef PROBE_HAVE_X11
    (void)r;
    return "not_built";
#else
    const char *d = getenv("DISPLAY");
    Display *dpy = d && *d ? XOpenDisplay(d) : NULL;
    if (!dpy)
        return "open_failed";
    Atom names = XInternAtom(dpy, "_XKB_RULES_NAMES", True);
    Atom actual = None;
    int format = 0;
    unsigned long n = 0, after = 0;
    unsigned char *data = NULL;
    if (names == None
        || XGetWindowProperty(dpy, DefaultRootWindow(dpy), names, 0, 1024, False, XA_STRING, &actual, &format, &n,
                              &after, &data) != Success
        || actual == None || !data) {
        if (data)
            XFree(data);
        XCloseDisplay(dpy);
        return "no_property";
    }
    /* Five NUL-separated fields: rules, model, layouts, variants, options. */
    const char *field[5] = { "", "", "", "", "" };
    unsigned long at = 0;
    for (int k = 0; k < 5 && at < n; k++) {
        field[k] = (const char *)data + at;
        at += strlen(field[k]) + 1;
    }
    XkbStateRec st;
    int group = XkbGetState(dpy, XkbUseCoreKbd, &st) == Success ? st.group : 0;
    SET(r->rules, field[0]);
    SET(r->model, field[1]);
    nth_item(field[2], group, r->layout, sizeof r->layout);
    nth_item(field[3], group, r->variant, sizeof r->variant);
    SET(r->options, field[4]);
    XFree(data);
    XCloseDisplay(dpy);
    return r->layout[0] ? "ok" : "no_layout";
#endif
}

/* The first xkb source in gsettings' printout of an a(ss) value ("layout" or "layout+variant"); 0 when none. */
static int gnome_first_xkb(const char *text, char *out, size_t cap)
{
    const char *p = text;
    while ((p = strstr(p, "'xkb'")) != NULL) {
        p += 5;
        const char *q = strchr(p, '\'');
        const char *end = q ? strchr(q + 1, '\'') : NULL;
        if (!end)
            return 0;
        size_t len = (size_t)(end - q - 1);
        if (len && len < cap) {
            memcpy(out, q + 1, len);
            out[len] = '\0';
            return 1;
        }
        p = end + 1;
    }
    return 0;
}

/* gsettings' printout of an as value ("['a', 'b']", or "@as []"), joined with commas. */
static void gnome_list(const char *text, char *out, size_t cap)
{
    size_t len = 0;
    out[0] = '\0';
    for (const char *p = strchr(text, '\''); p;) {
        const char *end = strchr(p + 1, '\'');
        if (!end)
            break;
        size_t n = (size_t)(end - p - 1);
        if (len + n + 2 > cap)
            break;
        if (len)
            out[len++] = ',';
        memcpy(out + len, p + 1, n);
        len += n;
        out[len] = '\0';
        p = strchr(end + 1, '\'');
    }
}

/* GNOME keeps the most recently used input source first in mru-sources. */
static const char *from_gnome(rmlvo *r)
{
    char out[2048], id[128];
    char *mru[] = { "gsettings", "get", "org.gnome.desktop.input-sources", "mru-sources", NULL };
    char *all[] = { "gsettings", "get", "org.gnome.desktop.input-sources", "sources", NULL };
    char *opts[] = { "gsettings", "get", "org.gnome.desktop.input-sources", "xkb-options", NULL };
    const char *why = capture(mru, out, sizeof out);
    if (strcmp(why, "ok") != 0)
        return why;
    if (!gnome_first_xkb(out, id, sizeof id)) {
        why = capture(all, out, sizeof out);
        if (strcmp(why, "ok") != 0)
            return why;
        if (!gnome_first_xkb(out, id, sizeof id))
            return "no_xkb_source";
    }
    char *plus = strchr(id, '+');
    if (plus) {
        *plus = '\0';
        SET(r->variant, plus + 1);
    }
    SET(r->layout, id);
    if (strcmp(capture(opts, out, sizeof out), "ok") == 0)
        gnome_list(out, r->options, sizeof r->options);
    return "ok";
}

/* KDE's kxkbrc lists the layouts; the current one is not stored there, so the first is taken. */
static const char *from_kde(rmlvo *r)
{
    char path[1024];
    const char *xdg = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
    if (xdg && *xdg)
        snprintf(path, sizeof path, "%s/kxkbrc", xdg);
    else if (home && *home)
        snprintf(path, sizeof path, "%s/.config/kxkbrc", home);
    else
        return "no_home";
    FILE *f = fopen(path, "r");
    if (!f)
        return "no_file";
    char line[512], layouts[256] = "", variants[256] = "", use[16] = "";
    int in_layout = 0;
    while (fgets(line, sizeof line, f)) {
        char *s = trim(line);
        if (*s == '[') {
            in_layout = strcmp(s, "[Layout]") == 0;
            continue;
        }
        char *eq = strchr(s, '=');
        if (!in_layout || !eq)
            continue;
        *eq = '\0';
        char *key = trim(s), *value = trim(eq + 1);
        if (strcmp(key, "LayoutList") == 0)
            SET(layouts, value);
        else if (strcmp(key, "VariantList") == 0)
            SET(variants, value);
        else if (strcmp(key, "Options") == 0)
            SET(r->options, value);
        else if (strcmp(key, "Model") == 0)
            SET(r->model, value);
        else if (strcmp(key, "Use") == 0)
            SET(use, value);
    }
    fclose(f);
    if (strcmp(use, "false") == 0)
        return "not_in_use";
    nth_item(layouts, 0, r->layout, sizeof r->layout);
    nth_item(variants, 0, r->variant, sizeof r->variant);
    return r->layout[0] ? "ok" : "no_layout";
}

/* The system default (systemd-localed): what a session uses when it sets nothing of its own. */
static const char *from_localectl(rmlvo *r)
{
    char out[2048], layouts[256] = "", variants[256] = "";
    char *argv[] = { "localectl", "status", NULL };
    const char *why = capture(argv, out, sizeof out);
    if (strcmp(why, "ok") != 0)
        return why;
    for (char *line = strtok(out, "\n"); line; line = strtok(NULL, "\n")) {
        char *colon = strchr(line, ':');
        if (!colon)
            continue;
        *colon = '\0';
        char *key = trim(line), *value = trim(colon + 1);
        if (strcmp(key, "X11 Layout") == 0)
            SET(layouts, value);
        else if (strcmp(key, "X11 Variant") == 0)
            SET(variants, value);
        else if (strcmp(key, "X11 Model") == 0)
            SET(r->model, value);
        else if (strcmp(key, "X11 Options") == 0)
            SET(r->options, value);
    }
    nth_item(layouts, 0, r->layout, sizeof r->layout);
    nth_item(variants, 0, r->variant, sizeof r->variant);
    return r->layout[0] ? "ok" : "no_layout";
}

/* Prints the attempt; a failed one leaves nothing behind in r. */
static int try_source(const char *name, const char *result, rmlvo *r)
{
    char fields[200];
    snprintf(fields, sizeof fields, "\"event\":\"layout_source\",\"source\":\"%s\",\"result\":\"%s\"", name, result);
    probe_emit(fields);
    if (strcmp(result, "ok") != 0) {
        memset(r, 0, sizeof *r);
        return 0;
    }
    r->source = name;
    return 1;
}

static int learn_layout(const char *arg, rmlvo *r)
{
    memset(r, 0, sizeof *r);
    if (arg)
        return try_source("argument", from_argument(arg, r), r);
    const char *kind = probe_session_kind(getenv("WAYLAND_DISPLAY"), getenv("XDG_SESSION_TYPE"), getenv("DISPLAY"));
    const char *desktop = getenv("XDG_CURRENT_DESKTOP");
    if (strcmp(kind, "x11") == 0 && try_source("x11:_XKB_RULES_NAMES", from_x11(r), r))
        return 1;
    if (desktop && strstr(desktop, "GNOME") && try_source("gnome:org.gnome.desktop.input-sources", from_gnome(r), r))
        return 1;
    if (desktop && strstr(desktop, "KDE") && try_source("kde:kxkbrc", from_kde(r), r))
        return 1;
    return try_source("localectl", from_localectl(r), r);
}

static struct xkb_context *g_ctx;
static struct xkb_keymap *g_map;
static struct xkb_compose_table *g_compose; /* NULL: no compose table for the locale */
static char g_name[140];                    /* "de(nodeadkeys)", for the untyped reasons */
static char g_locale[80];

#define MAX_SEQ 4 /* the longest dead-key or compose sequence the search takes */

typedef struct {
    int code, mods; /* an evdev key code, and MOD_SHIFT | MOD_ALTGR */
} step;

typedef struct {
    const char *path; /* "direct", "dead" or "compose" */
    int n;
    step s[MAX_SEQ];
} route;

/* The first key, in code order, that gives the character cp (when sym is 0) or the keysym sym, with no modifier,
   then Shift, then AltGr, then both. A modifier's own key is skipped only while it is held, so a Multi_key on the
   right Alt (compose:ralt) is still found with nothing held. */
static int find_step(uint32_t cp, xkb_keysym_t sym, step *out)
{
    static const int combos[4] = { 0, MOD_SHIFT, MOD_ALTGR, MOD_SHIFT | MOD_ALTGR };
    xkb_keycode_t lo = xkb_keymap_min_keycode(g_map), hi = xkb_keymap_max_keycode(g_map);
    if (lo < 9)
        lo = 9; /* evdev code 1 */
    if (hi > 255)
        hi = 255; /* evdev code 247, the last the device declares */
    for (int c = 0; c < 4; c++) {
        struct xkb_state *st = xkb_state_new(g_map);
        if (!st)
            return 0;
        if (combos[c] & MOD_SHIFT)
            xkb_state_update_key(st, KEY_LEFTSHIFT + 8, XKB_KEY_DOWN);
        if (combos[c] & MOD_ALTGR)
            xkb_state_update_key(st, KEY_RIGHTALT + 8, XKB_KEY_DOWN);
        for (xkb_keycode_t kc = lo; kc <= hi; kc++) {
            if (((combos[c] & MOD_SHIFT) && kc == KEY_LEFTSHIFT + 8) || ((combos[c] & MOD_ALTGR) && kc == KEY_RIGHTALT + 8))
                continue;
            int match = sym ? xkb_state_key_get_one_sym(st, kc) == sym : xkb_state_key_get_utf32(st, kc) == cp;
            if (match) {
                out->code = (int)kc - 8;
                out->mods = combos[c];
                xkb_state_unref(st);
                return 1;
            }
        }
        xkb_state_unref(st);
    }
    return 0;
}

/* Whether a compose table entry produces cp (want is cp as UTF-8). */
static int entry_makes(struct xkb_compose_table_entry *e, uint32_t cp, const char *want)
{
    const char *utf8 = xkb_compose_table_entry_utf8(e);
    if (utf8 && *utf8)
        return strcmp(utf8, want) == 0;
    return xkb_keysym_to_utf32(xkb_compose_table_entry_keysym(e)) == cp;
}

/* A direct key; else the shortest compose table entry producing cp whose every keysym the layout can type, a
   dead-key sequence before a Multi_key one of the same length, else the first one found. */
static int find_route(uint32_t cp, route *r)
{
    if (find_step(cp, 0, &r->s[0])) {
        r->path = "direct";
        r->n = 1;
        return 1;
    }
    if (!g_compose)
        return 0;
    char want[8];
    probe_utf8_encode(cp, want);
    int found = 0;
    struct xkb_compose_table_iterator *it = xkb_compose_table_iterator_new(g_compose);
    struct xkb_compose_table_entry *e;
    while (it && (e = xkb_compose_table_iterator_next(it)) != NULL) {
        if (!entry_makes(e, cp, want))
            continue;
        size_t len = 0;
        const xkb_keysym_t *seq = xkb_compose_table_entry_sequence(e, &len);
        if (len == 0 || len > MAX_SEQ)
            continue;
        const char *path = seq[0] == XKB_KEY_Multi_key ? "compose" : "dead";
        if (found && ((int)len > r->n
                      || ((int)len == r->n && (strcmp(path, "compose") == 0 || strcmp(r->path, "dead") == 0))))
            continue;
        route c;
        c.path = path;
        c.n = (int)len;
        int ok = 1;
        for (size_t k = 0; k < len && ok; k++)
            ok = find_step(0, seq[k], &c.s[k]);
        if (ok) {
            *r = c;
            found = 1;
        }
    }
    if (it)
        xkb_compose_table_iterator_free(it);
    return found;
}

static const char *mods_json(int mods)
{
    return mods == (MOD_SHIFT | MOD_ALTGR) ? "\"Shift\",\"AltGr\""
           : mods == MOD_SHIFT             ? "\"Shift\""
           : mods == MOD_ALTGR             ? "\"AltGr\""
                                           : "";
}

/* One step: its modifiers down, the key down and up, the modifiers up. The compositor composes a dead-key or
   Multi_key sequence from the steps as it would from a real keyboard. */
static int press(const step *s)
{
    int ok = 1;
    if (s->mods & MOD_SHIFT)
        ok &= probe_uinput_key(KEY_LEFTSHIFT, 1) == 0;
    if (s->mods & MOD_ALTGR)
        ok &= probe_uinput_key(KEY_RIGHTALT, 1) == 0;
    ok &= probe_uinput_key((unsigned short)s->code, 1) == 0;
    ok &= probe_uinput_key((unsigned short)s->code, 0) == 0;
    if (s->mods & MOD_ALTGR)
        ok &= probe_uinput_key(KEY_RIGHTALT, 0) == 0;
    if (s->mods & MOD_SHIFT)
        ok &= probe_uinput_key(KEY_LEFTSHIFT, 0) == 0;
    return ok;
}
#endif

static int prepare(const char *arg)
{
#ifndef PROBE_HAVE_XKB
    (void)arg;
    probe_emit("\"event\":\"text_failed\",\"step\":\"not_built\",\"need\":\"libxkbcommon\"");
    fputs("probe_spawn text: built without libxkbcommon; nothing was typed\n", stderr);
    return 1;
#else
    if (geteuid() == 0)
        probe_emit("\"event\":\"layout_note\",\"note\":\"running as root: the desktop user's settings and display may "
                   "be out of reach; run as that user with /dev/uinput opened to it\"");
    rmlvo r;
    char a[160], b[160], c[160], d[160], e[400], fields[1200];
    if (!learn_layout(arg, &r)) {
        const char *desktop = getenv("XDG_CURRENT_DESKTOP");
        probe_json_string(a, sizeof a, desktop ? desktop : "");
        snprintf(fields, sizeof fields, "\"event\":\"layout_unknown\",\"session\":\"%s\",\"desktop\":%s",
                 probe_session_kind(getenv("WAYLAND_DISPLAY"), getenv("XDG_SESSION_TYPE"), getenv("DISPLAY")), a);
        probe_emit(fields);
        fputs("probe_spawn text: could not learn the session's keyboard layout (pass --layout); nothing was typed\n",
              stderr);
        return 1;
    }
    const char *rules = r.rules[0] ? r.rules : "evdev";
    probe_json_string(a, sizeof a, rules);
    probe_json_string(b, sizeof b, r.model);
    probe_json_string(c, sizeof c, r.layout);
    probe_json_string(d, sizeof d, r.variant);
    probe_json_string(e, sizeof e, r.options);
    snprintf(fields, sizeof fields,
             "\"event\":\"layout\",\"source\":\"%s\",\"rules\":%s,\"model\":%s,\"layout\":%s,\"variant\":%s,"
             "\"options\":%s",
             r.source, a, b, c, d, e);
    probe_emit(fields);
    if (r.variant[0])
        snprintf(g_name, sizeof g_name, "%s(%s)", r.layout, r.variant);
    else
        snprintf(g_name, sizeof g_name, "%s", r.layout);
    struct xkb_rule_names names = { rules, r.model[0] ? r.model : NULL, r.layout, r.variant[0] ? r.variant : NULL,
                                    r.options[0] ? r.options : NULL };
    g_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    g_map = g_ctx ? xkb_keymap_new_from_names(g_ctx, &names, XKB_KEYMAP_COMPILE_NO_FLAGS) : NULL;
    if (!g_map) {
        snprintf(fields, sizeof fields, "\"event\":\"text_failed\",\"step\":\"keymap\",\"layout\":%s,\"variant\":%s", c, d);
        probe_emit(fields);
        fprintf(stderr, "probe_spawn text: xkbcommon could not compile layout %s; nothing was typed\n", g_name);
        return 1;
    }
    /* The dead-key and compose sequences: the locale's compose table, as the session's own input stack loads it.
       xkbcommon itself honors XCOMPOSEFILE and ~/.XCompose. */
    static const char *const vars[] = { "LC_ALL", "LC_CTYPE", "LANG" };
    const char *loc = NULL;
    for (int k = 0; k < 3 && !loc; k++) {
        const char *v = getenv(vars[k]);
        if (v && *v)
            loc = v;
    }
    snprintf(g_locale, sizeof g_locale, "%s", loc ? loc : "C");
    g_compose = xkb_compose_table_new_from_locale(g_ctx, g_locale, XKB_COMPOSE_COMPILE_NO_FLAGS);
    probe_json_string(a, sizeof a, g_locale);
    snprintf(fields, sizeof fields, "\"event\":\"compose\",\"locale\":%s,\"table\":\"%s\"", a, g_compose ? "ok" : "none");
    probe_emit(fields);
    return 0;
#endif
}

static int plan(uint32_t cp, char *detail, size_t cap)
{
#ifdef PROBE_HAVE_XKB
    route r;
    if (find_route(cp, &r)) {
        if (strcmp(r.path, "direct") == 0) {
            snprintf(detail, cap, "\"path\":\"direct\",\"key\":%d,\"mods\":[%s]", r.s[0].code, mods_json(r.s[0].mods));
            return 1;
        }
        size_t len = (size_t)snprintf(detail, cap, "\"path\":\"%s\",\"keys\":[", r.path);
        for (int k = 0; k < r.n && len < cap; k++)
            len += (size_t)snprintf(detail + len, cap - len, "%s{\"key\":%d,\"mods\":[%s]}", k ? "," : "",
                                    r.s[k].code, mods_json(r.s[k].mods));
        if (len < cap)
            snprintf(detail + len, cap - len, "]");
        return 1;
    }
    if (!g_compose)
        snprintf(detail, cap,
                 "no key in layout %s gives it, and there is no compose table for locale %s, so no dead-key or compose "
                 "sequence could be tried",
                 g_name, g_locale);
    else
        snprintf(detail, cap, "no key, dead-key sequence or compose sequence in layout %s gives it", g_name);
    return 0;
#else
    (void)cp;
    snprintf(detail, cap, "no layout");
    return 0;
#endif
}

static int open_device(void)
{
    return probe_uinput_open(NULL, 0);
}

static int type_char(uint32_t cp)
{
#ifdef PROBE_HAVE_XKB
    route r;
    if (!find_route(cp, &r))
        return 0;
    int ok = 1;
    for (int k = 0; k < r.n; k++)
        ok &= press(&r.s[k]);
    if (!ok) {
        char fields[160];
        snprintf(fields, sizeof fields, "\"event\":\"send_failed\",\"cp\":\"U+%04X\",\"error\":%d", (unsigned)cp, errno);
        probe_emit(fields);
    }
    return ok;
#else
    (void)cp;
    return 0;
#endif
}

static const char *arg_utf8(int argc, char **argv, int i)
{
    (void)argc;
    return argv[i];
}

const probe_text_platform probe_text_platform_impl = { arg_utf8, prepare, plan, open_device, type_char, sleep_ms,
                                                       probe_uinput_close };

int probe_text(int argc, char **argv)
{
    return probe_text_main(argc, argv, &probe_text_platform_impl);
}
