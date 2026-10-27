/* probe_spawn: the Phase 0 spawner. One command per run; every result is a JSON line on stdout. */
#include <stdio.h>
#include <string.h>

#include "spawn.h"

void probe_lines_feed(probe_lines *l, const char *data, size_t n, probe_command_fn on_command, void *ctx)
{
    for (size_t i = 0; i < n; i++) {
        if (data[i] != '\n') {
            if (l->len + 1 < sizeof l->buf)
                l->buf[l->len++] = data[i];
            else
                l->overlong = 1;
            continue;
        }
        if (l->len && l->buf[l->len - 1] == '\r')
            l->len--;
        l->buf[l->len] = '\0';
        if (!l->overlong && strcmp(l->buf, "close") == 0)
            on_command(PROBE_CMD_CLOSE, ctx);
        else if (!l->overlong && strcmp(l->buf, "kill") == 0)
            on_command(PROBE_CMD_KILL, ctx);
        l->len = 0;
        l->overlong = 0;
    }
}

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

int probe_pad(int argc, char **argv) { (void)argc; (void)argv; return 2; }
int probe_focus(int argc, char **argv) { (void)argc; (void)argv; return 2; }
