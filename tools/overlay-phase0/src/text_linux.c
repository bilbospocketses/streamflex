/* probe_spawn text, Linux. This version knows no keyboard layouts, so after the shared checks it types nothing and
   says so. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>

#include "common.h"
#include "spawn.h"
#include "text.h"

static const char *arg_utf8(int argc, char **argv, int i)
{
    (void)argc;
    return argv[i];
}

static int prepare(const char *layout)
{
    (void)layout;
    probe_emit("\"event\":\"text_failed\",\"step\":\"not_built\",\"need\":\"libxkbcommon\"");
    fputs("probe_spawn text: built without keyboard layouts; nothing was typed\n", stderr);
    return 1;
}

static int plan(uint32_t cp, char *detail, size_t cap)
{
    (void)cp;
    snprintf(detail, cap, "no layout");
    return 0;
}

static int type_char(uint32_t cp)
{
    (void)cp;
    return 0;
}

static void sleep_ms(long ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

const probe_text_platform probe_text_platform_impl = { arg_utf8, prepare, plan, NULL, type_char, sleep_ms, NULL };

int probe_text(int argc, char **argv)
{
    return probe_text_main(argc, argv, &probe_text_platform_impl);
}
