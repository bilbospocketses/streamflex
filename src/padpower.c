#include <string.h>
#include "padpower.h"
#include "chroma.h"

// The glyph's colors, both far from the shipped chroma key (#010101)
static const unsigned char AMBER[3] = { 0xFF, 0xB0, 0x00 };
static const unsigned char RED[3] = { 0xE8, 0x20, 0x20 };

// A function to work out the glyph a pad's report shows: only a pad on its battery with a percent
// shows one, from 40 % down (red from 15 %), and once shown it stays until the pad is above 45 %
PadPowerLevel padpower_level(PadPowerState state, int percent, PadPowerLevel shown)
{
    if (state != PADPOWER_ON_BATTERY || percent < 0 || percent > 100)
        return PADPOWER_HIDDEN;
    if (percent <= PADPOWER_CRITICAL_PERCENT)
        return PADPOWER_CRITICAL;
    if (percent <= PADPOWER_LOW_PERCENT || (shown != PADPOWER_HIDDEN && percent <= PADPOWER_HIDE_PERCENT))
        return PADPOWER_LOW;
    return PADPOWER_HIDDEN;
}

// A function to pick the pad the one glyph speaks for: the lowest of those whose glyph shows, the
// first of two at the same percent; -1 when none shows
int padpower_pick(const PadPower *pads, int count)
{
    int lowest = -1;
    for (int i = 0; i < count; i++) {
        if (pads[i].level != PADPOWER_HIDDEN && (lowest < 0 || pads[i].percent < pads[lowest].percent))
            lowest = i;
    }
    return lowest;
}

// A function to give a pad nothing has been read from: unknown, with no percent and no glyph
PadPower padpower_unknown(void)
{
    PadPower power = { PADPOWER_UNKNOWN, -1, PADCONN_UNKNOWN, PADPOWER_HIDDEN };
    return power;
}

// A function to name a power state for the log
const char *padpower_state_name(PadPowerState state)
{
    switch (state) {
        case PADPOWER_ERROR:
            return "error";
        case PADPOWER_ON_BATTERY:
            return "on battery";
        case PADPOWER_NO_BATTERY:
            return "no battery";
        case PADPOWER_CHARGING:
            return "charging";
        case PADPOWER_CHARGED:
            return "charged";
        default:
            return "unknown";
    }
}

// A function to name a connection for the log; SDL's invalid one reads as unknown
const char *padpower_connection_name(PadConnection connection)
{
    if (connection == PADCONN_WIRED)
        return "wired";
    return connection == PADCONN_WIRELESS ? "wireless" : "unknown";
}

// A function to give the glyph's color for its level, moved off the chroma key KEY (RGB) by
// chroma.h's rule when it lands within a step of it; NULL: no key to keep off
void padpower_color(PadPowerLevel level, const unsigned char *key, unsigned char rgb[3])
{
    const unsigned char *color = level == PADPOWER_CRITICAL ? RED : AMBER;
    unsigned char rgba[4] = { color[0], color[1], color[2], 0xFF };
    if (key != NULL)
        chroma_keep_off(rgba, 1, 1, 4, key[0], key[1], key[2]);
    memcpy(rgb, rgba, 3);
}
