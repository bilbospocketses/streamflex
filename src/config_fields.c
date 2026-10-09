#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include "launcher.h"
#include <launcher_config.h>
#include "config_fields.h"
#include "debug.h"

extern Config config;
extern Geometry geo;

// A function to put a value's color into an SDL color, leaving its alpha
static void store_rgb(SDL_Color *color, const SettingValue *value)
{
    color->r = value->color.r;
    color->g = value->color.g;
    color->b = value->color.b;
}

// A function to read an SDL color into a value
static void read_rgb(SettingValue *value, SDL_Color color)
{
    value->color.r = color.r;
    value->color.g = color.g;
    value->color.b = color.b;
}

// A function to replace a string setting with a copy of a value's text; NULL for none
static void store_text(char **field, const SettingValue *value)
{
    free(*field);
    *field = value->inherit || value->text[0] == '\0' ? NULL : strdup(value->text);
}

// A function to read a string setting into a value; one that is unset follows the default
static void read_text(SettingValue *value, const char *field, bool can_inherit)
{
    snprintf(value->text, sizeof(value->text), "%s", field != NULL ? field : "");
    value->inherit = can_inherit && field == NULL;
}

// A function to read a percentage or px setting into a value
static void read_percent(SettingValue *value, int number, bool percent)
{
    value->number = number;
    value->percent = percent;
}

// A function to derive a color's input for derive_settings(), its alpha unused
static DeriveColor derive_color(SDL_Color color)
{
    return (DeriveColor) { color.r, color.g, color.b, 0 };
}

// A function to set the values config.ini's defaults give as text (the percentages), before the
// file is read: the file's own values then replace them
void config_apply_defaults(void)
{
    for (int id = 0; id < SET_ID_GLOBAL_COUNT; id++) {
        const SettingDef *def = setting_def((SettingId) id);
        SettingValue value;
        if (def->fallback == NULL)
            continue;
        if (setting_parse(def, def->fallback, &value))
            config_store((SettingId) id, NULL, &value);
        else
            log_error("The built-in %s value '%s' does not read, ignoring it", def->key, def->fallback);
    }
}

