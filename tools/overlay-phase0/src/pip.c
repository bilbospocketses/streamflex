/* probe_spawn pip: run's supervision with a watch on the browser's top-level windows. Each new window is reported
   with its pid, title and class. One that looks like picture-in-picture is closed (it is always on top, so a raise
   alone would leave it covering the main window, and closing it returns the video to the page), then the browser's
   main window is raised; every step is timed from the window's first appearance. Both act on the browser's own
   windows, never on a page. The platform files list, close, raise and read the focus; this file decides. */
#include "pip.h"

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "spawn.h"

#define PIP_USAGE \
    "usage: probe_spawn pip [--no-close] [--no-raise] <program> [args...] | pip --classify <title> [<role>]\n"

const char *probe_pip_rule(const char *title, const char *role)
{
    if (role && strcmp(role, "PictureInPicture") == 0)
        return "role";
    char norm[64];
    size_t n = 0;
    const char *p = title ? title : "";
    while (*p == ' ')
        p++;
    for (; *p && n + 1 < sizeof norm; p++) {
        char c = *p;
        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        else if (c == '-')
            c = ' ';
        norm[n++] = c;
    }
    if (*p)
        return NULL; /* longer than any title the rule names */
    while (n && norm[n - 1] == ' ')
        n--;
    norm[n] = '\0';
    return strcmp(norm, "picture in picture") == 0 ? "title" : NULL;
}

const char *probe_session_kind(const char *wayland_display, const char *session_type, const char *display)
{
    if ((wayland_display && *wayland_display) || (session_type && strcmp(session_type, "wayland") == 0))
        return "wayland";
    if (display && *display)
        return "x11";
    return "none";
}

typedef struct {
    probe_win w;
    int64_t seen_ms;  /* probe_mono_ms when first listed */
    const char *rule; /* NULL: not picture-in-picture */
    int live;         /* listed in the latest pass */
    int closed;       /* the watch asked the browser to close it */
} tracked;

static struct {
    const probe_pip_platform *p;
    int can_list, no_close, no_raise, limit_reported;
    tracked win[PROBE_PIP_MAX_WINDOWS];
    int n;
    uint64_t want_fg, want_pip; /* the last raise: the main window that should get the focus, and its cause */
    int64_t want_since;         /* the cause's seen_ms */
    int windows, pips, closes, raises, focused;
} g;

static probe_win g_snap[PROBE_PIP_MAX_WINDOWS];

static void quoted_id(char *out, size_t cap, uint64_t id)
{
    snprintf(out, cap, "\"0x%llx\"", (unsigned long long)id);
}

static void emit_window(const char *event, const tracked *t)
{
    char title[1100], cls[300], inst[300], role[160], id[32], rule[16], fields[2200];
    probe_json_string(title, sizeof title, t->w.title);
    probe_json_string(cls, sizeof cls, t->w.cls);
    probe_json_string(inst, sizeof inst, t->w.instance);
    probe_json_string(role, sizeof role, t->w.role);
    quoted_id(id, sizeof id, t->w.id);
    if (t->rule)
        snprintf(rule, sizeof rule, "\"%s\"", t->rule);
    else
        snprintf(rule, sizeof rule, "null");
    snprintf(fields, sizeof fields,
             "\"event\":\"%s\",\"id\":%s,\"pid\":%ld,\"title\":%s,\"class\":%s,\"instance\":%s,\"role\":%s,"
             "\"topmost\":%s,\"area\":%lld,\"pip\":%s,\"rule\":%s",
             event, id, t->w.pid, title, cls, inst, role, t->w.topmost ? "true" : "false", t->w.area,
             t->rule ? "true" : "false", rule);
    probe_emit(fields);
}

static tracked *find(uint64_t id)
{
    for (int i = 0; i < g.n; i++)
        if (g.win[i].w.id == id)
            return &g.win[i];
    return NULL;
}

/* The browser's main window: the largest live window that is not picture-in-picture. */
static tracked *main_window(void)
{
    tracked *best = NULL;
    for (int i = 0; i < g.n; i++) {
        tracked *t = &g.win[i];
        if (t->live && !t->rule && (!best || t->w.area > best->w.area))
            best = t;
    }
    return best;
}

/* A picture-in-picture window: close it first, then raise the main window. */
static void on_pip(tracked *t, int64_t now)
{
    char id[32], main_id[32], method[64], fields[320];
    tracked *m = main_window();
    g.pips++;
    quoted_id(id, sizeof id, t->w.id);
    if (m)
        quoted_id(main_id, sizeof main_id, m->w.id);
    else
        snprintf(main_id, sizeof main_id, "null");
    snprintf(fields, sizeof fields, "\"event\":\"pip_seen\",\"id\":%s,\"main\":%s,\"rule\":\"%s\",\"ms_since_seen\":%lld",
             id, main_id, t->rule, (long long)(now - t->seen_ms));
    probe_emit(fields);
    if (g.no_close) {
        snprintf(fields, sizeof fields, "\"event\":\"close_skipped\",\"id\":%s,\"why\":\"no_close\"", id);
        probe_emit(fields);
    } else {
        method[0] = '\0';
        int sent = g.p->close(t->w.id, method, sizeof method);
        t->closed = 1;
        g.closes++;
        snprintf(fields, sizeof fields, "\"event\":\"pip_closed\",\"id\":%s,\"call_ok\":%s,\"method\":\"%s\",\"ms\":%lld",
                 id, sent ? "true" : "false", method, (long long)(probe_mono_ms() - t->seen_ms));
        probe_emit(fields);
    }
    if (!m || g.no_raise) {
        snprintf(fields, sizeof fields, "\"event\":\"raise_skipped\",\"id\":%s,\"why\":\"%s\"", id,
                 m ? "no_raise" : "no_main_window");
        probe_emit(fields);
        return;
    }
    method[0] = '\0';
    int ok = g.p->raise(m->w.id, method, sizeof method);
    int64_t after = probe_mono_ms();
    g.raises++;
    snprintf(fields, sizeof fields,
             "\"event\":\"raised\",\"id\":%s,\"pip_id\":%s,\"call_ok\":%s,\"method\":\"%s\",\"ms\":%lld", main_id, id,
             ok ? "true" : "false", method, (long long)(after - t->seen_ms));
    probe_emit(fields);
    g.want_fg = m->w.id;
    g.want_pip = t->w.id;
    g.want_since = t->seen_ms;
}

