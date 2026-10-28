// The desktop's media-key shortcuts as StreamFlex reads, saves and judges them: KDE Plasma's two
// kglobalaccel actions and GNOME's four gsettings keys, their names in config.ini's [Desktop Keys], the
// text each value is saved as, whether a value holds a key, and the edits that write [Desktop Keys].
// Pure: no SDL, no I/O, no globals, so tests/test_mediakeys.c builds it on its own; the backends that
// talk to the desktop live elsewhere.
#ifndef MEDIAKEYS_H
#define MEDIAKEYS_H

#include <stdbool.h>
#include <stddef.h>
#include "config_save.h"   // ConfigEdit only; nothing of config_save.c is linked

#define MEDIAKEYS_KEYS_MAX 4      // GNOME's four keys; Plasma has two
#define MEDIAKEYS_VALUE_MAX 256   // The longest value text kept, with its terminator
#define MEDIAKEYS_EDITS_MAX 7     // The most edits a builder below makes: Desktop and all six value keys

#define MEDIAKEYS_SECTION "Desktop Keys"   // config.ini's section, which is never a menu
#define MEDIAKEYS_KEY_ASK "Ask"            // "no" when the prompt is never to be shown; anything else asks
#define MEDIAKEYS_KEY_DESKTOP "Desktop"    // Whose shortcuts the value keys hold: "plasma" or "gnome"

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

// Plasma's key sequences, 4 ints each, as value text; false (and "") when they do not fit in cap. seqs is
// read only; it is not const so that a caller's int[n][4] passes without a cast (ISO C before C23)
bool mediakeys_plasma_format(int (*seqs)[4], int nseq, char *out, size_t cap);
// Value text back to key sequences: how many (up to max), or -1 for text that is not that form
int mediakeys_plasma_parse(const char *text, int (*seqs)[4], int max);

// Whether [Desktop Keys] can hold a value: no line break, no whitespace at either end, no ';' or '#' at its
// start or after whitespace, and short enough for one config.ini line under the longest key
bool mediakeys_value_safe(const char *value);

// The [Desktop Keys] edits for config_save_with(), each new key placed after the section's last (a missing
// section is added at the end of the file), and how many; -1, with nothing made, when out has room for
// fewer than they need. The save's edits point into `state`, which must outlive them: Desktop and that
// desktop's values, the other desktop's value keys removed; -1 too for a state with no desktop or a value
// mediakeys_value_safe() refuses
int mediakeys_save_edits(const MediaKeysState *state, ConfigEdit *out, int cap);
int mediakeys_forget_edits(ConfigEdit *out, int cap);   // Desktop and every value key removed; Ask stays
int mediakeys_never_edits(ConfigEdit *out, int cap);    // Ask=no

#endif