// A function to store a setting's value into Config, or into `menu` for a per-menu setting
void config_store(SettingId id, Menu *menu, const SettingValue *value)
{
    int n = value->number;
    bool on = n != 0;
    switch (id) {
        case SET_ID_BACKGROUND_MODE: config.background_mode = (ModeBackground) n; break;
        case SET_ID_BACKGROUND_COLOR: store_rgb(&config.background_color, value); break;
        case SET_ID_BACKGROUND_IMAGE: store_text(&config.background_image, value); break;
        case SET_ID_SLIDESHOW_DIRECTORY: store_text(&config.slideshow_directory, value); break;
        case SET_ID_SLIDESHOW_DURATION: config.slideshow_image_duration = (Uint32) n * 1000; break;
        case SET_ID_SLIDESHOW_FADE: config.slideshow_transition_time = (Uint32) n; break;
        case SET_ID_LAYOUT_ROWS: config.rows = (unsigned int) n; break;
        case SET_ID_LAYOUT_COLUMNS: config.max_buttons = (unsigned int) n; break;
        case SET_ID_LAYOUT_ICON_SIZE: config.icon_size = value->inherit ? 0 : (Uint16) n; break;
        case SET_ID_TITLE_SIZE:
            if (value->percent)
                config.title_font_size_pct = n;
            else {
                config.title_font_size_pct = 0;
                config.title_font_size = (unsigned int) n;
            }
            break;
        case SET_ID_DEFAULT_MENU: store_text(&config.default_menu, value); break;
        case SET_ID_WRAP_ENTRIES: config.wrap_entries = on; break;
        case SET_ID_RESET_ON_BACK: config.reset_on_back = on; break;
        case SET_ID_MOUSE_SELECT: config.mouse_select = on; break;
        case SET_ID_INHIBIT_OS_SCREENSAVER: config.inhibit_os_screensaver = on; break;
        case SET_ID_VSYNC: config.vsync = on; break;
        case SET_ID_FPS_LIMIT: config.fps_limit = value->inherit ? -1 : n; break;
        case SET_ID_ON_LAUNCH: config.on_launch = (ModeOnLaunch) n; break;
        case SET_ID_APPLICATION_TIMEOUT: config.application_timeout = (Uint32) n * 1000; break;
        case SET_ID_STARTUP_CMD: store_text(&config.startup_cmd, value); break;
        case SET_ID_QUIT_CMD: store_text(&config.quit_cmd, value); break;
        case SET_ID_CHROMA_KEY_COLOR: store_rgb(&config.chroma_key_color, value); break;
        case SET_ID_OVERLAY: config.background_overlay = on; break;
        case SET_ID_OVERLAY_COLOR: store_rgb(&config.background_overlay_color, value); break;
        case SET_ID_OVERLAY_OPACITY: config.background_overlay_opacity = n; break;
        case SET_ID_ICON_SPACING:
            config.icon_spacing = n;
            config.icon_spacing_percent = value->percent;
            break;
        case SET_ID_VCENTER: config.vcenter = n; break;
        case SET_ID_TITLES_ENABLED: config.titles_enabled = on; break;
        case SET_ID_TITLE_FONT: store_text(&config.title_font_path, value); break;
        case SET_ID_TITLE_FONT_FACE: config.title_font_face = value->inherit ? 0 : n; break;
        case SET_ID_TITLE_COLOR: store_rgb(&config.title_font_color, value); break;
        case SET_ID_TITLE_OPACITY: config.title_opacity = n; break;
        case SET_ID_TITLE_SHADOWS: config.title_shadows = on; break;
        case SET_ID_TITLE_SHADOW_COLOR: store_rgb(&config.title_shadow_color, value); break;
        case SET_ID_TITLE_OVERSIZE: config.title_oversize_mode = (ModeOversize) n; break;
        case SET_ID_TITLE_PADDING:
            // A percentage in hundredths is whole here (SET_FLAG_WHOLE), as layout.c reads it
            config.title_padding_pct = value->percent ? n / 100 : 0;
            config.title_padding = value->percent ? 0 : n;
            break;
        case SET_ID_HIGHLIGHT_ENABLED: config.highlight = on; break;
        case SET_ID_HIGHLIGHT_FILL_COLOR: store_rgb(&config.highlight_fill_color, value); break;
        case SET_ID_HIGHLIGHT_FILL_OPACITY: config.highlight_fill_opacity = n; break;
        case SET_ID_HIGHLIGHT_OUTLINE_SIZE: config.highlight_outline_size = n; break;
        case SET_ID_HIGHLIGHT_OUTLINE_COLOR: store_rgb(&config.highlight_outline_color, value); break;
        case SET_ID_HIGHLIGHT_OUTLINE_OPACITY: config.highlight_outline_opacity = n; break;
        case SET_ID_HIGHLIGHT_CORNER_RADIUS: config.highlight_rx = n; break;
        case SET_ID_HIGHLIGHT_VPADDING: config.highlight_vpadding = n; break;
        case SET_ID_HIGHLIGHT_HPADDING: config.highlight_hpadding = n; break;
        case SET_ID_SCROLL_ENABLED: config.scroll_indicators = on; break;
        case SET_ID_SCROLL_FILL_COLOR: store_rgb(&config.scroll_indicator_fill_color, value); break;
        case SET_ID_SCROLL_OUTLINE_SIZE: config.scroll_indicator_outline_size = n; break;
        case SET_ID_SCROLL_OUTLINE_COLOR: store_rgb(&config.scroll_indicator_outline_color, value); break;
        case SET_ID_SCROLL_OPACITY: config.scroll_indicator_opacity = n; break;
        case SET_ID_CLOCK_ENABLED: config.clock_enabled = on; break;
        case SET_ID_CLOCK_SHOW_DATE: config.clock_show_date = on; break;
        case SET_ID_CLOCK_ALIGNMENT: config.clock_alignment = (Alignment) n; break;
        case SET_ID_CLOCK_FONT: store_text(&config.clock_font_path, value); break;
        case SET_ID_CLOCK_FONT_FACE: config.clock_font_face = value->inherit ? 0 : n; break;
        case SET_ID_CLOCK_COLOR: store_rgb(&config.clock_font_color, value); break;
        case SET_ID_CLOCK_SHADOWS: config.clock_shadows = on; break;
        case SET_ID_CLOCK_SHADOW_COLOR: store_rgb(&config.clock_shadow_color, value); break;
        case SET_ID_CLOCK_OPACITY: config.clock_opacity = n; break;
        case SET_ID_CLOCK_FONT_SIZE: config.clock_font_size = (unsigned int) n; break;
        case SET_ID_CLOCK_MARGIN:
            config.clock_margin = n;
            config.clock_margin_percent = value->percent;
            break;
        case SET_ID_CLOCK_TIME_FORMAT: config.clock_time_format = (TimeFormat) n; break;
        case SET_ID_CLOCK_DATE_FORMAT: config.clock_date_format = (DateFormat) n; break;
        case SET_ID_CLOCK_WEEKDAY: config.clock_include_weekday = on; break;
        case SET_ID_SCREENSAVER_ENABLED: config.screensaver_enabled = on; break;
        case SET_ID_SCREENSAVER_IDLE_TIME: config.screensaver_idle_time = (Uint32) n * 1000; break;
        case SET_ID_SCREENSAVER_INTENSITY: config.screensaver_intensity = n; break;
        case SET_ID_SCREENSAVER_PAUSE: config.screensaver_pause_slideshow = on; break;
        case SET_ID_GAMEPAD_ENABLED: config.gamepad_enabled = on; break;
        case SET_ID_GAMEPAD_DEVICE: config.gamepad_device = n; break;
        case SET_ID_GAMEPAD_MAPPINGS: store_text(&config.gamepad_mappings_file, value); break;
        case SET_ID_MENU_ROWS: menu->overrides.rows = value->inherit ? 0 : n; break;
        case SET_ID_MENU_COLUMNS: menu->overrides.columns = value->inherit ? 0 : n; break;
        case SET_ID_MENU_ICON_SIZE: menu->overrides.icon_cap = value->inherit ? 0 : n; break;
        case SET_ID_COUNT: break;
    }
}

