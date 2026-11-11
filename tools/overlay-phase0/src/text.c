/* probe_spawn text, the part every platform shares: arguments, decoding, the refusals, and plan-then-type. Every
   character is planned before the first one is typed, so the untyped ones are named before typing starts. */
#include "text.h"

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "keys.h"
#include "spawn.h"

#define TEXT_USAGE \
    "usage: probe_spawn text [--dry-run] [--layout <layout>[(<variant>)]] <delay_ms> (<string> | --file <path>)\n"

int probe_utf8_decode(const char *s, size_t n, uint32_t *out, int cap)
{
    const unsigned char *p = (const unsigned char *)s, *end = p + n;
    int count = 0;
    while (p < end) {
        unsigned char c = *p;
        uint32_t cp;
        int len;
        if (c < 0x80) {
            cp = c;
            len = 1;
        } else if (c >= 0xC2 && c <= 0xDF) {
            cp = c & 0x1Fu;
            len = 2;
        } else if (c >= 0xE0 && c <= 0xEF) {
            cp = c & 0x0Fu;
            len = 3;
        } else if (c >= 0xF0 && c <= 0xF4) {
            cp = c & 0x07u;
            len = 4;
        } else {
            return -1;
        }
        if (end - p < len)
            return -1;
        for (int k = 1; k < len; k++) {
            if ((p[k] & 0xC0) != 0x80)
                return -1;
            cp = (cp << 6) | (p[k] & 0x3Fu);
        }
        if ((len == 3 && cp < 0x800) || (len == 4 && (cp < 0x10000 || cp > 0x10FFFF)) || (cp >= 0xD800 && cp <= 0xDFFF))
            return -1;
        if (count == cap)
            return -2;
        out[count++] = cp;
        p += len;
    }
    return count;
}

int probe_utf8_encode(uint32_t cp, char *out)
{
    int n;
    if (cp < 0x80) {
        out[0] = (char)cp;
        n = 1;
    } else if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        n = 2;
    } else if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        n = 3;
    } else {
        out[0] = (char)(0xF0 | (cp >> 18));
        out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (char)(0x80 | (cp & 0x3F));
        n = 4;
    }
    out[n] = '\0';
    return n;
}

int probe_text_is_control(uint32_t cp)
{
    return cp < 0x20 || (cp >= 0x7F && cp <= 0x9F);
}

/* Prints text_refused and a stderr line; returns the refusal's exit code. */
static int refuse(const char *reason, const char *extra)
{
    char fields[200];
    snprintf(fields, sizeof fields, "\"event\":\"text_refused\",\"reason\":\"%s\"%s", reason, extra ? extra : "");
    probe_emit(fields);
    fprintf(stderr, "probe_spawn text: refused (%s); nothing was typed\n", reason);
    return 2;
}

/* Reads a UTF-8 path into buf, dropping a leading byte order mark and one trailing line ending (an editor's), and
   NUL-terminates it. Returns the length, -1 when the file cannot be read, -2 when it holds cap or more bytes. */
static long read_text_file(const char *path, char *buf, size_t cap)
{
    FILE *f = probe_fopen_utf8(path);
    if (!f)
        return -1;
    size_t n = fread(buf, 1, cap - 1, f);
    int more = n == cap - 1 && fgetc(f) != EOF;
    int err = ferror(f);
    fclose(f);
    if (err)
        return -1;
    if (more)
        return -2;
    size_t start = n >= 3 && memcmp(buf, "\xEF\xBB\xBF", 3) == 0 ? 3 : 0;
    if (n > start && buf[n - 1] == '\n') {
        n--;
        if (n > start && buf[n - 1] == '\r')
            n--;
    }
    memmove(buf, buf + start, n - start);
    buf[n - start] = '\0';
    return (long)(n - start);
}

