#include <stdbool.h>
#include <string.h>
#include "check.h"
#include "derive.h"

// A function to start from the built-in defaults on a 1920 x 1080 screen, as the launcher would
static DeriveInput defaults(void)
{
    DeriveInput in;
    memset(&in, 0, sizeof(in));
    in.screen_width = 1920;
    in.screen_height = 1080;
    in.titles_enabled = true;
    in.title_padding = 0;
    in.title_padding_pct = 8;
    in.title_color = (DeriveColor) { 0xFF, 0xFF, 0xFF, 0 };
    in.title_shadow_color = (DeriveColor) { 0x00, 0x00, 0x00, 0 };
    in.title_opacity = 10000;
    in.overlay_color = (DeriveColor) { 0x00, 0x00, 0x00, 0 };
    in.overlay_opacity = 5000;
    in.highlight_fill = (DeriveColor) { 0xFF, 0xFF, 0xFF, 0 };
    in.highlight_fill_opacity = 2500;
    in.highlight_outline = (DeriveColor) { 0x00, 0x00, 0xFF, 0 };
    in.highlight_outline_opacity = 10000;
    in.highlight_outline_size = 0;
    in.highlight_rx = 0;
    in.highlight_hpadding = 30;
    in.highlight_vpadding = 30;
    in.scroll_fill = (DeriveColor) { 0xFF, 0xFF, 0xFF, 0 };
    in.scroll_outline = (DeriveColor) { 0x00, 0x00, 0x00, 0 };
    in.scroll_opacity = 10000;
    in.scroll_outline_size = 0;
    in.clock_color = (DeriveColor) { 0xFF, 0xFF, 0xFF, 0 };
    in.clock_shadow_color = (DeriveColor) { 0x00, 0x00, 0x00, 0 };
    in.clock_opacity = 10000;
    in.icon_spacing = 500;
    in.icon_spacing_percent = true;
    in.vcenter = 5000;
    in.clock_margin = 500;
    in.clock_margin_percent = true;
    in.screensaver_intensity = 7000;
    return in;
}

// A function to test that the defaults give the values validate_settings() gave them
static void test_defaults(void)
{
    DeriveInput in = defaults();
    Effective eff;
    derive_settings(&in, &eff);
    CHECK_INT(eff.icon_spacing, 96);            // 5% of 1920
    CHECK_INT(eff.vcenter, 540);                // 50% of 1080
    CHECK_INT(eff.clock_margin, 54);            // 5% of 1080
    CHECK_INT(eff.highlight_hpadding, 30);
    CHECK_INT(eff.highlight_vpadding, 30);
    CHECK_INT(eff.title_color.a, 255);
    CHECK_INT(eff.title_shadow_color.a, 191);   // 0.75 of the title's alpha
    CHECK_INT(eff.overlay_color.a, 127);        // 50%
    CHECK_INT(eff.highlight_fill.a, 63);        // 25%, as FillOpacity=25% always gave
    CHECK_INT(eff.highlight_outline.a, 255);
    CHECK_INT(eff.scroll_fill.a, 255);
    CHECK_INT(eff.scroll_outline.a, 255);       // One opacity for both
    CHECK_INT(eff.clock_color.a, 255);
    CHECK_INT(eff.clock_shadow_color.a, 191);
    CHECK_INT(eff.screensaver_alpha, 178);      // 70%
    CHECK_INT(eff.title_padding_pct, 8);
    CHECK_INT(eff.highlight_fill.r, 0xFF);      // The color itself passes through
    CHECK_INT(eff.highlight_outline.b, 0xFF);
}

// A function to test that deriving twice gives the same answer: nothing is spent on the first run
static void test_twice(void)
{
    DeriveInput in = defaults();
    Effective first, second;
    derive_settings(&in, &first);
    derive_settings(&in, &second);
    CHECK(memcmp(&first, &second, sizeof(first)) == 0);
}