// A function to read a setting's value from Config, or from `menu` for a per-menu setting
SettingValue config_read(SettingId id, const Menu *menu)
{
    SettingValue v;
    memset(&v, 0, sizeof(v));
    switch (id) {
        case SET_ID_BACKGROUND_MODE: v.number = (int) config.background_mode; break;
        case SET_ID_BACKGROUND_COLOR: read_rgb(&v, config.background_color); break;
        case SET_ID_BACKGROUND_IMAGE: read_text(&v, config.background_image, false); break;
        case SET_ID_SLIDESHOW_DIRECTORY: read_text(&v, config.slideshow_directory, false); break;
        case SET_ID_SLIDESHOW_DURATION: v.number = (int) (config.slideshow_image_duration / 1000); break;
        case SET_ID_SLIDESHOW_FADE: v.number = (int) config.slideshow_transition_time; break;
        case SET_ID_LAYOUT_ROWS: v.number = (int) config.rows; break;
        case SET_ID_LAYOUT_COLUMNS: v.number = (int) config.max_buttons; break;
        case SET_ID_LAYOUT_ICON_SIZE:
            v.inherit = config.icon_size == 0;
            v.number = config.icon_size;
            break;
        case SET_ID_TITLE_SIZE:
            v.percent = config.title_font_size_pct > 0;
            v.number = v.percent ? config.title_font_size_pct : (int) config.title_font_size;
            break;
        case SET_ID_DEFAULT_MENU: read_text(&v, config.default_menu, false); break;
        case SET_ID_WRAP_ENTRIES: v.number = config.wrap_entries; break;
        case SET_ID_RESET_ON_BACK: v.number = config.reset_on_back; break;
        case SET_ID_MOUSE_SELECT: v.number = config.mouse_select; break;
        case SET_ID_INHIBIT_OS_SCREENSAVER: v.number = config.inhibit_os_screensaver; break;
        case SET_ID_VSYNC: v.number = config.vsync; break;
        case SET_ID_FPS_LIMIT:
            v.inherit = config.fps_limit < 0;
            v.number = config.fps_limit;
            break;
        case SET_ID_ON_LAUNCH: v.number = (int) config.on_launch; break;
        case SET_ID_APPLICATION_TIMEOUT: v.number = (int) (config.application_timeout / 1000); break;
        case SET_ID_STARTUP_CMD: read_text(&v, config.startup_cmd, true); break;
        case SET_ID_QUIT_CMD: read_text(&v, config.quit_cmd, true); break;
        case SET_ID_CHROMA_KEY_COLOR: read_rgb(&v, config.chroma_key_color); break;
        case SET_ID_OVERLAY: v.number = config.background_overlay; break;
        case SET_ID_OVERLAY_COLOR: read_rgb(&v, config.background_overlay_color); break;
        case SET_ID_OVERLAY_OPACITY: read_percent(&v, config.background_overlay_opacity, true); break;
        case SET_ID_ICON_SPACING: read_percent(&v, config.icon_spacing, config.icon_spacing_percent); break;
        case SET_ID_VCENTER: read_percent(&v, config.vcenter, true); break;
        case SET_ID_TITLES_ENABLED: v.number = config.titles_enabled; break;
        case SET_ID_TITLE_FONT: read_text(&v, config.title_font_path, false); break;
        case SET_ID_TITLE_FONT_FACE:
            v.inherit = config.title_font_face == 0;
            v.number = config.title_font_face;
            break;
        case SET_ID_TITLE_COLOR: read_rgb(&v, config.title_font_color); break;
        case SET_ID_TITLE_OPACITY: read_percent(&v, config.title_opacity, true); break;
        case SET_ID_TITLE_SHADOWS: v.number = config.title_shadows; break;
        case SET_ID_TITLE_SHADOW_COLOR: read_rgb(&v, config.title_shadow_color); break;
        case SET_ID_TITLE_OVERSIZE: v.number = (int) config.title_oversize_mode; break;
        case SET_ID_TITLE_PADDING:
            // A 0 px padding reads as 0%: the same, and the one the steps hold
            if (config.title_padding_pct > 0 || config.title_padding == 0)
                read_percent(&v, config.title_padding_pct * 100, true);
            else
                read_percent(&v, config.title_padding, false);
            break;
        case SET_ID_HIGHLIGHT_ENABLED: v.number = config.highlight; break;
        case SET_ID_HIGHLIGHT_FILL_COLOR: read_rgb(&v, config.highlight_fill_color); break;
        case SET_ID_HIGHLIGHT_FILL_OPACITY: read_percent(&v, config.highlight_fill_opacity, true); break;
        case SET_ID_HIGHLIGHT_OUTLINE_SIZE: v.number = config.highlight_outline_size; break;
        case SET_ID_HIGHLIGHT_OUTLINE_COLOR: read_rgb(&v, config.highlight_outline_color); break;
        case SET_ID_HIGHLIGHT_OUTLINE_OPACITY: read_percent(&v, config.highlight_outline_opacity, true); break;
        case SET_ID_HIGHLIGHT_CORNER_RADIUS: v.number = config.highlight_rx; break;
        case SET_ID_HIGHLIGHT_VPADDING: v.number = config.highlight_vpadding; break;
        case SET_ID_HIGHLIGHT_HPADDING: v.number = config.highlight_hpadding; break;
        case SET_ID_SCROLL_ENABLED: v.number = config.scroll_indicators; break;
        case SET_ID_SCROLL_FILL_COLOR: read_rgb(&v, config.scroll_indicator_fill_color); break;
        case SET_ID_SCROLL_OUTLINE_SIZE: v.number = config.scroll_indicator_outline_size; break;
        case SET_ID_SCROLL_OUTLINE_COLOR: read_rgb(&v, config.scroll_indicator_outline_color); break;
        case SET_ID_SCROLL_OPACITY: read_percent(&v, config.scroll_indicator_opacity, true); break;
        case SET_ID_CLOCK_ENABLED: v.number = config.clock_enabled; break;
        case SET_ID_CLOCK_SHOW_DATE: v.number = config.clock_show_date; break;
        case SET_ID_CLOCK_ALIGNMENT: v.number = (int) config.clock_alignment; break;
        case SET_ID_CLOCK_FONT: read_text(&v, config.clock_font_path, false); break;
        case SET_ID_CLOCK_FONT_FACE:
            v.inherit = config.clock_font_face == 0;
            v.number = config.clock_font_face;
            break;
        case SET_ID_CLOCK_COLOR: read_rgb(&v, config.clock_font_color); break;
        case SET_ID_CLOCK_SHADOWS: v.number = config.clock_shadows; break;
        case SET_ID_CLOCK_SHADOW_COLOR: read_rgb(&v, config.clock_shadow_color); break;
        case SET_ID_CLOCK_OPACITY: read_percent(&v, config.clock_opacity, true); break;
        case SET_ID_CLOCK_FONT_SIZE: v.number = (int) config.clock_font_size; break;
        case SET_ID_CLOCK_MARGIN: read_percent(&v, config.clock_margin, config.clock_margin_percent); break;
        case SET_ID_CLOCK_TIME_FORMAT: v.number = (int) config.clock_time_format; break;
        case SET_ID_CLOCK_DATE_FORMAT: v.number = (int) config.clock_date_format; break;
        case SET_ID_CLOCK_WEEKDAY: v.number = config.clock_include_weekday; break;
        case SET_ID_SCREENSAVER_ENABLED: v.number = config.screensaver_enabled; break;
        case SET_ID_SCREENSAVER_IDLE_TIME: v.number = (int) (config.screensaver_idle_time / 1000); break;
        case SET_ID_SCREENSAVER_INTENSITY: read_percent(&v, config.screensaver_intensity, true); break;
        case SET_ID_SCREENSAVER_PAUSE: v.number = config.screensaver_pause_slideshow; break;
        case SET_ID_GAMEPAD_ENABLED: v.number = config.gamepad_enabled; break;
        case SET_ID_GAMEPAD_DEVICE: v.number = config.gamepad_device; break;
        case SET_ID_GAMEPAD_MAPPINGS: read_text(&v, config.gamepad_mappings_file, false); break;
        case SET_ID_MENU_ROWS:
            v.inherit = menu->overrides.rows == 0;
            v.number = menu->overrides.rows;
            break;
        case SET_ID_MENU_COLUMNS:
            v.inherit = menu->overrides.columns == 0;
            v.number = menu->overrides.columns;
            break;
        case SET_ID_MENU_ICON_SIZE:
            v.inherit = menu->overrides.icon_cap == 0;
            v.number = menu->overrides.icon_cap;
            break;
        case SET_ID_COUNT:
            break;
    }
    return v;
}