int probe_text_main(int argc, char **argv, const probe_text_platform *p)
{
    int i = 0, dry = 0;
    const char *layout = NULL;
    while (i < argc) {
        if (strcmp(argv[i], "--dry-run") == 0) {
            dry = 1;
            i++;
        } else if (strcmp(argv[i], "--layout") == 0 && i + 1 < argc) {
            layout = argv[i + 1];
            i += 2;
        } else {
            break;
        }
    }
    long delay; /* never negative: that is Sleep(INFINITE) on Windows */
    if (argc - i < 2 || probe_parse_count(argv[i], PROBE_KEYS_MAX_DELAY_MS, &delay) < 0) {
        fputs(TEXT_USAGE, stderr);
        return 2;
    }
    i++;
    static char buf[PROBE_TEXT_MAX * 4 + 8];
    char too_long[32];
    snprintf(too_long, sizeof too_long, ",\"max\":%d", PROBE_TEXT_MAX);
    const char *text;
    size_t len;
    if (strcmp(argv[i], "--file") == 0) {
        if (argc - i != 2) {
            fputs(TEXT_USAGE, stderr);
            return 2;
        }
        const char *path = p->arg(argc, argv, i + 1);
        long got = path ? read_text_file(path, buf, sizeof buf) : -1;
        if (got == -1)
            return refuse("file_unreadable", NULL);
        if (got == -2)
            return refuse("too_long", too_long);
        text = buf;
        len = (size_t)got;
    } else {
        if (argc - i != 1) {
            fputs(TEXT_USAGE, stderr);
            return 2;
        }
        text = p->arg(argc, argv, i);
        if (!text)
            return refuse("not_utf8", NULL);
        len = strlen(text);
    }
    static uint32_t cps[PROBE_TEXT_MAX];
    int n = probe_utf8_decode(text, len, cps, PROBE_TEXT_MAX);
    if (n == -1)
        return refuse("not_utf8", NULL);
    if (n == -2)
        return refuse("too_long", too_long);
    if (n == 0)
        return refuse("empty", NULL);
    for (int k = 0; k < n; k++) {
        if (probe_text_is_control(cps[k])) {
            char extra[64];
            snprintf(extra, sizeof extra, ",\"index\":%d,\"cp\":\"U+%04X\"", k, (unsigned)cps[k]);
            return refuse("control", extra);
        }
    }
    int rc = p->prepare(layout);
    if (rc)
        return rc;

    static char detail[PROBE_TEXT_MAX][320]; /* room for a four-key dead-key or compose sequence on Linux */
    static int typable[PROBE_TEXT_MAX];
    char ch[8], chj[32], why[700], fields[900];
    int untyped = 0;
    for (int k = 0; k < n; k++) {
        typable[k] = p->plan(cps[k], detail[k], sizeof detail[k]);
        if (typable[k])
            continue;
        untyped++;
        probe_utf8_encode(cps[k], ch);
        probe_json_string(chj, sizeof chj, ch);
        probe_json_string(why, sizeof why, detail[k]);
        snprintf(fields, sizeof fields, "\"event\":\"untyped\",\"index\":%d,\"char\":%s,\"cp\":\"U+%04X\",\"why\":%s", k,
                 chj, (unsigned)cps[k], why);
        probe_emit(fields);
        fprintf(stderr, "probe_spawn text: cannot type U+%04X (%s): %s\n", (unsigned)cps[k], ch, detail[k]);
    }
    if (!dry) {
        if (p->open && p->open() != 0)
            return 1;
        p->sleep_ms(delay);
    }
    int failed = 0, typed = 0;
    for (int k = 0; k < n; k++) {
        if (!typable[k])
            continue;
        int sent = 0;
        if (!dry) {
            sent = p->type(cps[k]);
            if (sent)
                typed++;
            else
                failed = 1;
            p->sleep_ms(20); /* one character at a time, as a person types; not a timing check */
        }
        probe_utf8_encode(cps[k], ch);
        probe_json_string(chj, sizeof chj, ch);
        /* %.*s bounds the detail to its own row, which plan always NUL-terminates inside; GCC cannot see that */
        snprintf(fields, sizeof fields, "\"event\":\"char\",\"index\":%d,\"char\":%s,\"cp\":\"U+%04X\",%.*s,\"dry\":%s,\"sent\":%s",
                 k, chj, (unsigned)cps[k], (int)sizeof detail[0] - 1, detail[k], dry ? "true" : "false",
                 sent ? "true" : "false");
        probe_emit(fields);
    }
    if (!dry && p->close)
        p->close();
    snprintf(fields, sizeof fields,
             "\"event\":\"text_end\",\"chars\":%d,\"typable\":%d,\"typed\":%d,\"untyped\":%d,\"dry\":%s", n,
             n - untyped, typed, untyped, dry ? "true" : "false");
    probe_emit(fields);
    return failed ? 1 : untyped ? 3 : 0;
}
