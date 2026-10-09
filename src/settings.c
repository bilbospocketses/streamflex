#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "settings.h"
#include "layout.h"
#include "fileio.h"
#include "inidoc.h"
#include "alloc.h"
#include "colorpick.h"
#include <launcher_config.h>

#define ARROW " \xE2\x80\xBA "   // U+203A with a space either side, between the pages in the page path
#define TIMES "\xC3\x97"         // U+00D7, the multiplication sign
#define ELLIPSIS "\xE2\x80\xA6"  // U+2026, the ellipsis
#define LENGTH(array) ((int) (sizeof(array) / sizeof((array)[0])))

// The names each choice setting reads and writes, NULL-terminated, and what the screen calls them
static const char *const MODE_NAMES[] = { "Color", "Image", "Slideshow", "Transparent", NULL };
static const char *const MODE_LABELS[] = { "Colour", "Image", "Slideshow", "Transparent" };
static const char *const ON_LAUNCH_NAMES[] = { "Blank", "None", "Quit", NULL };
static const char *const ON_LAUNCH_LABELS[] = { "Blank screen", "Keep showing", "Quit" };
static const char *const OVERSIZE_NAMES[] = { "Truncate", "Shrink", "None", NULL };
static const char *const OVERSIZE_LABELS[] = { "Truncate", "Shrink", "Leave as is" };
static const char *const ALIGNMENT_NAMES[] = { "Left", "Right", NULL };
static const char *const ALIGNMENT_LABELS[] = { "Left", "Right" };
static const char *const TIME_NAMES[] = { "24hr", "12hr", "Auto", NULL };
static const char *const TIME_LABELS[] = { "14:05", "2:05 PM", "Auto" };
static const char *const DATE_NAMES[] = { "Big", "Little", "Auto", NULL };
static const char *const DATE_LABELS[] = { "Sep 28", "28 Sep", "Auto" };
#define MODE_IMAGE 1
#define MODE_SLIDESHOW 2

static const int ICON_STEPS[] = { 64, 96, 128, 160, 192, 256, 320, 384, 512, 768, 1024 };
static const int SECOND_STEPS[] = { 5, 10, 15, 30, 60, 120, 300, 600, 1800, 3600 };
static const int MILLI_STEPS[] = { 0, 500, 1000, 1500, 2000, 2500, 3000 };
static const int TITLE_STEPS[] = { 11, 14, 17 };  // Small, Medium, Large
static const int TIMEOUT_STEPS[] = { 3, 5, 10, 15, 20, 30 };
static const int IDLE_STEPS[] = { 3, 5, 10, 15, 30, 60, 120, 300, 600, 900 };
static const int FPS_STEPS[] = { 30, 60, 75, 120, 144, 165, 240 };

static const char *const TRANSPARENT_NOTE =
    "The desktop shows through. On Linux this needs a compositor: see Transparent Backgrounds in the configuration docs.";
static const char *const MENU_NOTE = "The lowest step, All menus, follows the shared grid.";