// A function to test that a clamp limits only the effective value, so raising the setting that
// caused it gives the full value back
static void test_clamps_restore(void)
{
    DeriveInput in = defaults();
    Effective eff;

    // HPadding is kept to half the gap between buttons; with a wider gap it comes back
    in.highlight_hpadding = 100;
    in.icon_spacing = 50;
    in.icon_spacing_percent = false;
    derive_settings(&in, &eff);
    CHECK_INT(eff.highlight_hpadding, 25);
    in.icon_spacing = 400;
    derive_settings(&in, &eff);
    CHECK_INT(eff.highlight_hpadding, 100);

    // The outline is kept inside the smaller padding
    in.highlight_outline_size = 50;
    in.highlight_vpadding = 10;
    derive_settings(&in, &eff);
    CHECK_INT(eff.highlight_outline_size, 10);
    in.highlight_vpadding = 80;
    derive_settings(&in, &eff);
    CHECK_INT(eff.highlight_outline_size, 50);

    // Rounded corners with an outline cannot be drawn (NanoSVG): the radius goes, and comes back
    in.highlight_rx = 20;
    derive_settings(&in, &eff);
    CHECK_INT(eff.highlight_rx, 0);
    in.highlight_outline_size = 0;
    derive_settings(&in, &eff);
    CHECK_INT(eff.highlight_rx, 20);

    // A gap wider than the screen is the screen's width
    in.icon_spacing = 5000;
    derive_settings(&in, &eff);
    CHECK_INT(eff.icon_spacing, 1920);

    // The clock's margin is at most 10% of the screen height
    in.clock_margin = 5000;
    derive_settings(&in, &eff);
    CHECK_INT(eff.clock_margin, 108);
    in.clock_margin = 20;
    in.clock_margin_percent = false;
    derive_settings(&in, &eff);
    CHECK_INT(eff.clock_margin, 20);

    // The vertical center stays between 25% and 75%
    in.vcenter = 1000;
    derive_settings(&in, &eff);
    CHECK_INT(eff.vcenter, 270);
    in.vcenter = 9000;
    derive_settings(&in, &eff);
    CHECK_INT(eff.vcenter, 810);

    // The scroll arrow's outline is at most 1% of the screen height
    in.scroll_outline_size = 40;
    derive_settings(&in, &eff);
    CHECK_INT(eff.scroll_outline_size, 10);

    // HPadding is kept to half the clamped gap, not the configured one: 5000 px is 1920 on screen
    in = defaults();
    in.icon_spacing = 5000;
    in.icon_spacing_percent = false;
    in.highlight_hpadding = 1000;
    derive_settings(&in, &eff);
    CHECK_INT(eff.icon_spacing, 1920);
    CHECK_INT(eff.highlight_hpadding, 960);

    // An outline with no padding to sit in is no outline, so the corners stay rounded
    in = defaults();
    in.highlight_outline_size = 5;
    in.highlight_hpadding = 0;
    in.highlight_rx = 20;
    derive_settings(&in, &eff);
    CHECK_INT(eff.highlight_outline_size, 0);
    CHECK_INT(eff.highlight_rx, 20);
}

// A function to test that a negative px value is never drawn: each one reads as 0
static void test_negative_px(void)
{
    DeriveInput in = defaults();
    Effective eff;
    in.icon_spacing = -1;
    in.icon_spacing_percent = false;
    in.clock_margin = -1;
    in.clock_margin_percent = false;
    in.highlight_hpadding = -1;
    in.highlight_vpadding = -1;
    in.highlight_outline_size = -1;
    in.highlight_rx = -1;
    in.scroll_outline_size = -1;
    derive_settings(&in, &eff);
    CHECK_INT(eff.icon_spacing, 0);
    CHECK_INT(eff.clock_margin, 0);
    CHECK_INT(eff.highlight_hpadding, 0);
    CHECK_INT(eff.highlight_vpadding, 0);
    CHECK_INT(eff.highlight_outline_size, 0);
    CHECK_INT(eff.highlight_rx, 0);
    CHECK_INT(eff.scroll_outline_size, 0);
}

// A function to test that each color takes its alpha from its own feature's opacity: with every
// opacity different, a color reading another feature's opacity gets the wrong alpha
static void test_opacities_distinct(void)
{
    DeriveInput in = defaults();
    Effective eff;
    in.title_opacity = 9500;
    in.overlay_opacity = 8500;
    in.highlight_fill_opacity = 7500;
    in.highlight_outline_opacity = 6500;
    in.scroll_opacity = 5500;
    in.clock_opacity = 4500;
    in.screensaver_intensity = 3500;
    derive_settings(&in, &eff);
    CHECK_INT(eff.title_color.a, 242);          // 255 x 95%
    CHECK_INT(eff.title_shadow_color.a, 181);   // 0.75 of 242
    CHECK_INT(eff.overlay_color.a, 216);        // 255 x 85%
    CHECK_INT(eff.highlight_fill.a, 191);       // 255 x 75%
    CHECK_INT(eff.highlight_outline.a, 165);    // 255 x 65%
    CHECK_INT(eff.scroll_fill.a, 140);          // 255 x 55%
    CHECK_INT(eff.scroll_outline.a, 140);
    CHECK_INT(eff.clock_color.a, 114);          // 255 x 45%
    CHECK_INT(eff.clock_shadow_color.a, 85);    // 0.75 of 114
    CHECK_INT(eff.screensaver_alpha, 89);       // 255 x 35%
}

// A function to test every percentage and opacity conversion at its ends and between
static void test_conversions(void)
{
    CHECK_INT(derive_alpha(0), 0);
    CHECK_INT(derive_alpha(10000), 255);
    CHECK_INT(derive_alpha(5000), 127);
    CHECK_INT(derive_alpha(1250), 31);          // 12.5%
    CHECK_INT(derive_alpha(-5), 0);             // Out of range reads as the nearest end
    CHECK_INT(derive_alpha(20000), 255);

    DeriveInput in = defaults();
    Effective eff;
    in.icon_spacing = 1250;                     // 12.5% of 1920
    derive_settings(&in, &eff);
    CHECK_INT(eff.icon_spacing, 240);
    in.title_opacity = 0;
    derive_settings(&in, &eff);
    CHECK_INT(eff.title_color.a, 0);
    CHECK_INT(eff.title_shadow_color.a, 0);
    in.screensaver_intensity = 0;               // Dims nothing: the screensaver cannot start
    derive_settings(&in, &eff);
    CHECK_INT(eff.screensaver_alpha, 0);

    // A percentage of the screen is exact: convert_percent_to_int()'s float path gave 467 here
    in = defaults();
    in.screen_height = 900;
    in.vcenter = 5200;
    derive_settings(&in, &eff);
    CHECK_INT(eff.vcenter, 468);
}

