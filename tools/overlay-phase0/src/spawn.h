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

int probe_run(int argc, char **argv);    /* run <program> [args...]; stdin lines: close | kill */
int probe_keys(int argc, char **argv);   /* keys [--dry-run] <delay_ms> <key>[+<key>...] ... */
int probe_keylog(int argc, char **argv); /* keylog <seconds> */
int probe_pad(int argc, char **argv);    /* pad <seconds> <default|xinput|rawinput|wgi> */
int probe_focus(int argc, char **argv);  /* focus <program> [args...] */

#endif