// Rows marked with the same comment belong to the same page (Task 5 lays the pages out)
static const SettingDef DEFS[SET_ID_COUNT] = {
    // 3a's
    [SET_ID_BACKGROUND_MODE] = { .id = SET_ID_BACKGROUND_MODE, .label = "Mode", .section = "Background",
        .key = SETTING_BACKGROUND_MODE, .type = SET_TYPE_CHOICE, .refresh = SET_REFRESH_BACKGROUND,
        .lo = 0, .hi = 3, .names = MODE_NAMES, .labels = MODE_LABELS },
    [SET_ID_BACKGROUND_COLOR] = { .id = SET_ID_BACKGROUND_COLOR, .label = "Colour", .section = "Background",
        .key = SETTING_BACKGROUND_COLOR, .type = SET_TYPE_COLOR, .refresh = SET_REFRESH_BACKGROUND },
    [SET_ID_BACKGROUND_IMAGE] = { .id = SET_ID_BACKGROUND_IMAGE, .label = "Image", .section = "Background",
        .key = SETTING_BACKGROUND_IMAGE, .type = SET_TYPE_PATH, .refresh = SET_REFRESH_BACKGROUND },
    [SET_ID_SLIDESHOW_DIRECTORY] = { .id = SET_ID_SLIDESHOW_DIRECTORY, .label = "Folder", .section = "Background",
        .key = SETTING_SLIDESHOW_DIRECTORY, .type = SET_TYPE_PATH, .refresh = SET_REFRESH_BACKGROUND },
    [SET_ID_SLIDESHOW_DURATION] = { .id = SET_ID_SLIDESHOW_DURATION, .label = "Change every", .section = "Background",
        .key = SETTING_SLIDESHOW_IMAGE_DURATION, .type = SET_TYPE_SECONDS, .min = 5, .max = 3600,
        .steps = SECOND_STEPS, .step_count = LENGTH(SECOND_STEPS) },
    [SET_ID_SLIDESHOW_FADE] = { .id = SET_ID_SLIDESHOW_FADE, .label = "Fade", .section = "Background",
        .key = SETTING_SLIDESHOW_TRANSITION_TIME, .type = SET_TYPE_MILLIS, .min = 0, .max = 3000 },
    [SET_ID_LAYOUT_ROWS] = { .id = SET_ID_LAYOUT_ROWS, .label = "Rows", .section = "Layout", .key = SETTING_ROWS,
        .type = SET_TYPE_COUNT, .min = 1, .max = 10, .refresh = SET_REFRESH_LAYOUT },
    [SET_ID_LAYOUT_COLUMNS] = { .id = SET_ID_LAYOUT_COLUMNS, .label = "Columns", .section = "Layout",
        .key = SETTING_COLUMNS, .alias = SETTING_MAX_BUTTONS, .type = SET_TYPE_COUNT, .min = 1, .max = 12,
        .refresh = SET_REFRESH_LAYOUT },
    [SET_ID_LAYOUT_ICON_SIZE] = { .id = SET_ID_LAYOUT_ICON_SIZE, .label = "Largest button", .section = "Layout",
        .key = SETTING_ICON_SIZE, .type = SET_TYPE_ICON_SIZE, .can_inherit = true, .refresh = SET_REFRESH_LAYOUT },
    [SET_ID_TITLE_SIZE] = { .id = SET_ID_TITLE_SIZE, .label = "Size", .section = "Titles",
        .key = SETTING_TITLE_FONT_SIZE, .type = SET_TYPE_TITLE_SIZE, .refresh = SET_REFRESH_TITLES },

    // General
    [SET_ID_DEFAULT_MENU] = { .id = SET_ID_DEFAULT_MENU, .label = "Default menu", .section = "General",
        .key = SETTING_DEFAULT_MENU, .type = SET_TYPE_MENU },
    [SET_ID_WRAP_ENTRIES] = { .id = SET_ID_WRAP_ENTRIES, .label = "Wrap around", .section = "General",
        .key = SETTING_WRAP_ENTRIES, .type = SET_TYPE_BOOL },
    [SET_ID_RESET_ON_BACK] = { .id = SET_ID_RESET_ON_BACK, .label = "Reset on Back", .section = "General",
        .key = SETTING_RESET_ON_BACK, .type = SET_TYPE_BOOL },
    [SET_ID_MOUSE_SELECT] = { .id = SET_ID_MOUSE_SELECT, .label = "Mouse select", .section = "General",
        .key = SETTING_MOUSE_SELECT, .type = SET_TYPE_BOOL },
    [SET_ID_INHIBIT_OS_SCREENSAVER] = { .id = SET_ID_INHIBIT_OS_SCREENSAVER, .label = "Block the OS screensaver",
        .section = "General", .key = SETTING_INHIBIT_OS_SCREENSAVER, .type = SET_TYPE_BOOL },
    [SET_ID_VSYNC] = { .id = SET_ID_VSYNC, .label = "VSync", .section = "General", .key = SETTING_VSYNC,
        .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_FRAME },
    [SET_ID_FPS_LIMIT] = { .id = SET_ID_FPS_LIMIT, .label = "FPS limit", .section = "General",
        .key = SETTING_FPS_LIMIT, .type = SET_TYPE_NUMBER, .min = 10, .max = 1000, .can_inherit = true,
        .refresh = SET_REFRESH_FRAME, .steps = FPS_STEPS, .step_count = LENGTH(FPS_STEPS), .unit = " fps",
        .inherit_label = "Off" },
    [SET_ID_ON_LAUNCH] = { .id = SET_ID_ON_LAUNCH, .label = "After launching an app", .section = "General",
        .key = SETTING_ON_LAUNCH, .type = SET_TYPE_CHOICE, .lo = 0, .hi = 2, .names = ON_LAUNCH_NAMES,
        .labels = ON_LAUNCH_LABELS },
    [SET_ID_APPLICATION_TIMEOUT] = { .id = SET_ID_APPLICATION_TIMEOUT, .label = "App timeout", .section = "General",
        .key = SETTING_APPLICATION_TIMEOUT, .type = SET_TYPE_SECONDS, .min = 3, .max = 30,
        .steps = TIMEOUT_STEPS, .step_count = LENGTH(TIMEOUT_STEPS) },
    [SET_ID_STARTUP_CMD] = { .id = SET_ID_STARTUP_CMD, .label = "Startup command", .section = "General",
        .key = SETTING_STARTUP_CMD, .type = SET_TYPE_COMMAND, .can_inherit = true, .inherit_label = "None" },
    [SET_ID_QUIT_CMD] = { .id = SET_ID_QUIT_CMD, .label = "Quit command", .section = "General",
        .key = SETTING_QUIT_CMD, .type = SET_TYPE_COMMAND, .can_inherit = true, .inherit_label = "None" },

    // Background, beyond 3a's
    [SET_ID_CHROMA_KEY_COLOR] = { .id = SET_ID_CHROMA_KEY_COLOR, .label = "See-through colour", .section = "Background",
        .key = SETTING_CHROMA_KEY_COLOR, .type = SET_TYPE_COLOR, .refresh = SET_REFRESH_BACKGROUND },
    [SET_ID_OVERLAY] = { .id = SET_ID_OVERLAY, .label = "Overlay", .section = "Background",
        .key = SETTING_BACKGROUND_OVERLAY, .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_BACKGROUND },
    [SET_ID_OVERLAY_COLOR] = { .id = SET_ID_OVERLAY_COLOR, .label = "Overlay colour", .section = "Background",
        .key = SETTING_BACKGROUND_OVERLAY_COLOR, .type = SET_TYPE_COLOR, .refresh = SET_REFRESH_BACKGROUND },
    [SET_ID_OVERLAY_OPACITY] = { .id = SET_ID_OVERLAY_OPACITY, .label = "Overlay opacity", .section = "Background",
        .key = SETTING_BACKGROUND_OVERLAY_OPACITY, .type = SET_TYPE_PERCENT, .min = 0, .max = 10000,
        .refresh = SET_REFRESH_BACKGROUND, .lo = 0, .hi = 10000, .step = 500,
        .fallback = DEFAULT_BACKGROUND_OVERLAY_OPACITY },

    // Layout, beyond 3a's (the All menus page)
    [SET_ID_ICON_SPACING] = { .id = SET_ID_ICON_SPACING, .label = "Icon spacing", .section = "Layout",
        .key = SETTING_ICON_SPACING, .type = SET_TYPE_PERCENT, .min = 0, .max = 10000, .refresh = SET_REFRESH_LAYOUT,
        .lo = 0, .hi = 1000, .step = 100, .max_px = INT_MAX, .flags = SET_FLAG_PX, .fallback = DEFAULT_ICON_SPACING },
    [SET_ID_VCENTER] = { .id = SET_ID_VCENTER, .label = "Vertical centre", .section = "Layout",
        .key = SETTING_VCENTER, .type = SET_TYPE_PERCENT, .min = 0, .max = 10000, .refresh = SET_REFRESH_LAYOUT,
        .lo = 2500, .hi = 7500, .step = 500, .fallback = DEFAULT_VCENTER },

    // Titles, beyond 3a's
    [SET_ID_TITLES_ENABLED] = { .id = SET_ID_TITLES_ENABLED, .label = "Show titles", .section = "Titles",
        .key = SETTING_TITLES_ENABLED, .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_TITLE_FONT },
    [SET_ID_TITLE_FONT] = { .id = SET_ID_TITLE_FONT, .label = "Font", .section = "Titles",
        .key = SETTING_TITLE_FONT, .type = SET_TYPE_FONT, .refresh = SET_REFRESH_TITLE_FONT },
    [SET_ID_TITLE_FONT_FACE] = { .id = SET_ID_TITLE_FONT_FACE, .label = "Font face", .section = "Titles",
        .key = SETTING_TITLE_FONT_FACE, .type = SET_TYPE_NUMBER, .min = 0, .max = 65535, .can_inherit = true,
        .refresh = SET_REFRESH_TITLE_FONT, .flags = SET_FLAG_HIDDEN, .inherit_label = "0" },
    [SET_ID_TITLE_COLOR] = { .id = SET_ID_TITLE_COLOR, .label = "Colour", .section = "Titles",
        .key = SETTING_TITLE_FONT_COLOR, .type = SET_TYPE_COLOR, .refresh = SET_REFRESH_TITLES },
    [SET_ID_TITLE_OPACITY] = { .id = SET_ID_TITLE_OPACITY, .label = "Opacity", .section = "Titles",
        .key = SETTING_TITLE_OPACITY, .type = SET_TYPE_PERCENT, .min = 0, .max = 10000, .refresh = SET_REFRESH_TITLES,
        .lo = 0, .hi = 10000, .step = 500, .fallback = DEFAULT_TITLE_OPACITY },
    [SET_ID_TITLE_SHADOWS] = { .id = SET_ID_TITLE_SHADOWS, .label = "Shadows", .section = "Titles",
        .key = SETTING_TITLE_SHADOWS, .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_TITLES },
    [SET_ID_TITLE_SHADOW_COLOR] = { .id = SET_ID_TITLE_SHADOW_COLOR, .label = "Shadow colour", .section = "Titles",
        .key = SETTING_TITLE_SHADOW_COLOR, .type = SET_TYPE_COLOR, .refresh = SET_REFRESH_TITLES },
    [SET_ID_TITLE_OVERSIZE] = { .id = SET_ID_TITLE_OVERSIZE, .label = "Too long", .section = "Titles",
        .key = SETTING_TITLE_OVERSIZE_MODE, .type = SET_TYPE_CHOICE, .refresh = SET_REFRESH_TITLES,
        .lo = 0, .hi = 1, .names = OVERSIZE_NAMES, .labels = OVERSIZE_LABELS, .legacy = "Truncated", .legacy_index = 0 },
    [SET_ID_TITLE_PADDING] = { .id = SET_ID_TITLE_PADDING, .label = "Padding", .section = "Titles",
        .key = SETTING_TITLE_PADDING, .type = SET_TYPE_PERCENT, .min = 0, .max = 5000, .refresh = SET_REFRESH_TITLES,
        .lo = 0, .hi = 2000, .step = 200, .max_px = LAYOUT_MAX_BUTTON, .flags = SET_FLAG_PX | SET_FLAG_WHOLE },

    // Highlight
    [SET_ID_HIGHLIGHT_ENABLED] = { .id = SET_ID_HIGHLIGHT_ENABLED, .label = "Show", .section = "Highlight",
        .key = SETTING_HIGHLIGHT_ENABLED, .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_HIGHLIGHT },
    [SET_ID_HIGHLIGHT_FILL_COLOR] = { .id = SET_ID_HIGHLIGHT_FILL_COLOR, .label = "Fill colour", .section = "Highlight",
        .key = SETTING_HIGHLIGHT_FILL_COLOR, .type = SET_TYPE_COLOR, .refresh = SET_REFRESH_HIGHLIGHT },
    [SET_ID_HIGHLIGHT_FILL_OPACITY] = { .id = SET_ID_HIGHLIGHT_FILL_OPACITY, .label = "Fill opacity",
        .section = "Highlight", .key = SETTING_HIGHLIGHT_FILL_OPACITY, .type = SET_TYPE_PERCENT, .min = 0,
        .max = 10000, .refresh = SET_REFRESH_HIGHLIGHT, .lo = 0, .hi = 10000, .step = 500,
        .fallback = DEFAULT_HIGHLIGHT_FILL_OPACITY },
    [SET_ID_HIGHLIGHT_OUTLINE_SIZE] = { .id = SET_ID_HIGHLIGHT_OUTLINE_SIZE, .label = "Outline size",
        .section = "Highlight", .key = SETTING_HIGHLIGHT_OUTLINE_SIZE, .type = SET_TYPE_NUMBER, .min = 0,
        .max = INT_MAX, .refresh = SET_REFRESH_HIGHLIGHT, .lo = 0, .hi = 10, .step = 1, .unit = " px" },
    [SET_ID_HIGHLIGHT_OUTLINE_COLOR] = { .id = SET_ID_HIGHLIGHT_OUTLINE_COLOR, .label = "Outline colour",
        .section = "Highlight", .key = SETTING_HIGHLIGHT_OUTLINE_COLOR, .type = SET_TYPE_COLOR,
        .refresh = SET_REFRESH_HIGHLIGHT },
    [SET_ID_HIGHLIGHT_OUTLINE_OPACITY] = { .id = SET_ID_HIGHLIGHT_OUTLINE_OPACITY, .label = "Outline opacity",
        .section = "Highlight", .key = SETTING_HIGHLIGHT_OUTLINE_OPACITY, .type = SET_TYPE_PERCENT, .min = 0,
        .max = 10000, .refresh = SET_REFRESH_HIGHLIGHT, .lo = 0, .hi = 10000, .step = 500,
        .fallback = DEFAULT_HIGHLIGHT_OUTLINE_OPACITY },
    [SET_ID_HIGHLIGHT_CORNER_RADIUS] = { .id = SET_ID_HIGHLIGHT_CORNER_RADIUS, .label = "Corner radius",
        .section = "Highlight", .key = SETTING_HIGHLIGHT_CORNER_RADIUS, .type = SET_TYPE_NUMBER, .min = 0,
        .max = 100, .refresh = SET_REFRESH_HIGHLIGHT, .lo = 0, .hi = 100, .step = 5 },
    [SET_ID_HIGHLIGHT_VPADDING] = { .id = SET_ID_HIGHLIGHT_VPADDING, .label = "Vertical padding",
        .section = "Highlight", .key = SETTING_HIGHLIGHT_VPADDING, .type = SET_TYPE_NUMBER, .min = 0,
        .max = INT_MAX, .refresh = SET_REFRESH_LAYOUT, .lo = 0, .hi = 100, .step = 5, .unit = " px" },
    [SET_ID_HIGHLIGHT_HPADDING] = { .id = SET_ID_HIGHLIGHT_HPADDING, .label = "Horizontal padding",
        .section = "Highlight", .key = SETTING_HIGHLIGHT_HPADDING, .type = SET_TYPE_NUMBER, .min = 0,
        .max = INT_MAX, .refresh = SET_REFRESH_LAYOUT, .lo = 0, .hi = 100, .step = 5, .unit = " px" },

    // Scroll indicators
    [SET_ID_SCROLL_ENABLED] = { .id = SET_ID_SCROLL_ENABLED, .label = "Show", .section = "Scroll Indicators",
        .key = SETTING_SCROLL_INDICATORS, .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_SCROLL },
    [SET_ID_SCROLL_FILL_COLOR] = { .id = SET_ID_SCROLL_FILL_COLOR, .label = "Fill colour",
        .section = "Scroll Indicators", .key = SETTING_SCROLL_INDICATOR_FILL_COLOR, .type = SET_TYPE_COLOR,
        .refresh = SET_REFRESH_SCROLL },
    [SET_ID_SCROLL_OUTLINE_SIZE] = { .id = SET_ID_SCROLL_OUTLINE_SIZE, .label = "Outline size",
        .section = "Scroll Indicators", .key = SETTING_SCROLL_INDICATOR_OUTLINE_SIZE, .type = SET_TYPE_NUMBER,
        .min = 0, .max = INT_MAX, .refresh = SET_REFRESH_SCROLL, .lo = 0, .hi = 10, .step = 1, .unit = " px" },
    [SET_ID_SCROLL_OUTLINE_COLOR] = { .id = SET_ID_SCROLL_OUTLINE_COLOR, .label = "Outline colour",
        .section = "Scroll Indicators", .key = SETTING_SCROLL_INDICATOR_OUTLINE_COLOR, .type = SET_TYPE_COLOR,
        .refresh = SET_REFRESH_SCROLL },
    [SET_ID_SCROLL_OPACITY] = { .id = SET_ID_SCROLL_OPACITY, .label = "Opacity", .section = "Scroll Indicators",
        .key = SETTING_SCROLL_INDICATOR_OPACITY, .type = SET_TYPE_PERCENT, .min = 0, .max = 10000,
        .refresh = SET_REFRESH_SCROLL, .lo = 0, .hi = 10000, .step = 500, .fallback = DEFAULT_SCROLL_INDICATOR_OPACITY },

    // Clock
    [SET_ID_CLOCK_ENABLED] = { .id = SET_ID_CLOCK_ENABLED, .label = "Show", .section = "Clock",
        .key = SETTING_CLOCK_ENABLED, .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_CLOCK },
    [SET_ID_CLOCK_SHOW_DATE] = { .id = SET_ID_CLOCK_SHOW_DATE, .label = "Show date", .section = "Clock",
        .key = SETTING_CLOCK_SHOW_DATE, .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_CLOCK },
    [SET_ID_CLOCK_ALIGNMENT] = { .id = SET_ID_CLOCK_ALIGNMENT, .label = "Alignment", .section = "Clock",
        .key = SETTING_CLOCK_ALIGNMENT, .type = SET_TYPE_CHOICE, .refresh = SET_REFRESH_CLOCK, .lo = 0, .hi = 1,
        .names = ALIGNMENT_NAMES, .labels = ALIGNMENT_LABELS },
    [SET_ID_CLOCK_FONT] = { .id = SET_ID_CLOCK_FONT, .label = "Font", .section = "Clock", .key = SETTING_CLOCK_FONT,
        .type = SET_TYPE_FONT, .refresh = SET_REFRESH_CLOCK },
    [SET_ID_CLOCK_FONT_FACE] = { .id = SET_ID_CLOCK_FONT_FACE, .label = "Font face", .section = "Clock",
        .key = SETTING_CLOCK_FONT_FACE, .type = SET_TYPE_NUMBER, .min = 0, .max = 65535, .can_inherit = true,
        .refresh = SET_REFRESH_CLOCK, .flags = SET_FLAG_HIDDEN, .inherit_label = "0" },
    [SET_ID_CLOCK_COLOR] = { .id = SET_ID_CLOCK_COLOR, .label = "Colour", .section = "Clock",
        .key = SETTING_CLOCK_FONT_COLOR, .type = SET_TYPE_COLOR, .refresh = SET_REFRESH_CLOCK },
    [SET_ID_CLOCK_SHADOWS] = { .id = SET_ID_CLOCK_SHADOWS, .label = "Shadows", .section = "Clock",
        .key = SETTING_CLOCK_SHADOWS, .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_CLOCK },
    [SET_ID_CLOCK_SHADOW_COLOR] = { .id = SET_ID_CLOCK_SHADOW_COLOR, .label = "Shadow colour", .section = "Clock",
        .key = SETTING_CLOCK_SHADOW_COLOR, .type = SET_TYPE_COLOR, .refresh = SET_REFRESH_CLOCK },
    [SET_ID_CLOCK_OPACITY] = { .id = SET_ID_CLOCK_OPACITY, .label = "Opacity", .section = "Clock",
        .key = SETTING_CLOCK_OPACITY, .type = SET_TYPE_PERCENT, .min = 0, .max = 10000, .refresh = SET_REFRESH_CLOCK,
        .lo = 0, .hi = 10000, .step = 500, .fallback = DEFAULT_CLOCK_OPACITY },
    [SET_ID_CLOCK_FONT_SIZE] = { .id = SET_ID_CLOCK_FONT_SIZE, .label = "Size", .section = "Clock",
        .key = SETTING_CLOCK_FONT_SIZE, .type = SET_TYPE_NUMBER, .min = 1, .max = INT_MAX,
        .refresh = SET_REFRESH_CLOCK, .lo = 20, .hi = 120, .step = 5 },
    [SET_ID_CLOCK_MARGIN] = { .id = SET_ID_CLOCK_MARGIN, .label = "Margin", .section = "Clock",
        .key = SETTING_CLOCK_MARGIN, .type = SET_TYPE_PERCENT, .min = 0, .max = 10000, .refresh = SET_REFRESH_CLOCK,
        .lo = 0, .hi = 1000, .step = 100, .max_px = INT_MAX, .flags = SET_FLAG_PX, .fallback = DEFAULT_CLOCK_MARGIN },
    [SET_ID_CLOCK_TIME_FORMAT] = { .id = SET_ID_CLOCK_TIME_FORMAT, .label = "Time", .section = "Clock",
        .key = SETTING_CLOCK_TIME_FORMAT, .type = SET_TYPE_CHOICE, .refresh = SET_REFRESH_CLOCK, .lo = 0, .hi = 2,
        .names = TIME_NAMES, .labels = TIME_LABELS },
    [SET_ID_CLOCK_DATE_FORMAT] = { .id = SET_ID_CLOCK_DATE_FORMAT, .label = "Date", .section = "Clock",
        .key = SETTING_CLOCK_DATE_FORMAT, .type = SET_TYPE_CHOICE, .refresh = SET_REFRESH_CLOCK, .lo = 0, .hi = 2,
        .names = DATE_NAMES, .labels = DATE_LABELS },
    [SET_ID_CLOCK_WEEKDAY] = { .id = SET_ID_CLOCK_WEEKDAY, .label = "Weekday", .section = "Clock",
        .key = SETTING_CLOCK_INCLUDE_WEEKDAY, .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_CLOCK },

    // Screensaver
    [SET_ID_SCREENSAVER_ENABLED] = { .id = SET_ID_SCREENSAVER_ENABLED, .label = "On", .section = "Screensaver",
        .key = SETTING_SCREENSAVER_ENABLED, .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_SCREENSAVER },
    [SET_ID_SCREENSAVER_IDLE_TIME] = { .id = SET_ID_SCREENSAVER_IDLE_TIME, .label = "Idle time",
        .section = "Screensaver", .key = SETTING_SCREENSAVER_IDLE_TIME, .type = SET_TYPE_SECONDS, .min = 3,
        .max = 900, .steps = IDLE_STEPS, .step_count = LENGTH(IDLE_STEPS) },
    [SET_ID_SCREENSAVER_INTENSITY] = { .id = SET_ID_SCREENSAVER_INTENSITY, .label = "Dim level",
        .section = "Screensaver", .key = SETTING_SCREENSAVER_INTENSITY, .type = SET_TYPE_PERCENT, .min = 0,
        .max = 10000, .refresh = SET_REFRESH_SCREENSAVER, .lo = 1000, .hi = 10000, .step = 1000,
        .fallback = DEFAULT_SCREENSAVER_INTENSITY },
    [SET_ID_SCREENSAVER_PAUSE] = { .id = SET_ID_SCREENSAVER_PAUSE, .label = "Pause slideshow",
        .section = "Screensaver", .key = SETTING_SCREENSAVER_PAUSE_SLIDESHOW, .type = SET_TYPE_BOOL },

    // Gamepad
    [SET_ID_GAMEPAD_ENABLED] = { .id = SET_ID_GAMEPAD_ENABLED, .label = "On", .section = "Gamepad",
        .key = SETTING_GAMEPAD_ENABLED, .type = SET_TYPE_BOOL, .refresh = SET_REFRESH_GAMEPAD },
    [SET_ID_GAMEPAD_DEVICE] = { .id = SET_ID_GAMEPAD_DEVICE, .label = "Device", .section = "Gamepad",
        .key = SETTING_GAMEPAD_DEVICE, .type = SET_TYPE_DEVICE, .min = -1, .max = 15, .refresh = SET_REFRESH_GAMEPAD },
    [SET_ID_GAMEPAD_MAPPINGS] = { .id = SET_ID_GAMEPAD_MAPPINGS, .label = "Mappings file", .section = "Gamepad",
        .key = SETTING_GAMEPAD_MAPPINGS_FILE, .type = SET_TYPE_PATH, .flags = SET_FLAG_NEXT_START },

    // Per menu
    [SET_ID_MENU_ROWS] = { .id = SET_ID_MENU_ROWS, .label = "Rows", .key = SETTING_ROWS, .type = SET_TYPE_COUNT,
        .min = 1, .max = 10, .can_inherit = true, .refresh = SET_REFRESH_LAYOUT },
    [SET_ID_MENU_COLUMNS] = { .id = SET_ID_MENU_COLUMNS, .label = "Columns", .key = SETTING_COLUMNS,
        .type = SET_TYPE_COUNT, .min = 1, .max = 12, .can_inherit = true, .refresh = SET_REFRESH_LAYOUT },
    [SET_ID_MENU_ICON_SIZE] = { .id = SET_ID_MENU_ICON_SIZE, .label = "Largest button", .key = SETTING_ICON_SIZE,
        .type = SET_TYPE_ICON_SIZE, .can_inherit = true, .refresh = SET_REFRESH_LAYOUT }
};

