/* probe_spawn: the Phase 0 spawner. One command per run; every result is a JSON line on stdout. */
#include <stdio.h>
#include <string.h>

#include "spawn.h"

static int usage(void)
{
    fputs("usage: probe_spawn run|keys|keylog|pad|focus ...\n", stderr);
    return 2;
}

int main(int argc, char **argv)
{
    if (argc < 2)
        return usage();
    const char *cmd = argv[1];
    if (strcmp(cmd, "run") == 0)
        return probe_run(argc - 2, argv + 2);
    if (strcmp(cmd, "keys") == 0)
        return probe_keys(argc - 2, argv + 2);
    if (strcmp(cmd, "keylog") == 0)
        return probe_keylog(argc - 2, argv + 2);
    if (strcmp(cmd, "pad") == 0)
        return probe_pad(argc - 2, argv + 2);
    if (strcmp(cmd, "focus") == 0)
        return probe_focus(argc - 2, argv + 2);
    return usage();
}

int probe_keys(int argc, char **argv) { (void)argc; (void)argv; return 2; }
int probe_keylog(int argc, char **argv) { (void)argc; (void)argv; return 2; }
int probe_pad(int argc, char **argv) { (void)argc; (void)argv; return 2; }
int probe_focus(int argc, char **argv) { (void)argc; (void)argv; return 2; }
