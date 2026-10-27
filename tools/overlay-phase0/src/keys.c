#include "keys.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

const probe_key probe_keys_table[] = {
    { "Up", 0x26, 103, 1 },          { "Down", 0x28, 108, 1 },     { "Left", 0x25, 105, 1 },
    { "Right", 0x27, 106, 1 },       { "Enter", 0x0D, 28, 0 },     { "Backspace", 0x08, 14, 0 },
    { "Escape", 0x1B, 1, 0 },        { "Space", 0x20, 57, 0 },     { "Tab", 0x09, 15, 0 },
    { "F", 0x46, 33, 0 },            { "P", 0x50, 25, 0 },         { "Alt", 0x12, 56, 0 },
    { "F4", 0x73, 62, 0 },           { "Apps", 0x5D, 127, 1 },     { "BrowserHome", 0xAC, 172, 1 },
    { "MediaPlayPause", 0xB3, 164, 1 },
};
const unsigned probe_keys_count = sizeof probe_keys_table / sizeof probe_keys_table[0];

const probe_key *probe_key_find(const char *name)
{
    for (unsigned i = 0; i < probe_keys_count; i++) {
        if (strcmp(probe_keys_table[i].name, name) == 0)
            return &probe_keys_table[i];
    }
    return NULL;
}

int probe_parse_chord(const char *arg, const probe_key **out, char *bad, size_t bad_cap)
{
    int n = 0;
    const char *part = arg;
    for (;;) {
        const char *plus = strchr(part, '+');
        size_t len = plus ? (size_t)(plus - part) : strlen(part);
        char name[64];
        snprintf(name, sizeof name, "%.*s", (int)len, part);
        const probe_key *k = probe_key_find(name);
        if (!k || n == PROBE_MAX_CHORD) {
            snprintf(bad, bad_cap, "%s", name);
            return -1;
        }
        out[n++] = k;
        if (!plus)
            return n;
        part = plus + 1;
    }
}

int probe_parse_count(const char *s, long max, long *out)
{
    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || errno || v < 0 || v > max)
        return -1;
    *out = v;
    return 0;
}

int probe_keys_main(int argc, char **argv, const probe_key_sender *sender)
{
    int i = 0, dry = 0;
    if (argc > 0 && strcmp(argv[0], "--dry-run") == 0) {
        dry = 1;
        i = 1;
    }
    long delay; /* never negative: that is Sleep(INFINITE) on Windows */
    if (argc - i < 2 || probe_parse_count(argv[i++], LONG_MAX, &delay) < 0) {
        fputs("usage: probe_spawn keys [--dry-run] <delay_ms> <chord>...\n", stderr);
        return 2;
    }
    const probe_key *chord[PROBE_MAX_CHORD];
    char bad[64], name[400], fields[512];
    for (int a = i; a < argc; a++) {
        if (probe_parse_chord(argv[a], chord, bad, sizeof bad) < 0) {
            probe_json_string(name, sizeof name, bad);
            snprintf(fields, sizeof fields, "\"event\":\"unknown_key\",\"name\":%s", name);
            probe_emit(fields);
            return 2;
        }
    }
    if (!dry) {
        if (sender->open && sender->open() != 0)
            return 1;
        sender->sleep_ms(delay);
    }
    for (int a = i; a < argc; a++) {
        int n = probe_parse_chord(argv[a], chord, bad, sizeof bad);
        for (int k = 0; k < n; k++) {
            if (!dry)
                sender->send(chord[k], 0);
            snprintf(fields, sizeof fields, "\"event\":\"key\",\"name\":\"%s\",\"vk\":%u,\"evdev\":%u,\"dry\":%s",
                     chord[k]->name, chord[k]->vk, chord[k]->evdev, dry ? "true" : "false");
            probe_emit(fields);
        }
        for (int k = n - 1; k >= 0; k--) {
            if (!dry)
                sender->send(chord[k], 1);
        }
        if (!dry)
            sender->sleep_ms(50);
    }
    if (!dry && sender->close)
        sender->close();
    return 0;
}
