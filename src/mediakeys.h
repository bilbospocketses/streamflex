// The desktop's media-key shortcuts as StreamFlex reads, saves and judges them: KDE Plasma's two
// kglobalaccel actions and GNOME's four gsettings keys, their names in config.ini's [Desktop Keys], the
// text each value is saved as, and whether a value holds a key. Pure: no SDL, no I/O, no globals, so
// tests/test_mediakeys.c builds it on its own; the backends that talk to the desktop live elsewhere.
#ifndef MEDIAKEYS_H
#define MEDIAKEYS_H

#include <stdbool.h>
#include <stddef.h>

#define MEDIAKEYS_KEYS_MAX 4      // GNOME's four keys; Plasma has two
#define MEDIAKEYS_VALUE_MAX 256   // The longest value text kept, with its terminator

typedef enum {
    MEDIAKEYS_NONE,
    MEDIAKEYS_PLASMA,
    MEDIAKEYS_GNOME
} MediaKeysDesktop;

// One desktop's shortcuts, each key's value as text: Plasma's key sequences ("16777347,0,0,0", joined by
// '|'), GNOME's `gsettings get` output as it came ("['XF86AudioNext']", "@as []"). Keys in
// mediakeys_key_name()'s order: the first is next track, the second previous track.
typedef struct {
    MediaKeysDesktop desktop;
    char values[MEDIAKEYS_KEYS_MAX][MEDIAKEYS_VALUE_MAX];
} MediaKeysState;

int mediakeys_key_count(MediaKeysDesktop desktop);                // 2 for Plasma, 4 for GNOME, 0 for none
const char *mediakeys_key_name(MediaKeysDesktop desktop, int i);  // The D-Bus action or gsettings key; NULL out of range
const char *mediakeys_config_key(MediaKeysDesktop desktop, int i);   // Its [Desktop Keys] key ("PlasmaNext"); NULL out of range

const char *mediakeys_desktop_name(MediaKeysDesktop desktop);         // "KDE Plasma" or "GNOME"; NULL for none
const char *mediakeys_desktop_short_name(MediaKeysDesktop desktop);   // "Plasma" or "GNOME"; NULL for none
const char *mediakeys_desktop_config_name(MediaKeysDesktop desktop);  // "plasma" or "gnome", [Desktop Keys] Desktop; NULL for none
MediaKeysDesktop mediakeys_desktop_from_config(const char *name);     // Desktop's value back; MEDIAKEYS_NONE for anything else

bool mediakeys_value_holds(MediaKeysDesktop desktop, const char *value);   // Whether one key's value binds a key at all
bool mediakeys_holds_next(const MediaKeysState *state);       // Whether the desktop holds next track
bool mediakeys_holds_previous(const MediaKeysState *state);   // Whether the desktop holds previous track

// Plasma's key sequences, 4 ints each, as value text; false (and "") when they do not fit in cap
bool mediakeys_plasma_format(const int (*seqs)[4], int nseq, char *out, size_t cap);
// Value text back to key sequences: how many (up to max), or -1 for text that is not that form
int mediakeys_plasma_parse(const char *text, int (*seqs)[4], int max);

bool mediakeys_value_safe(const char *value);   // Whether config.ini reads the value back exactly as written

#endif
