#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "mediakeys.h"

// Each desktop's keys, next track first and previous track second: kglobalaccel's actions in Plasma's
// "mediacontrol" component, and the keys of GNOME's org.gnome.settings-daemon.plugins.media-keys, whose
// static keys hold the plain media keys apart from the user's own
static const char *const PLASMA_KEYS[] = { "nextmedia", "previousmedia" };
static const char *const PLASMA_CONFIG_KEYS[] = { "PlasmaNext", "PlasmaPrevious" };
static const char *const GNOME_KEYS[] = { "next", "previous", "next-static", "previous-static" };
static const char *const GNOME_CONFIG_KEYS[] = { "GnomeNext", "GnomePrevious", "GnomeNextStatic", "GnomePreviousStatic" };

#define PLASMA_KEY_COUNT 2
#define GNOME_KEY_COUNT 4

// A function to tell whether a byte is whitespace as config.ini reads it
static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

// A function to give a desktop's key count
int mediakeys_key_count(MediaKeysDesktop desktop)
{
    switch (desktop) {
    case MEDIAKEYS_PLASMA:
        return PLASMA_KEY_COUNT;
    case MEDIAKEYS_GNOME:
        return GNOME_KEY_COUNT;
    default:
        return 0;
    }
}

// A function to give a desktop's i-th key as the desktop names it; NULL out of range
const char *mediakeys_key_name(MediaKeysDesktop desktop, int i)
{
    if (i < 0 || i >= mediakeys_key_count(desktop))
        return NULL;
    return desktop == MEDIAKEYS_PLASMA ? PLASMA_KEYS[i] : GNOME_KEYS[i];
}

// A function to give a desktop's i-th key as [Desktop Keys] names it; NULL out of range
const char *mediakeys_config_key(MediaKeysDesktop desktop, int i)
{
    if (i < 0 || i >= mediakeys_key_count(desktop))
        return NULL;
    return desktop == MEDIAKEYS_PLASMA ? PLASMA_CONFIG_KEYS[i] : GNOME_CONFIG_KEYS[i];
}

// A function to give a desktop's name, as the prompt and the log say it
const char *mediakeys_desktop_name(MediaKeysDesktop desktop)
{
    return desktop == MEDIAKEYS_PLASMA ? "KDE Plasma" : desktop == MEDIAKEYS_GNOME ? "GNOME" : NULL;
}

// A function to give a desktop's short name, which the prompt makes possessive ("Plasma's")
const char *mediakeys_desktop_short_name(MediaKeysDesktop desktop)
{
    return desktop == MEDIAKEYS_PLASMA ? "Plasma" : desktop == MEDIAKEYS_GNOME ? "GNOME" : NULL;
}

// A function to give a desktop's name as [Desktop Keys] Desktop stores it
const char *mediakeys_desktop_config_name(MediaKeysDesktop desktop)
{
    return desktop == MEDIAKEYS_PLASMA ? "plasma" : desktop == MEDIAKEYS_GNOME ? "gnome" : NULL;
}

// A function to tell whether two texts are the same, ASCII capitals or not
static bool same_name(const char *a, const char *b)
{
    for (; *a != '\0' && *b != '\0'; a++, b++) {
        char x = (*a >= 'A' && *a <= 'Z') ? (char) (*a - 'A' + 'a') : *a;
        char y = (*b >= 'A' && *b <= 'Z') ? (char) (*b - 'A' + 'a') : *b;
        if (x != y)
            return false;
    }
    return *a == *b;
}

// A function to read [Desktop Keys] Desktop back, in any case a hand edit gave it
MediaKeysDesktop mediakeys_desktop_from_config(const char *name)
{
    if (name == NULL)
        return MEDIAKEYS_NONE;
    if (same_name(name, "plasma"))
        return MEDIAKEYS_PLASMA;
    if (same_name(name, "gnome"))
        return MEDIAKEYS_GNOME;
    return MEDIAKEYS_NONE;
}

// A function to read one int of Plasma's value text at *p: an optional '-' and decimal digits, in range,
// leaving *p past it; false for anything else (strtol would let spaces and a '+' through)
static bool parse_int(const char **p, int *out)
{
    const char *s = *p;
    bool negative = *s == '-';
    if (negative)
        s++;
    if (*s < '0' || *s > '9')
        return false;
    long long n = 0;
    for (; *s >= '0' && *s <= '9'; s++) {
        n = n * 10 + (*s - '0');
        if (n > (long long) INT_MAX + 1)
            return false;
    }
    if (negative)
        n = -n;
    if (n < INT_MIN || n > INT_MAX)
        return false;
    *out = (int) n;
    *p = s;
    return true;
}

