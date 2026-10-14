#include "check.h"
#include "padpower.h"

// A function to make one pad's power for padpower_pick()
static PadPower pad(PadPowerState state, int percent, PadPowerLevel level)
{
    PadPower power = { state, percent, PADCONN_WIRELESS, level };
    return power;
}

// A function to test a pad on its battery from no glyph: amber at or below 40 %, red at or below
// 15 %, nothing above 40 %; a percent SDL did not give (-1) or one past 100 shows nothing
static void test_on_battery(void)
{
    CHECK_INT(padpower_level(PADPOWER_ON_BATTERY, 100, PADPOWER_HIDDEN), PADPOWER_HIDDEN);
    CHECK_INT(padpower_level(PADPOWER_ON_BATTERY, 46, PADPOWER_HIDDEN), PADPOWER_HIDDEN);
    CHECK_INT(padpower_level(PADPOWER_ON_BATTERY, 45, PADPOWER_HIDDEN), PADPOWER_HIDDEN);
    CHECK_INT(padpower_level(PADPOWER_ON_BATTERY, 41, PADPOWER_HIDDEN), PADPOWER_HIDDEN);
    CHECK_INT(padpower_level(PADPOWER_ON_BATTERY, 40, PADPOWER_HIDDEN), PADPOWER_LOW);
    CHECK_INT(padpower_level(PADPOWER_ON_BATTERY, 16, PADPOWER_HIDDEN), PADPOWER_LOW);
    CHECK_INT(padpower_level(PADPOWER_ON_BATTERY, 15, PADPOWER_HIDDEN), PADPOWER_CRITICAL);
    CHECK_INT(padpower_level(PADPOWER_ON_BATTERY, 0, PADPOWER_HIDDEN), PADPOWER_CRITICAL);
    CHECK_INT(padpower_level(PADPOWER_ON_BATTERY, -1, PADPOWER_HIDDEN), PADPOWER_HIDDEN);
    CHECK_INT(padpower_level(PADPOWER_ON_BATTERY, -1, PADPOWER_LOW), PADPOWER_HIDDEN);
    CHECK_INT(padpower_level(PADPOWER_ON_BATTERY, 101, PADPOWER_LOW), PADPOWER_HIDDEN);
}

// A function to test the steps pads report in: an Xbox pad's four (10, 40, 70, 100), and a pad
// that reports in tens, walked down and back up
static void test_steps(void)
{
    CHECK_INT(padpower_level(PADPOWER_ON_BATTERY, 100, PADPOWER_HIDDEN), PADPOWER_HIDDEN);
    CHECK_INT(padpower_level(PADPOWER_ON_BATTERY, 70, PADPOWER_HIDDEN), PADPOWER_HIDDEN);
    CHECK_INT(padpower_level(PADPOWER_ON_BATTERY, 40, PADPOWER_HIDDEN), PADPOWER_LOW);
    CHECK_INT(padpower_level(PADPOWER_ON_BATTERY, 10, PADPOWER_LOW), PADPOWER_CRITICAL);
    PadPowerLevel level = PADPOWER_HIDDEN;
    static const int tens[] = { 50, 40, 30, 20, 10, 20, 30, 40, 50 };
    static const PadPowerLevel shown[] = { PADPOWER_HIDDEN, PADPOWER_LOW, PADPOWER_LOW, PADPOWER_LOW,
        PADPOWER_CRITICAL, PADPOWER_LOW, PADPOWER_LOW, PADPOWER_LOW, PADPOWER_HIDDEN };
    for (int i = 0; i < (int) (sizeof(tens) / sizeof(tens[0])); i++) {
        level = padpower_level(PADPOWER_ON_BATTERY, tens[i], level);
        CHECK_INT(level, shown[i]);
    }
}

// A function to test the hysteresis: once shown, the glyph hides only above 45 %, so a pad that
// reports 40 and 44 in turn does not flicker; from nothing, 44 shows nothing
static void test_hysteresis(void)
{
    CHECK_INT(padpower_level(PADPOWER_ON_BATTERY, 44, PADPOWER_LOW), PADPOWER_LOW);
    CHECK_INT(padpower_level(PADPOWER_ON_BATTERY, 45, PADPOWER_LOW), PADPOWER_LOW);
    CHECK_INT(padpower_level(PADPOWER_ON_BATTERY, 46, PADPOWER_LOW), PADPOWER_HIDDEN);
    CHECK_INT(padpower_level(PADPOWER_ON_BATTERY, 44, PADPOWER_CRITICAL), PADPOWER_LOW);
    CHECK_INT(padpower_level(PADPOWER_ON_BATTERY, 20, PADPOWER_CRITICAL), PADPOWER_LOW);
    CHECK_INT(padpower_level(PADPOWER_ON_BATTERY, 44, PADPOWER_HIDDEN), PADPOWER_HIDDEN);
}

// A function to test the reports that look like a level and are not: only ON_BATTERY shows, so a
// wired Xbox pad's CHARGING 100, a charging pad at 20 %, and a driver's UNKNOWN with a percent show
// nothing, and a pad plugged in while its glyph shows hides it at once
static void test_not_on_battery(void)
{
    CHECK_INT(padpower_level(PADPOWER_CHARGING, 100, PADPOWER_HIDDEN), PADPOWER_HIDDEN);
    CHECK_INT(padpower_level(PADPOWER_CHARGING, 20, PADPOWER_HIDDEN), PADPOWER_HIDDEN);
    CHECK_INT(padpower_level(PADPOWER_CHARGING, 20, PADPOWER_LOW), PADPOWER_HIDDEN);
    CHECK_INT(padpower_level(PADPOWER_CHARGED, 100, PADPOWER_HIDDEN), PADPOWER_HIDDEN);
    CHECK_INT(padpower_level(PADPOWER_UNKNOWN, 40, PADPOWER_HIDDEN), PADPOWER_HIDDEN);
    CHECK_INT(padpower_level(PADPOWER_UNKNOWN, 10, PADPOWER_LOW), PADPOWER_HIDDEN);
    CHECK_INT(padpower_level(PADPOWER_UNKNOWN, -1, PADPOWER_HIDDEN), PADPOWER_HIDDEN);
    CHECK_INT(padpower_level(PADPOWER_NO_BATTERY, -1, PADPOWER_HIDDEN), PADPOWER_HIDDEN);
    CHECK_INT(padpower_level(PADPOWER_ERROR, -1, PADPOWER_HIDDEN), PADPOWER_HIDDEN);
}

