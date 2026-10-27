#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "common.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>

int64_t probe_now_ms(void)
{
    FILETIME ft;
    ULARGE_INTEGER u;
    GetSystemTimeAsFileTime(&ft);
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return (int64_t)((u.QuadPart - 116444736000000000ULL) / 10000ULL);
}
#else
#include <time.h>

int64_t probe_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
#endif

void probe_emit(const char *fields)
{
    printf("{\"t\":%lld,%s}\n", (long long)probe_now_ms(), fields);
    fflush(stdout);
}

void probe_json_string(char *out, size_t cap, const char *s)
{
    size_t n = 0;
    if (cap < 3) {
        if (cap)
            out[0] = '\0';
        return;
    }
    out[n++] = '"';
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        char esc[8];
        size_t len = 1;
        if (*p == '"' || *p == '\\') {
            esc[0] = '\\';
            esc[1] = (char)*p;
            len = 2;
        } else if (*p < 0x20) {
            len = (size_t)snprintf(esc, sizeof esc, "\\u%04x", *p);
        } else {
            esc[0] = (char)*p;
        }
        if (n + len + 2 > cap) { /* room for the closing quote and the NUL */
            if ((*p & 0xC0) == 0x80) { /* mid-character: drop its leading bytes too, so out stays UTF-8 */
                while (n > 1 && ((unsigned char)out[n - 1] & 0xC0) == 0x80)
                    n--;
                if (n > 1 && (unsigned char)out[n - 1] >= 0xC0)
                    n--;
            }
            break;
        }
        memcpy(out + n, esc, len);
        n += len;
    }
    out[n++] = '"';
    out[n] = '\0';
}
