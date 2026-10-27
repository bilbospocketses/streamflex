/* probe_spawn's commands. Each takes the arguments after its own name and returns the process exit code. */
#ifndef PROBE_SPAWN_H
#define PROBE_SPAWN_H

int probe_run(int argc, char **argv);    /* run <program> [args...]; stdin lines: close | kill */
int probe_keys(int argc, char **argv);   /* keys [--dry-run] <delay_ms> <key>[+<key>...] ... */
int probe_keylog(int argc, char **argv); /* keylog <seconds> */
int probe_pad(int argc, char **argv);    /* pad <seconds> <default|xinput|rawinput|wgi> */
int probe_focus(int argc, char **argv);  /* focus <program> [args...] */

#endif