// A function to gather the configured values derive_settings() works from
DeriveInput derive_input(void)
{
    DeriveInput in;
    memset(&in, 0, sizeof(in));
    in.screen_width = geo.screen_width;
    in.screen_height = geo.screen_height;
    in.titles_enabled = config.titles_enabled;
    in.title_padding = config.title_padding;
    in.title_padding_pct = config.title_padding_pct;
    in.title_color = derive_color(config.title_font_color);
    in.title_shadow_color = derive_color(config.title_shadow_color);
    in.title_opacity = config.title_opacity;
    in.overlay_color = derive_color(config.background_overlay_color);
    in.overlay_opacity = config.background_overlay_opacity;
    in.highlight_fill = derive_color(config.highlight_fill_color);
    in.highlight_fill_opacity = config.highlight_fill_opacity;
    in.highlight_outline = derive_color(config.highlight_outline_color);
    in.highlight_outline_opacity = config.highlight_outline_opacity;
    in.highlight_outline_size = config.highlight_outline_size;
    in.highlight_rx = config.highlight_rx;
    in.highlight_hpadding = config.highlight_hpadding;
    in.highlight_vpadding = config.highlight_vpadding;
    in.scroll_fill = derive_color(config.scroll_indicator_fill_color);
    in.scroll_outline = derive_color(config.scroll_indicator_outline_color);
    in.scroll_opacity = config.scroll_indicator_opacity;
    in.scroll_outline_size = config.scroll_indicator_outline_size;
    in.clock_color = derive_color(config.clock_font_color);
    in.clock_shadow_color = derive_color(config.clock_shadow_color);
    in.clock_opacity = config.clock_opacity;
    in.icon_spacing = config.icon_spacing;
    in.icon_spacing_percent = config.icon_spacing_percent;
    in.vcenter = config.vcenter;
    in.clock_margin = config.clock_margin;
    in.clock_margin_percent = config.clock_margin_percent;
    in.screensaver_intensity = config.screensaver_intensity;
    return in;
}
