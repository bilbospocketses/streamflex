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

// A held pad control run frame by frame as poll_gamepad() runs it: pressed on frame 0 (SDL's ticks, at
// next_start), held `hold_ms`, at `fps` frames a second, each frame at its whole millisecond. How many
// times it repeated, when it first did (ms after the press; -1 for never), and the longest gap between
// two repeats.
typedef struct {
    int repeats;
    int first;
    int longest;
} HoldRun;

// Each run is pressed at its own time, from 256 ms before the ticks' wrap down to 151 ms before it, so
// each 2 s hold crosses the wrap, and even its first repeat is due after it
static unsigned int next_start = 0xFFFFFF00u;

static HoldRun hold_at(int fps, int hold_ms)
{
    HoldRun run = { 0, -1, 0 };
    unsigned int pressed = next_start;
    next_start += 7;
    unsigned int due = DERIVE_REPEAT_DELAY_MS;
    int last = 0;
    for (int frame = 1;; frame++) {
        int held = frame * 1000 / fps;
        if (held > hold_ms)
            break;
        unsigned int now = pressed + (unsigned int) held;
        if (!derive_repeat_due(now, pressed, due))
            continue;
        due = derive_next_repeat(now, pressed, due, DERIVE_REPEAT_INTERVAL_MS);
        if (run.repeats == 0)
            run.first = held;
        else if (held - last > run.longest)
            run.longest = held - last;
        last = held;
        run.repeats++;
    }
    return run;
}

// The frame time at `fps`, in whole ms rounded up
static int frame_ms(int fps)
{
    return (1000 + fps - 1) / fps;
}

// A function to tell whether a 2 s hold at `fps` first repeats at the delay, within one frame
static bool first_repeat_at_delay(int fps)
{
    HoldRun run = hold_at(fps, 2000);
    return run.first >= DERIVE_REPEAT_DELAY_MS && run.first <= DERIVE_REPEAT_DELAY_MS + frame_ms(fps);
}

// A function to tell whether a 2 s hold at `fps` then repeats every interval, or every frame when a
// frame is longer: as many repeats as that makes, give or take one, and no gap longer than that by more
// than a frame
static bool repeats_keep_interval(int fps)
{
    HoldRun run = hold_at(fps, 2000);
    int every = frame_ms(fps) > DERIVE_REPEAT_INTERVAL_MS ? 1000 / fps : DERIVE_REPEAT_INTERVAL_MS;
    int wanted = (2000 - run.first) / every + 1;
    return run.first >= 0 && run.repeats >= wanted - 1 && run.repeats <= wanted + 1
           && run.longest <= every + frame_ms(fps);
}

// A function to tell whether a hold of `hold_ms` at `fps` never repeats
static bool never_repeats(int fps, int hold_ms)
{
    return hold_at(fps, hold_ms).repeats == 0;
}

// A function to test a held pad control's repeat: timed in wall time, it comes at the same times at 30,
// 60, 100 and 144 fps. Counted in frames, as it was, a 100 fps machine repeated a 300 ms tap and a
// 30 fps one waited a second.
static void test_repeat_wall_time(void)
{
    CHECK(first_repeat_at_delay(30));
    CHECK(first_repeat_at_delay(60));
    CHECK(first_repeat_at_delay(100));
    CHECK(first_repeat_at_delay(144));
    CHECK(repeats_keep_interval(30));
    CHECK(repeats_keep_interval(60));
    CHECK(repeats_keep_interval(100));
    CHECK(repeats_keep_interval(144));
    CHECK(never_repeats(30, 300));
    CHECK(never_repeats(60, 300));
    CHECK(never_repeats(100, 300));
    CHECK(never_repeats(144, 300));
    CHECK(never_repeats(30, 400));
    CHECK(never_repeats(60, 400));
    CHECK(never_repeats(100, 400));
    CHECK(never_repeats(144, 400));
    // The two functions alone: due at the delay to the millisecond, across the ticks' wrap (and not 16 ms
    // after a press 256 ms before it, as `now >= pressed + due` would say), and a frame that came late moves
    // the next repeat an interval past it rather than leaving a burst due
    CHECK(!derive_repeat_due(0xFFFFFF00u + 16, 0xFFFFFF00u, 500));
    CHECK(!derive_repeat_due(0xFFFFFF00u + 499, 0xFFFFFF00u, 500));
    CHECK(derive_repeat_due(0xFFFFFF00u + 500, 0xFFFFFF00u, 500));
    CHECK_INT((int) derive_next_repeat(1000 + 510, 1000, 500, 25), 525);
    CHECK_INT((int) derive_next_repeat(1000 + 600, 1000, 500, 25), 625);
}

