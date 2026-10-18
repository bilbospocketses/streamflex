#include <string.h>
#include "derive.h"

// A function to return the smaller of two ints
static int min_int(int a, int b)
{
    return a < b ? a : b;
}

// A function to return the larger of two ints
static int max_int(int a, int b)
{
    return a > b ? a : b;
}

// A function to take hundredths of a percent of a size, exactly and then truncated; the float path in
// convert_percent_to_int() could come out 1 px low (900 x 52% gave 467, where this gives 468)
static int hundredths_of(int size, int hundredths)
{
    return (int) ((long long) size * hundredths / 10000);
}

// A function to turn an opacity in hundredths of a percent into an alpha byte; out of range reads
// as the nearest end
int derive_alpha(int hundredths)
{
    return hundredths_of(255, max_int(0, min_int(hundredths, 10000)));
}

// A function to give a color the alpha of an opacity
static DeriveColor with_alpha(DeriveColor color, int alpha)
{
    color.a = (unsigned char) alpha;
    return color;
}

// A function to work out every value the launcher draws with from the configured ones. It changes
// nothing it is given, so it can run again after any change.
void derive_settings(const DeriveInput *in, Effective *out)
{
    memset(out, 0, sizeof(*out));
    int w = in->screen_width;
    int h = in->screen_height;

    // A gap wider than the screen leaves no room for a single button
    int spacing = in->icon_spacing_percent ? hundredths_of(w, in->icon_spacing) : in->icon_spacing;
    out->icon_spacing = max_int(0, min_int(spacing, w));

    int vcenter = hundredths_of(h, in->vcenter);
    out->vcenter = max_int(h * DERIVE_MIN_VCENTER_PM / 1000, min_int(vcenter, h * DERIVE_MAX_VCENTER_PM / 1000));

    int margin = in->clock_margin_percent ? hundredths_of(h, in->clock_margin) : in->clock_margin;
    out->clock_margin = max_int(0, min_int(margin, h * DERIVE_MAX_CLOCK_MARGIN_PM / 1000));

    // Titles turned off take their padding with them. The padding is a percentage or px, never both:
    // a percentage wins, and without one the px value is used
    if (in->titles_enabled && in->title_padding_pct > 0)
        out->title_padding_pct = in->title_padding_pct;
    else if (in->titles_enabled)
        out->title_padding = in->title_padding;

    // The highlight's padding never overlaps the next button, and its outline stays inside it
    out->highlight_hpadding = max_int(0, min_int(in->highlight_hpadding, out->icon_spacing / 2));
    out->highlight_vpadding = max_int(0, in->highlight_vpadding);
    int max_outline = max_int(0, min_int(out->highlight_hpadding, out->highlight_vpadding));
    out->highlight_outline_size = max_int(0, min_int(in->highlight_outline_size, max_outline));

    // NanoSVG cannot draw rounded corners with an outline
    out->highlight_rx = out->highlight_outline_size > 0 ? 0 : max_int(0, in->highlight_rx);

    out->scroll_outline_size = max_int(0, min_int(in->scroll_outline_size, h * DERIVE_MAX_SCROLL_OUTLINE_PM / 1000));

    int title_alpha = derive_alpha(in->title_opacity);
    out->title_color = with_alpha(in->title_color, title_alpha);
    out->title_shadow_color = with_alpha(in->title_shadow_color, title_alpha * DERIVE_SHADOW_ALPHA_PERCENT / 100);
    out->overlay_color = with_alpha(in->overlay_color, derive_alpha(in->overlay_opacity));
    out->highlight_fill = with_alpha(in->highlight_fill, derive_alpha(in->highlight_fill_opacity));
    out->highlight_outline = with_alpha(in->highlight_outline, derive_alpha(in->highlight_outline_opacity));
    int scroll_alpha = derive_alpha(in->scroll_opacity);
    out->scroll_fill = with_alpha(in->scroll_fill, scroll_alpha);
    out->scroll_outline = with_alpha(in->scroll_outline, scroll_alpha);
    int clock_alpha = derive_alpha(in->clock_opacity);
    out->clock_color = with_alpha(in->clock_color, clock_alpha);
    out->clock_shadow_color = with_alpha(in->clock_shadow_color, clock_alpha * DERIVE_SHADOW_ALPHA_PERCENT / 100);
    out->screensaver_alpha = derive_alpha(in->screensaver_intensity);
}

