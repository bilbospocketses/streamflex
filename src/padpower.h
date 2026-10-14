// The gamepad battery glyph's rules (spec section 7): the level a pad's power report shows, and the
// pad the one glyph speaks for. Pure: no SDL, so tests/test_padpower.c builds it on its own (with
// chroma.c, whose rule keeps the glyph's color off a chroma key). The state and connection enums
// hold SDL's own values, which launcher.c checks when it compiles.
#ifndef PADPOWER_H
#define PADPOWER_H

#define PADPOWER_LOW_PERCENT 40        // On its battery at or below this, a pad shows the glyph, amber...
#define PADPOWER_CRITICAL_PERCENT 15   // ...and red at or below this
#define PADPOWER_HIDE_PERCENT 45       // Once shown, it hides only above this, so a pad that reports in
                                       // steps of ten does not flicker at the edge

typedef enum {   // SDL_PowerState's values
    PADPOWER_ERROR = -1,
    PADPOWER_UNKNOWN,
    PADPOWER_ON_BATTERY,
    PADPOWER_NO_BATTERY,
    PADPOWER_CHARGING,
    PADPOWER_CHARGED
} PadPowerState;

typedef enum {   // SDL_JoystickConnectionState's values
    PADCONN_INVALID = -1,
    PADCONN_UNKNOWN,
    PADCONN_WIRED,
    PADCONN_WIRELESS
} PadConnection;

typedef enum {
    PADPOWER_HIDDEN,
    PADPOWER_LOW,
    PADPOWER_CRITICAL
} PadPowerLevel;

// One pad's power, as StreamFlex holds it
typedef struct {
    PadPowerState state;
    int percent;               // -1: the pad gave none
    PadConnection connection;
    PadPowerLevel level;       // What padpower_level() last gave this pad
} PadPower;

PadPowerLevel padpower_level(PadPowerState state, int percent, PadPowerLevel shown);   // shown: the pad's level until now
int padpower_pick(const PadPower *pads, int count);   // The lowest pad whose glyph shows; -1 for none
PadPower padpower_unknown(void);                      // A pad nothing has been read from
const char *padpower_state_name(PadPowerState state);             // For the log: "on battery", "charging", ...
const char *padpower_connection_name(PadConnection connection);   // "wired", "wireless" or "unknown"
void padpower_color(PadPowerLevel level, const unsigned char *key, unsigned char rgb[3]);   // key: an RGB to keep off, or NULL

#endif