// A fade run frame by frame as update_slideshow() and update_screensaver() run it: begun on frame 0 (SDL's
// ticks, at next_fade_start), at `fps` frames a second, each frame at its whole millisecond, and run on to
// twice its duration. When its alpha first reached end_value (ms after its start; -1 for never), its alpha
// on the first frame at half its duration or later, the highest alpha seen, and whether it ever went down.
typedef struct {
    int ended;
    int at_half;
    int highest;
    bool monotonic;
} FadeRun;

// Each run begins at its own time, from 256 ms before the ticks' wrap down to 95 ms before it, so each run
// straddles the wrap: its first frame comes before it, and its end after it
static unsigned int next_fade_start = 0xFFFFFF00u;

static FadeRun fade_at(int fps, unsigned int duration, int end_value)
{
    FadeRun run = { -1, -1, 0, true };
    unsigned int start = next_fade_start;
    next_fade_start += 7;
    int last = 0;
    for (int frame = 1;; frame++) {
        int elapsed = frame * 1000 / fps;
        if (elapsed > 2 * (int) duration)
            break;
        int alpha = derive_fade_alpha(start + (unsigned int) elapsed, start, duration, end_value);
        if (alpha < last)
            run.monotonic = false;
        if (alpha > run.highest)
            run.highest = alpha;
        if (run.at_half < 0 && elapsed >= (int) duration / 2)
            run.at_half = alpha;
        if (run.ended < 0 && alpha >= end_value)
            run.ended = elapsed;
        last = alpha;
    }
    return run;
}

// A function to tell whether a fade at `fps` ends at its duration, within one frame
static bool fade_ends_at_duration(int fps, unsigned int duration, int end_value)
{
    FadeRun run = fade_at(fps, duration, end_value);
    return run.ended >= (int) duration && run.ended <= (int) duration + frame_ms(fps);
}

// A function to tell whether a fade at `fps` rises steadily: never back down, and half way at half its
// duration, give or take what one frame adds
static bool fade_rises_steadily(int fps, unsigned int duration, int end_value)
{
    FadeRun run = fade_at(fps, duration, end_value);
    int slack = end_value * frame_ms(fps) / (int) duration + 1;
    return run.monotonic && run.at_half >= end_value / 2 - 1 && run.at_half <= end_value / 2 + slack;
}

// A function to tell whether a fade at `fps`, run on past its end, stays at end_value and never passes it
static bool fade_stops_at_end(int fps, unsigned int duration, int end_value)
{
    return fade_at(fps, duration, end_value).highest == end_value;
}