// A function to test the pick: the lowest pad whose glyph shows, the first of two at the same
// percent; none when no pad's glyph shows, a charging pad at a lower percent included
static void test_pick(void)
{
    CHECK_INT(padpower_pick(NULL, 0), -1);
    PadPower none[] = { pad(PADPOWER_CHARGING, 20, PADPOWER_HIDDEN), pad(PADPOWER_UNKNOWN, -1, PADPOWER_HIDDEN) };
    CHECK_INT(padpower_pick(none, 2), -1);
    PadPower two[] = { pad(PADPOWER_ON_BATTERY, 40, PADPOWER_LOW), pad(PADPOWER_ON_BATTERY, 10, PADPOWER_CRITICAL) };
    CHECK_INT(padpower_pick(two, 2), 1);
    PadPower reversed[] = { pad(PADPOWER_ON_BATTERY, 10, PADPOWER_CRITICAL), pad(PADPOWER_ON_BATTERY, 40, PADPOWER_LOW) };
    CHECK_INT(padpower_pick(reversed, 2), 0);
    PadPower charging[] = { pad(PADPOWER_CHARGING, 5, PADPOWER_HIDDEN), pad(PADPOWER_ON_BATTERY, 30, PADPOWER_LOW) };
    CHECK_INT(padpower_pick(charging, 2), 1);
    PadPower held[] = { pad(PADPOWER_ON_BATTERY, 70, PADPOWER_HIDDEN), pad(PADPOWER_ON_BATTERY, 44, PADPOWER_LOW) };
    CHECK_INT(padpower_pick(held, 2), 1);
    PadPower tie[] = { pad(PADPOWER_ON_BATTERY, 30, PADPOWER_LOW), pad(PADPOWER_ON_BATTERY, 30, PADPOWER_LOW) };
    CHECK_INT(padpower_pick(tie, 2), 0);
}

// A function to test a pad nothing has been read from, and the names the log uses
static void test_unknown_and_names(void)
{
    PadPower power = padpower_unknown();
    CHECK_INT(power.state, PADPOWER_UNKNOWN);
    CHECK_INT(power.percent, -1);
    CHECK_INT(power.connection, PADCONN_UNKNOWN);
    CHECK_INT(power.level, PADPOWER_HIDDEN);
    CHECK_STR(padpower_state_name(PADPOWER_ERROR), "error");
    CHECK_STR(padpower_state_name(PADPOWER_UNKNOWN), "unknown");
    CHECK_STR(padpower_state_name(PADPOWER_ON_BATTERY), "on battery");
    CHECK_STR(padpower_state_name(PADPOWER_NO_BATTERY), "no battery");
    CHECK_STR(padpower_state_name(PADPOWER_CHARGING), "charging");
    CHECK_STR(padpower_state_name(PADPOWER_CHARGED), "charged");
    CHECK_STR(padpower_connection_name(PADCONN_INVALID), "unknown");
    CHECK_STR(padpower_connection_name(PADCONN_UNKNOWN), "unknown");
    CHECK_STR(padpower_connection_name(PADCONN_WIRED), "wired");
    CHECK_STR(padpower_connection_name(PADCONN_WIRELESS), "wireless");
}

// A function to test the glyph's colors: amber and red, left alone by the shipped key (#010101),
// and moved two steps off a key set on them or one step off them, by chroma.h's rule
static void test_color(void)
{
    unsigned char rgb[3];
    padpower_color(PADPOWER_LOW, NULL, rgb);
    CHECK(rgb[0] == 0xFF && rgb[1] == 0xB0 && rgb[2] == 0x00);
    padpower_color(PADPOWER_CRITICAL, NULL, rgb);
    CHECK(rgb[0] == 0xE8 && rgb[1] == 0x20 && rgb[2] == 0x20);
    const unsigned char shipped[3] = { 0x01, 0x01, 0x01 };
    padpower_color(PADPOWER_LOW, shipped, rgb);
    CHECK(rgb[0] == 0xFF && rgb[1] == 0xB0 && rgb[2] == 0x00);
    padpower_color(PADPOWER_CRITICAL, shipped, rgb);
    CHECK(rgb[0] == 0xE8 && rgb[1] == 0x20 && rgb[2] == 0x20);
    const unsigned char amber[3] = { 0xFF, 0xB0, 0x00 };
    padpower_color(PADPOWER_LOW, amber, rgb);
    CHECK(rgb[0] == 0xFD && rgb[1] == 0xAE && rgb[2] == 0x02);
    const unsigned char near_red[3] = { 0xE9, 0x21, 0x1F };
    padpower_color(PADPOWER_CRITICAL, near_red, rgb);
    CHECK(rgb[0] == 0xE7 && rgb[1] == 0x23 && rgb[2] == 0x21);
}

int main(void)
{
    test_on_battery();
    test_steps();
    test_hysteresis();
    test_not_on_battery();
    test_pick();
    test_unknown_and_names();
    test_color();
    return check_report();
}