// A function to test that titles turned off take their padding with them, as validate_settings() did
static void test_titles_off(void)
{
    DeriveInput in = defaults();
    Effective eff;
    in.titles_enabled = false;
    in.title_padding = 20;
    in.title_padding_pct = 0;
    derive_settings(&in, &eff);
    CHECK_INT(eff.title_padding, 0);
    CHECK_INT(eff.title_padding_pct, 0);
    in.titles_enabled = true;
    derive_settings(&in, &eff);
    CHECK_INT(eff.title_padding, 20);
    CHECK_INT(eff.title_padding_pct, 0);
    in.titles_enabled = false;
    in.title_padding_pct = 8;
    derive_settings(&in, &eff);
    CHECK_INT(eff.title_padding, 0);
    CHECK_INT(eff.title_padding_pct, 0);
    in.titles_enabled = true;
    derive_settings(&in, &eff);
    CHECK_INT(eff.title_padding, 0);
    CHECK_INT(eff.title_padding_pct, 8);
}

// A function to test that the title padding is a percentage or px, never both: a percentage wins,
// and without one the px value is used
static void test_title_padding_one_of_two(void)
{
    DeriveInput in = defaults();
    Effective eff;
    in.title_padding = 20;
    in.title_padding_pct = 8;
    derive_settings(&in, &eff);
    CHECK_INT(eff.title_padding, 0);
    CHECK_INT(eff.title_padding_pct, 8);
    in.title_padding_pct = 0;
    derive_settings(&in, &eff);
    CHECK_INT(eff.title_padding, 20);
    CHECK_INT(eff.title_padding_pct, 0);
    in.title_padding_pct = -3;                  // Not a percentage, so the px value is used
    derive_settings(&in, &eff);
    CHECK_INT(eff.title_padding, 20);
    CHECK_INT(eff.title_padding_pct, 0);
}

// A function to test a held gamepad control's count after a repeat: at a fixed rate one interval short
// of the delay; past a delay that a change of frame timing shortened, back to it, so the repeats keep
// their interval rather than coming on every frame
static void test_repeated_count(void)
{
    CHECK_INT((int) derive_repeated_count(31, 31, 1), 30);   // 60 Hz: 500 ms, then every frame
    CHECK_INT((int) derive_repeated_count(62, 62, 3), 59);   // 120 fps: every 3 frames
    CHECK_INT((int) derive_repeated_count(5, 5, 1), 4);      // 10 fps, the slowest frame time
    CHECK_INT((int) derive_repeated_count(3, 5, 1), 2);      // A count short of the delay keeps its place
    CHECK_INT((int) derive_repeated_count(1, 1, 3), 0);      // An interval past the count stops at 0
    // 240 Hz to 120 fps while held: the count is 119 after a repeat at 240 Hz's delay (125, interval
    // 6); with the delay 62 and the interval 3, the next 30 frames repeat on every third
    unsigned int count = 119;
    int repeats = 0;
    for (int frame = 0; frame < 30; frame++) {
        count++;
        if (count >= 62) {
            repeats++;
            count = derive_repeated_count(count, 62, 3);
        }
    }
    CHECK_INT(repeats, 10);
}

// A function to test when a refused Windows exit hotkey is tried again: only while refused, every
// DERIVE_EXIT_RETRY_MS of the clock, or at once (focus gained, settings closed); SDL's ticks wrap
static void test_exit_retry_due(void)
{
    CHECK(!derive_exit_retry_due(false, false, 20000, 0));        // Registered: never
    CHECK(!derive_exit_retry_due(false, true, 20000, 0));         // Registered: not even at once
    CHECK(!derive_exit_retry_due(true, false, 4999, 0));          // Refused, short of 5 s
    CHECK(derive_exit_retry_due(true, false, 5000, 0));           // Refused, 5 s on
    CHECK(derive_exit_retry_due(true, false, 12345, 1000));       // Refused, well past
    CHECK(!derive_exit_retry_due(true, false, 1000, 1000));       // Refused, tried this tick
    CHECK(derive_exit_retry_due(true, true, 1000, 1000));         // Refused, at once: whenever last tried
    CHECK(!derive_exit_retry_due(true, false, 1000, 0xFFFFFC17)); // 2 s across the wrap
    CHECK(derive_exit_retry_due(true, false, 4000, 0xFFFFFC17));  // 5 s across the wrap
}

int main(void)
{
    test_exit_retry_due();
    test_repeated_count();
    test_defaults();
    test_twice();
    test_clamps_restore();
    test_negative_px();
    test_opacities_distinct();
    test_conversions();
    test_titles_off();
    test_title_padding_one_of_two();
    return check_report();
}