static void pip_tick(void *unused)
{
    uint64_t pending[PROBE_PIP_MAX_WINDOWS];
    int npending = 0;
    (void)unused;
    if (!g.can_list)
        return;
    int n = g.p->snapshot(g_snap, PROBE_PIP_MAX_WINDOWS);
    int64_t now = probe_mono_ms();
    for (int i = 0; i < g.n; i++)
        g.win[i].live = 0;
    for (int i = 0; i < n; i++) {
        const probe_win *s = &g_snap[i];
        tracked *t = find(s->id);
        if (!t) {
            if (g.n == PROBE_PIP_MAX_WINDOWS) {
                if (!g.limit_reported)
                    probe_emit("\"event\":\"pip_window_limit\",\"max\":256");
                g.limit_reported = 1;
                continue;
            }
            t = &g.win[g.n++];
            t->w = *s;
            t->seen_ms = now;
            t->live = 1;
            t->rule = probe_pip_rule(s->title, s->role);
            g.windows++;
            emit_window("window", t);
            if (t->rule)
                pending[npending++] = t->w.id;
            continue;
        }
        t->live = 1;
        if (strcmp(t->w.title, s->title) != 0 || strcmp(t->w.role, s->role) != 0 || t->w.topmost != s->topmost) {
            const char *was = t->rule;
            t->w = *s; /* seen_ms stays: the time runs from the window's first appearance */
            t->rule = probe_pip_rule(s->title, s->role);
            emit_window("window_changed", t);
            if (t->rule && !was)
                pending[npending++] = t->w.id;
        } else {
            t->w.area = s->area; /* a size change alone is not reported */
        }
    }
    /* After the whole pass, so the main window is live whatever order the platform listed the windows in. */
    for (int k = 0; k < npending; k++) {
        tracked *t = find(pending[k]);
        if (t)
            on_pip(t, now);
    }
    for (int i = 0; i < g.n;) {
        tracked *t = &g.win[i];
        if (t->live) {
            i++;
            continue;
        }
        emit_window("window_gone", t);
        if (t->rule) {
            char id[32], fields[160];
            quoted_id(id, sizeof id, t->w.id);
            snprintf(fields, sizeof fields, "\"event\":\"pip_gone\",\"id\":%s,\"closed\":%s,\"ms\":%lld", id,
                     t->closed ? "true" : "false", (long long)(now - t->seen_ms));
            probe_emit(fields);
        }
        *t = g.win[--g.n];
    }
    if (g.want_fg && g.p->foreground() == g.want_fg) {
        char id[32], pip_id[32], fields[200];
        quoted_id(id, sizeof id, g.want_fg);
        quoted_id(pip_id, sizeof pip_id, g.want_pip);
        snprintf(fields, sizeof fields, "\"event\":\"foreground\",\"id\":%s,\"pip_id\":%s,\"ms\":%lld", id, pip_id,
                 (long long)(probe_mono_ms() - g.want_since));
        probe_emit(fields);
        g.focused++;
        g.want_fg = 0;
    }
}

static void pip_ended(void *unused)
{
    char fields[200];
    (void)unused;
    snprintf(fields, sizeof fields,
             "\"event\":\"pip_summary\",\"listed\":%s,\"windows\":%d,\"pip\":%d,\"closed\":%d,\"raised\":%d,"
             "\"foreground\":%d",
             g.can_list ? "true" : "false", g.windows, g.pips, g.closes, g.raises, g.focused);
    probe_emit(fields);
}

int probe_pip_main(int argc, char **argv, const probe_pip_platform *p)
{
    if (argc >= 1 && strcmp(argv[0], "--classify") == 0) {
        if (argc < 2 || argc > 3) {
            fputs(PIP_USAGE, stderr);
            return 2;
        }
        const char *rule = probe_pip_rule(argv[1], argc == 3 ? argv[2] : "");
        char r[16], fields[96];
        if (rule)
            snprintf(r, sizeof r, "\"%s\"", rule);
        else
            snprintf(r, sizeof r, "null");
        snprintf(fields, sizeof fields, "\"event\":\"classify\",\"pip\":%s,\"rule\":%s", rule ? "true" : "false", r);
        probe_emit(fields);
        return 0;
    }
    memset(&g, 0, sizeof g);
    int i = 0;
    for (; i < argc; i++) {
        if (strcmp(argv[i], "--no-raise") == 0)
            g.no_raise = 1;
        else if (strcmp(argv[i], "--no-close") == 0)
            g.no_close = 1;
        else
            break;
    }
    if (argc - i < 1) {
        fputs(PIP_USAGE, stderr);
        return 2;
    }
    g.p = p;
    g.can_list = p->session();
    probe_watch w = { pip_tick, pip_ended, NULL };
    return probe_supervise(argc - i, argv + i, PIP_USAGE, &w);
}

int probe_pip(int argc, char **argv)
{
    return probe_pip_main(argc, argv, &probe_pip_platform_impl);
}
