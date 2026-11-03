/* probe_spawn's commands. Each takes the arguments after its own name and returns the process exit code. */
#ifndef PROBE_SPAWN_H
#define PROBE_SPAWN_H

#include <stddef.h>

/* run's stdin, cut into lines. A line is a command only when it is exactly "close" or "kill" (a trailing \r is
   dropped); a line too long for buf is no command. Both platforms feed their stdin through this. */
enum { PROBE_CMD_CLOSE = 1, PROBE_CMD_KILL = 2 };
typedef struct {
    char buf[64];
    size_t len;
    int overlong;
} probe_lines;
typedef void (*probe_command_fn)(int cmd, void *ctx);
/* Feeds n bytes; calls on_command for each command line they complete, in order. */
void probe_lines_feed(probe_lines *l, const char *data, size_t n, probe_command_fn on_command, void *ctx);

/* What a supervised run calls while its child runs: tick on every pass of run's loop (about every 50 ms, sooner when
   the child's processes change), ended once, after the exited line. Either may be NULL. */
typedef struct {
    void (*tick)(void *ctx);
    void (*ended)(void *ctx);
    void *ctx;
} probe_watch;

/* run's whole job with an optional watch: usage (printed when argc is 0), spawn, supervision with close and kill on
   stdin, and the exit lines. Returns run's exit code. */
int probe_supervise(int argc, char **argv, const char *usage, const probe_watch *watch);

/* The supervised child's processes, once probe_supervise has started it. Windows: up to cap pids of the job's
   processes; returns the count. Linux: whether pid is the child or one of its descendants (orphans reparent to this
   subreaper, so a descendant's PPid chain always reaches this process). */
#ifdef _WIN32
int probe_child_pids(unsigned long *out, int cap);
#else
int probe_child_has_pid(long pid);
#endif

int probe_run(int argc, char **argv);    /* run <program> [args...]; stdin lines: close | kill */
int probe_pip(int argc, char **argv);    /* pip [--no-close] [--no-raise] <program> [args...] | pip --classify <title> [<role>] */
int probe_text(int argc, char **argv);   /* text [--dry-run] [--layout <l>] <delay_ms> (<string> | --file <path>) */
int probe_keys(int argc, char **argv);   /* keys [--dry-run] <delay_ms> <key>[+<key>...] ... */
int probe_keylog(int argc, char **argv); /* keylog <seconds> */
int probe_pad(int argc, char **argv);    /* pad <seconds> <default|xinput|rawinput|wgi> */
int probe_focus(int argc, char **argv);  /* focus <program> [args...] */

#endif
