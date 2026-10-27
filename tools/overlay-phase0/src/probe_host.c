/* Phase 0 probe native-messaging host: logs every frame as one JSON line and acknowledges it. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#define PROBE_PID ((unsigned long)GetCurrentProcessId())
#define PROBE_SEP "\\"
#else
#include <sys/stat.h>
#include <unistd.h>
#define PROBE_PID ((unsigned long)getpid())
#define PROBE_SEP "/"
#endif

#define PROBE_MAX_IN (64u * 1024u * 1024u)

static FILE *open_log(void)
{
    char dir[1024];
    char path[1100];
    const char *override = getenv("CPTK_PROBE_LOG_DIR");
#ifdef _WIN32
    const char *base = getenv("LOCALAPPDATA");
    if (override)
        snprintf(dir, sizeof dir, "%s", override);
    else
        snprintf(dir, sizeof dir, "%s\\cptk-probe", base ? base : ".");
    CreateDirectoryA(dir, NULL);
#else
    const char *base = getenv("HOME");
    if (override)
        snprintf(dir, sizeof dir, "%s", override);
    else
        snprintf(dir, sizeof dir, "%s/.cptk-probe", base ? base : ".");
    mkdir(dir, 0700);
#endif
    snprintf(path, sizeof path, "%s" PROBE_SEP "probe.log", dir);
    return fopen(path, "ab");
}

static void log_start(FILE *f, const char *event)
{
    fprintf(f, "{\"t\":%lld,\"pid\":%lu,\"event\":\"%s\"", (long long)probe_now_ms(), PROBE_PID, event);
}

int main(int argc, char **argv)
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    FILE *log = open_log();
    if (!log)
        return 2;

    log_start(log, "start");
    fputs(",\"argv\":[", log);
    for (int i = 0; i < argc; i++) {
        char arg[4096];
        probe_json_string(arg, sizeof arg, argv[i]);
        if (i)
            fputc(',', log);
        fputs(arg, log);
    }
    fputs("]}\n", log);
    fflush(log);

    unsigned long seq = 0;
    for (;;) {
        uint32_t len;
        if (fread(&len, sizeof len, 1, stdin) != 1)
            break; /* the port closed */
        if (len == 0 || len > PROBE_MAX_IN) {
            log_start(log, len == 0 ? "empty" : "oversize");
            if (len)
                fprintf(log, ",\"len\":%lu", (unsigned long)len);
            fputs("}\n", log);
            fclose(log);
            return 1;
        }
        char *buf = malloc(len);
        if (!buf || fread(buf, 1, len, stdin) != len) {
            free(buf);
            break; /* truncated: the browser went away mid-frame */
        }
        seq++;
        log_start(log, "msg");
        fprintf(log, ",\"seq\":%lu,\"body\":", seq);
        fwrite(buf, 1, len, log);
        fputs("}\n", log);
        fflush(log);
        free(buf);

        char reply[96];
        int n = snprintf(reply, sizeof reply, "{\"type\":\"ack\",\"seq\":%lu,\"pid\":%lu}", seq, PROBE_PID);
        uint32_t rlen = (uint32_t)n;
        fwrite(&rlen, sizeof rlen, 1, stdout);
        fwrite(reply, 1, (size_t)n, stdout);
        fflush(stdout);
    }
    log_start(log, "end");
    fputs("}\n", log);
    fclose(log);
    return 0;
}