// A function to find a global setting by the section and key config.ini gives it (or its older
// name); NULL for a key the table does not hold, and for the per-menu keys, which menu sections hold
const SettingDef *setting_find(const char *section, const char *key)
{
    for (int i = 0; i < SET_ID_GLOBAL_COUNT; i++) {
        const SettingDef *def = &DEFS[i];
        if (strcmp(def->section, section) == 0 &&
            (strcmp(def->key, key) == 0 || (def->alias != NULL && strcmp(def->alias, key) == 0)))
            return def;
    }
    return NULL;
}

// A function to get a setting's row in the table
const SettingDef *setting_def(SettingId id)
{
    return &DEFS[id];
}

// A function to read "#RRGGBB", strictly: six hex digits
static bool parse_hex_color(const char *text, SettingColor *color)
{
    if (text[0] != '#' || strlen(text) != 7)
        return false;
    unsigned int rgb = 0;
    for (int i = 1; i < 7; i++) {
        char c = text[i];
        unsigned int digit;
        if (c >= '0' && c <= '9')
            digit = (unsigned int) (c - '0');
        else if (c >= 'a' && c <= 'f')
            digit = (unsigned int) (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            digit = (unsigned int) (c - 'A' + 10);
        else
            return false;
        rgb = rgb * 16 + digit;
    }
    color->r = (unsigned char) (rgb >> 16);
    color->g = (unsigned char) ((rgb >> 8) & 0xFF);
    color->b = (unsigned char) (rgb & 0xFF);
    return true;
}

// A function to count a choice setting's names
static int choice_count(const SettingDef *def)
{
    int count = 0;
    while (def->names[count] != NULL)
        count++;
    return count;
}

// A function to read a whole number, strictly: an optional minus and digits, within an int.
// f15-limits' IconSpacing=2000000000 must still read, as atoi read it.
static bool parse_integer(const char *text, int *number)
{
    const char *digits = text[0] == '-' ? text + 1 : text;
    size_t length = strlen(digits);
    if (length == 0 || length > 10)
        return false;
    long long n = 0;
    for (size_t i = 0; i < length; i++) {
        if (digits[i] < '0' || digits[i] > '9')
            return false;
        n = n * 10 + (digits[i] - '0');
    }
    if (n > INT_MAX)
        return false;
    *number = (int) (text[0] == '-' ? -n : n);
    return true;
}

// A function to read "N%", "N.N%" or "N.NN%" into hundredths of a percent; with `whole`, "N%" only
static bool parse_percent(const char *text, bool whole, int *hundredths)
{
    size_t length = strlen(text);
    if (length < 2 || text[length - 1] != '%')
        return false;
    size_t end = length - 1;
    size_t i = 0;
    int number = 0;
    while (i < end && text[i] >= '0' && text[i] <= '9') {
        if (i >= 5)
            return false;
        number = number * 10 + (text[i] - '0');
        i++;
    }
    if (i == 0)
        return false;
    int fraction = 0;
    if (i < end) {
        if (whole || text[i] != '.')
            return false;
        size_t first = ++i;
        while (i < end && text[i] >= '0' && text[i] <= '9' && i - first < 2) {
            fraction = fraction * 10 + (text[i] - '0');
            i++;
        }
        if (i != end || i == first)
            return false;
        if (i - first == 1)
            fraction *= 10;
    }
    *hundredths = number * 100 + fraction;
    return true;
}

// A function to find a name among a choice setting's names, or its older spelling; -1 when neither
static int choice_index(const SettingDef *def, const char *text)
{
    for (int i = 0; def->names[i] != NULL; i++) {
        if (strcmp(def->names[i], text) == 0)
            return i;
    }
    return def->legacy != NULL && strcmp(def->legacy, text) == 0 ? def->legacy_index : -1;
}

// A function to copy a path from the file, dropping quotes round it as clean_path() does
static bool copy_path_text(const char *text, char *out)
{
    size_t length = strlen(text);
    if (length == 0 || length >= SETTING_TEXT_MAX)
        return false;
    if (length >= 3 && text[0] == '"' && text[length - 1] == '"') {
        memcpy(out, text + 1, length - 2);
        out[length - 2] = '\0';
    }
    else
        memcpy(out, text, length + 1);
    return true;
}

// A function to read a setting's value from config.ini's text, as the launcher has always read it,
// except where the spec fixes a bug (FPSLimit=10, a negative clock size) or asks for a log line
// in place of a silent misreading ("40px", "abc%")
bool setting_parse(const SettingDef *def, const char *text, SettingValue *value)
{
    SettingValue v;
    memset(&v, 0, sizeof(v));
    switch (def->type) {
        case SET_TYPE_COUNT:
            if (!layout_parse_count(text, &v.number))
                return false;
            break;
        case SET_TYPE_ICON_SIZE:
            if (!layout_parse_icon_size(text, &v.number))
                return false;
            break;
        case SET_TYPE_CHOICE:
            v.number = choice_index(def, text);
            if (v.number < 0)
                return false;
            break;
        case SET_TYPE_COLOR:
            if (!parse_hex_color(text, &v.color))
                return false;
            break;
        case SET_TYPE_PATH:
        case SET_TYPE_FONT:
            if (!copy_path_text(text, v.text))
                return false;
            break;
        case SET_TYPE_COMMAND:
        case SET_TYPE_MENU: {
            size_t length = strlen(text);
            if (length == 0 || length >= SETTING_TEXT_MAX)
                return false;
            memcpy(v.text, text, length + 1);
            break;
        }
        case SET_TYPE_SECONDS:
            v.number = atoi(text);
            if (v.number < def->min || v.number > def->max)
                return false;
            break;
        case SET_TYPE_MILLIS: {
            double seconds = atof(text);
            if (!(seconds >= 0.0) || seconds * 1000.0 >= (double) def->max + 0.5)
                return false;
            v.number = (int) (seconds * 1000.0 + 0.5);
            break;
        }
        case SET_TYPE_TITLE_SIZE:
            if (!layout_parse_title_size(text, &v.number, &v.percent))
                return false;
            break;
        case SET_TYPE_BOOL:
            if (strcmp(text, "true") == 0 || strcmp(text, "True") == 0)
                v.number = 1;
            else if (strcmp(text, "false") != 0 && strcmp(text, "False") != 0)
                return false;
            break;
        case SET_TYPE_NUMBER:
        case SET_TYPE_DEVICE:
            if (!parse_integer(text, &v.number) || v.number < def->min || v.number > def->max)
                return false;
            break;
        case SET_TYPE_PERCENT:
            if (parse_percent(text, (def->flags & SET_FLAG_WHOLE) != 0, &v.number)) {
                v.percent = true;
                if (v.number < def->min || v.number > def->max)
                    return false;
            }
            else if (!(def->flags & SET_FLAG_PX) || text[0] == '-' || !parse_integer(text, &v.number) ||
                     v.number > def->max_px)
                return false;
            break;
    }
    *value = v;
    return true;
}

// A function to write ms as seconds with only the decimals it needs: 1500 is "1.5", 3000 is "3"
static void format_millis(int ms, char *out, size_t size)
{
    int whole = ms / 1000;
    int part = ms % 1000;
    if (part == 0)
        snprintf(out, size, "%d", whole);
    else if (part % 100 == 0)
        snprintf(out, size, "%d.%d", whole, part / 100);
    else if (part % 10 == 0)
        snprintf(out, size, "%d.%02d", whole, part / 10);
    else
        snprintf(out, size, "%d.%03d", whole, part);
}

// A function to write hundredths of a percent with only the decimals they need: 1250 is "12.5%"
static void format_percent(int hundredths, char *out, size_t size)
{
    int whole = hundredths / 100;
    int part = hundredths % 100;
    if (part == 0)
        snprintf(out, size, "%d%%", whole);
    else if (part % 10 == 0)
        snprintf(out, size, "%d.%d%%", whole, part / 10);
    else
        snprintf(out, size, "%d.%02d%%", whole, part);
}

// A function to write a value as config.ini holds it; "" for a value that removes the key
void setting_format(const SettingDef *def, const SettingValue *value, char *out, size_t size)
{
    if (size == 0)
        return;
    out[0] = '\0';
    if (value->inherit)
        return;
    switch (def->type) {
        case SET_TYPE_COUNT:
        case SET_TYPE_ICON_SIZE:
        case SET_TYPE_SECONDS:
        case SET_TYPE_NUMBER:
        case SET_TYPE_DEVICE:
            snprintf(out, size, "%d", value->number);
            break;
        case SET_TYPE_CHOICE:
            if (value->number >= 0 && value->number < choice_count(def))
                snprintf(out, size, "%s", def->names[value->number]);
            break;
        case SET_TYPE_COLOR:
            snprintf(out, size, "#%02X%02X%02X", value->color.r, value->color.g, value->color.b);
            break;
        case SET_TYPE_PATH:
        case SET_TYPE_FONT:
        case SET_TYPE_COMMAND:
        case SET_TYPE_MENU:
            snprintf(out, size, "%s", value->text);
            break;
        case SET_TYPE_MILLIS:
            format_millis(value->number, out, size);
            break;
        case SET_TYPE_TITLE_SIZE:
            if (value->percent)
                snprintf(out, size, "%d%%", value->number);
            else
                snprintf(out, size, "%d", value->number);
            break;
        case SET_TYPE_BOOL:
            snprintf(out, size, "%s", value->number ? "true" : "false");
            break;
        case SET_TYPE_PERCENT:
            if (value->percent)
                format_percent(value->number, out, size);
            else
                snprintf(out, size, "%d", value->number);
            break;
    }
}

// A function to compare two values of a setting
bool setting_equal(const SettingDef *def, const SettingValue *a, const SettingValue *b)
{
    if (a->inherit || b->inherit)
        return a->inherit == b->inherit;
    switch (def->type) {
        case SET_TYPE_COLOR:
            return a->color.r == b->color.r && a->color.g == b->color.g && a->color.b == b->color.b;
        case SET_TYPE_PATH:
        case SET_TYPE_FONT:
        case SET_TYPE_COMMAND:
        case SET_TYPE_MENU:
            return strcmp(a->text, b->text) == 0;
        case SET_TYPE_TITLE_SIZE:
        case SET_TYPE_PERCENT:
            return a->percent == b->percent && a->number == b->number;
        default:
            return a->number == b->number;
    }
}

// One step a row can take; a SettingValue without its path buffer
typedef struct {
    bool inherit;
    int number;
    bool percent;
    SettingColor color;
} Candidate;

#define MAX_CANDIDATES 64

// A function to find a colour among the presets; -1 when it is not one
static int preset_index(SettingColor color)
{
    for (int i = 0; i < COLORPICK_PRESETS; i++) {
        SettingColor preset = colorpick_swatch(i);
        if (preset.r == color.r && preset.g == color.g && preset.b == color.b)
            return i;
    }
    return -1;
}

// A function to give a step its place: following the default first, then a custom colour, a fixed
// title size or a px value, then the rest in order
static long long sort_key(const SettingDef *def, const Candidate *c)
{
    if (c->inherit)
        return LLONG_MIN;
    if (def->type == SET_TYPE_COLOR)
        return preset_index(c->color);
    if (def->type == SET_TYPE_TITLE_SIZE)
        return c->percent ? c->number : -1;
    if (def->type == SET_TYPE_PERCENT)   // Above every int, so any px value sorts first
        return c->percent ? 0x100000000LL + c->number : c->number;
    return c->number;
}

// A function to compare two steps
static bool same_candidate(const SettingDef *def, const Candidate *a, const Candidate *b)
{
    if (a->inherit || b->inherit)
        return a->inherit == b->inherit;
    if (def->type == SET_TYPE_COLOR)
        return a->color.r == b->color.r && a->color.g == b->color.g && a->color.b == b->color.b;
    if (def->type == SET_TYPE_TITLE_SIZE || def->type == SET_TYPE_PERCENT)
        return a->percent == b->percent && a->number == b->number;
    return a->number == b->number;
}

// A function to add a step to a list once
static void add_candidate(const SettingDef *def, Candidate *list, int *count, Candidate c)
{
    for (int i = 0; i < *count; i++) {
        if (same_candidate(def, &list[i], &c))
            return;
    }
    if (*count < MAX_CANDIDATES)
        list[(*count)++] = c;
}

// A function to make a step from a number
static Candidate number_candidate(int number, bool percent)
{
    Candidate c;
    memset(&c, 0, sizeof(c));
    c.number = number;
    c.percent = percent;
    return c;
}

// A function to make a step from a value
static Candidate value_candidate(const SettingValue *value)
{
    Candidate c;
    memset(&c, 0, sizeof(c));
    c.inherit = value->inherit;
    c.number = value->number;
    c.percent = value->percent;
    c.color = value->color;
    return c;
}

// A function to list a row's steps in order. The value on entry and the current value are always
// among them, in their sorted place, so the user can step back to what the file said.
static int build_candidates(const SettingDef *def, const SettingValue *current, const SettingValue *entry, Candidate *list)
{
    int count = 0;
    if (def->can_inherit) {
        Candidate inherit = number_candidate(0, false);
        inherit.inherit = true;
        add_candidate(def, list, &count, inherit);
    }
    switch (def->type) {
        case SET_TYPE_COUNT:
            for (int n = def->min; n <= def->max; n++)
                add_candidate(def, list, &count, number_candidate(n, false));
            break;
        case SET_TYPE_CHOICE:
            for (int n = def->lo; n <= def->hi; n++)
                add_candidate(def, list, &count, number_candidate(n, false));
            break;
        case SET_TYPE_BOOL:
            add_candidate(def, list, &count, number_candidate(0, false));
            add_candidate(def, list, &count, number_candidate(1, false));
            break;
        case SET_TYPE_ICON_SIZE:
            for (int i = 0; i < LENGTH(ICON_STEPS); i++)
                add_candidate(def, list, &count, number_candidate(ICON_STEPS[i], false));
            break;
        case SET_TYPE_SECONDS:
        case SET_TYPE_NUMBER:
            if (def->steps != NULL) {
                for (int i = 0; i < def->step_count; i++)
                    add_candidate(def, list, &count, number_candidate(def->steps[i], false));
            }
            else if (def->step > 0) {
                for (int n = def->lo; n <= def->hi; n += def->step)
                    add_candidate(def, list, &count, number_candidate(n, false));
            }
            break;
        case SET_TYPE_PERCENT:
            for (int n = def->lo; def->step > 0 && n <= def->hi; n += def->step)
                add_candidate(def, list, &count, number_candidate(n, true));
            break;
        case SET_TYPE_MILLIS:
            for (int i = 0; i < LENGTH(MILLI_STEPS); i++)
                add_candidate(def, list, &count, number_candidate(MILLI_STEPS[i], false));
            break;
        case SET_TYPE_TITLE_SIZE:
            for (int i = 0; i < LENGTH(TITLE_STEPS); i++)
                add_candidate(def, list, &count, number_candidate(TITLE_STEPS[i], true));
            break;
        case SET_TYPE_COLOR:
            for (int i = 0; i < COLORPICK_PRESETS; i++) {
                Candidate c = number_candidate(0, false);
                c.color = colorpick_swatch(i);
                add_candidate(def, list, &count, c);
            }
            break;
        case SET_TYPE_PATH:
        case SET_TYPE_FONT:
        case SET_TYPE_COMMAND:
        case SET_TYPE_MENU:
        case SET_TYPE_DEVICE:
            break;
    }
    add_candidate(def, list, &count, value_candidate(entry));
    add_candidate(def, list, &count, value_candidate(current));

    // Put them in order: an insertion sort, since a row has a few dozen steps at most
    for (int i = 1; i < count; i++) {
        Candidate c = list[i];
        long long key = sort_key(def, &c);
        int j = i - 1;
        while (j >= 0 && sort_key(def, &list[j]) > key) {
            list[j + 1] = list[j];
            j--;
        }
        list[j + 1] = c;
    }
    return count;
}

// A function to step a value one place left (direction < 0) or right; it stops at the ends
SettingValue setting_step(const SettingDef *def, const SettingValue *current, const SettingValue *entry, int direction)
{
    SettingValue result = *current;
    if (def->type == SET_TYPE_PATH || def->type == SET_TYPE_FONT || def->type == SET_TYPE_COMMAND ||
        def->type == SET_TYPE_MENU || def->type == SET_TYPE_DEVICE || direction == 0)
        return result;
    Candidate list[MAX_CANDIDATES];
    int count = build_candidates(def, current, entry, list);
    Candidate now = value_candidate(current);
    int index = -1;
    for (int i = 0; i < count && index < 0; i++) {
        if (same_candidate(def, &list[i], &now))
            index = i;
    }
    int next = index + (direction > 0 ? 1 : -1);
    if (index < 0 || next < 0 || next >= count)
        return result;
    result.inherit = list[next].inherit;
    result.number = list[next].number;
    result.percent = list[next].percent;
    result.color = list[next].color;
    return result;
}

// The special commands and what the screen calls them
static const struct {
    const char *command;
    const char *label;
} COMMAND_LABELS[] = {
    { ":left", "Left" }, { ":right", "Right" }, { ":up", "Up" }, { ":down", "Down" },
    { ":select", "OK" }, { ":back", "Back" }, { ":home", "Home" }, { ":settings", "Settings" },
    { ":quit", "Quit StreamFlex" }, { ":shutdown", "Shut down" }, { ":restart", "Restart" },
    { ":sleep", "Sleep" }, { ":exit", "Close the app on show" }
};

// A function to describe a command as the screen shows it: a special command by what it does,
// ":submenu X" as the submenu it opens, anything else as it is written
void setting_command_label(const char *command, char *out, size_t size)
{
    for (int i = 0; i < LENGTH(COMMAND_LABELS); i++) {
        if (strcmp(command, COMMAND_LABELS[i].command) == 0) {
            snprintf(out, size, "%s", COMMAND_LABELS[i].label);
            return;
        }
    }
    if (strncmp(command, ":submenu ", 9) == 0 && command[9] != '\0')
        snprintf(out, size, "Open submenu: %s", command + 9);
    else
        snprintf(out, size, "%s", command);
}

// A function to describe a value as the screen shows it. `inherited` is the value a per-menu
// setting follows (from [Layout]); NULL for the others.
void setting_describe(const SettingDef *def, const SettingValue *value, const SettingValue *inherited, char *out, size_t size)
{
    char text[64];
    switch (def->type) {
        case SET_TYPE_COUNT:
            if (value->inherit)
                snprintf(out, size, "All menus (%d)", inherited != NULL ? inherited->number : 0);
            else
                snprintf(out, size, "%d", value->number);
            break;
        case SET_TYPE_ICON_SIZE:
            if (!value->inherit)
                snprintf(out, size, "%d px", value->number);
            else if (def->section != NULL)
                snprintf(out, size, "Fill");
            else if (inherited == NULL || inherited->inherit)
                snprintf(out, size, "All menus (Fill)");
            else
                snprintf(out, size, "All menus (%d px)", inherited->number);
            break;
        case SET_TYPE_CHOICE:
            snprintf(out, size, "%s", value->number >= 0 && value->number < choice_count(def)
                                      ? def->labels[value->number] : "?");
            break;
        case SET_TYPE_COLOR: {
            // Named as the colour picker names it: any of its swatches, not only the presets
            int swatch = colorpick_find(value->color);
            if (swatch >= 0)
                snprintf(out, size, "%s", colorpick_name(swatch));
            else
                snprintf(out, size, "Custom #%02X%02X%02X", value->color.r, value->color.g, value->color.b);
            break;
        }
        case SET_TYPE_PATH:
        case SET_TYPE_FONT:
            if (value->text[0] == '\0')
                snprintf(out, size, "Choose" ELLIPSIS);
            else
                fileio_base_name(value->text, out, size);
            break;
        case SET_TYPE_SECONDS:
            if (value->number >= 60 && value->number % 60 == 0)
                snprintf(out, size, "%d min", value->number / 60);
            else
                snprintf(out, size, "%d s", value->number);
            break;
        case SET_TYPE_MILLIS:
            format_millis(value->number, text, sizeof(text));
            snprintf(out, size, "%s s", text);
            break;
        case SET_TYPE_TITLE_SIZE:
            if (!value->percent)
                snprintf(out, size, "Fixed %d", value->number);
            else if (value->number == TITLE_STEPS[0])
                snprintf(out, size, "Small");
            else if (value->number == TITLE_STEPS[1])
                snprintf(out, size, "Medium");
            else if (value->number == TITLE_STEPS[2])
                snprintf(out, size, "Large");
            else
                snprintf(out, size, "%d%%", value->number);
            break;
        case SET_TYPE_BOOL:
            snprintf(out, size, "%s", value->number ? "On" : "Off");
            break;
        case SET_TYPE_NUMBER:
            if (value->inherit)
                snprintf(out, size, "%s", def->inherit_label != NULL ? def->inherit_label : "");
            else
                snprintf(out, size, "%d%s", value->number, def->unit != NULL ? def->unit : "");
            break;
        case SET_TYPE_PERCENT:
            if (value->percent)
                format_percent(value->number, out, size);
            else
                snprintf(out, size, "%d px", value->number);
            break;
        case SET_TYPE_COMMAND:
            if (value->inherit || value->text[0] == '\0')
                snprintf(out, size, "None");
            else
                setting_command_label(value->text, out, size);
            break;
        case SET_TYPE_MENU:
            snprintf(out, size, "%s", value->text);
            break;
        case SET_TYPE_DEVICE:
            if (value->number < 0)
                snprintf(out, size, "Any");
            else
                snprintf(out, size, "Pad %d", value->number);
            break;
    }
}

// One page on the stack
typedef struct {
    SettingsPage page;
    int menu;          // MENU pages: the menu's index, -1 for All menus
    int cursor;
    int entry_mode;    // The background mode when the page opened, for the incomplete-mode rule
    int device;        // The bindings the page is about (BindingsDevice): the Keyboard and Gamepad pages
                       // set it, and the binding pages opened from them inherit it
} PageRef;

struct SettingsState {
    SettingSlot *slots;
    int slot_count;
    char **names;
    int menu_count;
    PageRef stack[SETTINGS_MAX_DEPTH];
    int depth;
    char notice[256];
    char failure[1400];
    char more_menus[128];  // The Menus page's note for the menus it has no room to list
    char *pads[SETTINGS_MAX_PADS]; // The gamepads present, by device index, for the Device row; NULL: no name
    char pad_fallback[SETTINGS_MAX_PADS][16];  // "Pad N", for a pad with no name
    int pad_count;
    Bindings *bindings;          // The key and gamepad bindings, while settings are open; NULL: no binding rows
    SettingsKeyNamer namer;
    struct {                     // The binding the binding page edits
        int device;
        int index;               // -1: a new one, not yet in the list
        int code;                // -1 until a key is kept
        char command[BINDINGS_COMMAND_MAX];
        int captured;            // The key the confirm page asks about
    } pending;
    struct {                     // What the 10 s would put back
        bool active;
        int device;
        int index;
        int code;                // The key that confirms it
        bool added;
        Binding before;
    } undo;
    char confirm_note[96];       // The confirm page's "Captured: ..."
    char restart_note[320];      // The restart page's question
};

// A function to start the model over the launcher's menus; the caller then sets every entry value
SettingsState *settings_create(const char *const *menu_names, int menu_count)
{
    SettingsState *state = alloc_calloc(1, sizeof(SettingsState));
    if (state == NULL)
        return NULL;
    state->menu_count = menu_count;
    state->slot_count = SET_ID_GLOBAL_COUNT + menu_count * SET_ID_PER_MENU_COUNT;
    state->slots = alloc_calloc((size_t) state->slot_count, sizeof(SettingSlot));
    state->names = alloc_calloc((size_t) (menu_count > 0 ? menu_count : 1), sizeof(char*));
    if (state->slots == NULL || state->names == NULL) {
        settings_free(state);
        return NULL;
    }
    for (int i = 0; i < menu_count; i++) {
        state->names[i] = alloc_strdup(menu_names[i]);
        if (state->names[i] == NULL) {
            settings_free(state);
            return NULL;
        }
    }
    for (int id = 0; id < SET_ID_GLOBAL_COUNT; id++) {
        state->slots[id].def = &DEFS[id];
        state->slots[id].menu = -1;
    }
    for (int m = 0; m < menu_count; m++) {
        for (int k = 0; k < SET_ID_PER_MENU_COUNT; k++) {
            SettingSlot *slot = &state->slots[SET_ID_GLOBAL_COUNT + m * SET_ID_PER_MENU_COUNT + k];
            slot->def = &DEFS[SET_ID_MENU_ROWS + k];
            slot->menu = m;
            slot->value.inherit = true;
            slot->entry.inherit = true;
        }
    }
    state->stack[0].page = SETTINGS_PAGE_TOP;
    state->stack[0].menu = -1;
    return state;
}

// A function to free the model
void settings_free(SettingsState *state)
{
    if (state == NULL)
        return;
    for (int i = 0; state->names != NULL && i < state->menu_count; i++)
        alloc_free(state->names[i]);
    for (int i = 0; i < state->pad_count; i++)
        alloc_free(state->pads[i]);
    alloc_free(state->names);
    alloc_free(state->slots);
    alloc_free(state);
}

// A function to name the gamepads present, by device index, for the Device row. A negative count
// (a failed count) names none, and pads past SETTINGS_MAX_PADS are left out. A pad whose name is
// NULL, or whose copy ran out of memory, is named "Pad N" (N its device index), as the row shows it.
void settings_set_pads(SettingsState *state, const char *const *names, int count)
{
    for (int i = 0; i < state->pad_count; i++) {
        alloc_free(state->pads[i]);
        state->pads[i] = NULL;
    }
    state->pad_count = count > SETTINGS_MAX_PADS ? SETTINGS_MAX_PADS : (count > 0 ? count : 0);
    for (int i = 0; i < state->pad_count; i++) {
        snprintf(state->pad_fallback[i], sizeof(state->pad_fallback[i]), "Pad %d", i);
        state->pads[i] = names[i] != NULL ? alloc_strdup(names[i]) : NULL;
    }
}

// A function to count the pads named
int settings_pad_count(const SettingsState *state)
{
    return state->pad_count;
}

// A function to get a pad's name by device index, for the Device row and list: its own name, or
// "Pad N" when it has none; NULL only past the pads named (index < 0 or >= the count)
const char *settings_pad_name(const SettingsState *state, int index)
{
    if (index < 0 || index >= state->pad_count)
        return NULL;
    return state->pads[index] != NULL ? state->pads[index] : state->pad_fallback[index];
}

// A function to find a setting's slot; `menu` matters only for the per-menu settings
SettingSlot *settings_slot(SettingsState *state, SettingId id, int menu)
{
    if (id < SET_ID_GLOBAL_COUNT)
        return &state->slots[id];
    if (id >= SET_ID_COUNT || menu < 0 || menu >= state->menu_count)
        return NULL;
    return &state->slots[SET_ID_GLOBAL_COUNT + menu * SET_ID_PER_MENU_COUNT + ((int) id - SET_ID_MENU_ROWS)];
}

// A function to count the slots, for walking them all
int settings_slot_count(const SettingsState *state)
{
    return state->slot_count;
}

// A function to get a slot by its place
SettingSlot *settings_slot_at(SettingsState *state, int index)
{
    return index >= 0 && index < state->slot_count ? &state->slots[index] : NULL;
}

// A function to set a setting's value as it is when settings open
void settings_set_entry(SettingsState *state, SettingId id, int menu, const SettingValue *value)
{
    SettingSlot *slot = settings_slot(state, id, menu);
    if (slot != NULL) {
        slot->value = *value;
        slot->entry = *value;
    }
}

// A function to tell whether a setting differs from what it was when settings opened
bool settings_changed(const SettingSlot *slot)
{
    return !setting_equal(slot->def, &slot->value, &slot->entry);
}

// A function to tell whether anything has changed: a setting, or a binding
bool settings_any_changed(const SettingsState *state)
{
    for (int i = 0; i < state->slot_count; i++) {
        if (settings_changed(&state->slots[i]))
            return true;
    }
    return state->bindings != NULL && bindings_changed(state->bindings);
}

// A function to get the value a per-menu setting follows when it inherits
static const SettingValue *inherited_value(SettingsState *state, const SettingSlot *slot)
{
    switch (slot->def->id) {
        case SET_ID_MENU_ROWS:
            return &settings_slot(state, SET_ID_LAYOUT_ROWS, -1)->value;
        case SET_ID_MENU_COLUMNS:
            return &settings_slot(state, SET_ID_LAYOUT_COLUMNS, -1)->value;
        case SET_ID_MENU_ICON_SIZE:
            return &settings_slot(state, SET_ID_LAYOUT_ICON_SIZE, -1)->value;
        default:
            return NULL;
    }
}

static const char *const WHY_OVERLAY = "Turn Overlay on to change this";
static const char *const WHY_TITLES = "Titles are off";
static const char *const WHY_SHADOWS = "Turn Shadows on to change this";
static const char *const WHY_HIGHLIGHT = "The highlight is off";
static const char *const WHY_NO_OUTLINE = "The outline's size is 0";
static const char *const WHY_ROUNDED = "Rounded corners cannot be drawn with an outline";
static const char *const WHY_SCROLL = "The scroll indicators are off";
static const char *const WHY_CLOCK = "The clock is off";
static const char *const WHY_DATE = "Turn Show date on to change this";
static const char *const WHY_SCREENSAVER = "The screensaver is off";
static const char *const WHY_GAMEPAD = "The gamepad is off";
static const char *const WHY_VSYNC = "Used only while VSync is off";
static const char *const MAPPINGS_NOTE = "The mappings file applies at next start: SDL can add mappings, but never take one back.";
static const char *const KEYBOARD_NOTE = "The arrows, OK (Enter) and Back (Backspace) always keep their own meaning.";
static const char *const BUILT_IN_NOTE = "Up, Down and Settings have built-in buttons while nothing else is bound to them.";
static const char *const WHY_NO_NAME =
    "This line has no name in config.ini, so removing it would change how the lines after it read";

// A function to start a row of a kind
static SettingsRow new_row(SettingsRowKind kind, const char *label)
{
    SettingsRow row;
    memset(&row, 0, sizeof(row));
    row.kind = kind;
    row.menu = -1;
    row.enabled = true;
    snprintf(row.label, sizeof(row.label), "%s", label);
    return row;
}

// A function to tell how a setting's row acts: stepped, a picker, or the folder browser
static SettingsRowKind row_kind(const SettingDef *def)
{
    switch (def->type) {
        case SET_TYPE_PATH:
            return SETTINGS_ROW_BROWSE;
        case SET_TYPE_COLOR:
        case SET_TYPE_FONT:
        case SET_TYPE_COMMAND:
        case SET_TYPE_MENU:
        case SET_TYPE_DEVICE:
            return SETTINGS_ROW_PICK;
        default:
            return SETTINGS_ROW_SETTING;
    }
}

// A function to tell whether Left and Right step a row of a kind and type: every setting row, and a
// picker whose value has an order to step through (a colour's presets, the menus, the pads). A font's
// or a command's picker only opens with OK.
static bool row_steps(SettingsRowKind kind, const SettingDef *def)
{
    if (kind == SETTINGS_ROW_SETTING)
        return true;
    return kind == SETTINGS_ROW_PICK &&
           (def->type == SET_TYPE_COLOR || def->type == SET_TYPE_MENU || def->type == SET_TYPE_DEVICE);
}

// A function to make a setting's row. A device shows the pad's name when it is present.
static SettingsRow setting_row(SettingsState *state, SettingSlot *slot)
{
    SettingsRow row = new_row(row_kind(slot->def), slot->def->label);
    row.slot = slot;
    row.steps = row_steps(row.kind, slot->def);
    setting_describe(slot->def, &slot->value, inherited_value(state, slot), row.value, sizeof(row.value));
    int pad = slot->value.number;
    if (slot->def->type == SET_TYPE_DEVICE && pad >= 0) {
        if (pad < state->pad_count)
            snprintf(row.value, sizeof(row.value), "%s", settings_pad_name(state, pad));
        else
            snprintf(row.value, sizeof(row.value), "Pad %d (not connected)", pad);
    }
    return row;
}

// A function to make a global setting's row
static SettingsRow global_row(SettingsState *state, SettingId id)
{
    return setting_row(state, settings_slot(state, id, -1));
}

// A function to grey a row out with its reason when `grey` holds
static SettingsRow greyed(SettingsRow row, bool grey, const char *why)
{
    if (grey) {
        row.enabled = false;
        row.why = why;
    }
    return row;
}

// A function to tell whether an on/off setting is on
static bool is_on(SettingsState *state, SettingId id)
{
    return settings_slot(state, id, -1)->value.number != 0;
}

// A function to summarise an on/off setting for the top page
static const char *on_off(SettingsState *state, SettingId id)
{
    return is_on(state, id) ? "On" : "Off";
}

// A function to make a row that opens a page
static SettingsRow link_row(const char *label, const char *value, SettingsPage target, int menu)
{
    SettingsRow row = new_row(SETTINGS_ROW_LINK, label);
    snprintf(row.value, sizeof(row.value), "%s", value);
    row.target = target;
    row.menu = menu;
    return row;
}

// A function to make a row that does something
static SettingsRow action_row(const char *label, SettingsAction action, bool enabled)
{
    SettingsRow row = new_row(SETTINGS_ROW_ACTION, label);
    row.action = action;
    row.enabled = enabled;
    return row;
}

// A function to make a row of text
static SettingsRow note_row(const char *text)
{
    SettingsRow row = new_row(SETTINGS_ROW_NOTE, "");
    row.note = text;
    return row;
}

// A function to summarise a menu's grid (columns x rows), or say it follows All menus
static void grid_summary(SettingsState *state, int menu, char *out, size_t size)
{
    const SettingValue *rows = &settings_slot(state, SET_ID_LAYOUT_ROWS, -1)->value;
    const SettingValue *columns = &settings_slot(state, SET_ID_LAYOUT_COLUMNS, -1)->value;
    if (menu >= 0) {
        const SettingValue *menu_rows = &settings_slot(state, SET_ID_MENU_ROWS, menu)->value;
        const SettingValue *menu_columns = &settings_slot(state, SET_ID_MENU_COLUMNS, menu)->value;
        const SettingValue *menu_icon = &settings_slot(state, SET_ID_MENU_ICON_SIZE, menu)->value;
        if (menu_rows->inherit && menu_columns->inherit && menu_icon->inherit) {
            snprintf(out, size, "All menus");
            return;
        }
        if (!menu_rows->inherit)
            rows = menu_rows;
        if (!menu_columns->inherit)
            columns = menu_columns;
    }
    snprintf(out, size, "%d " TIMES " %d", columns->number, rows->number);
}

// A function to add a row when there is room
static int add_row(SettingsRow *rows, int count, int max, SettingsRow row)
{
    if (count < max)
        rows[count] = row;
    return count + 1;
}

// A function to give the model its bindings, and a way to name keys and buttons
void settings_set_bindings(SettingsState *state, Bindings *bindings, SettingsKeyNamer namer)
{
    state->bindings = bindings;
    state->namer = namer;
}

// A function to name a key or button through the screen's namer
static void key_name(const SettingsState *state, int device, int code, char *out, size_t size)
{
    if (state->namer != NULL)
        state->namer(device, code, out, size);
    else
        snprintf(out, size, "#%X", (unsigned int) code);
}

// A function to add a device's binding rows: Add binding, then every binding that is not removed
static int binding_rows(SettingsState *state, int device, SettingsRow *rows, int n, int max)
{
    char text[BINDINGS_COMMAND_MAX];
    SettingsRow add = action_row("Add binding", SETTINGS_ACTION_ADD_BINDING, true);
    n = add_row(rows, n, max, add);
    for (int i = 0; i < bindings_count(state->bindings, (BindingsDevice) device); i++) {
        const Binding *binding = bindings_at(state->bindings, (BindingsDevice) device, i);
        if (binding->removed)
            continue;
        key_name(state, device, binding->code, text, sizeof(text));
        SettingsRow row = new_row(SETTINGS_ROW_BINDING, text);
        setting_command_label(binding->command, row.value, sizeof(row.value));
        row.binding = i;
        n = add_row(rows, n, max, row);
    }
    return n;
}

// A function to say why the binding page's binding cannot be removed, or NULL when it can: a
// [Hotkeys] line with no name cannot be (inidoc refuses removing it, which would fail the whole
// save), and the floor may refuse it
static const char *refuse_remove(const SettingsState *state)
{
    BindingsDevice device = (BindingsDevice) state->pending.device;
    const Binding *binding = bindings_at(state->bindings, device, state->pending.index);
    if (binding->original[0] != '\0' && binding->key[0] == '\0')
        return WHY_NO_NAME;
    return bindings_refuse_change(state->bindings, device, state->pending.index, 0, NULL, true);
}

// A function to list the rows of the page on show
int settings_rows(SettingsState *state, SettingsRow *rows, int max)
{
    const PageRef *top = &state->stack[state->depth];
    char text[256];
    int n = 0;
    switch (top->page) {
        case SETTINGS_PAGE_TOP: {
            n = add_row(rows, n, max, link_row("General", settings_slot(state, SET_ID_DEFAULT_MENU, -1)->value.text,
                                               SETTINGS_PAGE_GENERAL, -1));
            SettingSlot *mode = settings_slot(state, SET_ID_BACKGROUND_MODE, -1);
            setting_describe(mode->def, &mode->value, NULL, text, sizeof(text));
            n = add_row(rows, n, max, link_row("Background", text, SETTINGS_PAGE_BACKGROUND, -1));
            snprintf(text, sizeof(text), state->menu_count == 1 ? "%d menu" : "%d menus", state->menu_count);
            n = add_row(rows, n, max, link_row("Menus", text, SETTINGS_PAGE_MENUS, -1));
            SettingSlot *size = settings_slot(state, SET_ID_TITLE_SIZE, -1);
            setting_describe(size->def, &size->value, NULL, text, sizeof(text));
            n = add_row(rows, n, max, link_row("Titles", is_on(state, SET_ID_TITLES_ENABLED) ? text : "Off",
                                               SETTINGS_PAGE_TITLES, -1));
            n = add_row(rows, n, max, link_row("Highlight", on_off(state, SET_ID_HIGHLIGHT_ENABLED), SETTINGS_PAGE_HIGHLIGHT, -1));
            n = add_row(rows, n, max, link_row("Scroll indicators", on_off(state, SET_ID_SCROLL_ENABLED), SETTINGS_PAGE_SCROLL, -1));
            n = add_row(rows, n, max, link_row("Clock", on_off(state, SET_ID_CLOCK_ENABLED), SETTINGS_PAGE_CLOCK, -1));
            SettingSlot *idle = settings_slot(state, SET_ID_SCREENSAVER_IDLE_TIME, -1);
            char after[sizeof(text) + 8] = "Off";   // "After " and any description, never cut short
            if (is_on(state, SET_ID_SCREENSAVER_ENABLED)) {
                setting_describe(idle->def, &idle->value, NULL, text, sizeof(text));
                snprintf(after, sizeof(after), "After %s", text);
            }
            n = add_row(rows, n, max, link_row("Screensaver", after, SETTINGS_PAGE_SCREENSAVER, -1));
            n = add_row(rows, n, max, link_row("Controls", is_on(state, SET_ID_GAMEPAD_ENABLED) ? "Gamepad on" : "Gamepad off",
                                               SETTINGS_PAGE_CONTROLS, -1));
            n = add_row(rows, n, max, new_row(SETTINGS_ROW_DIVIDER, ""));
            n = add_row(rows, n, max, action_row("Discard changes", SETTINGS_ACTION_DISCARD, settings_any_changed(state)));
            break;
        }
        case SETTINGS_PAGE_GENERAL: {
            static const SettingId ids[] = { SET_ID_DEFAULT_MENU, SET_ID_WRAP_ENTRIES, SET_ID_RESET_ON_BACK,
                SET_ID_MOUSE_SELECT, SET_ID_INHIBIT_OS_SCREENSAVER, SET_ID_VSYNC, SET_ID_FPS_LIMIT, SET_ID_ON_LAUNCH,
                SET_ID_APPLICATION_TIMEOUT, SET_ID_STARTUP_CMD, SET_ID_QUIT_CMD };
            for (int i = 0; i < LENGTH(ids); i++) {
                SettingsRow row = global_row(state, ids[i]);
                if (ids[i] == SET_ID_FPS_LIMIT)
                    row = greyed(row, is_on(state, SET_ID_VSYNC), WHY_VSYNC);
                n = add_row(rows, n, max, row);
            }
            break;
        }
        case SETTINGS_PAGE_BACKGROUND: {
            SettingSlot *mode = settings_slot(state, SET_ID_BACKGROUND_MODE, -1);
            n = add_row(rows, n, max, setting_row(state, mode));
            if (mode->value.number == 0)
                n = add_row(rows, n, max, setting_row(state, settings_slot(state, SET_ID_BACKGROUND_COLOR, -1)));
            else if (mode->value.number == MODE_IMAGE)
                n = add_row(rows, n, max, setting_row(state, settings_slot(state, SET_ID_BACKGROUND_IMAGE, -1)));
            else if (mode->value.number == MODE_SLIDESHOW) {
                n = add_row(rows, n, max, setting_row(state, settings_slot(state, SET_ID_SLIDESHOW_DIRECTORY, -1)));
                n = add_row(rows, n, max, setting_row(state, settings_slot(state, SET_ID_SLIDESHOW_DURATION, -1)));
                n = add_row(rows, n, max, setting_row(state, settings_slot(state, SET_ID_SLIDESHOW_FADE, -1)));
            }
            else {
                n = add_row(rows, n, max, note_row(TRANSPARENT_NOTE));
                n = add_row(rows, n, max, global_row(state, SET_ID_CHROMA_KEY_COLOR));
            }
            bool overlay = is_on(state, SET_ID_OVERLAY);
            n = add_row(rows, n, max, global_row(state, SET_ID_OVERLAY));
            n = add_row(rows, n, max, greyed(global_row(state, SET_ID_OVERLAY_COLOR), !overlay, WHY_OVERLAY));
            n = add_row(rows, n, max, greyed(global_row(state, SET_ID_OVERLAY_OPACITY), !overlay, WHY_OVERLAY));
            break;
        }
        case SETTINGS_PAGE_MENUS: {
            // A page holds SETTINGS_MAX_ROWS rows. Menus past that are counted in a note in the last
            // row, never cut off unseen.
            int room = SETTINGS_MAX_ROWS - 2;
            int shown = state->menu_count <= room ? state->menu_count : room - 1;
            grid_summary(state, -1, text, sizeof(text));
            n = add_row(rows, n, max, link_row("All menus", text, SETTINGS_PAGE_MENU, -1));
            n = add_row(rows, n, max, new_row(SETTINGS_ROW_DIVIDER, ""));
            for (int m = 0; m < shown; m++) {
                grid_summary(state, m, text, sizeof(text));
                n = add_row(rows, n, max, link_row(state->names[m], text, SETTINGS_PAGE_MENU, m));
            }
            if (shown < state->menu_count) {
                snprintf(state->more_menus, sizeof(state->more_menus),
                    "%d more menus have no room here: set their grids in config.ini", state->menu_count - shown);
                n = add_row(rows, n, max, note_row(state->more_menus));
            }
            break;
        }
        case SETTINGS_PAGE_MENU: {
            int m = top->menu;
            n = add_row(rows, n, max, setting_row(state, settings_slot(state, m < 0 ? SET_ID_LAYOUT_ROWS : SET_ID_MENU_ROWS, m)));
            n = add_row(rows, n, max, setting_row(state, settings_slot(state, m < 0 ? SET_ID_LAYOUT_COLUMNS : SET_ID_MENU_COLUMNS, m)));
            n = add_row(rows, n, max, setting_row(state, settings_slot(state, m < 0 ? SET_ID_LAYOUT_ICON_SIZE : SET_ID_MENU_ICON_SIZE, m)));
            if (m >= 0)
                n = add_row(rows, n, max, note_row(MENU_NOTE));
            else {
                n = add_row(rows, n, max, global_row(state, SET_ID_ICON_SPACING));
                n = add_row(rows, n, max, global_row(state, SET_ID_VCENTER));
            }
            break;
        }
        case SETTINGS_PAGE_TITLES: {
            bool off = !is_on(state, SET_ID_TITLES_ENABLED);
            static const SettingId ids[] = { SET_ID_TITLE_SIZE, SET_ID_TITLES_ENABLED, SET_ID_TITLE_FONT,
                SET_ID_TITLE_COLOR, SET_ID_TITLE_OPACITY, SET_ID_TITLE_SHADOWS, SET_ID_TITLE_SHADOW_COLOR,
                SET_ID_TITLE_OVERSIZE, SET_ID_TITLE_PADDING };
            for (int i = 0; i < LENGTH(ids); i++) {
                SettingsRow row = global_row(state, ids[i]);
                if (ids[i] == SET_ID_TITLE_SHADOW_COLOR)
                    row = greyed(row, !is_on(state, SET_ID_TITLE_SHADOWS), WHY_SHADOWS);
                if (ids[i] != SET_ID_TITLES_ENABLED)
                    row = greyed(row, off, WHY_TITLES);
                n = add_row(rows, n, max, row);
            }
            break;
        }
        case SETTINGS_PAGE_HIGHLIGHT: {
            bool off = !is_on(state, SET_ID_HIGHLIGHT_ENABLED);
            bool outline = settings_slot(state, SET_ID_HIGHLIGHT_OUTLINE_SIZE, -1)->value.number > 0;
            static const SettingId ids[] = { SET_ID_HIGHLIGHT_ENABLED, SET_ID_HIGHLIGHT_FILL_COLOR,
                SET_ID_HIGHLIGHT_FILL_OPACITY, SET_ID_HIGHLIGHT_OUTLINE_SIZE, SET_ID_HIGHLIGHT_OUTLINE_COLOR,
                SET_ID_HIGHLIGHT_OUTLINE_OPACITY, SET_ID_HIGHLIGHT_CORNER_RADIUS, SET_ID_HIGHLIGHT_VPADDING,
                SET_ID_HIGHLIGHT_HPADDING };
            for (int i = 0; i < LENGTH(ids); i++) {
                SettingsRow row = global_row(state, ids[i]);
                if (ids[i] == SET_ID_HIGHLIGHT_OUTLINE_COLOR || ids[i] == SET_ID_HIGHLIGHT_OUTLINE_OPACITY)
                    row = greyed(row, !outline, WHY_NO_OUTLINE);
                if (ids[i] == SET_ID_HIGHLIGHT_CORNER_RADIUS)
                    row = greyed(row, outline, WHY_ROUNDED);
                if (ids[i] != SET_ID_HIGHLIGHT_ENABLED)
                    row = greyed(row, off, WHY_HIGHLIGHT);
                n = add_row(rows, n, max, row);
            }
            break;
        }
        case SETTINGS_PAGE_SCROLL: {
            bool off = !is_on(state, SET_ID_SCROLL_ENABLED);
            bool outline = settings_slot(state, SET_ID_SCROLL_OUTLINE_SIZE, -1)->value.number > 0;
            static const SettingId ids[] = { SET_ID_SCROLL_ENABLED, SET_ID_SCROLL_FILL_COLOR,
                SET_ID_SCROLL_OUTLINE_SIZE, SET_ID_SCROLL_OUTLINE_COLOR, SET_ID_SCROLL_OPACITY };
            for (int i = 0; i < LENGTH(ids); i++) {
                SettingsRow row = global_row(state, ids[i]);
                if (ids[i] == SET_ID_SCROLL_OUTLINE_COLOR)
                    row = greyed(row, !outline, WHY_NO_OUTLINE);
                if (ids[i] != SET_ID_SCROLL_ENABLED)
                    row = greyed(row, off, WHY_SCROLL);
                n = add_row(rows, n, max, row);
            }
            break;
        }
        case SETTINGS_PAGE_CLOCK: {
            bool off = !is_on(state, SET_ID_CLOCK_ENABLED);
            bool date = is_on(state, SET_ID_CLOCK_SHOW_DATE);
            static const SettingId ids[] = { SET_ID_CLOCK_ENABLED, SET_ID_CLOCK_SHOW_DATE, SET_ID_CLOCK_WEEKDAY,
                SET_ID_CLOCK_ALIGNMENT, SET_ID_CLOCK_FONT, SET_ID_CLOCK_FONT_SIZE, SET_ID_CLOCK_COLOR,
                SET_ID_CLOCK_OPACITY, SET_ID_CLOCK_SHADOWS, SET_ID_CLOCK_SHADOW_COLOR, SET_ID_CLOCK_MARGIN,
                SET_ID_CLOCK_TIME_FORMAT, SET_ID_CLOCK_DATE_FORMAT };
            for (int i = 0; i < LENGTH(ids); i++) {
                SettingsRow row = global_row(state, ids[i]);
                if (ids[i] == SET_ID_CLOCK_WEEKDAY || ids[i] == SET_ID_CLOCK_DATE_FORMAT)
                    row = greyed(row, !date, WHY_DATE);
                if (ids[i] == SET_ID_CLOCK_SHADOW_COLOR)
                    row = greyed(row, !is_on(state, SET_ID_CLOCK_SHADOWS), WHY_SHADOWS);
                if (ids[i] != SET_ID_CLOCK_ENABLED)
                    row = greyed(row, off, WHY_CLOCK);
                n = add_row(rows, n, max, row);
            }
            break;
        }
        case SETTINGS_PAGE_SCREENSAVER: {
            bool off = !is_on(state, SET_ID_SCREENSAVER_ENABLED);
            static const SettingId ids[] = { SET_ID_SCREENSAVER_ENABLED, SET_ID_SCREENSAVER_IDLE_TIME,
                SET_ID_SCREENSAVER_INTENSITY, SET_ID_SCREENSAVER_PAUSE };
            for (int i = 0; i < LENGTH(ids); i++) {
                SettingsRow row = global_row(state, ids[i]);
                if (ids[i] != SET_ID_SCREENSAVER_ENABLED)
                    row = greyed(row, off, WHY_SCREENSAVER);
                n = add_row(rows, n, max, row);
            }
            break;
        }
        case SETTINGS_PAGE_CONTROLS:
            if (state->bindings != NULL) {
                int keys = 0;
                for (int i = 0; i < bindings_count(state->bindings, BINDINGS_KEYBOARD); i++)
                    keys += bindings_at(state->bindings, BINDINGS_KEYBOARD, i)->removed ? 0 : 1;
                snprintf(text, sizeof(text), keys == 1 ? "%d hotkey" : "%d hotkeys", keys);
                n = add_row(rows, n, max, link_row("Keyboard", text, SETTINGS_PAGE_KEYBOARD, -1));
            }
            n = add_row(rows, n, max, link_row("Gamepad", on_off(state, SET_ID_GAMEPAD_ENABLED), SETTINGS_PAGE_GAMEPAD, -1));
            break;
        case SETTINGS_PAGE_GAMEPAD: {
            bool off = !is_on(state, SET_ID_GAMEPAD_ENABLED);
            n = add_row(rows, n, max, global_row(state, SET_ID_GAMEPAD_ENABLED));
            n = add_row(rows, n, max, greyed(global_row(state, SET_ID_GAMEPAD_DEVICE), off, WHY_GAMEPAD));
            n = add_row(rows, n, max, greyed(global_row(state, SET_ID_GAMEPAD_MAPPINGS), off, WHY_GAMEPAD));
            n = add_row(rows, n, max, note_row(MAPPINGS_NOTE));
            if (state->bindings != NULL) {
                n = binding_rows(state, BINDINGS_GAMEPAD, rows, n, max);
                n = add_row(rows, n, max, note_row(BUILT_IN_NOTE));
            }
            break;
        }
        case SETTINGS_PAGE_KEYBOARD:
            n = binding_rows(state, BINDINGS_KEYBOARD, rows, n, max);
            n = add_row(rows, n, max, note_row(KEYBOARD_NOTE));
            break;
        case SETTINGS_PAGE_BINDING: {
            char name[64];
            if (state->pending.code >= 0)
                key_name(state, state->pending.device, state->pending.code, name, sizeof(name));
            else
                snprintf(name, sizeof(name), "Choose" ELLIPSIS);
            SettingsRow key = action_row("Key", SETTINGS_ACTION_CAPTURE, true);
            snprintf(key.value, sizeof(key.value), "%s", name);
            n = add_row(rows, n, max, key);
            SettingsRow command = action_row("Command", SETTINGS_ACTION_BIND_COMMAND, true);
            if (state->pending.command[0] != '\0')
                setting_command_label(state->pending.command, command.value, sizeof(command.value));
            else
                snprintf(command.value, sizeof(command.value), "None");
            n = add_row(rows, n, max, command);
            if (state->pending.index < 0)
                n = add_row(rows, n, max, action_row("Cancel", SETTINGS_ACTION_CANCEL, true));
            else {
                const char *why = refuse_remove(state);
                n = add_row(rows, n, max, greyed(action_row("Remove", SETTINGS_ACTION_REMOVE_BINDING, true), why != NULL, why));
            }
            break;
        }
        case SETTINGS_PAGE_CAPTURE:
            n = add_row(rows, n, max, note_row("Press the key or button" ELLIPSIS));
            break;
        case SETTINGS_PAGE_CONFIRM: {
            char name[64];
            key_name(state, state->pending.device, state->pending.captured, name, sizeof(name));
            snprintf(state->confirm_note, sizeof(state->confirm_note), "Captured: %s", name);
            n = add_row(rows, n, max, note_row(state->confirm_note));
            n = add_row(rows, n, max, action_row("Keep", SETTINGS_ACTION_KEEP, true));
            n = add_row(rows, n, max, action_row("Try again", SETTINGS_ACTION_TRY_AGAIN, true));
            n = add_row(rows, n, max, action_row("Cancel", SETTINGS_ACTION_CANCEL, true));
            break;
        }
        case SETTINGS_PAGE_SAVE_FAILED:
            n = add_row(rows, n, max, note_row(state->failure));
            n = add_row(rows, n, max, action_row("Try again", SETTINGS_ACTION_RETRY, true));
            n = add_row(rows, n, max, action_row("Leave without saving", SETTINGS_ACTION_LEAVE, true));
            break;
        case SETTINGS_PAGE_RESTART:
            n = add_row(rows, n, max, note_row(state->restart_note));
            n = add_row(rows, n, max, action_row("Yes", SETTINGS_ACTION_RESTART, true));
            n = add_row(rows, n, max, action_row("No", SETTINGS_ACTION_NO_RESTART, true));
            break;
    }
    return n < max ? n : max;
}

// A function to tell whether the cursor may rest on a row: any row but a divider or a note, unless
// it is greyed with no reason to give (Discard with nothing to discard). The screen asks it too.
bool settings_row_selectable(const SettingsRow *row)
{
    return row->kind != SETTINGS_ROW_DIVIDER && row->kind != SETTINGS_ROW_NOTE && (row->enabled || row->why != NULL);
}

// A function to keep the cursor on a row it may rest on, since the rows under it can change
static void fix_cursor(SettingsState *state)
{
    SettingsRow rows[SETTINGS_MAX_ROWS];
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    PageRef *top = &state->stack[state->depth];
    if (top->cursor >= count)
        top->cursor = count - 1;
    if (top->cursor < 0)
        top->cursor = 0;
    if (count == 0 || settings_row_selectable(&rows[top->cursor]))
        return;
    for (int i = top->cursor; i < count; i++) {
        if (settings_row_selectable(&rows[i])) {
            top->cursor = i;
            return;
        }
    }
    for (int i = top->cursor; i >= 0; i--) {
        if (settings_row_selectable(&rows[i])) {
            top->cursor = i;
            return;
        }
    }
}

// A function to open a page on top of the one on show
static void push_page(SettingsState *state, SettingsPage page, int menu)
{
    if (state->depth + 1 >= SETTINGS_MAX_DEPTH)
        return;
    state->depth++;
    PageRef *top = &state->stack[state->depth];
    memset(top, 0, sizeof(*top));
    top->page = page;
    top->menu = menu;
    top->entry_mode = settings_slot(state, SET_ID_BACKGROUND_MODE, -1)->value.number;
    top->device = page == SETTINGS_PAGE_KEYBOARD ? BINDINGS_KEYBOARD
                : page == SETTINGS_PAGE_GAMEPAD ? BINDINGS_GAMEPAD : state->stack[state->depth - 1].device;
    fix_cursor(state);
}

// A function to apply the incomplete-mode rule when the Background page is left: Image with no
// image, or Slideshow with no folder, goes back to the mode the page opened with
static SettingsEvent leave_background(SettingsState *state)
{
    SettingsEvent event;
    memset(&event, 0, sizeof(event));
    event.kind = SETTINGS_EVENT_NONE;
    SettingSlot *mode = settings_slot(state, SET_ID_BACKGROUND_MODE, -1);
    const char *missing = NULL;
    if (mode->value.number == MODE_IMAGE && settings_slot(state, SET_ID_BACKGROUND_IMAGE, -1)->value.text[0] == '\0')
        missing = "No image was chosen";
    else if (mode->value.number == MODE_SLIDESHOW && settings_slot(state, SET_ID_SLIDESHOW_DIRECTORY, -1)->value.text[0] == '\0')
        missing = "No folder was chosen";
    // A page opened in that mode already (Mode=Image with no Image= line, say) has nothing to go
    // back to: the mode stays, with no event and no notice
    if (missing == NULL || mode->value.number == state->stack[state->depth].entry_mode)
        return event;
    event.before = mode->value;
    mode->value.number = state->stack[state->depth].entry_mode;
    snprintf(state->notice, sizeof(state->notice), "%s, so Mode went back to %s", missing, MODE_LABELS[mode->value.number]);
    event.kind = SETTINGS_EVENT_CHANGED;
    event.slot = mode;
    return event;
}

// A function to step the default menu through the menus, in file order; false at either end. The
// launcher refuses to start with a missing or unknown DefaultMenu, so the model never opens with one.
static bool step_menu(SettingsState *state, const SettingSlot *slot, int direction, SettingValue *next)
{
    int index = -1;
    for (int i = 0; i < state->menu_count && index < 0; i++) {
        if (strcmp(state->names[i], slot->value.text) == 0)
            index = i;
    }
    int to = index < 0 ? (direction > 0 ? 0 : -1) : index + (direction > 0 ? 1 : -1);
    if (to < 0 || to >= state->menu_count)
        return false;
    *next = slot->value;
    snprintf(next->text, sizeof(next->text), "%s", state->names[to]);
    return true;
}

// A function to step the device through Any and the pads present, in order; a device index the file
// names that is not present stays reachable in its place. False at either end.
static bool step_device(SettingsState *state, const SettingSlot *slot, int direction, SettingValue *next)
{
    int list[SETTINGS_MAX_PADS + 3];
    int count = 0;
    int wanted[SETTINGS_MAX_PADS + 3];
    int wanted_count = 0;
    wanted[wanted_count++] = -1;
    for (int i = 0; i < state->pad_count; i++)
        wanted[wanted_count++] = i;
    wanted[wanted_count++] = slot->entry.number;
    wanted[wanted_count++] = slot->value.number;
    for (int i = 0; i < wanted_count; i++) {
        int at = count;
        bool seen = false;
        for (int k = 0; k < count && !seen; k++)
            seen = list[k] == wanted[i];
        if (seen)
            continue;
        while (at > 0 && list[at - 1] > wanted[i]) {
            list[at] = list[at - 1];
            at--;
        }
        list[at] = wanted[i];
        count++;
    }
    int index = 0;
    while (index < count && list[index] != slot->value.number)
        index++;
    int to = index + (direction > 0 ? 1 : -1);
    if (to < 0 || to >= count)
        return false;
    *next = slot->value;
    next->number = list[to];
    return true;
}

// A function to read a [Hotkeys] key's number as the save numbers new lines (next_key() in
// config_save.c): "Hotkey12" is 12; 0 for any other key, or one too long to add to
static long long hotkey_number(const char *key)
{
    const char *digits = key + 6;
    if (strncmp(key, "Hotkey", 6) != 0 || strspn(digits, "0123456789") != strlen(digits) || strlen(digits) > 18)
        return 0;
    return strtoll(digits, NULL, 10);
}

// A function to say why config.ini could not hold the binding page's binding as a line, or NULL when
// it could: a hotkey's "#<HEX>;<command>" or a control's command, under the key the save gives it. A
// line loaded keeps its own key; a new hotkey is numbered one above the highest HotkeyN loaded, and
// one more for each new hotkey (every one in the list counts, removed or not, which can only make the
// key longer than the save's); a control's key is its label.
static const char *refuse_line(const SettingsState *state)
{
    char value[BINDINGS_VALUE_MAX];
    char key[64];
    BindingsDevice device = (BindingsDevice) state->pending.device;
    const Binding *binding = bindings_at(state->bindings, device, state->pending.index);
    if (device == BINDINGS_GAMEPAD) {
        snprintf(value, sizeof(value), "%s", state->pending.command);
        snprintf(key, sizeof(key), "%s", bindings_label(state->pending.code));
    }
    else {
        snprintf(value, sizeof(value), "#%X;%s", (unsigned int) state->pending.code, state->pending.command);
        if (binding != NULL && binding->original[0] != '\0')
            snprintf(key, sizeof(key), "%s", binding->key);
        else {
            long long highest = 0;
            int added = binding == NULL ? 1 : 0;
            for (int i = 0; i < bindings_count(state->bindings, device); i++) {
                const Binding *other = bindings_at(state->bindings, device, i);
                long long number = hotkey_number(other->key);
                highest = number > highest ? number : highest;
                added += other->original[0] == '\0' ? 1 : 0;
            }
            snprintf(key, sizeof(key), "Hotkey%lld", highest + added);
        }
    }
    return inidoc_check(key, value);
}

// A function to commit the binding page's binding once it has a key and a command: refused with the
// reason (the key's, the floor's, or a line config.ini could not hold), or set (added when new); a
// change that takes a key's navigation away asks for the 10 s confirmation. Back to the list unless
// refused.
static SettingsEvent commit_binding(SettingsState *state)
{
    SettingsEvent event;
    memset(&event, 0, sizeof(event));
    event.kind = SETTINGS_EVENT_MOVED;
    BindingsDevice device = (BindingsDevice) state->pending.device;
    const char *why = bindings_refuse_key(state->bindings, device, state->pending.code, state->pending.command);
    if (why == NULL)
        why = bindings_refuse_change(state->bindings, device, state->pending.index, state->pending.code,
                                     state->pending.command, false);
    if (why != NULL) {
        snprintf(state->notice, sizeof(state->notice), "%s", why);
        return event;
    }
    why = refuse_line(state);
    if (why != NULL) {
        snprintf(state->notice, sizeof(state->notice), "config.ini cannot hold this binding: %s", why);
        return event;
    }
    bool added = state->pending.index < 0;
    if (!added)
        state->undo.before = *bindings_at(state->bindings, device, state->pending.index);
    int index = bindings_set(state->bindings, device, state->pending.index, state->pending.code, state->pending.command);
    if (index < 0) {
        snprintf(state->notice, sizeof(state->notice), "out of memory");
        return event;
    }
    event.kind = SETTINGS_EVENT_BINDINGS;
    event.confirm = bindings_takes_navigation(device, state->pending.code, state->pending.command);
    event.code = state->pending.code;
    event.device = (int) device;
    state->undo.active = event.confirm;
    state->undo.device = device;
    state->undo.index = index;
    state->undo.code = state->pending.code;
    state->undo.added = added;
    state->depth--;   // Back to the list
    return event;
}

// A function to take a captured key or button: a refused one ends the capture with its reason; any
// other goes to the confirm page
SettingsEvent settings_captured(SettingsState *state, int code)
{
    SettingsEvent event;
    memset(&event, 0, sizeof(event));
    event.kind = SETTINGS_EVENT_MOVED;
    if (state->stack[state->depth].page != SETTINGS_PAGE_CAPTURE)
        return event;
    state->depth--;
    const char *why = bindings_refuse_key(state->bindings, (BindingsDevice) state->pending.device, code, "");
    if (why != NULL) {
        snprintf(state->notice, sizeof(state->notice), "%s", why);
        fix_cursor(state);
        return event;
    }
    state->pending.captured = code;
    push_page(state, SETTINGS_PAGE_CONFIRM, -1);
    return event;
}

// A function to end a capture that caught nothing (a timeout), with the reason for the caption
void settings_capture_ended(SettingsState *state, const char *why)
{
    if (state->stack[state->depth].page != SETTINGS_PAGE_CAPTURE)
        return;
    state->depth--;
    snprintf(state->notice, sizeof(state->notice), "%s", why != NULL ? why : "");
    fix_cursor(state);
}

// A function to set the binding page's command, chosen in the command picker; with a key already, it
// commits the binding
SettingsEvent settings_bind_command(SettingsState *state, const char *command)
{
    char before[BINDINGS_COMMAND_MAX];
    snprintf(before, sizeof(before), "%s", state->pending.command);
    snprintf(state->pending.command, sizeof(state->pending.command), "%s", command);
    SettingsEvent event;
    memset(&event, 0, sizeof(event));
    event.kind = SETTINGS_EVENT_MOVED;
    state->notice[0] = '\0';
    if (state->pending.code >= 0 && command[0] != '\0') {
        event = commit_binding(state);
        if (event.kind != SETTINGS_EVENT_BINDINGS)   // Refused: the page shows the command it had
            snprintf(state->pending.command, sizeof(state->pending.command), "%s", before);
    }
    fix_cursor(state);
    return event;
}

// A function to put back the change the 10 s were for, unconfirmed
SettingsEvent settings_revert_binding(SettingsState *state)
{
    SettingsEvent event;
    memset(&event, 0, sizeof(event));
    event.kind = SETTINGS_EVENT_NONE;
    if (!state->undo.active)
        return event;
    BindingsDevice device = (BindingsDevice) state->undo.device;
    if (state->undo.added)
        bindings_remove(state->bindings, device, state->undo.index);
    else
        bindings_set(state->bindings, device, state->undo.index, state->undo.before.code, state->undo.before.command);
    state->undo.active = false;
    event.kind = SETTINGS_EVENT_BINDINGS;
    fix_cursor(state);
    return event;
}

// A function to keep the change the 10 s were for: it was confirmed, so nothing goes back
void settings_keep_binding(SettingsState *state)
{
    state->undo.active = false;
}

// A function to get the command the binding page shows, for the command picker's cursor
const char *settings_binding_command(const SettingsState *state)
{
    return state->pending.command;
}

// A function to tell whether a binding's page may open: not while a change waits to be confirmed,
// which a new change would leave nothing to put back to (the screen is told why)
static bool binding_page_may_open(SettingsState *state)
{
    char name[64];
    if (!state->undo.active)
        return true;
    key_name(state, state->undo.device, state->undo.code, name, sizeof(name));
    snprintf(state->notice, sizeof(state->notice), "Press %s again to keep the last change first, or wait for it to go back", name);
    return false;
}

// A function to act on one key of the remote
SettingsEvent settings_command(SettingsState *state, SettingsCommand command)
{
    SettingsEvent event;
    memset(&event, 0, sizeof(event));
    event.kind = SETTINGS_EVENT_NONE;
    state->notice[0] = '\0';
    SettingsRow rows[SETTINGS_MAX_ROWS];
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    PageRef *top = &state->stack[state->depth];
    SettingsRow *row = top->cursor >= 0 && top->cursor < count ? &rows[top->cursor] : NULL;
    bool failure_page = top->page == SETTINGS_PAGE_SAVE_FAILED;
    bool restart_page = top->page == SETTINGS_PAGE_RESTART;   // Saved already: Back is No

    switch (command) {
        case SETTINGS_UP:
        case SETTINGS_DOWN: {
            int step = command == SETTINGS_UP ? -1 : 1;
            for (int i = top->cursor + step; i >= 0 && i < count; i += step) {
                if (settings_row_selectable(&rows[i])) {
                    top->cursor = i;
                    event.kind = SETTINGS_EVENT_MOVED;
                    break;
                }
            }
            break;
        }
        case SETTINGS_LEFT:
        case SETTINGS_RIGHT:
            if (row != NULL && row->enabled && row->steps) {
                SettingSlot *slot = row->slot;
                int direction = command == SETTINGS_RIGHT ? 1 : -1;
                SettingValue next = slot->value;
                bool moved = true;
                if (slot->def->type == SET_TYPE_MENU)
                    moved = step_menu(state, slot, direction, &next);
                else if (slot->def->type == SET_TYPE_DEVICE)
                    moved = step_device(state, slot, direction, &next);
                else
                    next = setting_step(slot->def, &slot->value, &slot->entry, direction);
                if (moved && !setting_equal(slot->def, &next, &slot->value)) {
                    event.before = slot->value;
                    slot->value = next;
                    event.kind = SETTINGS_EVENT_CHANGED;
                    event.slot = slot;
                }
            }
            break;
        case SETTINGS_OK:
            if (row == NULL || !settings_row_selectable(row))
                break;
            if (!row->enabled && row->kind != SETTINGS_ROW_ACTION)
                break;
            if (row->kind == SETTINGS_ROW_LINK) {
                push_page(state, row->target, row->menu);
                event.kind = SETTINGS_EVENT_MOVED;
            }
            else if (row->kind == SETTINGS_ROW_BROWSE) {
                event.kind = SETTINGS_EVENT_BROWSE;
                event.slot = row->slot;
            }
            else if (row->kind == SETTINGS_ROW_PICK) {
                event.kind = SETTINGS_EVENT_PICK;
                event.slot = row->slot;
            }
            else if (row->kind == SETTINGS_ROW_BINDING) {
                if (!binding_page_may_open(state))
                    break;
                const Binding *binding = bindings_at(state->bindings, (BindingsDevice) top->device, row->binding);
                state->pending.device = top->device;
                state->pending.index = row->binding;
                state->pending.code = binding->code;
                snprintf(state->pending.command, sizeof(state->pending.command), "%s", binding->command);
                push_page(state, SETTINGS_PAGE_BINDING, -1);
                event.kind = SETTINGS_EVENT_MOVED;
            }
            else if (row->kind == SETTINGS_ROW_ACTION && row->action == SETTINGS_ACTION_ADD_BINDING) {
                if (!binding_page_may_open(state))
                    break;
                state->pending.device = top->device;
                state->pending.index = -1;
                state->pending.code = -1;
                state->pending.command[0] = '\0';
                push_page(state, SETTINGS_PAGE_BINDING, -1);
                event.kind = SETTINGS_EVENT_MOVED;
            }
            else if (row->kind == SETTINGS_ROW_ACTION &&
                     (row->action == SETTINGS_ACTION_CAPTURE || row->action == SETTINGS_ACTION_TRY_AGAIN)) {
                if (row->action == SETTINGS_ACTION_TRY_AGAIN)
                    state->depth--;
                push_page(state, SETTINGS_PAGE_CAPTURE, -1);
                event.kind = SETTINGS_EVENT_CAPTURE;
                event.device = state->pending.device;
            }
            else if (row->kind == SETTINGS_ROW_ACTION && row->action == SETTINGS_ACTION_BIND_COMMAND)
                event.kind = SETTINGS_EVENT_PICK_COMMAND;
            else if (row->kind == SETTINGS_ROW_ACTION && row->action == SETTINGS_ACTION_KEEP) {
                int before = state->pending.code;
                state->pending.code = state->pending.captured;
                state->depth--;
                event.kind = SETTINGS_EVENT_MOVED;
                if (state->pending.command[0] != '\0') {
                    event = commit_binding(state);
                    if (event.kind != SETTINGS_EVENT_BINDINGS)   // Refused: the page shows the key it had
                        state->pending.code = before;
                }
            }
            else if (row->kind == SETTINGS_ROW_ACTION && row->action == SETTINGS_ACTION_CANCEL) {
                state->depth--;
                event.kind = SETTINGS_EVENT_MOVED;
            }
            else if (row->kind == SETTINGS_ROW_ACTION && row->action == SETTINGS_ACTION_REMOVE_BINDING) {
                if (!row->enabled)
                    break;   // Greyed: its reason is on show
                bindings_remove(state->bindings, (BindingsDevice) state->pending.device, state->pending.index);
                state->depth--;
                event.kind = SETTINGS_EVENT_BINDINGS;
            }
            else if (row->kind == SETTINGS_ROW_ACTION && row->action == SETTINGS_ACTION_DISCARD) {
                for (int i = 0; i < state->slot_count; i++)
                    state->slots[i].value = state->slots[i].entry;
                if (state->bindings != NULL)
                    bindings_discard(state->bindings);
                state->undo.active = false;
                event.kind = SETTINGS_EVENT_DISCARD;
            }
            else if (row->kind == SETTINGS_ROW_ACTION && row->action == SETTINGS_ACTION_RETRY)
                event.kind = SETTINGS_EVENT_RETRY;
            else if (row->kind == SETTINGS_ROW_ACTION && row->action == SETTINGS_ACTION_LEAVE)
                event.kind = SETTINGS_EVENT_LEAVE;
            else if (row->kind == SETTINGS_ROW_ACTION && row->action == SETTINGS_ACTION_RESTART)
                event.kind = SETTINGS_EVENT_RESTART;
            else if (row->kind == SETTINGS_ROW_ACTION && row->action == SETTINGS_ACTION_NO_RESTART)
                event.kind = SETTINGS_EVENT_CLOSE_SAVED;
            break;
        case SETTINGS_BACK:
            if (restart_page) {
                event.kind = SETTINGS_EVENT_CLOSE_SAVED;
                break;
            }
            if (state->depth == 0) {
                event.kind = SETTINGS_EVENT_CLOSE;
                break;
            }
            if (top->page == SETTINGS_PAGE_BACKGROUND)
                event = leave_background(state);
            state->depth--;
            if (event.kind == SETTINGS_EVENT_NONE)
                event.kind = SETTINGS_EVENT_MOVED;
            break;
        case SETTINGS_HOME:
        case SETTINGS_CLOSE:
            if (failure_page || restart_page)
                break;
            if (top->page == SETTINGS_PAGE_BACKGROUND)
                event = leave_background(state);
            event.kind = command == SETTINGS_HOME ? SETTINGS_EVENT_CLOSE_HOME : SETTINGS_EVENT_CLOSE;
            break;
    }
    fix_cursor(state);
    return event;
}

// A function to set a path chosen in the folder browser; a change that waits for the next start
// (the mappings file) says so in the caption
SettingsEvent settings_choose(SettingsState *state, SettingSlot *slot, const char *path)
{
    SettingsEvent event;
    memset(&event, 0, sizeof(event));
    event.kind = SETTINGS_EVENT_NONE;
    state->notice[0] = '\0';
    if (strlen(path) >= SETTING_TEXT_MAX || strcmp(slot->value.text, path) == 0)
        return event;
    event.before = slot->value;
    snprintf(slot->value.text, SETTING_TEXT_MAX, "%s", path);
    event.kind = SETTINGS_EVENT_CHANGED;
    event.slot = slot;
    if (slot->def->flags & SET_FLAG_NEXT_START)
        snprintf(state->notice, sizeof(state->notice), "This applies at next start");
    return event;
}

// A function to set a value chosen in a picker (a colour, a font, a command, a menu, a device)
SettingsEvent settings_choose_value(SettingsState *state, SettingSlot *slot, const SettingValue *value)
{
    SettingsEvent event;
    memset(&event, 0, sizeof(event));
    event.kind = SETTINGS_EVENT_NONE;
    state->notice[0] = '\0';
    if (setting_equal(slot->def, &slot->value, value))
        return event;
    event.before = slot->value;
    slot->value = *value;
    event.kind = SETTINGS_EVENT_CHANGED;
    event.slot = slot;
    return event;
}

// A function to show why a save failed, with Try again and Leave without saving
void settings_show_save_failed(SettingsState *state, const char *message)
{
    snprintf(state->failure, sizeof(state->failure), "%s", message);
    if (state->stack[state->depth].page != SETTINGS_PAGE_SAVE_FAILED)
        push_page(state, SETTINGS_PAGE_SAVE_FAILED, -1);
    else
        fix_cursor(state);
}

// A function to name a setting as the restart prompt does: "the" and its label, whose first letter
// is lower-cased unless the second is a capital too ("Mappings file" is "the mappings file", "VSync"
// stays "the VSync"). Every label in the table has a letter at least.
void settings_restart_name(const char *label, char *out, size_t size)
{
    bool capital = label[0] >= 'A' && label[0] <= 'Z';
    bool acronym = capital && label[1] >= 'A' && label[1] <= 'Z';
    char first = capital && !acronym ? (char) (label[0] - 'A' + 'a') : label[0];
    snprintf(out, size, "the %c%s", first, label + 1);
}

// A function to join names as a sentence lists them: "A", "A and B", "A, B and C"; "" for none
void settings_join_names(const char *const *names, int count, char *out, size_t size)
{
    out[0] = '\0';
    for (int i = 0; i < count; i++) {
        size_t used = strlen(out);
        snprintf(out + used, size - used, "%s%s", i == 0 ? "" : i == count - 1 ? " and " : ", ", names[i]);
    }
}

// A function to name, joined, the settings a save writes that apply at next start (SET_FLAG_NEXT_START),
// each name once. A setting counts by the save's own test, a value that differs from the one settings
// opened with, so one changed and then changed back does not. Returns how many it names; 0 for none.
int settings_next_start(const SettingsState *state, char *out, size_t size)
{
    char names[SET_ID_COUNT][64];   // Never more names than settings: a per-menu one names itself alike for every menu
    const char *list[SET_ID_COUNT];
    int count = 0;
    for (int i = 0; i < state->slot_count; i++) {
        const SettingSlot *slot = &state->slots[i];
        if (!(slot->def->flags & SET_FLAG_NEXT_START) || !settings_changed(slot))
            continue;
        char name[sizeof(names[0])];
        settings_restart_name(slot->def->label, name, sizeof(name));
        bool named = false;
        for (int k = 0; k < count && !named; k++)
            named = strcmp(list[k], name) == 0;
        if (!named) {
            snprintf(names[count], sizeof(names[count]), "%s", name);
            list[count] = names[count];
            count++;
        }
    }
    settings_join_names(list, count, out, size);
    return count;
}

// A function to ask, once the save is done, whether to restart StreamFlex now to apply the settings
// named: Yes, or No (Back too), with the cursor on Yes. Settings close either way, so the page
// replaces the ones open.
void settings_show_restart(SettingsState *state, const char *names)
{
    snprintf(state->restart_note, sizeof(state->restart_note), "Restart StreamFlex now to apply %s?", names);
    state->depth = 0;
    push_page(state, SETTINGS_PAGE_RESTART, -1);
}

// A function to get the cursor on the page on show
int settings_cursor(const SettingsState *state)
{
    return state->stack[state->depth].cursor;
}

// A function to get the page on show
SettingsPage settings_page(const SettingsState *state)
{
    return state->stack[state->depth].page;
}

// A function to write the path of pages to the one on show: "Settings", "Menus", "Games", joined by ARROW
void settings_path(const SettingsState *state, char *out, size_t size)
{
    snprintf(out, size, "Settings");
    for (int i = 1; i <= state->depth; i++) {
        const PageRef *page = &state->stack[i];
        const char *name = "";
        switch (page->page) {
            case SETTINGS_PAGE_BACKGROUND: name = "Background"; break;
            case SETTINGS_PAGE_MENUS: name = "Menus"; break;
            case SETTINGS_PAGE_MENU: name = page->menu < 0 ? "All menus" : state->names[page->menu]; break;
            case SETTINGS_PAGE_TITLES: name = "Titles"; break;
            case SETTINGS_PAGE_GENERAL: name = "General"; break;
            case SETTINGS_PAGE_HIGHLIGHT: name = "Highlight"; break;
            case SETTINGS_PAGE_SCROLL: name = "Scroll indicators"; break;
            case SETTINGS_PAGE_CLOCK: name = "Clock"; break;
            case SETTINGS_PAGE_SCREENSAVER: name = "Screensaver"; break;
            case SETTINGS_PAGE_CONTROLS: name = "Controls"; break;
            case SETTINGS_PAGE_GAMEPAD: name = "Gamepad"; break;
            case SETTINGS_PAGE_KEYBOARD: name = "Keyboard"; break;
            case SETTINGS_PAGE_BINDING: name = "Binding"; break;
            case SETTINGS_PAGE_CAPTURE: name = "Press a key"; break;
            case SETTINGS_PAGE_CONFIRM: name = "Keep it?"; break;
            case SETTINGS_PAGE_SAVE_FAILED: name = "Couldn't save"; break;
            case SETTINGS_PAGE_RESTART: name = "Restart?"; break;
            case SETTINGS_PAGE_TOP: break;
        }
        size_t used = strlen(out);
        snprintf(out + used, size - used, "%s%s", ARROW, name);
    }
}

// A function to say which menu the preview should show: the one a MENU page edits, or the one
// highlighted on the Menus page; -1 for the menu settings were opened from
int settings_preview_menu(SettingsState *state)
{
    const PageRef *top = &state->stack[state->depth];
    if (top->page == SETTINGS_PAGE_MENU)
        return top->menu;
    if (top->page == SETTINGS_PAGE_MENUS) {
        SettingsRow rows[SETTINGS_MAX_ROWS];
        int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
        if (top->cursor >= 0 && top->cursor < count && rows[top->cursor].kind == SETTINGS_ROW_LINK)
            return rows[top->cursor].menu;
    }
    return -1;
}

// A function to get the note the last command left for the caption, or ""
const char *settings_notice(const SettingsState *state)
{
    return state->notice;
}