// A function to read Plasma's value text into key sequences: each 4 ints joined by ',', the sequences
// joined by '|', "" for none. Gives how many, or -1 for text in any other form or with more than max
int mediakeys_plasma_parse(const char *text, int (*seqs)[4], int max)
{
    if (text == NULL)
        return -1;
    if (text[0] == '\0')
        return 0;
    int count = 0;
    const char *p = text;
    for (;;) {
        int seq[4];
        for (int k = 0; k < 4; k++) {
            if (k > 0 && *p++ != ',')
                return -1;
            if (!parse_int(&p, &seq[k]))
                return -1;
        }
        if (count >= max)
            return -1;
        memcpy(seqs[count++], seq, sizeof(seq));
        if (*p == '\0')
            return count;
        if (*p++ != '|')
            return -1;     // A fifth int, or anything but the next sequence; a trailing '|' fails on its empty sequence
    }
}

// A function to write key sequences as Plasma's value text; false, with "" written, when it does not fit
bool mediakeys_plasma_format(const int (*seqs)[4], int nseq, char *out, size_t cap)
{
    if (out == NULL || cap == 0)
        return false;
    out[0] = '\0';
    size_t used = 0;
    for (int i = 0; i < nseq; i++) {
        int n = snprintf(out + used, cap - used, "%s%d,%d,%d,%d", i > 0 ? "|" : "", seqs[i][0], seqs[i][1],
                         seqs[i][2], seqs[i][3]);
        if (n < 0 || (size_t) n >= cap - used) {
            out[0] = '\0';
            return false;
        }
        used += (size_t) n;
    }
    return true;
}

// A function to skip whitespace
static const char *skip_space(const char *p)
{
    while (is_space(*p))
        p++;
    return p;
}

// A function to tell whether GNOME's value text, a GVariant string array as `gsettings get` prints it,
// has a string that is not empty: "['XF86AudioNext', '']" does, "['']", "[]" and "@as []" do not. A
// string is quoted with ' (or ", when GLib printed one holding a ') and a backslash escapes the next
// character. Text in any other form holds nothing
static bool gnome_holds(const char *value)
{
    const char *p = skip_space(value);
    if (strncmp(p, "@as", 3) == 0 && is_space(p[3]))
        p = skip_space(p + 3);    // The type GLib names before an empty array
    if (*p++ != '[')
        return false;
    p = skip_space(p);
    if (*p == ']')
        return false;             // An empty array
    bool holds = false;
    for (;;) {
        char quote = *p++;
        if (quote != '\'' && quote != '"')
            return false;
        bool empty = true;
        while (*p != quote) {
            if (*p == '\0')
                return false;
            if (*p == '\\' && *++p == '\0')
                return false;
            empty = false;
            p++;
        }
        holds = holds || !empty;
        p = skip_space(p + 1);
        if (*p == ']')
            return *skip_space(p + 1) == '\0' && holds;
        if (*p++ != ',')
            return false;
        p = skip_space(p);
    }
}

// A function to tell whether one key's value binds a key: Plasma, a sequence whose first int is not 0;
// GNOME, a string in the array that is not empty. Text in neither form binds nothing
bool mediakeys_value_holds(MediaKeysDesktop desktop, const char *value)
{
    if (value == NULL)
        return false;
    if (desktop == MEDIAKEYS_PLASMA) {
        int seqs[MEDIAKEYS_VALUE_MAX][4];
        int count = mediakeys_plasma_parse(value, seqs, MEDIAKEYS_VALUE_MAX);
        for (int i = 0; i < count; i++) {
            if (seqs[i][0] != 0)
                return true;
        }
        return false;
    }
    return desktop == MEDIAKEYS_GNOME && gnome_holds(value);
}

// A function to tell whether either of two keys holds: the first, or (GNOME) its static twin two along
static bool holds_either(const MediaKeysState *state, int key)
{
    if (state == NULL)
        return false;
    if (mediakeys_value_holds(state->desktop, state->values[key]))
        return true;
    return state->desktop == MEDIAKEYS_GNOME && mediakeys_value_holds(state->desktop, state->values[key + 2]);
}

// A function to tell whether the desktop holds next track: Plasma's nextmedia, GNOME's next or next-static
bool mediakeys_holds_next(const MediaKeysState *state)
{
    return holds_either(state, 0);
}

// A function to tell whether the desktop holds previous track: Plasma's previousmedia, GNOME's previous or
// previous-static
bool mediakeys_holds_previous(const MediaKeysState *state)
{
    return holds_either(state, 1);
}

// A function to tell whether config.ini reads a value back exactly as written: not when it has a line
// break, starts or ends with whitespace (dropped), or has a ';' or '#' at its start or after whitespace
// (read as a comment)
bool mediakeys_value_safe(const char *value)
{
    if (value == NULL)
        return false;
    size_t length = strlen(value);
    if (strchr(value, '\n') != NULL || strchr(value, '\r') != NULL)
        return false;
    if (length > 0 && (is_space(value[0]) || is_space(value[length - 1])))
        return false;
    for (size_t i = 0; i < length; i++) {
        if ((value[i] == ';' || value[i] == '#') && (i == 0 || is_space(value[i - 1])))
            return false;
    }
    return true;
}
