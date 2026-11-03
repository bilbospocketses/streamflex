/* probe_spawn text, Windows: each UTF-16 code unit as a KEYEVENTF_UNICODE key press and release, whatever the
   keyboard layout. A character outside the BMP is its two surrogates, sent in one SendInput call so no other input
   lands between them. */
#include <windows.h>
#include <shellapi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "spawn.h"
#include "text.h"

/* main's argv is in the ANSI code page, so text it cannot hold arrives as '?'. The command line itself is UTF-16:
   argument i of text is the command line's argument wargc - argc + i, since argv is the tail of main's. The buffer is
   sized to the argument, so a long one reaches the character count's too_long refusal rather than reading as not
   UTF-8; NULL means it is not valid UTF-16 (a lone surrogate) or memory ran out. */
static const char *arg_utf8(int argc, char **argv, int i)
{
    static char *buf[2];
    int wargc = 0;
    (void)argv;
    wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    int at = wargc - argc + i;
    const char *res = NULL;
    if (wargv && at >= 0 && at < wargc) {
        int need = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wargv[at], -1, NULL, 0, NULL, NULL);
        char *out = need > 0 ? realloc(buf[i & 1], (size_t)need) : NULL;
        if (out) {
            buf[i & 1] = out;
            if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wargv[at], -1, out, need, NULL, NULL))
                res = out;
        }
    }
    if (wargv)
        LocalFree(wargv);
    return res;
}

static int units_of(uint32_t cp, WORD units[2])
{
    if (cp < 0x10000) {
        units[0] = (WORD)cp;
        return 1;
    }
    cp -= 0x10000;
    units[0] = (WORD)(0xD800 | (cp >> 10));
    units[1] = (WORD)(0xDC00 | (cp & 0x3FF));
    return 2;
}

static int prepare(const char *layout)
{
    probe_emit(layout ? "\"event\":\"layout\",\"source\":\"not_needed\",\"ignored\":\"--layout\","
                        "\"note\":\"KEYEVENTF_UNICODE sends each UTF-16 code unit, whatever the layout\""
                      : "\"event\":\"layout\",\"source\":\"not_needed\","
                        "\"note\":\"KEYEVENTF_UNICODE sends each UTF-16 code unit, whatever the layout\"");
    return 0;
}

static int plan(uint32_t cp, char *detail, size_t cap)
{
    WORD u[2];
    if (units_of(cp, u) == 1)
        snprintf(detail, cap, "\"units\":[%u]", (unsigned)u[0]);
    else
        snprintf(detail, cap, "\"units\":[%u,%u]", (unsigned)u[0], (unsigned)u[1]);
    return 1;
}

static int type_char(uint32_t cp)
{
    WORD u[2];
    int n = units_of(cp, u);
    INPUT in[4];
    memset(in, 0, sizeof in);
    for (int k = 0; k < n; k++) {
        in[2 * k].type = in[2 * k + 1].type = INPUT_KEYBOARD;
        in[2 * k].ki.wScan = in[2 * k + 1].ki.wScan = u[k];
        in[2 * k].ki.dwFlags = KEYEVENTF_UNICODE;
        in[2 * k + 1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    }
    UINT want = (UINT)(2 * n), sent = SendInput(want, in, sizeof in[0]);
    if (sent == want)
        return 1;
    char fields[160];
    snprintf(fields, sizeof fields, "\"event\":\"send_failed\",\"cp\":\"U+%04X\",\"sent\":%u,\"of\":%u,\"error\":%lu",
             (unsigned)cp, sent, want, GetLastError());
    probe_emit(fields);
    return 0;
}

static void sleep_ms(long ms)
{
    Sleep((DWORD)ms);
}

const probe_text_platform probe_text_platform_impl = { arg_utf8, prepare, plan, NULL, type_char, sleep_ms, NULL };

int probe_text(int argc, char **argv)
{
    return probe_text_main(argc, argv, &probe_text_platform_impl);
}
