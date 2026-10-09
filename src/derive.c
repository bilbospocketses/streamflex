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

// A function to work out a held gamepad control's count of frames once it has repeated: one interval
// short of the delay, so the next repeat comes one interval later. A count past the delay (a change of
// frame timing shortened the delay under a held control) comes back to it, rather than repeating on
// every frame until it falls under the delay.
unsigned int derive_repeated_count(unsigned int count, unsigned int delay, unsigned int interval)
{
    unsigned int from = count < delay ? count : delay;
    return from > interval ? from - interval : 0;
}