// A function to decide if a held gamepad control repeats now: held since `pressed`, with its next repeat
// due `due` ms after that. Timed in wall time, never in frames, so the frame rate cannot change it. The
// times are SDL's ticks, so the subtraction is unsigned and holds across their wrap.
bool derive_repeat_due(unsigned int now, unsigned int pressed, unsigned int due)
{
    return now - pressed >= due;
}

// A function to work out when a held control's next repeat is due, in ms after its press, once it has
// repeated now: an interval after the last due time, so the repeats keep the interval on average
// whatever the frame time, or an interval after now when the frame came later than that, so a slow
// frame repeats once rather than catching up in a burst
unsigned int derive_next_repeat(unsigned int now, unsigned int pressed, unsigned int due, unsigned int interval)
{
    unsigned int held = now - pressed;
    return due + interval > held ? due + interval : held + interval;
}

// A function to decide whether OK, pressed at `pressed_at`, has been held long enough by `now` to open
// settings at its tile (plan ruling 1): HOLD_OK_MS or more. The times are SDL's ticks, so the subtraction
// is unsigned and holds across their wrap. A press stamped after `now` (an event's time, read against a
// clock read just before it) is a hold of no time, not one of 49 days: any difference past half the
// ticks' range is.
bool derive_hold_fired(unsigned int now, unsigned int pressed_at)
{
    unsigned int held = now - pressed_at;
    return held >= HOLD_OK_MS && held < 0x80000000u;
}

// A function to work out a fade's alpha now (the slideshow's fade, the screensaver's dim): begun at
// `start`, it rises to end_value over `duration` ms in step with the time passed, and is end_value from
// then on, or at once for a fade of no time. Timed in wall time, never in frames, so the frame rate cannot
// stretch or shorten it. The times are SDL's ticks, so the subtraction is unsigned and holds across their
// wrap.
int derive_fade_alpha(unsigned int now, unsigned int start, unsigned int duration, int end_value)
{
    unsigned int elapsed = now - start;
    if (elapsed >= duration)
        return end_value;
    return (int) ((unsigned long long) end_value * elapsed / duration);
}

// A function to decide if the Windows exit hotkey is tried again now: only while Windows refuses it,
// and then at once (the window came to the front, settings closed) or DERIVE_EXIT_RETRY_MS after the
// last try. The times are SDL's ticks, so the subtraction is unsigned and holds across their wrap.
bool derive_exit_retry_due(bool refused, bool at_once, unsigned int now, unsigned int last_try)
{
    return refused && (at_once || now - last_try >= DERIVE_EXIT_RETRY_MS);
}

// A function to work out the screen's geometry from the window's size in pixels, with the arithmetic
// and rounding the launcher has always used: the margin truncated, the title minimum rounded
ScreenGeometry geometry_from_pixels(int width, int height)
{
    ScreenGeometry screen;
    screen.screen_width = width;
    screen.screen_height = height;
    screen.screen_margin = (int) (DERIVE_SCREEN_MARGIN * (float) height);
    screen.title_min_size = (int) (DERIVE_TITLE_MIN_SIZE * (float) height + 0.5F);
    return screen;
}

// A function to take the display's refresh rate in whole hertz, rounded (SDL3 gives a float: 59.94);
// a display that reports less than 1 Hz gets the default, since every frame time divides by it
int derive_refresh_rate(float reported)
{
    return reported >= DERIVE_MIN_REFRESH_RATE ? (int) (reported + 0.5F) : DERIVE_DEFAULT_REFRESH_RATE;
}

// A function to decide what a change of the window's pixel size does (spec section 6): the size laid
// out already does nothing; a new size reloads, or waits while an app is launching or running, so a
// game's temporary resolution never becomes the layout. A recreated window is not decided here: the
// recreation marks its own reload (RELOAD_RECREATE), whatever the size.
SizeDecision derive_size_decision(int laid_out_width, int laid_out_height, int width, int height, bool app_in_front)
{
    if (width == laid_out_width && height == laid_out_height)
        return SIZE_NOTHING;
    return app_in_front ? SIZE_WAIT : SIZE_RELOAD;
}
