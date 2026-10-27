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
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#define PROBE_PID ((unsigned long)getpid())
#define PROBE_SEP "/"
#endif

#define PROBE_MAX_IN (64u * 1024u * 1024u)

static FILE *open_log(char *path, size_t cap)
{
    char dir[1024];
    const char *override = getenv("CPTK_PROBE_LOG_DIR");
#ifdef _WIN32
    const char *base = getenv("LOCALAPPDATA");
    if (override && *override)
        snprintf(dir, sizeof dir, "%s", override);
    else
        snprintf(dir, sizeof dir, "%s\\cptk-probe", base ? base : ".");
    CreateDirectoryA(dir, NULL);
#else
    const char *base = getenv("HOME");
    if (override && *override)
        snprintf(dir, sizeof dir, "%s", override);
    else
        snprintf(dir, sizeof dir, "%s/.cptk-probe", base ? base : ".");
    mkdir(dir, 0700);
#endif
    snprintf(path, cap, "%s" PROBE_SEP "probe.log", dir);
    return fopen(path, "ab");
}

static void log_start(FILE *f, const char *event)
{
    fprintf(f, "{\"t\":%lld,\"pid\":%lu,\"event\":\"%s\"", (long long)probe_now_ms(), PROBE_PID, event);
}

static void log_arg(FILE *f, int i, const char *s)
{
    char arg[4096];
    probe_json_string(arg, sizeof arg, s);
    if (i)
        fputc(',', f);
    fputs(arg, f);
}

/* The argv field. On Windows argv is in the ANSI code page, not UTF-8, so it is rebuilt from the wide command line. */
static void log_argv(FILE *f, int argc, char **argv)
{
    fputs(",\"argv\":[", f);
#ifdef _WIN32
    int wargc = 0;
    LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (wargv) {
        for (int i = 0; i < wargc; i++) {
            int len = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);
            char *utf8 = len > 0 ? malloc((size_t)len) : NULL;
            if (utf8 && WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, utf8, len, NULL, NULL) > 0)
                log_arg(f, i, utf8);
            else
                log_arg(f, i, "");
            free(utf8);
        }
        LocalFree(wargv);
        fputs("]}\n", f);
        return;
    }
#endif
    for (int i = 0; i < argc; i++)
        log_arg(f, i, argv[i]);
    fputs("]}\n", f);
}

int main(int argc, char **argv)
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#else
    signal(SIGPIPE, SIG_IGN); /* a browser that closes the port early must not cost the end line */
#endif
    char path[1100];
    FILE *log = open_log(path, sizeof path);
    if (!log) {
        fprintf(stderr, "probe_host: cannot open %s\n", path);
        return 2;
    }

    log_start(log, "start");
    log_argv(log, argc, argv);
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