// A function to test the slideshow's fade and the screensaver's dim: timed in wall time, each ends at its
// duration at 30, 60, 100 and 144 fps. Counted in frames, as they were, each frame added the share of the
// fade a 16 ms display frame holds, so a 30 fps machine took twice as long and a 100 fps one 0.6 times as
// long. The fade is the default 1.5 s to full; the dim is DERIVE_SCREENSAVER_DIM_MS to the default
// intensity's alpha, 70%.
static void test_fade_wall_time(void)
{
    unsigned int fade = 1500;
    unsigned int dim = DERIVE_SCREENSAVER_DIM_MS;
    int dark = derive_alpha(7000);
    CHECK(fade_ends_at_duration(30, fade, 255));
    CHECK(fade_ends_at_duration(60, fade, 255));
    CHECK(fade_ends_at_duration(100, fade, 255));
    CHECK(fade_ends_at_duration(144, fade, 255));
    CHECK(fade_rises_steadily(30, fade, 255));
    CHECK(fade_rises_steadily(60, fade, 255));
    CHECK(fade_rises_steadily(100, fade, 255));
    CHECK(fade_rises_steadily(144, fade, 255));
    CHECK(fade_stops_at_end(30, fade, 255));
    CHECK(fade_stops_at_end(60, fade, 255));
    CHECK(fade_stops_at_end(100, fade, 255));
    CHECK(fade_stops_at_end(144, fade, 255));
    CHECK(fade_ends_at_duration(30, dim, dark));
    CHECK(fade_ends_at_duration(60, dim, dark));
    CHECK(fade_ends_at_duration(100, dim, dark));
    CHECK(fade_ends_at_duration(144, dim, dark));
    CHECK(fade_rises_steadily(30, dim, dark));
    CHECK(fade_rises_steadily(60, dim, dark));
    CHECK(fade_rises_steadily(100, dim, dark));
    CHECK(fade_rises_steadily(144, dim, dark));
    CHECK(fade_stops_at_end(30, dim, dark));
    CHECK(fade_stops_at_end(60, dim, dark));
    CHECK(fade_stops_at_end(100, dim, dark));
    CHECK(fade_stops_at_end(144, dim, dark));
    // The function alone: none at the start, the share of end_value the time passed is of the duration
    // (truncated), all of it from the duration on, and all of it at once for a fade of no time. Across the
    // ticks' wrap, a fade begun 256 ms before it is 16 ms in at 16 ms after its start (and not done, as
    // `now >= start + duration` would say), and done 1500 ms after it
    CHECK_INT(derive_fade_alpha(1000, 1000, 1500, 255), 0);
    CHECK_INT(derive_fade_alpha(1000 + 750, 1000, 1500, 255), 127);
    CHECK_INT(derive_fade_alpha(1000 + 1499, 1000, 1500, 255), 254);
    CHECK_INT(derive_fade_alpha(1000 + 1500, 1000, 1500, 255), 255);
    CHECK_INT(derive_fade_alpha(1000 + 9000, 1000, 1500, 255), 255);
    CHECK_INT(derive_fade_alpha(1000, 1000, 0, dark), dark);
    CHECK_INT(derive_fade_alpha(0xFFFFFF00u + 16, 0xFFFFFF00u, 1500, 255), 2);
    CHECK_INT(derive_fade_alpha(0xFFFFFF00u + 1500, 0xFFFFFF00u, 1500, 255), 255);
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

// Unit test 9 (spec section 9): the screen's geometry from the window's size in pixels, with the
// arithmetic and rounding init_sdl() always used: the margin truncated, the title minimum rounded
static void test_geometry_from_pixels(void)
{
    ScreenGeometry hd = geometry_from_pixels(1920, 1080);
    CHECK_INT(hd.screen_width, 1920);
    CHECK_INT(hd.screen_height, 1080);
    CHECK_INT(hd.screen_margin, 54);
    CHECK_INT(hd.title_min_size, 22);
    ScreenGeometry uhd = geometry_from_pixels(3840, 2160);
    CHECK_INT(uhd.screen_width, 3840);
    CHECK_INT(uhd.screen_height, 2160);
    CHECK_INT(uhd.screen_margin, 108);
    CHECK_INT(uhd.title_min_size, 43);
    ScreenGeometry small = geometry_from_pixels(1280, 720);
    CHECK_INT(small.screen_width, 1280);
    CHECK_INT(small.screen_height, 720);
    CHECK_INT(small.screen_margin, 36);
    CHECK_INT(small.title_min_size, 14);
}

// A function to test the refresh rate taken from the display: rounded to whole hertz, and 60 when the
// display reports none (Xvfb, some VMs and remote desktops report 0), since every timing divides by it
static void test_refresh_rate(void)
{
    CHECK_INT(derive_refresh_rate(0.0f), 60);
    CHECK_INT(derive_refresh_rate(-1.0f), 60);
    CHECK_INT(derive_refresh_rate(0.99f), 60);
    CHECK_INT(derive_refresh_rate(1.0f), 1);
    CHECK_INT(derive_refresh_rate(59.94f), 60);
    CHECK_INT(derive_refresh_rate(60.0f), 60);
    CHECK_INT(derive_refresh_rate(143.98f), 144);
    CHECK_INT(derive_refresh_rate(29.97f), 30);
}

// Unit test 10 (spec section 9): what a change of the window's pixel size does. The size laid out
// already does nothing; a new size reloads, or waits while an app is in front; a size that came back
// by the time the app is done reloads nothing. A recreated window reloads through its own mark
// (RELOAD_RECREATE), which check 60's f60-winrest proves, not through this decision.
static void test_size_decision(void)
{
    CHECK_INT(derive_size_decision(1920, 1080, 1920, 1080, false), SIZE_NOTHING);   // As the window is made
    CHECK_INT(derive_size_decision(1920, 1080, 1920, 1080, true), SIZE_NOTHING);
    CHECK_INT(derive_size_decision(1920, 1080, 1280, 720, false), SIZE_RELOAD);
    CHECK_INT(derive_size_decision(1920, 1080, 1280, 720, true), SIZE_WAIT);
    CHECK_INT(derive_size_decision(1920, 1080, 1280, 1080, false), SIZE_RELOAD);    // Only the width
    CHECK_INT(derive_size_decision(1920, 1080, 1920, 720, false), SIZE_RELOAD);     // Only the height
    CHECK_INT(derive_size_decision(1920, 1080, 1920, 1080, false), SIZE_NOTHING);   // Came back: post_launch() asks again
}

int main(void)
{
    test_exit_retry_due();
    test_repeat_wall_time();
    test_fade_wall_time();
    test_defaults();
    test_twice();
    test_clamps_restore();
    test_negative_px();
    test_opacities_distinct();
    test_conversions();
    test_titles_off();
    test_title_padding_one_of_two();
    test_geometry_from_pixels();
    test_refresh_rate();
    test_size_decision();
    return check_report();
}
