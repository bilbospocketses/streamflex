// The values the launcher draws with, worked out from the values config.ini holds: percentages of
// the screen in px, opacities as alpha bytes, and the limits one setting puts on another. Pure: no
// SDL, no globals, and nothing is changed in place, so it can run after every change, as often as
// needed, with the same answer each time. tests/test_derive.c builds it on its own.
#ifndef DERIVE_H
#define DERIVE_H

#include <stdbool.h>

#define DERIVE_MAX_CLOCK_MARGIN_PM 100        // The clock's margin: at most 10% of the screen height
#define DERIVE_MIN_VCENTER_PM 250             // The vertical center: 25% to 75% of the screen height
#define DERIVE_MAX_VCENTER_PM 750
#define DERIVE_MAX_SCROLL_OUTLINE_PM 10       // The scroll arrow's outline: at most 1% of the screen height
#define DERIVE_SHADOW_ALPHA_PERCENT 75        // A shadow is three quarters as opaque as its text
#define DERIVE_EXIT_RETRY_MS 5000             // A refused Windows exit hotkey is tried again this often

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} DeriveColor;

// The configured values: as config.ini says, percentages in hundredths. A color's `a` is ignored.
typedef struct DeriveInput {
    int screen_width;
    int screen_height;
    bool titles_enabled;
    int title_padding;             // px; used when title_padding_pct is 0 or less
    int title_padding_pct;         // Whole percent of the button
    DeriveColor title_color;
    DeriveColor title_shadow_color;
    int title_opacity;             // Hundredths of a percent, as every opacity below
    DeriveColor overlay_color;
    int overlay_opacity;
    DeriveColor highlight_fill;
    int highlight_fill_opacity;
    DeriveColor highlight_outline;
    int highlight_outline_opacity;
    int highlight_outline_size;
    int highlight_rx;
    int highlight_hpadding;
    int highlight_vpadding;
    DeriveColor scroll_fill;
    DeriveColor scroll_outline;
    int scroll_opacity;
    int scroll_outline_size;
    DeriveColor clock_color;
    DeriveColor clock_shadow_color;
    int clock_opacity;
    int icon_spacing;              // Hundredths of a percent of the screen width, or px
    bool icon_spacing_percent;
    int vcenter;                   // Hundredths of a percent of the screen height
    int clock_margin;              // Hundredths of a percent of the screen height, or px
    bool clock_margin_percent;
    int screensaver_intensity;     // Hundredths of a percent
} DeriveInput;

// The values the launcher draws with
typedef struct Effective {
    int icon_spacing;              // px
    int vcenter;                   // px from the top of the screen
    int clock_margin;              // px
    int title_padding;             // px; 0 without titles, or for a percentage padding
    int title_padding_pct;         // Whole percent; 0 without titles, or for a px padding
    int highlight_hpadding;        // px, at most half the gap between buttons
    int highlight_vpadding;
    int highlight_outline_size;    // px, inside the smaller padding
    int highlight_rx;              // 0 when there is an outline (NanoSVG cannot draw both)
    int scroll_outline_size;       // px
    DeriveColor title_color;       // Every color below with its alpha from its opacity
    DeriveColor title_shadow_color;
    DeriveColor overlay_color;
    DeriveColor highlight_fill;
    DeriveColor highlight_outline;
    DeriveColor scroll_fill;
    DeriveColor scroll_outline;
    DeriveColor clock_color;
    DeriveColor clock_shadow_color;
    int screensaver_alpha;         // 0-255 at full dim; below 1 the screensaver cannot dim
} Effective;

int derive_alpha(int hundredths);
void derive_settings(const DeriveInput *in, Effective *out);
unsigned int derive_repeated_count(unsigned int count, unsigned int delay, unsigned int interval);
bool derive_exit_retry_due(bool refused, bool at_once, unsigned int now, unsigned int last_try);

#endif
