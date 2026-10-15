#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>     // main() is here: on Windows this header gives SDL3's WinMain
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "launcher.h"
#include <launcher_config.h>
#include "image.h"
#include "util.h"
#include "videodriver.h"
#include "library.h"
#include "debug.h"
#include "clock.h"
#include "config_fields.h"
#include "settings_screen.h"
#include "settings_pickers.h"
#include "platform/platform.h"
#include "alloc.h"
#include "fileio.h"
#include "test_hooks.h"
#include "winplan.h"
#include "vsyncwatch.h"
#include "keycodes.h"

static void init_sdl(void);
static void create_window(void);
static WindowPlan plan_for(int background);
static WindowPlan plan_in_force(void);
static SDL_Window *make_window(const WindowPlan *plan, SDL_DisplayID display);
static bool recreate_window(const WindowPlan *plan);
static void fall_back_to_opaque(const char *reason);
static void transparent_reason(const WindowPlan *plan, char *out, size_t size);
static void run_recreation(void);
static void refit_transparent_window(void);
static int drop_textures(void);
static bool create_renderer(const WindowPlan *plan);
static void raise_restarted_window(void);
static void init_sdl_ttf(void);
static int load_menu(Menu *menu, bool set_back_menu, bool reset_position);
static int load_menu_by_name(const char *menu_name, bool set_back_menu, bool reset_position);
static void update_slideshow(void);
static void resume_slideshow(void);
static void fall_back_from_slideshow(SDL_Surface *surface);
static void update_screensaver(void);
static void update_clock(bool block);
static void init_slideshow(void);
static void stop_slideshow(void);
static void start_screensaver(void);
static void stop_screensaver(void);
static void start_overlay(void);
static void stop_overlay(void);
static void start_highlight(void);
static void stop_highlight(void);
static void start_scroll(void);
static void stop_scroll(void);
static void start_clock(void);
static void start_gamepad(void);
static void stop_gamepad(void);
static void connect_present_pads(void);
static void rebase_held_controls(void);
#ifdef STREAMFLEX_TEST_HOOKS
static void test_pad_attach(void);
static void test_pad_swap(void);
static void test_pad_update(void);
static void test_pad_power(void);
static void test_pad_stop(void);
static void test_app_update(void);
static void test_pixel_size_update(void);
static void test_display_event_update(void);
static void test_render_event_update(void);
static void test_list_without(const char *list, const char *name, char *out, size_t size);
static bool test_pixel_size(int *width, int *height);
#endif
static SDL_Thread *start_clock_thread(void);
static inline void pre_launch(void);
static inline void post_launch(void);
#ifdef _WIN32
static void retry_exit_hotkey_events(bool at_once);
#endif
static bool renderer_vsync(void);
static void check_vsync(void);
static void calculate_layout_area(void);
static int apply_layout(Menu *menu);
static void render_buttons(Menu *menu, const LayoutGeometry *geometry);
static void place_entries(void);
static void move_selection(LayoutDirection direction);
static void load_submenu(const char *submenu);
static void load_back_menu(Menu *menu);
static void draw_screen(void);
static void handle_keypress(SDL_Keycode key, bool repeat);
static bool hotkey_bound(SDL_Keycode keycode);
static bool menu_key(SDL_Keycode keycode);
static void execute_command(const char *command);
static void poll_gamepad(void);
static void init_gamepad(Gamepad **gamepad, int device_index);
static void connect_gamepad(int device_index, bool open, bool raise_error);
static void disconnect_gamepad(int id, bool disconnect, bool remove);
static int gamepad_device_index(const Gamepad *gamepad);
static void open_controller(Gamepad *gamepad, bool raise_error);
static void log_pad_power(const char *what, const Gamepad *gamepad);
static void gamepad_power_changed(SDL_JoystickID id, SDL_PowerState power_state, int percent);
static void draw_pad_battery(void);
static void cleanup(void);
static void close_log(void);
static void release_last(void);

// Initialize default settings
Config config = {
    .default_menu                     = NULL,
    .background_image                 = NULL,
    .slideshow_directory              = NULL,
    .title_font_path                  = NULL,
    .vsync                            = true,
    .fps_limit                        = -1,
    .application_timeout              = DEFAULT_APPLICATION_TIMEOUT * 1000,
    .titles_enabled                   = DEFAULT_TITLES_ENABLED,
    .title_font_size                  = DEFAULT_FONT_SIZE,
    .title_font_size_pct              = DEFAULT_FONT_SIZE_PERCENT,
    .title_font_color.r               = DEFAULT_TITLE_FONT_COLOR_R,
    .title_font_color.g               = DEFAULT_TITLE_FONT_COLOR_G,
    .title_font_color.b               = DEFAULT_TITLE_FONT_COLOR_B,
    .title_font_color.a               = DEFAULT_TITLE_FONT_COLOR_A,
    .title_shadows                    = DEFAULT_TITLE_SHADOWS,
    .title_shadow_color.r             = DEFAULT_TITLE_SHADOW_COLOR_R,
    .title_shadow_color.g             = DEFAULT_TITLE_SHADOW_COLOR_G,
    .title_shadow_color.b             = DEFAULT_TITLE_SHADOW_COLOR_B,
    .title_shadow_color.a             = DEFAULT_TITLE_SHADOW_COLOR_A,
    .background_mode                  = BACKGROUND_COLOR,
    .background_color.r               = DEFAULT_BACKGROUND_COLOR_R,
    .background_color.g               = DEFAULT_BACKGROUND_COLOR_G,
    .background_color.b               = DEFAULT_BACKGROUND_COLOR_B,
    .chroma_key_color.r               = DEFAULT_CHROMA_KEY_COLOR_R,
    .chroma_key_color.g               = DEFAULT_CHROMA_KEY_COLOR_G,
    .chroma_key_color.b               = DEFAULT_CHROMA_KEY_COLOR_B,
    .chroma_key_color.a               = DEFAULT_CHROMA_KEY_COLOR_A,
    .background_color.a               = 0xFF,
    .background_overlay               = DEFAULT_BACKGROUND_OVERLAY,
    .background_overlay_color.r       = DEFAULT_BACKGROUND_OVERLAY_COLOR_R,
    .background_overlay_color.g       = DEFAULT_BACKGROUND_OVERLAY_COLOR_G,
    .background_overlay_color.b       = DEFAULT_BACKGROUND_OVERLAY_COLOR_B,
    .background_overlay_color.a       = DEFAULT_BACKGROUND_OVERLAY_COLOR_A,
    .highlight                        = true,
    .icon_size                        = 0, // IconSize is an optional cap on button size; 0 = none
    .highlight_fill_color.r           = DEFAULT_HIGHLIGHT_FILL_COLOR_R,
    .highlight_fill_color.g           = DEFAULT_HIGHLIGHT_FILL_COLOR_G,
    .highlight_fill_color.b           = DEFAULT_HIGHLIGHT_FILL_COLOR_B,
    .highlight_outline_color.r        = DEFAULT_HIGHLIGHT_OUTLINE_COLOR_R,
    .highlight_outline_color.g        = DEFAULT_HIGHLIGHT_OUTLINE_COLOR_G,
    .highlight_outline_color.b        = DEFAULT_HIGHLIGHT_OUTLINE_COLOR_B,
    .highlight_outline_color.a        = DEFAULT_HIGHLIGHT_OUTLINE_COLOR_A,
    .highlight_outline_size           = DEFAULT_HIGHLIGHT_OUTLINE_SIZE,
    .highlight_rx                     = DEFAULT_HIGHLIGHT_CORNER_RADIUS,
    .title_padding                    = 0,
    .title_padding_pct                = DEFAULT_TITLE_PADDING_PERCENT,
    .max_buttons                      = DEFAULT_MAX_BUTTONS,
    .rows                             = DEFAULT_ROWS,
    .highlight_vpadding               = DEFAULT_HIGHLIGHT_VPADDING,
    .highlight_hpadding               = DEFAULT_HIGHLIGHT_HPADDING,
    .scroll_indicators                = DEFAULT_SCROLL_INDICATORS,
    .scroll_indicator_fill_color.r    = DEFAULT_SCROLL_INDICATOR_FILL_COLOR_R,
    .scroll_indicator_fill_color.g    = DEFAULT_SCROLL_INDICATOR_FILL_COLOR_G,
    .scroll_indicator_fill_color.b    = DEFAULT_SCROLL_INDICATOR_FILL_COLOR_B,
    .scroll_indicator_fill_color.a    = DEFAULT_SCROLL_INDICATOR_FILL_COLOR_A,
    .scroll_indicator_outline_size    = DEFAULT_SCROLL_INDICATOR_OUTLINE_SIZE,
    .scroll_indicator_outline_color.r = DEFAULT_SCROLL_INDICATOR_OUTLINE_COLOR_R,
    .scroll_indicator_outline_color.g = DEFAULT_SCROLL_INDICATOR_OUTLINE_COLOR_G,
    .scroll_indicator_outline_color.b = DEFAULT_SCROLL_INDICATOR_OUTLINE_COLOR_B,
    .scroll_indicator_outline_color.a = DEFAULT_SCROLL_INDICATOR_OUTLINE_COLOR_A,
    .title_oversize_mode              = OVERSIZE_TRUNCATE,
    .wrap_entries                     = DEFAULT_WRAP_ENTRIES,
    .reset_on_back                    = DEFAULT_RESET_ON_BACK,
    .mouse_select                     = DEFAULT_MOUSE_SELECT,
    .inhibit_os_screensaver           = DEFAULT_INHIBIT_OS_SCREENSAVER,
    .startup_cmd                      = NULL,
    .quit_cmd                         = NULL,
    .screensaver_enabled              = false,
    .screensaver_idle_time            = DEFAULT_SCREENSAVER_IDLE_TIME*1000,
    .screensaver_pause_slideshow      = DEFAULT_SCREENSAVER_PAUSE_SLIDESHOW,
    .gamepad_enabled                  = DEFAULT_GAMEPAD_ENABLED,
    .gamepad_device                   = DEFAULT_GAMEPAD_DEVICE,
    .gamepad_mappings_file            = NULL,
    .on_launch                        = ON_LAUNCH_BLANK,
    .debug                            = false,
    .restarted                        = false,
    .exe_path                         = NULL,
    .first_menu                       = NULL,
    .num_menus                        = 0,
    .clock_enabled                    = DEFAULT_CLOCK_ENABLED,
    .clock_show_date                  = DEFAULT_CLOCK_SHOW_DATE,
    .clock_alignment                  = DEFAULT_CLOCK_ALIGNMENT,
    .clock_font_path                  = NULL,
    .clock_font_color.r               = DEFAULT_CLOCK_FONT_COLOR_R,
    .clock_font_color.g               = DEFAULT_CLOCK_FONT_COLOR_G,
    .clock_font_color.b               = DEFAULT_CLOCK_FONT_COLOR_B,
    .clock_font_color.a               = DEFAULT_CLOCK_FONT_COLOR_A,
    .clock_shadows                    = DEFAULT_CLOCK_SHADOWS,
    .clock_shadow_color.r             = DEFAULT_CLOCK_SHADOW_COLOR_R,
    .clock_shadow_color.g             = DEFAULT_CLOCK_SHADOW_COLOR_G,
    .clock_shadow_color.b             = DEFAULT_CLOCK_SHADOW_COLOR_B,
    .clock_shadow_color.a             = DEFAULT_CLOCK_SHADOW_COLOR_A,
    .clock_font_size                  = DEFAULT_CLOCK_FONT_SIZE,
    .clock_time_format                = DEFAULT_CLOCK_TIME_FORMAT,
    .clock_date_format                = DEFAULT_CLOCK_DATE_FORMAT,
    .clock_include_weekday            = DEFAULT_CLOCK_INCLUDE_WEEKDAY,
    .slideshow_image_duration         = DEFAULT_SLIDESHOW_IMAGE_DURATION,
    .slideshow_transition_time        = DEFAULT_SLIDESHOW_TRANSITION_TIME
};

// Initialize default states
State state = { false };

// Global variables
SDL_Window *window                    = NULL;
SDL_Renderer *renderer                = NULL;
SDL_Texture *background_texture       = NULL;
SDL_Texture *background_overlay       = NULL;
SDL_Texture *background_override      = NULL; // The image being browsed in settings, shown in their preview
Menu *default_menu                    = NULL;
Menu *current_menu                    = NULL;
ModeBackground background_shown       = BACKGROUND_COLOR; // What is on screen: the color when the chosen background failed
double background_luminance           = -1.0; // The image on show's mean luminance, for the contrast warning; -1 unknown
Entry *current_entry                  = NULL;
Highlight *highlight                  = NULL;
Scroll *scroll                        = NULL;
Slideshow *slideshow                  = NULL;
Screensaver *screensaver              = NULL;
FILE *log_file                        = NULL;
Gamepad *gamepads                     = NULL;
GamepadControl *gamepad_controls      = NULL;
Hotkey *hotkeys                       = NULL;
Clock *clk                            = NULL;
SDL_Thread *Slideshowhread            = NULL;
SDL_Thread *clock_thread              = NULL;
SDL_Event event;
static int display_rate = DEFAULT_REFRESH_RATE;   // The window's display's refresh rate, in whole hertz
static float display_scale = 1.0F;                // The OS's scale for that display: logged, never applied (spec section 6)
static char **start_argv              = NULL; // The arguments StreamFlex was started with, as main() had them,
                                              // and --restarted after them for a restart; NULL-terminated
TextInfo title_info;
static FontSet *fixed_title_font = NULL; // The title font's set at the fixed FontSize
Ticks ticks;
Geometry geo;
LayoutGeometry layout;                     // The current menu's layout
LayoutArea layout_area;                    // The part of the screen the buttons may use
Uint32 refresh_period;
Effective eff;                        // The values drawn with, derived from config (derive.h)
SDL_Color title_color;                // eff's title colors as SDL colors, for the titles (the clock keeps its own)
SDL_Color title_shadow_color;
static bool gamepad_on = false;   // The game controller subsystem is running
static bool vsync_wanted = true; // VSync and FPSLimit ask the renderer for VSync
static bool vsync_on = false;    // The renderer really gives it; unless both are true present_frame() paces each frame
static VsyncWatch vsync_watch;   // Whether a renderer that reports VSync presents at the display's pace (vsyncwatch.h)
#define RENDERER_RETRIES 5        // Attempts at a new renderer after a lost device...
#define RENDERER_RETRY_MS 2000    // ...this far apart, the main loop pumping events between them
static bool renderer_owed = false;       // The device was lost: a new renderer is owed
static int renderer_attempt = 0;         // The attempts made at it so far; 0 at startup
static Uint64 renderer_next_try = 0;     // When the next one is due (SDL_GetTicks)
static SDL_WindowID window_id = 0;                  // The window's ID: a window event naming another is a replaced window's
static WindowKind window_kind = WINDOW_FULLSCREEN;  // The window in force: fullscreen, or Transparent one row short
static bool transparent_failed = false;             // No renderer could draw a transparent window since Transparent was chosen
static bool recreate_wanted = false;                // window_follow_background() asked for a window of another kind

COMPILE_CHECK(winplan_knows_transparent, WINPLAN_BACKGROUND_TRANSPARENT == BACKGROUND_TRANSPARENT);

// StreamFlex's power and connection values are SDL's own, so a pad's report is stored as it comes
COMPILE_CHECK(padpower_error_is_sdls, PADPOWER_ERROR == (PadPowerState) SDL_POWERSTATE_ERROR);
COMPILE_CHECK(padpower_unknown_is_sdls, PADPOWER_UNKNOWN == (PadPowerState) SDL_POWERSTATE_UNKNOWN);
COMPILE_CHECK(padpower_on_battery_is_sdls, PADPOWER_ON_BATTERY == (PadPowerState) SDL_POWERSTATE_ON_BATTERY);
COMPILE_CHECK(padpower_no_battery_is_sdls, PADPOWER_NO_BATTERY == (PadPowerState) SDL_POWERSTATE_NO_BATTERY);
COMPILE_CHECK(padpower_charging_is_sdls, PADPOWER_CHARGING == (PadPowerState) SDL_POWERSTATE_CHARGING);
COMPILE_CHECK(padpower_charged_is_sdls, PADPOWER_CHARGED == (PadPowerState) SDL_POWERSTATE_CHARGED);
COMPILE_CHECK(padconn_invalid_is_sdls, PADCONN_INVALID == (PadConnection) SDL_JOYSTICK_CONNECTION_INVALID);
COMPILE_CHECK(padconn_unknown_is_sdls, PADCONN_UNKNOWN == (PadConnection) SDL_JOYSTICK_CONNECTION_UNKNOWN);
COMPILE_CHECK(padconn_wired_is_sdls, PADCONN_WIRED == (PadConnection) SDL_JOYSTICK_CONNECTION_WIRED);
COMPILE_CHECK(padconn_wireless_is_sdls, PADCONN_WIRELESS == (PadConnection) SDL_JOYSTICK_CONNECTION_WIRELESS);

static PadPowerLevel battery_shown = PADPOWER_HIDDEN;   // The glyph as last drawn, for the log
static SDL_Rect battery_rect = { 0, 0, 0, 0 };


// A function to initialize SDL
static void init_sdl()
{
    // Set flags, hints. SDL3 has no SDL_HINT_RENDER_SCALE_QUALITY: each texture is made linear as it is
    // created (surface_texture()); the other two hints are SDL3's under the same names
    SDL_InitFlags sdl_flags = SDL_INIT_VIDEO;
#ifdef __unix__
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#endif
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, config.inhibit_os_screensaver ? "0" : "1");

    // The app id Wayland compositors (and X11's WM_CLASS) match to streamflex.desktop, however
    // StreamFlex was started: without it SDL falls back to the program file's name. No video driver
    // is set: SDL's own choice (Wayland when the compositor offers fifo-v1, else X11) stands, and
    // SDL_VIDEO_DRIVER overrides it (spec section 5).
    SDL_SetAppMetadata(PROJECT_NAME, PROJECT_VERSION, "streamflex");

    // Initialize SDL
    if (!SDL_Init(sdl_flags))
        log_fatal("Could not initialize SDL\n%s", SDL_GetError());

    // Say where the driver list came from when the environment set it, or why SDL kept to X11
    // while a Wayland compositor runs
    char note[160];
    if (video_driver_note(SDL_GetCurrentVideoDriver(), SDL_getenv("SDL_VIDEO_DRIVER"), SDL_getenv("SDL_VIDEODRIVER"),
                          SDL_getenv("WAYLAND_DISPLAY"), note, sizeof(note)))
        log_debug("%s", note);
}

// A function to tell whether the renderer really presents with VSync, by SDL's answer
// (SDL_GetRenderVSync). SDL3 stands in for VSync a driver refuses, pacing each present itself, and
// reports whatever VSync was asked for, so outside the test hook this never reads a refusal. A
// renderer that claims VSync and does not deliver it is the VSync watch's to find (vsyncwatch.h).
static bool renderer_vsync()
{
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: its renderers never refuse VSync, so a refusal is made
    // up here. "off" is a renderer that keeps VSync on when it is not wanted; any other value never
    // gives it.
    const char *refused = getenv("STREAMFLEX_TEST_VSYNC_REFUSED");
    if (refused != NULL)
        return strcmp(refused, "off") == 0;
#endif
    int vsync = 0;
    return renderer != NULL && SDL_GetRenderVSync(renderer, &vsync) && vsync != 0;
}

// A function to find out whether the renderer gives the VSync wanted, and say so; without it,
// present_frame() paces each frame to refresh_period. A renderer that says it gives VSync is
// watched afresh from here, for presents that come faster than the display's
static void check_vsync()
{
    vsync_on = renderer_vsync();
    vsyncwatch_reset(&vsync_watch);
    log_debug("Frame timing: VSync wanted %s, the renderer gives %s", vsync_wanted ? "on" : "off", vsync_on ? "on" : "off");
    if (vsync_wanted && !vsync_on)
        log_error("The renderer refused VSync: each frame is paced to %u ms instead", refresh_period);
    else if (!vsync_wanted && vsync_on)
        log_error("The renderer would not turn VSync off: each frame is still paced to %u ms", refresh_period);
}

// A function to work out the frame timing from VSync and FPSLimit, live: VSync, or the FPS limit's
// own frame time when it is off and the limit lies between the minimum and the display's rate
// (otherwise VSync, as always). The settings stay as written; the renderer follows (SDL_SetRenderVSync).
// Nothing else follows the frame time: the gamepad's repeat (poll_gamepad()), the slideshow's fade and
// the screensaver's dim (derive_fade_alpha()) are timed in wall time.
void apply_frame_timing()
{
    int rate = display_rate;
    bool wanted = config.vsync || config.fps_limit < MIN_FPS_LIMIT || config.fps_limit > rate;
    bool changed = wanted != vsync_wanted;
    vsync_wanted = wanted;

    // With VSync wanted the frame is the display's, which also paces a renderer that refused it
    refresh_period = 1000 / (Uint32) (wanted ? rate : config.fps_limit);
    if (wanted && !config.vsync && config.fps_limit < MIN_FPS_LIMIT)
        log_debug("Frame timing: VSync is off, but no FPS limit is set, so frames keep the display's %i Hz, %u ms a frame",
                  rate, refresh_period);
    else if (wanted)
        log_debug("Frame timing: VSync at %i Hz, %u ms a frame", rate, refresh_period);
    else
        log_debug("Frame timing: FPS limit %i, %u ms a frame", config.fps_limit, refresh_period);

    // The renderer is asked only when the wish changes, so one that refused VSync is not asked, or
    // logged, again for each FPSLimit change. A failure to switch counts as a refusal.
    if (renderer != NULL && changed) {
        if (!SDL_SetRenderVSync(renderer, wanted ? 1 : 0))
            log_error("Could not turn VSync %s\n%s", wanted ? "on" : "off", SDL_GetError());
        check_vsync();
    }
}

// A function to let the OS screensaver run, or block it, as InhibitOSScreensaver says
void apply_os_screensaver()
{
    if (config.inhibit_os_screensaver)
        SDL_DisableScreenSaver();
    else
        SDL_EnableScreenSaver();
}

// A function to point :home at the default menu the config names; one that is gone keeps the last
void apply_default_menu()
{
    Menu *menu = config.default_menu != NULL ? get_menu(config.default_menu) : NULL;
    if (menu != NULL)
        default_menu = menu;
}

// A function to make the renderer for the window in force, the renderer half of create_window()
// (spec section 3): the plan's list, or none when SDL_RENDER_DRIVER chose; the blend mode; the
// Video: and Renderer: lines; VSync as the frame timing wants it, and whether the renderer gives it;
// the draw color. False, with SDL's reason logged, when no renderer could be made: the caller
// decides what that means.
static bool create_renderer(const WindowPlan *plan)
{
    const char *list = plan->render_list;
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this, for a new renderer after a lost device: the renderer
    // STREAMFLEX_TEST_RENDERER_SKIP names is left out of the list, as a GPU that no longer offers it
    // would leave it, and the first STREAMFLEX_TEST_RENDERER_FAIL attempts fail
    char without[128];
    const char *skip = getenv("STREAMFLEX_TEST_RENDERER_SKIP");
    if (renderer_attempt > 0 && list != NULL && skip != NULL) {
        test_list_without(list, skip, without, sizeof(without));
        list = without;
    }
    const char *fail = getenv("STREAMFLEX_TEST_RENDERER_FAIL");
    if (renderer_attempt > 0 && fail != NULL && renderer_attempt <= atoi(fail)) {
        SDL_SetError("Test hook: attempt %i fails", renderer_attempt);
        log_error("Could not initialize renderer\n%s", SDL_GetError());
        return false;
    }
#endif
    renderer = SDL_CreateRenderer(window, list);
    if (renderer == NULL) {
        log_error("Could not initialize renderer\n%s", SDL_GetError());
        return false;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Which video driver and renderer SDL chose, and whether the list's first choice was not it
    const char *name = SDL_GetRendererName(renderer);
    char line[160];
    winplan_renderer_line(plan->render_list, name, line, sizeof(line));
    log_debug("Video: SDL's %s driver, the %s renderer", SDL_GetCurrentVideoDriver(), name);
    log_debug("%s", line);

    if (!SDL_SetRenderVSync(renderer, vsync_wanted ? 1 : 0))
        log_error("Could not turn VSync %s\n%s", vsync_wanted ? "on" : "off", SDL_GetError());
    check_vsync();
    set_draw_color();
    return true;
}

// A function to read the window's size in pixels, the screen's one source (spec section 6): never the
// desktop mode, which Wayland gives in scaled units
static void window_pixel_size(int *width, int *height)
{
    *width = 0;
    *height = 0;
    SDL_GetWindowSizeInPixels(window, width, height);
#ifdef STREAMFLEX_TEST_HOOKS
    test_pixel_size(width, height);   // Only the headless harness builds this: a forced size wins
#endif
}

// A function to set the screen geometry from the window once the window system has carried out what
// was asked of it (the fullscreen request, a resize); true when the size changed
static bool read_screen_geometry(void)
{
    SDL_SyncWindow(window);   // At worst it times out, and the size read is the one in force
    int width, height;
    window_pixel_size(&width, &height);
    if (width == geo.screen_width && height == geo.screen_height)
        return false;
    ScreenGeometry screen = geometry_from_pixels(width, height);
    geo.screen_width = screen.screen_width;
    geo.screen_height = screen.screen_height;
    geo.screen_margin = screen.screen_margin;
    geo.title_min_size = screen.title_min_size;
    title_info.min_size = screen.title_min_size;
    return true;
}

// A function to read the refresh rate of the display the window is on, the one thing the desktop mode
// still gives. No mode, or a rate of 0 (Xvfb, some VMs and remote desktops), keeps 60 Hz.
static void read_refresh_rate(void)
{
    const SDL_DisplayMode *mode = SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(window));
    display_rate = derive_refresh_rate(mode != NULL ? mode->refresh_rate : 0.0F);
    if (mode == NULL || mode->refresh_rate < DERIVE_MIN_REFRESH_RATE)
        log_debug("The display reports no refresh rate, using %i Hz", DEFAULT_REFRESH_RATE);
}

// A function to read what a window startup has just made gives: the screen from its pixel size (spec
// section 6), the OS's scale, logged and never applied, its display's refresh rate, and the frame
// timing worked out from that rate, whose VSync wish create_renderer() then asks for
static void window_made(void)
{
    // The screen is the window's pixel size (spec section 6). The OS's scale is logged, never applied:
    // pixel and point settings keep meaning pixels and points.
    read_screen_geometry();
    display_scale = SDL_GetWindowDisplayScale(window);
    log_debug("Screen: %ix%i px, pixel density %.2f, display scale %.2f", geo.screen_width, geo.screen_height,
              (double) SDL_GetWindowPixelDensity(window), (double) display_scale);
    read_refresh_rate();
    apply_frame_timing();
}

// A function to create the window and its renderer, the window made as the kind the background needs
// from the start (spec section 3): Transparent on Windows is a see-through window one row short of the
// display, every other background is fullscreen. An override outside the transparent list is refused
// by its name before any renderer is made, and a Transparent window that gets no renderer falls back
// to an opaque one; fall_back_to_opaque() then makes the window and its renderer itself, as below.
static void create_window()
{
    WindowPlan plan = plan_for((int) config.background_mode);
    if (plan.refused)
        fall_back_to_opaque(plan.reason);
    else {
        window = make_window(&plan, SDL_GetPrimaryDisplay());
        if (window == NULL)
            log_fatal("Could not create SDL Window\n%s", SDL_GetError());
        window_id = SDL_GetWindowID(window);
        window_kind = plan.kind;
        window_made();
        if (!create_renderer(&plan)) {
            if (plan.kind != WINDOW_TRANSPARENT_SHORT)
                quit(EXIT_FAILURE);   // create_renderer() logged SDL's reason, as log_fatal would
            char why[512];
            transparent_reason(&plan, why, sizeof(why));
            SDL_DestroyWindow(window);
            window = NULL;
            fall_back_to_opaque(why);
        }
    }
    SDL_HideCursor();

    // One cross-check, once: what the renderer draws to is the window's size. SDL_GetCurrentRenderOutputSize
    // is never asked: it follows the render target, which is the settings preview while that draws.
    int output_width = 0, output_height = 0;
    if (SDL_GetRenderOutputSize(renderer, &output_width, &output_height) &&
        (output_width != geo.screen_width || output_height != geo.screen_height))
        log_error("Screen: the renderer draws %ix%i px, not the window's %ix%i",
                  output_width, output_height, geo.screen_width, geo.screen_height);
#ifdef _WIN32
    // The handle, read after the renderer exists (OpenGL's replaces the native window), and with it
    // the taskbar's mark for a Transparent window (Task 1.8a's call inside platform_window_changed())
    platform_window_changed(NULL, window);
#endif
}

// A function to bring a restart's fresh copy to the front, with the keyboard: on Windows the copy
// before it gave it the foreground, which a new process cannot take otherwise, and the log says
// whether Windows put it in front
static void raise_restarted_window()
{
    log_debug("Restarted, so the window is brought to the front");
    SDL_RaiseWindow(window);
#ifdef _WIN32
    if (take_foreground())
        log_debug("Restart: the window is in front, with the keyboard");
    else
        log_error("Restart: Windows kept the window behind another, so it has no keyboard until it is brought to the front");
#endif
}

// A function to plan the window for a background on this platform, under SDL_RENDER_DRIVER when it is
// set. SDL_getenv, not getenv, which MSVC's /W4 /WX refuses as unsafe.
static WindowPlan plan_for(int background)
{
    bool windows = WINPLAN_WINDOWS;
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: STREAMFLEX_TEST_WINDOWS_PLAN plans every window as Windows
    // does, so Xvfb runs the Transparent window, its recreation and its fallbacks (the Windows-only
    // calls around them stay Windows')
    if (getenv("STREAMFLEX_TEST_WINDOWS_PLAN") != NULL)
        windows = true;
#endif
    return window_plan(background, windows, SDL_getenv("SDL_RENDER_DRIVER"));
}

// A function to plan a new renderer for the window in force, after a lost device: the window's kind
// picks the list, never the Mode setting, which may have moved on while settings wait for a rest
static WindowPlan plan_in_force(void)
{
    return plan_for(window_kind == WINDOW_TRANSPARENT_SHORT ? BACKGROUND_TRANSPARENT : BACKGROUND_COLOR);
}

// A function to make the window a plan asks for, on a display, at native pixels: fullscreen (SDL3's
// desktop fullscreen), or for Transparent on Windows borderless and see-through at the display's
// top-left corner, one pixel row short of it. Shown, with no renderer yet; NULL, with SDL's reason,
// when it could not be made.
static SDL_Window *make_window(const WindowPlan *plan, SDL_DisplayID display)
{
    SDL_PropertiesID props = SDL_CreateProperties();
    if (props == 0)
        return NULL;
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, PROJECT_NAME);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
    SDL_Rect bounds = { 0 };
    bool bounds_read = SDL_GetDisplayBounds(display, &bounds);
    if (plan->kind == WINDOW_TRANSPARENT_SHORT) {
        if (!bounds_read) {
            SDL_DestroyProperties(props);
            return NULL;
        }
        WindowRect rect = window_short_rect((WindowRect) { bounds.x, bounds.y, bounds.w, bounds.h });
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, true);
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_TRANSPARENT_BOOLEAN, true);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, rect.x);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, rect.y);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, rect.w);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, rect.h);
    }
    else {
        // The display's bounds as its size: with no window manager to carry out the fullscreen request
        // (Xvfb), a window asked for at 0 x 0 stays 1 x 1. A display whose bounds cannot be read leaves
        // them 0 x 0, as before.
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_UNDEFINED_DISPLAY(display));
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_UNDEFINED_DISPLAY(display));
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, bounds.w);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, bounds.h);
    }
    SDL_Window *made = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    return made;
}

// A function to say why no renderer could draw a transparent window, right after create_renderer()
// failed for its plan, while SDL's error is still that failure's
static void transparent_reason(const WindowPlan *plan, char *out, size_t size)
{
    if (plan->render_list != NULL)
        snprintf(out, size, "no renderer on the list %s could be made: %s", plan->render_list, SDL_GetError());
    else
        snprintf(out, size, "SDL_RENDER_DRIVER=%s could not be made: %s", SDL_getenv("SDL_RENDER_DRIVER"), SDL_GetError());
}

// A function to replace the window with one of the kind a plan gives, keeping StreamFlex in front, its
// hotkeys live and its focus logic quiet. The new window and its renderer are made while the old ones
// still stand: a process whose only window is gone loses its right to the foreground, and SDL drops
// pad events for an unfocused window. Every texture goes with the old renderer, every holder of the
// window moves to the new one, the new window takes the foreground while the old one still holds it,
// and only then does the old one go. The full reload is marked, whatever the new window's size. A new
// renderer a lost device still owed is owed no more: the new window's renderer stands in for it.
// False, with SDL's reason logged and still in SDL_GetError(), when the new window or its renderer
// could not be made: the old window, its renderer, its textures and any renderer still owed stand as
// they were.
static bool recreate_window(const WindowPlan *plan)
{
    SDL_Window *old_window = window;
    SDL_Renderer *old_renderer = renderer;
    SDL_DisplayID display = SDL_GetDisplayForWindow(old_window);
    SDL_Window *made = make_window(plan, display != 0 ? display : SDL_GetPrimaryDisplay());
    if (made == NULL) {
        log_error("Could not create SDL Window\n%s", SDL_GetError());
        return false;
    }
    window = made;
    renderer = NULL;

    // Not a retry: Task 1.7b's schedule ends here, and its test hooks (which fail and trim retries by
    // their attempt number) leave this renderer alone. A failure puts the schedule back as it was.
    bool was_owed = renderer_owed;
    int was_attempt = renderer_attempt;
    renderer_owed = false;
    renderer_attempt = 0;
    if (!create_renderer(plan)) {
        char why[256];
        SDL_strlcpy(why, SDL_GetError(), sizeof(why));
        SDL_DestroyWindow(made);
        window = old_window;
        renderer = old_renderer;
        renderer_owed = was_owed;
        renderer_attempt = was_attempt;
        SDL_SetError("%s", why);   // The caller's reason: destroying the window may have set another error
        return false;
    }

    // Every texture belongs to the old renderer and goes before it; the reload makes them again
    int dropped = drop_textures();
    log_debug("Renderer: dropped %i textures before the renderer went", dropped);
    if (old_renderer != NULL)
        SDL_DestroyRenderer(old_renderer);
    window_id = SDL_GetWindowID(window);
    window_kind = plan->kind;
#ifdef _WIN32
    platform_window_changed(old_window, window);   // The hotkeys leave the old HWND for the new, which is marked if Transparent
#endif

    // To the front while the old window still holds the foreground, which this process may then give
    SDL_RaiseWindow(window);
#ifdef _WIN32
    state.has_focus = take_foreground();
    if (state.has_focus)
        log_debug("Video: the new window is in front, with the keyboard");
    else
        log_error("Video: Windows kept the new window behind another, so it has no keyboard until it is brought to the front");
#endif
    SDL_DestroyWindow(old_window);
    reload_mark(RELOAD_RECREATE);
    return true;
}

// A function to give up on a transparent window: say why, show the Color background, and see that an
// opaque fullscreen window with a renderer is in force. A recreation that failed, or an override
// refused before it began, leaves the opaque window it would have replaced, still in front with its
// renderer and textures. A Transparent window whose renderer is gone (every device-loss attempt
// failed) is recreated opaque. At startup, with no window yet, the opaque one is made. Transparent is
// not tried again until another Mode is chosen and then Transparent again. Only an opaque window that
// gets no renderer ends StreamFlex.
static void fall_back_to_opaque(const char *reason)
{
    log_error("Video: no renderer can draw a transparent window (%s); using an opaque background", reason);
    transparent_failed = true;
    background_shown = BACKGROUND_COLOR;
    if (window != NULL && window_kind == WINDOW_FULLSCREEN && renderer != NULL) {
        set_draw_color();
        return;
    }
    WindowPlan plan = plan_for(BACKGROUND_COLOR);
    if (window != NULL) {
        if (!recreate_window(&plan))
            quit(EXIT_FAILURE);   // recreate_window() or create_renderer() logged SDL's reason
        return;
    }
    window = make_window(&plan, SDL_GetPrimaryDisplay());
    if (window == NULL)
        log_fatal("Could not create SDL Window\n%s", SDL_GetError());
    window_id = SDL_GetWindowID(window);
    window_kind = plan.kind;
    window_made();
    if (!create_renderer(&plan))
        quit(EXIT_FAILURE);   // create_renderer() logged SDL's reason, as log_fatal would
}

// A function to give the window the kind the Mode setting needs. Closing settings, Discard and the
// Mode row's rest ask for it (Task 1.9a); the recreation itself runs in the main loop, between the
// drain and the reload, once no app is in front and a renderer exists. An override outside the
// transparent list is refused here, by its name, before any renderer is made. On Linux every
// background is fullscreen, so this asks for nothing, unless the harness forces the Windows plan
// (STREAMFLEX_TEST_WINDOWS_PLAN).
void window_follow_background(void)
{
    if (config.background_mode != BACKGROUND_TRANSPARENT)
        transparent_failed = false;   // Chosen again, Transparent is tried again
    WindowPlan plan = plan_for((int) config.background_mode);
    if (plan.refused) {
        if (!transparent_failed)
            fall_back_to_opaque(plan.reason);
        return;
    }
    recreate_wanted = window_must_change(window_kind, plan.kind, transparent_failed);
}

// A function to make the recreation window_follow_background() asked for, from the main loop after the
// drain and before the reload it marks. One asked for while an app is in front waits for post_launch().
static void run_recreation(void)
{
    recreate_wanted = false;
    WindowPlan plan = plan_for(transparent_failed ? BACKGROUND_COLOR : (int) config.background_mode);
    if (plan.refused || plan.kind == window_kind)
        return;   // Asked for, then changed back before it ran
    if (plan.kind == WINDOW_TRANSPARENT_SHORT)
        log_debug("Video: recreating the window for a transparent background");
    else
        log_debug("Video: recreating the window for an opaque background");
    if (recreate_window(&plan))
        return;
    if (plan.kind != WINDOW_TRANSPARENT_SHORT)
        quit(EXIT_FAILURE);   // recreate_window() or create_renderer() logged SDL's reason
    char why[512];
    transparent_reason(&plan, why, sizeof(why));
    fall_back_to_opaque(why);
}

// A function to fit a Transparent window to the display it is on again, one row short, after a
// display-mode change or a move to another display: SDL re-fits only a fullscreen window. No
// recreation: the SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED that follows runs the reload, and waits for an
// app in front as any size change does.
static void refit_transparent_window(void)
{
    if (window_kind != WINDOW_TRANSPARENT_SHORT)
        return;
    SDL_Rect bounds;
    SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    if (display == 0 || !SDL_GetDisplayBounds(display, &bounds)) {
        log_error("Video: could not fit the transparent window to its display\n%s", SDL_GetError());
        return;
    }
    WindowRect rect = window_short_rect((WindowRect) { bounds.x, bounds.y, bounds.w, bounds.h });
    SDL_SetWindowPosition(window, rect.x, rect.y);
    SDL_SetWindowSize(window, rect.w, rect.h);
}

// A function to set the color of the renderer
void set_draw_color()
{
    if (renderer == NULL)
        return;   // A new renderer sets its draw color as it is made
#ifdef _WIN32
    // Per-pixel alpha: the background clears to nothing, and SDL's blending over it gives the
    // premultiplied pixels the compositor is told to expect. ChromaKeyColor is not read.
    if (background_shown == BACKGROUND_TRANSPARENT) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        return;
    }
#endif
    SDL_Color *color = NULL;
    if (background_shown == BACKGROUND_COLOR)
        color = &config.background_color;
    else if (background_shown == BACKGROUND_TRANSPARENT)
        color = &config.chroma_key_color;

    if (color == NULL)
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    else
        SDL_SetRenderDrawColor(renderer,
            color->r,
            color->g,
            color->b,
            color->a
        );
}

// A function to derive the values the launcher draws with from config, after the parse and after
// every change the settings screen makes
void refresh_effective()
{
    DeriveInput in = derive_input();
    derive_settings(&in, &eff);
    geo.vcenter = eff.vcenter;
    title_color = (SDL_Color) { eff.title_color.r, eff.title_color.g, eff.title_color.b, eff.title_color.a };
    title_shadow_color = (SDL_Color) { eff.title_shadow_color.r, eff.title_shadow_color.g,
                                       eff.title_shadow_color.b, eff.title_shadow_color.a };
}

// A function to initialize SDL's TTF subsystem and open the title font
static void init_sdl_ttf()
{
    if (!TTF_Init())
        log_fatal("Could not initialize SDL_ttf\n%s", SDL_GetError());

    title_info = (TextInfo) {
        .font_size = (int) config.title_font_size,
        .shadow = config.title_shadows,
        .font_path = NULL,
        .max_width = 0, // Set per menu in render_buttons: its button size, less room for a shadow
        .min_size = geo.title_min_size,
        .oversize_mode = config.title_oversize_mode,
        .color = &title_color,
        .shadow_color = config.title_shadows ? &title_shadow_color : NULL
    };
    if (load_font(&title_info, config.title_font_path, config.title_font_face, FILENAME_DEFAULT_FONT, "titles"))
        log_fatal("Could not load title font");
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: STREAMFLEX_TEST_RELOAD_FONTS opens the font again, as a
    // reload will, so the leak pass shows whether the one it replaces is closed
    if (getenv("STREAMFLEX_TEST_RELOAD_FONTS") != NULL) {
        log_debug("Test hook: the title font opens again");
        if (load_font(&title_info, config.title_font_path, config.title_font_face, FILENAME_DEFAULT_FONT, "titles"))
            log_fatal("Could not load title font");
    }
#endif
    log_debug("Title font: %s (face %i)", title_info.font_path, title_info.font_face);
    fixed_title_font = title_info.set;
    geo.font_height = config.titles_enabled ? TTF_GetFontHeight(title_info.font) : 0;

    // A percentage FontSize sizes each menu's titles from its buttons, so measure the font's line
    // height per point once, at a large size, for the layout to reserve room for any size
    TTF_Font *probe = open_font_face(title_info.font_path, TITLE_MEASURE_SIZE, title_info.font_face);
    geo.title_line_pm = probe != NULL ? TTF_GetFontHeight(probe) * 1000 / TITLE_MEASURE_SIZE : 1500;
    if (probe != NULL)
        TTF_CloseFont(probe);
}

// A function to close subsystems and free memory before quitting
static void cleanup()
{
    settings_close_now();
    pickers_quit();   // The font list kept for the session; a font scan's thread is waited for here, before SDL quits

    // Stop every feature while the renderer and SDL still run; the clock's stop also waits for
    // its thread
    stop_clock();
    stop_screensaver();
    stop_scroll();
    stop_highlight();
    stop_overlay();
    stop_gamepad();
    if (background_override != NULL)
        SDL_DestroyTexture(background_override);

    // Wait until all threads have completed; the slideshow's may have read an image, which is freed
    stop_slideshow();

#ifdef _WIN32
    taskbar_release();   // Before SDL_Quit(), while COM that SDL started still stands
#endif

    // Destroy renderer and window
    if (renderer != NULL) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    if (window != NULL) {
        SDL_DestroyWindow(window);
        window = NULL;
    }

    // Quit subsystems
    SDL_Quit();

    // Close every title set while SDL_ttf is still open (stop_clock() closed the clock's font), then
    // what the font sets keep for the session: their probes, the candidate list and fontconfig's sort
    // (fontconfig itself is left to process exit)
    title_fonts_free();
    fontset_close(fixed_title_font);
    fixed_title_font = NULL;
    title_info.set = NULL;
    title_info.font = NULL;
    free(title_info.font_path);
    title_info.font_path = NULL;
    fontset_quit();
    TTF_Quit();
    quit_svg();
    quit_image_formats();   // Every thread that decodes has ended: the slideshow's above, the preview's with settings

    // Free dynamically allocated memory
    free(config.default_menu);
    free(config.background_image);
    free(config.title_font_path);
    free(config.config_path);
    free(config.slideshow_directory);
    free(config.clock_font_path);
    free(config.gamepad_mappings_file);
    free(config.startup_cmd);
    free(config.quit_cmd);
    library_free();

    // Free menu and entry linked lists
    Entry *entry = NULL;
    Entry *tmp_entry = NULL;
    Menu *menu = config.first_menu;
    Menu *tmp_menu = NULL;
    for (size_t i = 0; i < config.num_menus; i++) {
        free(menu->name);
        free(menu->items);
        entry = menu->first_entry;
        for(size_t j = 0; j < menu->num_entries; j++) {
            free(entry->title);
            free(entry->icon_path);
            free(entry->icon_selected_path);
            free(entry->cmd);
            tmp_entry = entry;
            entry = entry->next;
            free(tmp_entry);
        }
        tmp_menu = menu;
        menu = menu->next;
        free(tmp_menu);
    }

    // Free the hotkey and gamepad control linked lists
    clear_hotkeys();
    clear_gamepad_controls();
}

// A function to check whether the config binds a hotkey to a key
static bool hotkey_bound(SDL_Keycode keycode)
{
    for (Hotkey *i = hotkeys; i != NULL; i = i->next) {
        if (i->keycode == keycode)
            return true;
    }
    return false;
}

// A function to tell whether a key is the built-in Menu key, which opens settings unless a hotkey
// has it: a keyboard's context-menu key (SDLK_APPLICATION) or a remote's Menu button (SDLK_MENU)
static bool menu_key(SDL_Keycode keycode)
{
    return (keycode == SDLK_APPLICATION || keycode == SDLK_MENU) && !hotkey_bound(keycode);
}

// A function to handle key presses from keyboard; `key` is the stored code (keycodes.h), SDL2's
// numbering as config.ini holds it, and `repeat` marks the keyboard's own repeats of a held key
static void handle_keypress(SDL_Keycode key, bool repeat)
{
    if (config.debug)
        log_debug("Key %s (#%X) detected", SDL_GetKeyName((SDL_Keycode) keycode_from_stored(key)), key);

    // A capture takes every key, and the 10 s the key that confirms them, before any other meaning
    if (settings_is_open() && settings_raw_key((int) key, repeat))
        return;

    // A held Menu key opens (or closes) settings once: its repeats would strobe them
    if (repeat && menu_key(key))
        return;

    // While settings are open, the built-in keys are their commands; other keys run their hotkey,
    // which execute_command() hands to settings (and settings ignore unless it is one of theirs)
    if (settings_is_open()) {
        const char *command = NULL;
        switch (key) {
            case SDLK_LEFT:
                command = SCMD_LEFT;
                break;
            case SDLK_RIGHT:
                command = SCMD_RIGHT;
                break;
            case SDLK_UP:
                command = SCMD_UP;
                break;
            case SDLK_DOWN:
                command = SCMD_DOWN;
                break;
            case SDLK_RETURN:
                command = SCMD_SELECT;
                break;
            case SDLK_BACKSPACE:
                command = SCMD_BACK;
                break;
            default:
                if (menu_key(key))
                    command = SCMD_SETTINGS;
                break;
        }
        if (command != NULL) {
            settings_handle_command(command);
            return;
        }
    }

    // Check default keys. Up and Down give way to a hotkey bound to the same key, so an
    // existing config's binding keeps working after the upgrade.
    if (key == SDLK_LEFT)
        move_selection(LAYOUT_LEFT);
    else if (key == SDLK_RIGHT)
        move_selection(LAYOUT_RIGHT);
    else if ((key == SDLK_UP || key == SDLK_DOWN) && !hotkey_bound(key))
        move_selection(key == SDLK_UP ? LAYOUT_UP : LAYOUT_DOWN);
    else if (key == SDLK_RETURN) {
        log_debug("Selected Entry:\n"
            "Title: %s\n"
            "Icon Path: %s\n"
            "Command: %s", 
            current_entry->title, 
            current_entry->icon_path, 
            current_entry->cmd
        );
        
        execute_command(current_entry->cmd);
    }
    else if (key == SDLK_BACKSPACE)
        load_back_menu(current_menu);

    // The Menu key opens settings, unless a hotkey has it
    else if (menu_key(key))
        execute_command(SCMD_SETTINGS);

    //Check hotkeys
    else {
        for (Hotkey *i = hotkeys; i != NULL; i = i->next) {
            if (key == i->keycode) {
                // A hotkey for :settings acts once however long it is held, as the Menu key does
                if (!repeat || strcmp(i->cmd, SCMD_SETTINGS) != 0)
                    execute_command(i->cmd);
                break;
            }
        }
    }
}

// A function to free the slideshow, if there is one
void quit_slideshow()
{
    if (slideshow == NULL)
        return;
    for (int i = 0; i < slideshow->num_images; i++)
        free(slideshow->images[i]);
    free(slideshow->images);
    free(slideshow->order);
    free(slideshow);
    slideshow = NULL;
}

// A function to log the mean luminance of the image on show, which the contrast warning reads
static void log_luminance(void)
{
    if (background_luminance >= 0.0)
        log_debug("Background: the image on show has a mean luminance of %.3f", background_luminance);
    else
        log_debug("Background: the image on show could not be measured");
}

// A function to stop a slideshow that can no longer show two images, on the main thread: show the
// one image that still loads (surface, the same image as the one on show), or the color when none
// does. The Mode setting stays Slideshow, so the folder is tried again when the background is next
// set up.
static void fall_back_from_slideshow(SDL_Surface *surface)
{
    if (surface != NULL) {
        log_error("Could only load one image from slideshow directory %s, showing it as a single image",
            config.slideshow_directory
        );
        SDL_DestroySurface(surface);
        background_shown = BACKGROUND_IMAGE;
    }
    else {
        log_error("Could not load any image from slideshow directory %s, showing the background color",
            config.slideshow_directory
        );
        if (background_texture != NULL) {
            SDL_DestroyTexture(background_texture);
            background_texture = NULL;
        }
        background_shown = BACKGROUND_COLOR;
    }
    if (slideshow->transition_texture != NULL)
        SDL_DestroyTexture(slideshow->transition_texture);
    quit_slideshow();
    set_draw_color();
}

// A function to scan the slideshow folder. What is shown falls back to the color, or to a single
// image, when the folder is missing or holds fewer than two images; the settings are left alone.
static void init_slideshow()
{
    // While settings are open, a mode with no folder chosen yet is not a config problem
    if (config.slideshow_directory == NULL && settings_is_open()) {
        log_debug("Settings: no slideshow folder chosen yet, the preview shows the color");
        background_shown = BACKGROUND_COLOR;
        return;
    }
    if (config.slideshow_directory == NULL || !directory_exists(config.slideshow_directory)) {
        log_error("Slideshow directory '%s' does not exist, "
            "Switching to color background mode",
            config.slideshow_directory != NULL ? config.slideshow_directory : "(none)"
        );
        background_shown = BACKGROUND_COLOR;
        return;
    }
    slideshow = malloc(sizeof(Slideshow));
    *slideshow = (Slideshow) {
        .i = -1,
        .num_images = 0,
        .transition_surface = NULL,
        .transition_texture = NULL,
        .transition_start = 0,
        .images = NULL,
        .order = NULL,
        .only_one = false,
        .transition_luminance = -1.0
    };
    scan_slideshow_directory(slideshow, config.slideshow_directory);
    if (!slideshow->num_images) {
        log_error("No images found in slideshow directory '%s', "
            "Changing background mode to color",
            config.slideshow_directory
        );
        background_shown = BACKGROUND_COLOR;
        quit_slideshow();
    }
    else if (slideshow->num_images == 1) {
        log_error("Only one image found in slideshow directory %s, showing it as a single image",
            config.slideshow_directory
        );
        background_texture = load_texture_measured(slideshow->images[0], &background_luminance);
        background_shown = background_texture != NULL ? BACKGROUND_IMAGE : BACKGROUND_COLOR;
        quit_slideshow();
    }
    else {
        slideshow->order = malloc(sizeof(int) * (size_t) slideshow->num_images);
        random_array(slideshow->order, slideshow->num_images);
        if (config.debug)
            debug_slideshow(slideshow);
    }
}

// A function to make the screensaver's dimming layer's pixels: a screen-sized black surface; NULL when
// it cannot be made (SDL says why)
static SDL_Surface *screensaver_surface(void)
{
    SDL_Surface *surface = SDL_CreateSurface(geo.screen_width, geo.screen_height, SDL_PIXELFORMAT_ARGB8888);
    if (surface != NULL)
        SDL_FillSurfaceRect(surface, NULL, SDL_MapSurfaceRGBA(surface, 0, 0, 0, 0xFF));
    return surface;
}

// A function to start the screensaver, when it is on and not already running
static void start_screensaver()
{
    if (!config.screensaver_enabled || screensaver != NULL)
        return;
    if (eff.screensaver_alpha < 1) {
        log_error("Invalid screensaver intensity value, so the screensaver was not started");
        return;
    }
    SDL_Surface *surface = screensaver_surface();
    if (surface == NULL) {
        log_error("Could not start the screensaver\n%s", SDL_GetError());
        return;
    }
    screensaver = malloc(sizeof(Screensaver));
    screensaver->alpha_end_value = (float) eff.screensaver_alpha;
    screensaver->transition_start = 0;   // Set as the dim begins (update_screensaver())
    screensaver->texture = load_texture(surface);
    screensaver->alpha = 0.0f;
    SDL_SetTextureAlphaMod(screensaver->texture, 0);
    log_debug("Screensaver started");
}

// A function to stop the screensaver, lifting its dim and resuming a slideshow it paused
static void stop_screensaver()
{
    if (screensaver == NULL)
        return;
    if (state.screensaver_active && background_shown == BACKGROUND_SLIDESHOW) {
        state.slideshow_paused = false;
        ticks.slideshow_load = ticks.main;
    }
    state.screensaver_active = false;
    state.screensaver_transition = false;
    if (screensaver->texture != NULL)
        SDL_DestroyTexture(screensaver->texture);
    free(screensaver);
    screensaver = NULL;
    log_debug("Screensaver stopped");
}

// A function to restart the screensaver after one of its settings changed
void reload_screensaver()
{
    stop_screensaver();
    start_screensaver();
}

// A function to make the screensaver's layer again for a reload of the whole launcher (a new screen
// size, a reset or new renderer), keeping whether it dims, how far, and its transition (spec section 3:
// the screen's state is kept): reload_screensaver() would stop it, lifting the dim
void remake_screensaver_texture(void)
{
    if (screensaver == NULL)
        return;
    if (screensaver->texture != NULL)
        SDL_DestroyTexture(screensaver->texture);
    SDL_Surface *surface = screensaver_surface();
    if (surface == NULL)
        log_error("Screensaver: its layer could not be made again\n%s", SDL_GetError());
    screensaver->texture = load_texture(surface);   // NULL for no surface
    // The dim as update_screensaver() last set it: the full dim once its transition is done, else
    // where the transition is, which is none while the screensaver is not dimming
    float dim = state.screensaver_active && !state.screensaver_transition ? screensaver->alpha_end_value : screensaver->alpha;
    if (screensaver->texture != NULL)
        SDL_SetTextureAlphaMod(screensaver->texture, (Uint8) dim);
    log_debug("Screensaver: its layer was made again at %ix%i px, %s", geo.screen_width, geo.screen_height,
              state.screensaver_active ? "still dimming" : "not dimming");
}

// A function to stop the overlay, freeing its texture
static void stop_overlay()
{
    if (background_overlay == NULL)
        return;
    SDL_DestroyTexture(background_overlay);
    background_overlay = NULL;
    log_debug("Overlay stopped");
}

// A function to start the overlay, when it is on: a screen-sized texture of its color and opacity
static void start_overlay()
{
    if (!config.background_overlay || background_overlay != NULL)
        return;
    SDL_Surface *surface = SDL_CreateSurface(geo.screen_width, geo.screen_height, SDL_PIXELFORMAT_ARGB8888);
    if (surface == NULL) {
        log_error("Could not start the overlay\n%s", SDL_GetError());
        return;
    }
    SDL_FillSurfaceRect(surface, NULL, SDL_MapSurfaceRGBA(surface,
        eff.overlay_color.r, eff.overlay_color.g, eff.overlay_color.b, eff.overlay_color.a));
    background_overlay = load_texture(surface);
    log_debug("Overlay started");
}

// A function to start the highlight, when it is on; its texture is rendered for each button size
// as menus load
static void start_highlight()
{
    if (!config.highlight || highlight != NULL)
        return;
    highlight = malloc(sizeof(Highlight));
    *highlight = (Highlight) { .texture = NULL, .button = 0, .hpad = 0, .vpad = 0, .title_block = 0 };
    log_debug("Highlight started");
}

// A function to stop the highlight, freeing its texture
static void stop_highlight()
{
    if (highlight == NULL)
        return;
    if (highlight->texture != NULL)
        SDL_DestroyTexture(highlight->texture);
    free(highlight);
    highlight = NULL;
    log_debug("Highlight stopped");
}

// A function to render the highlight again after one of its settings changed
void reload_highlight()
{
    stop_highlight();
    start_highlight();
    if (current_menu != NULL)
        apply_layout(current_menu);
}

// A function to start the scroll indicators, when they are on. An arrow that cannot be drawn
// leaves them stopped, with the setting as it was.
static void start_scroll()
{
    if (!config.scroll_indicators || scroll != NULL)
        return;
    scroll = malloc(sizeof(Scroll));
    scroll->texture = NULL;
    int scroll_indicator_height = (int) ((float) geo.screen_height * SCROLL_INDICATOR_HEIGHT);
    if (render_scroll_indicators(scroll, scroll_indicator_height, &geo)) {
        log_error("Could not render scroll indicator, so the scroll indicators were not started");
        free(scroll);
        scroll = NULL;
        return;
    }
    log_debug("Scroll indicators started");
}

// A function to stop the scroll indicators, freeing their texture
static void stop_scroll()
{
    if (scroll == NULL)
        return;
    if (scroll->texture != NULL)
        SDL_DestroyTexture(scroll->texture);
    free(scroll);
    scroll = NULL;
    log_debug("Scroll indicators stopped");
}

// A function to render the scroll indicators again after one of their settings changed
void reload_scroll()
{
    stop_scroll();
    start_scroll();
}

// A function to start the clock, when it is on: open its font and render the time now
static void start_clock()
{
    if (!config.clock_enabled || clk != NULL)
        return;
    clk = calloc(1, sizeof(Clock));
    SDL_SetAtomicInt(&state.clock_rendering, 0);
    SDL_SetAtomicInt(&state.clock_ready, 0);
    if (init_clock(clk)) {
        free(clk->text_info.font_path);
        free(clk);
        clk = NULL;
        log_error("The clock cannot start: no font opens");
        return;
    }
    ticks.clock_update = ticks.main;
    log_debug("Clock started");
}

// A function to stop the clock: wait for a render in flight on its thread, then free what it made
void stop_clock()
{
    if (clk == NULL)
        return;
    bool waited = clock_thread != NULL;
    if (clock_thread != NULL) {
        SDL_WaitThread(clock_thread, NULL);
        clock_thread = NULL;
    }
    SDL_SetAtomicInt(&state.clock_rendering, 0);
    SDL_SetAtomicInt(&state.clock_ready, 0);
    if (clk->time_surface != NULL)
        SDL_DestroySurface(clk->time_surface);
    if (clk->date_surface != NULL)
        SDL_DestroySurface(clk->date_surface);
    if (clk->time_texture != NULL)
        SDL_DestroyTexture(clk->time_texture);
    if (clk->date_texture != NULL)
        SDL_DestroyTexture(clk->date_texture);
    if (clk->text_info.font != NULL)
        TTF_CloseFont(clk->text_info.font);
    free(clk->text_info.font_path);
    free(clk);
    clk = NULL;
    // Two calls, not a ?: inside one: log_debug appends its newline to the literal before it, which
    // would be the second branch only
    if (waited)
        log_debug("Clock stopped (it waited for a render in progress)");
    else
        log_debug("Clock stopped");
}

// A function to restart the clock after one of its settings changed, rendering it at once, then lay
// the menu out again: the clock's size moves the buttons
void reload_clock()
{
    stop_clock();
    start_clock();
    calculate_layout_area();
    if (current_menu != NULL)
        apply_layout(current_menu);
}

// A function to resume the slideshow after a launched application returns
static void resume_slideshow()
{
    ticks.slideshow_load = ticks.main;
}

// A function to stop the slideshow, when the background changes and at quit: wait for an image
// being loaded on its thread, then free it all
static void stop_slideshow()
{
    if (Slideshowhread != NULL) {
        SDL_WaitThread(Slideshowhread, NULL);
        Slideshowhread = NULL;
    }
    if (slideshow != NULL) {
        // The next image may be read but not yet on its way in (only its surface), or fading in
        if (slideshow->transition_surface != NULL || slideshow->transition_texture != NULL)
            log_debug("Slideshow: dropped the fade in progress");
        if (slideshow->transition_surface != NULL)
            SDL_DestroySurface(slideshow->transition_surface);
        if (slideshow->transition_texture != NULL)
            SDL_DestroyTexture(slideshow->transition_texture);
        quit_slideshow();
    }
}

// A function to set the background up for config.background_mode: at startup, and whenever the
// settings screen changes it. What is shown (background_shown) falls back to the color when an
// image or slideshow cannot be used; the setting itself stays as it was chosen.
void reload_background()
{
    stop_slideshow();
    state.slideshow_transition = false;
    SDL_SetAtomicInt(&state.slideshow_background_rendering, 0);
    SDL_SetAtomicInt(&state.slideshow_background_ready, 0);
    if (background_texture != NULL) {
        SDL_DestroyTexture(background_texture);
        background_texture = NULL;
    }
    background_luminance = -1.0;

    background_shown = config.background_mode;
    // No renderer could draw a transparent window since Transparent was chosen: the color shows
    // instead (fall_back_to_opaque(); never set on Linux)
    if (background_shown == BACKGROUND_TRANSPARENT && transparent_failed)
        background_shown = BACKGROUND_COLOR;
    if (config.background_mode == BACKGROUND_IMAGE) {
        // While settings are open, a mode with no image chosen yet is not a config problem; an
        // image that was chosen and fails to load is one, and says so as it does at startup
        if (config.background_image == NULL && settings_is_open())
            log_debug("Settings: no image chosen yet, the preview shows the color");
        else if (config.background_image == NULL)
            log_error("Background 'Image' setting not specified in config file");
        else
            background_texture = load_texture_measured(config.background_image, &background_luminance);
        if (background_texture == NULL) {
            if (config.background_image != NULL || !settings_is_open())
                log_error("Couldn't load background image, defaulting to color background");
            background_shown = BACKGROUND_COLOR;
        }
    }
    else if (config.background_mode == BACKGROUND_SLIDESHOW) {
        init_slideshow();
        if (background_shown == BACKGROUND_SLIDESHOW) {
            Uint64 start = SDL_GetPerformanceCounter(), read = 0, decode = 0;
            SDL_Surface *surface = load_next_slideshow_background(slideshow, false, &read, &decode);
            if (surface != NULL) {
                Uint64 decoded = SDL_GetPerformanceCounter();
                background_luminance = surface_luminance(surface);
                Uint64 lit = SDL_GetPerformanceCounter();
                background_texture = load_texture(surface);
                Uint64 end = SDL_GetPerformanceCounter();
                log_debug("Timing: background: Slideshow image in %u ms (read %u, decode %u, luminance %u, texture %u)",
                    timing_ms(end - start), timing_ms(read), timing_ms(decode), timing_ms(lit - decoded), timing_ms(end - lit));
                ticks.slideshow_load = ticks.main;
            }
            else
                fall_back_from_slideshow(NULL);
        }
    }
    if (background_shown == BACKGROUND_IMAGE || background_shown == BACKGROUND_SLIDESHOW)
        log_luminance();
    stop_overlay();
    start_overlay();
    set_draw_color();
    log_debug("Background set up: %s", get_mode_setting(MODE_SETTING_BACKGROUND, (int) background_shown));
}

// A function to load a menu
static int load_menu(Menu *menu, bool set_back_menu, bool reset_position)
{
    if (menu == NULL)
        return 1;
    log_debug("Loading menu '%s'", menu->name);

    // Return error if the menu doesn't contain entries
    if (menu->num_entries == 0) {
        log_error("No valid entries found for Menu '%s'", menu->name);
        return 1;
    }

    Menu *previous_menu = current_menu;
    current_menu = menu;
    if (reset_position)
        current_menu->position = (LayoutPosition) { 0, 0 };
    if (apply_layout(current_menu)) {
        current_menu = previous_menu;
        return 1;
    }
    // A menu opened from itself keeps its back link, or Back would return to the same menu
    if (set_back_menu && menu != previous_menu)
        current_menu->back = previous_menu;
    return 0;
}

// A function to load a menu by its name
static int load_menu_by_name(const char *menu_name, bool set_back_menu, bool reset_position)
{
    Menu *menu = get_menu(menu_name);
    return load_menu(menu, set_back_menu, reset_position);
}

// A function to work out the screen area the buttons may use: the full width, and the height
// between the top and bottom margins, starting below the clock when it is shown
static void calculate_layout_area()
{
    int top = geo.screen_margin;
    if (clk != NULL) {
        SDL_Rect *lowest = clk->show_date ? &clk->date_rect : &clk->time_rect;
        if (lowest->y + lowest->h > top)
            top = lowest->y + lowest->h;
    }
    layout_area = (LayoutArea) {
        .x = 0,
        .y = top,
        .w = geo.screen_width,
        .h = geo.screen_height - geo.screen_margin - top,
        .vcenter = geo.vcenter
    };
    log_debug("Layout area: from y %i, %i px tall, centered at %i px", layout_area.y, layout_area.h, layout_area.vcenter);
}

// A function to work out a menu's grid on this screen without drawing anything, so the debug
// log can show every menu's layout, not only the ones that have been opened
int compute_menu_layout(const Menu *menu, LayoutGeometry *geometry, char *why, size_t why_size)
{
    LayoutOverrides global = { (int) config.rows, (int) config.max_buttons, (int) config.icon_size };
    LayoutOverrides builtin = { DEFAULT_ROWS, DEFAULT_MAX_BUTTONS, 0 };
    LayoutOverrides effective = layout_resolve(menu->overrides, global, builtin);
    bool titles = config.titles_enabled;
    bool scaled = titles && config.title_font_size_pct > 0 && !menu->fixed_titles;
    LayoutParams params = {
        .rows              = effective.rows,
        .columns           = effective.columns,
        .icon_cap          = effective.icon_cap,
        .spacing           = eff.icon_spacing,
        .title_block       = titles && !scaled ? geo.font_height : 0,
        .hpad              = eff.highlight_hpadding,
        .vpad              = eff.highlight_vpadding,
        .title_padding     = eff.title_padding,
        .title_padding_pct = eff.title_padding_pct,
        .title_size_pct    = scaled ? config.title_font_size_pct : 0,
        .title_min_size    = geo.title_min_size,
        .title_line_pm     = geo.title_line_pm
    };
    params.title_shadow = titles && config.title_shadows;
    return layout_compute(&params, &layout_area, (int) menu->num_entries, geometry, why, why_size);
}

// A function to describe a menu's titles for the log: "36 pt titles", or "no titles"
void describe_titles(const LayoutGeometry *geometry, char *out, size_t size)
{
    if (!config.titles_enabled)
        snprintf(out, size, "no titles");
    else
        snprintf(out, size, "%i pt titles", geometry->title_size > 0 ? geometry->title_size : (int) config.title_font_size);
}

// A function to lay out the current menu: size its buttons for its grid, re-render its
// textures if that size changed, and place the visible entries
static int apply_layout(Menu *menu)
{
    char why[256];
    if (compute_menu_layout(menu, &layout, why, sizeof(why))) {
        log_error("Menu '%s' cannot be shown: %s", menu->name, why);
        return 1;
    }

    // A title size whose font cannot be opened gives way to the fixed FontSize, and the menu is
    // laid out again for that font's height, so its titles fit the room kept for them
    if (layout.title_size > 0 && title_font(layout.title_size) == NULL) {
        log_error("Menu '%s': its titles use the fixed %u pt font instead", menu->name, config.title_font_size);
        menu->fixed_titles = true;
        menu->rendered_size = 0;
        if (compute_menu_layout(menu, &layout, why, sizeof(why))) {
            log_error("Menu '%s' cannot be shown: %s", menu->name, why);
            return 1;
        }
    }

    // A reduced grid is reported when the menu is first laid out at this size, not on every load
    if (why[0] != '\0' && menu->rendered_size != layout.button)
        log_error("Menu '%s': %s", menu->name, why);
    char titles[32];
    describe_titles(&layout, titles, sizeof(titles));
    log_debug("Menu '%s': %i x %i grid, %i px buttons, %s", menu->name, layout.columns, layout.rows, layout.button, titles);

    if (menu->rendered_size != layout.button) {
        render_buttons(menu, &layout);
        menu->rendered_size = layout.button;
    }
    if (highlight != NULL && (highlight->button != layout.button || highlight->hpad != layout.hpad ||
    highlight->vpad != layout.vpad || highlight->title_block != layout.title_block)) {
        if (highlight->texture != NULL)
            SDL_DestroyTexture(highlight->texture);
        int button_height = layout.button + layout.title_block;
        highlight->texture = render_highlight(layout.button + 2*layout.hpad,
                                 button_height + 2*layout.vpad,
                                 &highlight->rect
                             );
        highlight->button = layout.button;
        highlight->hpad = layout.hpad;
        highlight->vpad = layout.vpad;
        highlight->title_block = layout.title_block;
    }
    menu->position = layout_clamp(&layout, (int) menu->num_entries, menu->position);
    place_entries();
    return 0;
}

// A function to render all buttons (icon and title) of a menu for its layout: the icons at the
// button size, and the titles in the menu's own title size
static void render_buttons(Menu *menu, const LayoutGeometry *geometry)
{
    Uint64 timing_start = SDL_GetPerformanceCounter(), titles_time = 0;
    int icons = 0, titles = 0;
    timing_read = timing_decode = timing_texture = 0;
    int size = geometry->button;
    title_info.set = fixed_title_font;
    title_info.font = fontset_font(fixed_title_font);
    title_info.font_size = (int) config.title_font_size;
    if (geometry->title_size > 0) {
        FontSet *set = title_font(geometry->title_size);
        if (set != NULL) {
            title_info.set = set;
            title_info.font = fontset_font(set);
            title_info.font_size = geometry->title_size;
        }
    }
    int line_height = TTF_GetFontHeight(title_info.font);
    title_info.max_width = layout_title_width(config.title_shadows, size, line_height);   // Room for the shadow
    titles_time += SDL_GetPerformanceCounter() - timing_start;   // Opening the title font counts as titles

    // The icons are read and decoded on the decode threads first, two jobs an entry (the icon and its
    // selected icon); the textures are made below, on this thread, in the entries' order
    int job_count = (int) menu->num_entries * 2;
    IconJob *jobs = calloc((size_t) (job_count > 0 ? job_count : 1), sizeof(IconJob));
    if (jobs == NULL)
        log_fatal("Could not allocate the icon jobs for menu '%s'", menu->name);
    for (int i = 0; i < (int) menu->num_entries; i++) {
        const char *paths[2] = { menu->items[i]->icon_path, menu->items[i]->icon_selected_path };
        for (int k = 0; k < 2; k++) {
            IconJob *job = &jobs[i * 2 + k];
            job->path = paths[k];
            job->size = size;
            if (paths[k] == NULL)
                job->done = true;   // No icon: nothing to do
            else if (fileio_not_a_file(paths[k])) {
                SDL_snprintf(job->error, sizeof(job->error), "%s", fileio_last_error());
                job->done = true;
            }
            else {
                size_t length = strlen(paths[k]);
                job->svg = length > 4 && SDL_strcasecmp(paths[k] + length - 4, ".svg") == 0;
            }
        }
    }
    // The jobs with work left go to the pool as one packed array (iconload_batch() takes a
    // contiguous one), each remembering where it came from, and come back to their entries after
    IconJob *work = calloc((size_t) (job_count > 0 ? job_count : 1), sizeof(IconJob));
    int *origin = calloc((size_t) (job_count > 0 ? job_count : 1), sizeof(int));
    if (work == NULL || origin == NULL)
        log_fatal("Could not allocate the icon jobs for menu '%s'", menu->name);
    int work_count = 0;
    for (int j = 0; j < job_count; j++) {
        if (!jobs[j].done) {
            work[work_count] = jobs[j];
            origin[work_count++] = j;
        }
    }
    iconload_batch(work, work_count, iconload_workers(), decode_icon_job, icon_worker_start, icon_worker_end);
    for (int j = 0; j < work_count; j++)
        jobs[origin[j]] = work[j];
    free(work);
    free(origin);

    for (int i = 0; i < (int) menu->num_entries; i++) {
        Entry *entry = menu->items[i];
        if (entry->icon != NULL)
            SDL_DestroyTexture(entry->icon);
        if (entry->icon_selected != NULL)
            SDL_DestroyTexture(entry->icon_selected);
        for (int k = 0; k < 2; k++) {
            IconJob *job = &jobs[i * 2 + k];
            SDL_Texture **slot = k == 0 ? &entry->icon : &entry->icon_selected;
            *slot = NULL;
            if (job->path == NULL)
                continue;
            timing_read += job->read;   // A failed icon's read and decode count too (0 when the guard stopped it)
            timing_decode += job->decode;
            if (job->surface == NULL) {
                log_error("Could not load image %s\n%s", job->path, job->error);
                continue;
            }
#ifndef _WIN32
            // What the chroma pass would have logged on this thread, here
            if (job->kept_off == ICON_NOT_CONVERTED)
                log_error("Could not convert %s to keep it off the chroma key\n%s", job->path, job->error);
            else if (job->kept_off == ICON_NOT_LOCKED)
                log_debug("%s: could not be kept off the chroma key, so it is drawn as it is: %s", job->path, job->error);
            else if (job->moved > 0)
                log_debug("%s: moved %i pixel(s) off the chroma key #%02X%02X%02X", job->path, job->moved,
                    config.chroma_key_color.r, config.chroma_key_color.g, config.chroma_key_color.b);
#endif
            Uint64 made = SDL_GetPerformanceCounter();
            *slot = load_texture(job->surface);   // Frees the surface
            job->surface = NULL;
            timing_texture += SDL_GetPerformanceCounter() - made;
        }
        icons += (entry->icon_path != NULL) + (entry->icon_selected_path != NULL);
        if (config.titles_enabled) {
            Uint64 title_start = SDL_GetPerformanceCounter();
            int h;
            if (entry->title_texture != NULL)
                SDL_DestroyTexture(entry->title_texture);
            entry->title_texture = render_text_texture(entry->title, &title_info, &entry->text_rect, &h);
            entry->title_offset = (config.title_oversize_mode == OVERSIZE_SHRINK && h != line_height)
                                  ? (line_height - h) / 2 : 0;
            if (config.title_oversize_mode != OVERSIZE_NONE && entry->text_rect.w > size)
                log_debug("Menu '%s': the title '%s' is %i px wide, over its %i px button",
                    menu->name, entry->title, entry->text_rect.w, size);
            titles_time += SDL_GetPerformanceCounter() - title_start;
            titles++;
        }
    }

    free(jobs);

    // Keep no pointer to a cached set: another menu's title size may close it
    title_info.set = fixed_title_font;
    title_info.font = fontset_font(fixed_title_font);
    title_info.font_size = (int) config.title_font_size;
    log_debug("Menu '%s': rendered its buttons at %i px", menu->name, size);
    Uint64 icons_time = SDL_GetPerformanceCounter() - timing_start - titles_time;
    log_debug("Timing: menu '%s': %i icons at %i px in %u ms (read %u, decode %u, texture %u), %i titles in %u ms",
        menu->name, icons, size, timing_ms(icons_time), timing_ms(timing_read), timing_ms(timing_decode),
        timing_ms(timing_texture), titles, timing_ms(titles_time));
}

// A function to position the visible buttons and the highlight for the current menu
static void place_entries()
{
    for (unsigned int i = 0; i < current_menu->num_entries; i++) {
        Entry *entry = current_menu->items[i];
        int x, y;
        if (!layout_slot(&layout, current_menu->position, (int) i, &x, &y))
            continue;
        entry->icon_rect = (SDL_Rect) { x, y, layout.button, layout.button };
        entry->text_rect.x = x + (layout.button - entry->text_rect.w) / 2;
        entry->text_rect.y = y + layout.button + entry->title_offset + layout.title_padding;
    }
    current_entry = current_menu->items[current_menu->position.selected];
    if (highlight != NULL) {
        highlight->rect.x = current_entry->icon_rect.x - layout.hpad;
        highlight->rect.y = current_entry->icon_rect.y - layout.vpad;
    }
}

// A function to move the highlight, scrolling the strip or grid when needed
static void move_selection(LayoutDirection direction)
{
    LayoutPosition position = layout_move(&layout, (int) current_menu->num_entries,
                                  current_menu->position, direction, config.wrap_entries);
    if (position.selected == current_menu->position.selected && position.first == current_menu->position.first)
        return;
    current_menu->position = position;
    place_entries();
}

// A function to load a submenu
static void load_submenu(const char *submenu)
{
    load_menu_by_name(submenu, true, true);
}

// A function to load the previous menu
static void load_back_menu(Menu *menu)
{
    load_menu(menu->back, false, config.reset_on_back);
}

// A function to render every menu's titles again after the title size changed: the menu on show
// now, the others when they are next opened
void reload_titles()
{
    title_info.shadow = config.title_shadows;
    title_info.shadow_color = config.title_shadows ? &title_shadow_color : NULL;
    title_info.oversize_mode = config.title_oversize_mode;
    geo.font_height = config.titles_enabled ? TTF_GetFontHeight(fontset_font(fixed_title_font)) : 0;

    // A new size may open where the last one failed
    for (Menu *menu = config.first_menu; menu != NULL; menu = menu->next) {
        menu->rendered_size = 0;
        menu->fixed_titles = false;
    }
    apply_layout(current_menu);
}

// A function to open the title font again after its file or face changed: close the size cache and
// the fixed font, open the font, measure its height per point again, then render every menu's titles
void reload_title_font()
{
    title_fonts_free();
    fontset_close(fixed_title_font);
    fixed_title_font = NULL;
    title_info.set = NULL;
    title_info.font = NULL;
    title_info.font_size = (int) config.title_font_size;
    if (load_font(&title_info, config.title_font_path, config.title_font_face, FILENAME_DEFAULT_FONT, "titles"))
        return;   // log_fatal has quit: not even the bundled font opens
    fixed_title_font = title_info.set;
    TTF_Font *probe = open_font_face(title_info.font_path, TITLE_MEASURE_SIZE, title_info.font_face);
    geo.title_line_pm = probe != NULL ? TTF_GetFontHeight(probe) * 1000 / TITLE_MEASURE_SIZE : 1500;
    if (probe != NULL)
        TTF_CloseFont(probe);
    log_debug("Titles: opened %s (face %i)", title_info.font_path, title_info.font_face);
    reload_titles();
}

// A function to close the title fonts no menu uses any more, once settings have changed the sizes
void trim_title_fonts()
{
    int *sizes = calloc(config.num_menus > 0 ? config.num_menus : 1, sizeof(int));
    if (sizes == NULL)
        return;
    int count = 0;
    for (Menu *menu = config.first_menu; menu != NULL; menu = menu->next) {
        LayoutGeometry geometry;
        char why[256];
        if (compute_menu_layout(menu, &geometry, why, sizeof(why)) == 0 && geometry.title_size > 0)
            sizes[count++] = geometry.title_size;
    }
    title_fonts_keep(sizes, count);
    free(sizes);
}

// A function to lay the menu on show out again after its grid, or the area it goes in, changed: the
// area is worked out again first, so a new vertical center or a clock that moved reaches it
void refresh_layout()
{
    calculate_layout_area();
    apply_layout(current_menu);
}

// A function to show a menu without changing its back link or remembered position
int show_menu(Menu *menu)
{
    return load_menu(menu, false, false);
}

// A function to go to the default menu, as :home does; non-zero when it cannot be shown
int show_home()
{
    return load_menu(default_menu, false, true);
}

// A function to fill the screen with a gray checkerboard. In the settings preview it stands for a
// transparent background: a texture cannot show the desktop through.
static void draw_checkerboard()
{
    int square = geo.screen_height / 18 > 8 ? geo.screen_height / 18 : 8;
    for (int y = 0; y < geo.screen_height; y += square) {
        for (int x = 0; x < geo.screen_width; x += square) {
            Uint8 shade = ((x / square) + (y / square)) % 2 == 0 ? 0x55 : 0x88;
            SDL_Rect cell = { x, y, square, square };
            SDL_SetRenderDrawColor(renderer, shade, shade, shade, 0xFF);
            fill_rect(renderer, &cell);
        }
    }
}

// A function to draw the low-battery glyph for the lowest pad on its battery, from filled
// rectangles (no texture: nothing to rebuild after a device loss): in the top corner opposite the
// clock's alignment, at the clock's margin whether or not the clock is on, amber when low and red
// when critical, nudged off a chroma key on Linux. The log says when it shows, changes level or
// place, or hides.
static void draw_pad_battery()
{
    PadPower powers[MAX_BATTERY_PADS];
    int count = 0;
    for (Gamepad *pad = gamepads; pad != NULL && count < MAX_BATTERY_PADS; pad = pad->next) {
        if (pad->controller != NULL)
            powers[count++] = pad->power;
    }
    int lowest = padpower_pick(powers, count);
    PadPowerLevel level = lowest >= 0 ? powers[lowest].level : PADPOWER_HIDDEN;
    if (level == PADPOWER_HIDDEN) {
        if (battery_shown != PADPOWER_HIDDEN)
            log_debug("Battery indicator: hidden");
        battery_shown = PADPOWER_HIDDEN;
        return;
    }
    int h = geo.screen_height / BATTERY_GLYPH_DIVISOR > BATTERY_GLYPH_MIN ? geo.screen_height / BATTERY_GLYPH_DIVISOR : BATTERY_GLYPH_MIN;
    int w = 2 * h;
    SDL_Rect rect = {
        config.clock_alignment == ALIGNMENT_RIGHT ? eff.clock_margin : geo.screen_width - eff.clock_margin - w,
        eff.clock_margin, w, h
    };
    if (level != battery_shown || rect.x != battery_rect.x || rect.y != battery_rect.y || rect.h != battery_rect.h)
        log_debug("Battery indicator: shown, %s, %i%% at %i,%i %ix%i", level == PADPOWER_CRITICAL ? "critical" : "low",
            powers[lowest].percent, rect.x, rect.y, rect.w, rect.h);
    battery_shown = level;
    battery_rect = rect;

    unsigned char rgb[3];
#ifdef _WIN32
    padpower_color(level, NULL, rgb);   // Windows has no chroma key
#else
    const unsigned char key[3] = { config.chroma_key_color.r, config.chroma_key_color.g, config.chroma_key_color.b };
    padpower_color(level, key, rgb);
#endif
    int t = h / 9 > 1 ? h / 9 : 1;
    int nub = h / 6 > 2 ? h / 6 : 2;
    int body = w - nub;
    int inside = body - 4 * t;
    int charge = level == PADPOWER_LOW ? inside / 2 : inside / 4;
    SDL_Rect parts[6] = {
        { rect.x, rect.y, body, t },                         // The edges
        { rect.x, rect.y + h - t, body, t },
        { rect.x, rect.y, t, h },
        { rect.x + body - t, rect.y, t, h },
        { rect.x + body, rect.y + h / 4, nub, h - 2 * (h / 4) },   // The terminal
        { rect.x + 2 * t, rect.y + 2 * t, charge, h - 4 * t }      // The charge left
    };
    SDL_FRect drawn[6];
    for (int i = 0; i < 6; i++)
        drawn[i] = (SDL_FRect) { (float) parts[i].x, (float) parts[i].y, (float) parts[i].w, (float) parts[i].h };
    Uint8 r, g, b, a;
    SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
    SDL_SetRenderDrawColor(renderer, rgb[0], rgb[1], rgb[2], 0xFF);
    SDL_RenderFillRects(renderer, drawn, 6);
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

// A function to draw the launcher's scene: the background, its overlay, the scroll indicators,
// the clock, the highlight and the visible buttons. The settings screen draws it into its preview
// (preview true), where a transparent background shows as a checkerboard and an image being
// browsed replaces the background.
void draw_scene(bool preview)
{
    set_draw_color();
    SDL_RenderClear(renderer);
    if (preview && background_override != NULL)
        draw_texture(renderer, background_override, NULL, NULL);
    else if (preview && background_shown == BACKGROUND_TRANSPARENT)
        draw_checkerboard();
    else {
        if (background_shown == BACKGROUND_IMAGE || background_shown == BACKGROUND_SLIDESHOW)
            draw_texture(renderer, background_texture, NULL, NULL);
        if (background_shown == BACKGROUND_SLIDESHOW && state.slideshow_transition)
            draw_texture(renderer, slideshow->transition_texture, NULL, NULL);
    }

    // Draw background overlay
    if (background_overlay != NULL)
        draw_texture(renderer, background_overlay, NULL, NULL);

    // Draw scroll indicators: a strip's point left and right from the bottom corners,
    // a grid's are the same arrow turned to point up and down from the top and bottom margins
    if (scroll != NULL) {
        int count = (int) current_menu->num_entries;
        LayoutPosition position = current_menu->position;
        if (layout.rows == 1) {
            if (layout_can_scroll(&layout, count, position, LAYOUT_RIGHT))
                draw_texture(renderer, scroll->texture, NULL, &scroll->rect_right);
            if (layout_can_scroll(&layout, count, position, LAYOUT_LEFT))
                draw_texture_rotated(renderer, scroll->texture, &scroll->rect_left, 0, SDL_FLIP_HORIZONTAL);
        }
        else {
            if (layout_can_scroll(&layout, count, position, LAYOUT_UP))
                draw_texture_rotated(renderer, scroll->texture, &scroll->rect_up, 270.0, SDL_FLIP_NONE);
            if (layout_can_scroll(&layout, count, position, LAYOUT_DOWN))
                draw_texture_rotated(renderer, scroll->texture, &scroll->rect_down, 90.0, SDL_FLIP_NONE);
        }
    }

    // Draw clock
    if (clk != NULL) {
        draw_texture(renderer, clk->time_texture, NULL, &clk->time_rect);
        if (clk->show_date)
            draw_texture(renderer, clk->date_texture, NULL, &clk->date_rect);
    }

    // The low-battery glyph, in the top corner the clock leaves; never in the settings preview
    if (!preview)
        draw_pad_battery();

    // Draw highlight
    if (highlight != NULL)
        draw_texture(renderer,
            highlight->texture,
            NULL,
            &highlight->rect
        );

    // Draw the visible buttons
    for (unsigned int i = 0; i < current_menu->num_entries; i++) {
        int x, y;
        if (!layout_slot(&layout, current_menu->position, (int) i, &x, &y))
            continue;
        Entry *entry = current_menu->items[i];
        SDL_Texture *icon = (entry->icon_selected != NULL && (int) i == current_menu->position.selected)
                            ? entry->icon_selected : entry->icon;
        draw_texture(renderer, icon, NULL, &entry->icon_rect);
        if (config.titles_enabled)
            draw_texture(renderer, entry->title_texture, NULL, &entry->text_rect);
    }
}

#ifdef STREAMFLEX_TEST_HOOKS
// A function only the headless harness builds: with STREAMFLEX_TEST_FRAME_REPORT_MS set, it counts
// the frames shown, and those present_frame() paced, and logs both once that long has passed
static void test_frame_report(bool paced)
{
    static Uint32 start = 0;
    static Uint32 frames = 0;
    static Uint32 paced_frames = 0;
    static bool reported = false;
    const char *span = getenv("STREAMFLEX_TEST_FRAME_REPORT_MS");
    if (span == NULL || reported)
        return;
    Uint32 now = ticks_ms();
    if (frames == 0)
        start = now;
    frames++;
    if (paced)
        paced_frames++;
    if (now - start >= (Uint32) atoi(span)) {
        reported = true;
        log_debug("Test hook: %u frames in %u ms, %u of them paced", frames, now - start, paced_frames);
    }
}

static bool test_key_undrawn = false;   // A key was handled since the last frame was shown
static Uint64 test_present_start_ns = 0; // When the last present started, for STREAMFLEX_TEST_FRAME_PERIOD_US

// A function only the headless harness builds: it logs the first frame shown after a key was
// handled, once that frame is on screen, so the harness can wait for each key's frame before it
// sends the next key
static void test_frame_after_key()
{
    if (!test_key_undrawn)
        return;
    test_key_undrawn = false;
    log_debug("Test hook: a frame was drawn after a key");
}
#endif

// A function to show the frame, and without VSync wait out the rest of its time
void present_frame()
{
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: each frame held up this long as it is shown, as on a slow machine
    const char *frame_sleep = getenv("STREAMFLEX_TEST_FRAME_SLEEP_MS");
    if (frame_sleep != NULL)
        SDL_Delay((Uint32) atoi(frame_sleep));

    // Only the headless harness builds this too: each present held until this long after the last one
    // started, so presents start that far apart however long the frame took to draw, as long as the draw
    // and the last present took less. Timed from the start, the present's own time cancels out: the gap
    // between two presents' returns is the period plus the change in the present's time from one to the
    // next, where timed from the return it was the period plus the whole present (3 ms under a loaded
    // full set). Check 42 sets how far apart its full-size run's presents come with it, so it stays
    // before the present. The first present, with none before it, is not held.
    const char *frame_period = getenv("STREAMFLEX_TEST_FRAME_PERIOD_US");
    if (frame_period != NULL && test_present_start_ns != 0) {
        Uint64 due = test_present_start_ns + (Uint64) atoi(frame_period) * 1000;
        Uint64 now = SDL_GetTicksNS();
        if (now < due)
            SDL_DelayPrecise(due - now);
    }
    test_present_start_ns = SDL_GetTicksNS();
#endif
    static int present_failures = 0;   // Presents in a row that failed on a Transparent window
    // SDL_RenderPresent() returns true whatever its renderer did (SDL_render.c, 3.2 to 3.4), so a
    // Transparent window's draws are flushed first: a Vulkan swapchain that cannot be rebuilt fails the
    // flush (VULKAN_RunCommandQueue) in every frame. On Vulkan from SDL 3.4.18 the flush also submits the
    // frame and waits for the GPU (VULKAN_InvalidateCachedState), every frame on a Transparent window.
    // Every other window presents as before.
    bool transparent = window_kind == WINDOW_TRANSPARENT_SHORT;
    bool presented = !transparent || SDL_FlushRenderer(renderer);
    presented = SDL_RenderPresent(renderer) && presented;

    // Three in a row on a Transparent window are handled as SDL_EVENT_RENDER_DEVICE_LOST, which SDL
    // never sends for them: the next drain makes a new renderer from the Transparent list
    if (present_failed_lost(&present_failures, presented, transparent)) {
        log_error("Renderer: %i presents in a row failed on the transparent window\n%s", PRESENT_FAILURES_LOST, SDL_GetError());
        SDL_Event lost;
        SDL_zero(lost);
        lost.type = SDL_EVENT_RENDER_DEVICE_LOST;
        lost.render.windowID = window_id;
        if (!SDL_PushEvent(&lost))
            log_error("Renderer: could not treat the failing presents as a lost device\n%s", SDL_GetError());
    }

    // Paced unless the renderer gives the VSync wanted. A renderer that keeps VSync on when it is not
    // wanted still gets the delay, which holds the FPS limit: the present waits at most one refresh more.
    // One that says it gives VSync is timed at each present, and paced too once its presents are found
    // to come faster than the display's (vsyncwatch.h): SDL3 takes its word, where SDL2 simulated VSync
    bool paced = !vsync_wanted || !vsync_on || vsync_watch.undelivered;
    if (paced) {
        Uint32 elapsed = ticks_ms() - ticks.main;
        if (elapsed < refresh_period)
            SDL_Delay(refresh_period - elapsed);
    }
    else if (vsyncwatch_present(&vsync_watch, SDL_GetTicksNS(), display_rate)) {
        log_error("Frame timing: the renderer claims VSync but does not deliver it, so each frame is paced to %u ms",
                  refresh_period);
#ifdef STREAMFLEX_TEST_HOOKS
        // Only the headless harness builds this: how far apart the run of early presents that found it came
        log_debug("Test hook: the VSync watch's run of %i early presents came %llu to %llu us apart, %llu us on average",
                  vsync_watch.early_run, (unsigned long long) (vsync_watch.run_min_ns / 1000),
                  (unsigned long long) (vsync_watch.run_max_ns / 1000),
                  (unsigned long long) ((vsync_watch.last_ns - vsync_watch.run_first_ns) / (uint64_t) vsync_watch.early_run / 1000));
#endif
    }
#ifdef STREAMFLEX_TEST_HOOKS
    test_frame_report(paced);
    test_frame_after_key();
#endif
}

// A function to update the screen: the scene and the screensaver's dimming, or a blank screen
// while an application is launching
static void draw_screen()
{
    if (!(state.application_launching && config.on_launch == ON_LAUNCH_BLANK)) {
        draw_scene(false);
        if (state.screensaver_active && screensaver != NULL)
            draw_texture(renderer, screensaver->texture, NULL, NULL);
    }
    else {
        SDL_RenderClear(renderer);
        fill_rect(renderer, NULL);
    }
    present_frame();
}

// A function to execute the user's command
static void execute_command(const char *command)
{
    // While settings are open the remote's keys belong to them, and everything else waits
    if (settings_is_open()) {
        settings_handle_command(command);
        return;
    }

    // Copy command into separate buffer
    char *cmd = strdup(command);

    // Parse special commands
    if (cmd[0] == ':') {
        char *delimiter = " ";
        char *rest = NULL;
        char *special_command = strtok_r(cmd, delimiter, &rest);
        if (!strcmp(special_command, SCMD_SUBMENU)) {
            char *submenu = strtok_r(NULL, "", &rest);
            if (submenu != NULL)
                load_submenu(submenu);
        }
        else if (!strcmp(special_command, SCMD_FORK)) {
            char *fork_command = strtok_r(NULL, "", &rest);
            if (fork_command != NULL)
                start_process(fork_command, false);
        }
        else if (!strcmp(special_command, SCMD_LEFT))
            move_selection(LAYOUT_LEFT);
        else if (!strcmp(special_command, SCMD_RIGHT))
            move_selection(LAYOUT_RIGHT);
        else if (!strcmp(special_command, SCMD_UP))
            move_selection(LAYOUT_UP);
        else if (!strcmp(special_command, SCMD_DOWN))
            move_selection(LAYOUT_DOWN);
        else if (!strcmp(special_command, SCMD_SELECT))
            execute_command(current_entry->cmd);
        else if (!strcmp(special_command, SCMD_HOME))
            load_menu(default_menu, false, true);
        else if (!strcmp(special_command, SCMD_BACK))
            load_back_menu(current_menu);
        else if (!strcmp(special_command, SCMD_QUIT))
            quit(EXIT_SUCCESS);
        else if (!strcmp(special_command, SCMD_SHUTDOWN))
            scmd_shutdown();
        else if (!strcmp(special_command, SCMD_RESTART))
            scmd_restart();
        else if (!strcmp(special_command, SCMD_SLEEP))
            scmd_sleep();
        else if (!strcmp(special_command, SCMD_EXIT))
            log_error("':exit' works only as a hotkey on Windows, where it closes the app on show; ignoring it");
        else if (!strcmp(special_command, SCMD_SETTINGS)) {
            // Settings never open over an application being launched, which is about to take the
            // screen: no application runs behind them
            if (state.application_launching || state.application_running)
                log_debug("Settings: not opened while an application is launching or running");
            else
                settings_open();
        }
    }

    // Launch external application
    else {
        SDL_Delay(50);
        if (start_process(cmd, true)) {
            state.application_launching = true;
            ticks.application_launched = ticks.main;
            if (config.on_launch == ON_LAUNCH_BLANK)
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
            else if (config.on_launch == ON_LAUNCH_QUIT)
                quit(EXIT_SUCCESS);
        }
    }
    free(cmd);
}

// SDL3 names joysticks by instance id alone. A device index, as the Device setting and the log name
// one, is a joystick's place in the list SDL_GetJoysticks() gives now, as SDL2's device index was.
// A function to get the joystick at a device index now; 0, which no joystick has, when there is none
static SDL_JoystickID joystick_at(int device_index)
{
    int count = 0;
    SDL_JoystickID *ids = SDL_GetJoysticks(&count);
    SDL_JoystickID id = ids != NULL && device_index >= 0 && device_index < count ? ids[device_index] : 0;
    SDL_free(ids);
    return id;
}

// A function to find a joystick's device index now, from its instance id; -1 when it is gone
static int joystick_index(SDL_JoystickID id)
{
    int count = 0;
    SDL_JoystickID *ids = SDL_GetJoysticks(&count);
    int index = -1;
    for (int i = 0; ids != NULL && i < count && index < 0; i++) {
        if (ids[i] == id)
            index = i;
    }
    SDL_free(ids);
    return index;
}

// A function to count the joysticks present
static int joystick_count(void)
{
    int count = 0;
    SDL_JoystickID *ids = SDL_GetJoysticks(&count);
    if (ids == NULL)
        return 0;
    SDL_free(ids);
    return count;
}

// A function to initialize the gamepad struct. Only the instance id is kept: a device index moves
// whenever a pad before it goes, so it is looked up again each time the pad is opened.
static void init_gamepad(Gamepad **gamepad, int device_index)
{
    *gamepad = malloc(sizeof(Gamepad));
    **gamepad = (Gamepad) {
        .id = (int) joystick_at(device_index),
        .controller = NULL,
        .power = padpower_unknown(),
        .next = NULL,
        .previous = NULL,
    };
}

// A function to find a pad's device index now, from its instance id; -1 when it is gone
static int gamepad_device_index(const Gamepad *gamepad)
{
    return joystick_index((SDL_JoystickID) gamepad->id);
}

// A function to open the SDL controller, at the pad's device index as it is now
static void open_controller(Gamepad *gamepad, bool raise_error)
{
    int device_index = gamepad_device_index(gamepad);
    if (device_index < 0) {
        log_debug("Gamepad with instance id %i is no longer present, so it is not opened", gamepad->id);
        return;
    }
    gamepad->controller = SDL_OpenGamepad((SDL_JoystickID) gamepad->id);
    if (gamepad->controller == NULL) {
        if (raise_error)
            log_error("Could not open gamepad at device index %i", device_index);
        return;
    }
    log_debug("Gamepad opened at device index %i, instance id %i", device_index,
        (int) SDL_GetJoystickID(SDL_GetGamepadJoystick(gamepad->controller)));
    int percent = -1;
    SDL_PowerState power_state = SDL_GetGamepadPowerInfo(gamepad->controller, &percent);
    gamepad->power = (PadPower) {
        .state = (PadPowerState) power_state,
        .percent = percent,
        .connection = (PadConnection) SDL_GetGamepadConnectionState(gamepad->controller),
        .level = padpower_level((PadPowerState) power_state, percent, PADPOWER_HIDDEN)
    };
    log_pad_power("Gamepad power", gamepad);
    if (config.debug && raise_error) {
        char *mapping = SDL_GetGamepadMapping(gamepad->controller);
        log_debug("Gamepad Mapping:\n%s", mapping);
        SDL_free(mapping);
    }
}

// A function to log a pad's power as StreamFlex holds it: WHAT is "Gamepad power" as it opens and
// "Gamepad power changed" when a battery event changes it
static void log_pad_power(const char *what, const Gamepad *gamepad)
{
    char percent[16];
    if (gamepad->power.percent < 0)
        snprintf(percent, sizeof(percent), "no percent");
    else
        snprintf(percent, sizeof(percent), "%i%%", gamepad->power.percent);
    log_debug("%s: instance id %i, %s, %s, %s", what, (int) gamepad->id,
        padpower_connection_name(gamepad->power.connection), padpower_state_name(gamepad->power.state), percent);
}

// A function to take a battery event for the pad its instance id names, with the state and percent
// the event itself carries, never asked of SDL again (the harness's pushed event would prove nothing
// otherwise, since its virtual pad reads unknown). A report that changes nothing is not logged.
static void gamepad_power_changed(SDL_JoystickID id, SDL_PowerState power_state, int percent)
{
    for (Gamepad *pad = gamepads; pad != NULL; pad = pad->next) {
        if ((SDL_JoystickID) pad->id != id)
            continue;
        if (pad->power.state == (PadPowerState) power_state && pad->power.percent == percent)
            return;
        pad->power.state = (PadPowerState) power_state;
        pad->power.percent = percent;
        pad->power.level = padpower_level(pad->power.state, percent, pad->power.level);
        log_pad_power("Gamepad power changed", pad);
        return;
    }
}

// A function to connect gamepad(s): the pad at a device index, added to the list unless its
// instance id is there already, or with -1 every pad in the list. A pad already open is left as it
// is: SDL counts each open, and pre_launch()'s one close would not let it go.
static void connect_gamepad(int device_index, bool open, bool raise_error)
{
    if (device_index >= 0) {
        Gamepad *gamepad = NULL;
        SDL_JoystickID id = joystick_at(device_index);
        for (gamepad = gamepads; gamepad != NULL; gamepad = gamepad->next) {
            if (gamepad->id == (int) id)
                break;
        }
        bool added = gamepad == NULL;
        if (added) {
            init_gamepad(&gamepad, device_index);
            if (gamepads == NULL)
                gamepads = gamepad;

            // Add to end of the linked list
            else {
                Gamepad *i;
                for (i = gamepads; i->next != NULL; i = i->next);
                i->next = gamepad;
                gamepad->previous = i;
            }
        }
        bool opening = open && gamepad->controller == NULL;
        if (added || opening)
            log_debug("Gamepad connected with device index %i, instance id %i", device_index, gamepad->id);
        if (opening)
            open_controller(gamepad, raise_error);
    }
    else if (open) {
        for (Gamepad *i = gamepads; i != NULL; i = i->next) {
            if (i->controller == NULL)
                open_controller(i, raise_error);
        }
    }
}

// A function to disconnect gamepad(s)
static void disconnect_gamepad(int id, bool disconnect, bool remove)
{
    for (Gamepad *i = gamepads; i != NULL;) {
        if (id < 0 || i->id == id) {
            if (disconnect)
                SDL_CloseGamepad(i->controller);
            if (remove) {
                if (i->next != NULL)
                    i->next->previous = i->previous;
                if (i->previous != NULL)
                    i->previous->next = i->next;
                if (i == gamepads)
                    gamepads = i->next;
                Gamepad *tmp = i->next;
                free(i);
                i = tmp;
            }
            else {
                i->controller = NULL;
                i->power = padpower_unknown();
                i = i->next;
            }
        }
        else
            i = i->next;
    }
}

#ifdef STREAMFLEX_TEST_HOOKS
static SDL_Joystick *test_pad = NULL;   // The harness's virtual gamepad, while attached
static SDL_JoystickID test_pad_id = 0;  // Its instance id; 0, which no joystick has, while none is attached
static SDL_JoystickID test_swap[3] = { 0, 0, 0 };   // STREAMFLEX_TEST_PAD_SWAP's pads A, B and C
static SDL_JoystickID test_plug = 0;    // STREAMFLEX_TEST_PAD_PLUG's pad, while it is plugged in
static int test_swap_step = 0;
static bool test_pad_file = false;      // STREAMFLEX_TEST_PAD's file was there last frame
static int test_pad_frames = 0;         // Frames STREAMFLEX_TEST_PAD_FRAMES's press has left

// A function only the headless harness builds: attach a virtual gamepad with every button and axis
// SDL3 has, which SDL maps to them in their order (its masks left 0, SDL_virtualjoystick.c:201-222 at
// 3.2.10), so joystick button N is gamepad button N, as on SDL2. Its instance id; 0 when it failed.
static SDL_JoystickID test_pad_attach_virtual(void)
{
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = (Uint16) SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = (Uint16) SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = (Uint16) SDL_GAMEPAD_BUTTON_COUNT;
    return SDL_AttachVirtualJoystick(&desc);
}

// A function only the headless harness builds: with STREAMFLEX_TEST_PAD set, it attaches the
// virtual gamepad once the gamepad runs
static void test_pad_attach()
{
    if (getenv("STREAMFLEX_TEST_PAD") == NULL || !gamepad_on || test_pad_id != 0)
        return;
    test_pad_id = test_pad_attach_virtual();
    test_pad = test_pad_id != 0 ? SDL_OpenJoystick(test_pad_id) : NULL;
    if (test_pad == NULL)
        log_error("Test hook: no virtual gamepad\n%s", SDL_GetError());
}

// A function only the headless harness builds: with STREAMFLEX_TEST_PAD_SWAP set, it takes one step
// a frame through attach A, attach B, detach A, attach C, so C arrives at B's old device index with
// an instance id of its own; then a launch and a return (pre_launch(), then post_launch()), which
// close every pad and open each again, now that B's device index has moved
static void test_pad_swap()
{
    static const char *const names[] = { "A", "B", "C" };
    if (getenv("STREAMFLEX_TEST_PAD_SWAP") == NULL || !gamepad_on || test_swap_step > 5)
        return;
    int step = test_swap_step++;
    if (step == 4) {
        log_debug("Test hook: launch");
        pre_launch();
        return;
    }
    if (step == 5) {
        log_debug("Test hook: return");
        post_launch();
        return;
    }
    if (step == 2) {
        int index = joystick_index(test_swap[0]);
        if (index >= 0)
            SDL_DetachVirtualJoystick(test_swap[0]);
        test_swap[0] = 0;
        log_debug("Test hook: pad A detached from device index %i", index);
        return;
    }
    int slot = step == 3 ? 2 : step;
    SDL_JoystickID id = test_pad_attach_virtual();
    if (id == 0) {
        log_error("Test hook: no virtual gamepad %s\n%s", names[slot], SDL_GetError());
        return;
    }
    test_swap[slot] = id;
    log_debug("Test hook: pad %s attached at device index %i, instance id %i", names[slot], joystick_index(id), (int) id);
}

// A function only the headless harness builds: with STREAMFLEX_TEST_PAD_PLUG set, a virtual pad is
// plugged in while the gamepad runs and the file it names exists, and pulled out when the file goes,
// as a pad is plugged in or pulled out by hand at any moment (while settings are open, say)
static void test_pad_plug()
{
    const char *plug = getenv("STREAMFLEX_TEST_PAD_PLUG");
    if (plug == NULL || !gamepad_on)
        return;
    bool wanted = file_exists(plug);
    if (wanted && test_plug == 0) {
        SDL_JoystickID id = test_pad_attach_virtual();
        if (id == 0) {
            log_error("Test hook: no virtual gamepad to plug in\n%s", SDL_GetError());
            return;
        }
        test_plug = id;
        log_debug("Test hook: pad plugged in at device index %i", joystick_index(id));
    }
    else if (!wanted && test_plug != 0) {
        if (joystick_index(test_plug) >= 0)
            SDL_DetachVirtualJoystick(test_plug);
        test_plug = 0;
        log_debug("Test hook: pad unplugged");
    }
}

// A function only the headless harness builds: with STREAMFLEX_TEST_PAD_POWER set, it reads the file
// that names once a frame and, each time its line changes, pushes the battery event a real pad sends,
// with the line's state and percent: "<state> <percent>" (battery 40, charging 80, unknown -1), the
// state one of unknown, battery, none, charging, charged or error, for the first pad StreamFlex holds,
// or with A or B in front for the first or the second. The handler reads the event as it reads SDL's.
static void test_pad_power()
{
    static char last[64] = "";
    static const struct { const char *name; SDL_PowerState state; } states[] = {
        { "unknown", SDL_POWERSTATE_UNKNOWN }, { "battery", SDL_POWERSTATE_ON_BATTERY },
        { "none", SDL_POWERSTATE_NO_BATTERY }, { "charging", SDL_POWERSTATE_CHARGING },
        { "charged", SDL_POWERSTATE_CHARGED }, { "error", SDL_POWERSTATE_ERROR }
    };
    const char *path = getenv("STREAMFLEX_TEST_PAD_POWER");
    if (path == NULL || !gamepad_on)
        return;
    char *text = fileio_read_all(path, NULL);
    char line[64];
    snprintf(line, sizeof(line), "%s", text != NULL ? text : "");
    alloc_free(text);
    line[strcspn(line, "\r\n")] = '\0';
    if (strcmp(line, last) == 0)
        return;
    snprintf(last, sizeof(last), "%s", line);
    if (line[0] == '\0')
        return;
    char letter = 'A';
    const char *rest = line;
    if ((line[0] == 'A' || line[0] == 'B') && line[1] == ' ') {
        letter = line[0];
        rest = line + 2;
    }
    const char *space = strchr(rest, ' ');
    size_t length = space != NULL ? (size_t) (space - rest) : 0;
    char *end = NULL;
    long percent = space != NULL ? strtol(space + 1, &end, 10) : 0;
    size_t s = 0;
    while (s < SDL_arraysize(states) && !(strlen(states[s].name) == length && strncmp(states[s].name, rest, length) == 0))
        s++;
    if (space == NULL || end == space + 1 || *end != '\0' || s == SDL_arraysize(states)) {
        log_error("Test hook: the power file's line '%s' is not '[A|B] <state> <percent>'", line);
        return;
    }
    Gamepad *pad = gamepads;
    if (letter == 'B' && pad != NULL)
        pad = pad->next;
    if (pad == NULL) {
        log_error("Test hook: no pad %c for the power file's line '%s'", letter, line);
        return;
    }
    SDL_Event battery;
    SDL_zero(battery);
    battery.type = SDL_EVENT_JOYSTICK_BATTERY_UPDATED;
    battery.jbattery.which = (SDL_JoystickID) pad->id;
    battery.jbattery.state = states[s].state;
    battery.jbattery.percent = (int) percent;
    log_debug("Test hook: pad %c's power is %s %i", letter, states[s].name, (int) percent);
    if (!SDL_PushEvent(&battery))
        log_error("Test hook: the battery event was not pushed\n%s", SDL_GetError());
}

// A function only the headless harness builds, since it has no gamepad: with STREAMFLEX_TEST_PAD
// set, it attaches a virtual one while the gamepad runs, and holds a button while the file that
// names exists: the one STREAMFLEX_TEST_PAD_BUTTON names (SDL's name for it, "b"), else Start. A name
// that is an axis's instead ("rightx") pushes that axis to its positive end. With
// STREAMFLEX_TEST_PAD_FRAMES=N, the file's appearing holds it for N frames however long the file
// stays, and the file must go before it presses again: a tap shorter than a script can time.
static void test_pad_update()
{
    test_pad_swap();
    test_pad_plug();
    test_pad_power();
    const char *held = getenv("STREAMFLEX_TEST_PAD");
    if (held == NULL || !gamepad_on)
        return;
    test_pad_attach();
    bool there = file_exists(held);
    bool pressed = there;
    const char *frames = getenv("STREAMFLEX_TEST_PAD_FRAMES");
    if (frames != NULL) {
        if (there && !test_pad_file)
            test_pad_frames = atoi(frames);
        pressed = test_pad_frames > 0;
        if (test_pad_frames > 0)
            test_pad_frames--;
    }
    test_pad_file = there;
    const char *name = getenv("STREAMFLEX_TEST_PAD_BUTTON");
    SDL_GamepadButton button = name != NULL ? SDL_GetGamepadButtonFromString(name) : SDL_GAMEPAD_BUTTON_START;
    SDL_GamepadAxis axis = button == SDL_GAMEPAD_BUTTON_INVALID ? SDL_GetGamepadAxisFromString(name)
                                                                : SDL_GAMEPAD_AXIS_INVALID;
    if (test_pad != NULL && button != SDL_GAMEPAD_BUTTON_INVALID)
        SDL_SetJoystickVirtualButton(test_pad, button, pressed);
    else if (test_pad != NULL && axis != SDL_GAMEPAD_AXIS_INVALID)
        SDL_SetJoystickVirtualAxis(test_pad, axis, pressed ? SDL_JOYSTICK_AXIS_MAX : 0);
}

// A function to let the virtual gamepads go before their subsystem stops
static void test_pad_stop()
{
    if (test_pad != NULL)
        SDL_CloseJoystick(test_pad);
    if (test_pad_id != 0)
        SDL_DetachVirtualJoystick(test_pad_id);
    test_pad = NULL;
    test_pad_id = 0;
    // The plugged pad goes with the subsystem; test_pad_plug() plugs it in again once it restarts
    if (test_plug != 0 && joystick_index(test_plug) >= 0)
        SDL_DetachVirtualJoystick(test_plug);
    test_plug = 0;
    for (int i = 0; i < 3; i++) {
        if (test_swap[i] != 0 && joystick_index(test_swap[i]) >= 0)
            SDL_DetachVirtualJoystick(test_swap[i]);
        test_swap[i] = 0;
    }
}

// A function only the headless harness builds: with STREAMFLEX_TEST_APP set, an app takes the screen
// while the file it names exists, as a launched one does (launching, then the window loses the
// keyboard), and gives it back when the file goes. The focus events are pushed, so the real handlers
// run: Xvfb has no window manager to move the keyboard.
static void test_app_update()
{
    static bool in_front = false;
    const char *path = getenv("STREAMFLEX_TEST_APP");
    if (path == NULL || file_exists(path) == in_front)
        return;
    in_front = !in_front;
    if (in_front) {
        log_debug("Test hook: an app takes the screen");
        state.application_launching = true;
        ticks.application_launched = ticks.main;
    }
    else
        log_debug("Test hook: the app is done");
    SDL_Event focus;
    SDL_zero(focus);
    focus.type = in_front ? SDL_EVENT_WINDOW_FOCUS_LOST : SDL_EVENT_WINDOW_FOCUS_GAINED;
    focus.window.windowID = SDL_GetWindowID(window);
    if (!SDL_PushEvent(&focus))
        log_error("Test hook: the focus event was not pushed\n%s", SDL_GetError());
}

// A function only the headless harness builds: with STREAMFLEX_TEST_PIXEL_SIZE set, the file it names
// holds the window's size in pixels as "WxH", which stands in for SDL's while the file exists: SDL's
// X11 driver may not report a resize of a fullscreen window with no window manager. True when it did.
static bool test_pixel_size(int *width, int *height)
{
    const char *path = getenv("STREAMFLEX_TEST_PIXEL_SIZE");
    if (path == NULL)
        return false;
    size_t length = 0;
    char *text = fileio_read_all(path, &length);
    int w = 0, h = 0;
    bool forced = text != NULL && sscanf(text, "%ix%i", &w, &h) == 2;
    alloc_free(text);
    if (forced) {
        *width = w;
        *height = h;
    }
    return forced;
}

// A function only the headless harness builds: when what STREAMFLEX_TEST_PIXEL_SIZE's file holds
// changes, SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED is pushed with it, so the real handler runs
static void test_pixel_size_update()
{
    static int last_width = 0, last_height = 0;
    int width = 0, height = 0;
    if (!test_pixel_size(&width, &height) || (width == last_width && height == last_height))
        return;
    last_width = width;
    last_height = height;
    log_debug("Test hook: the window's size reads %ix%i px", width, height);
    SDL_Event resized;
    SDL_zero(resized);
    resized.type = SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;
    resized.window.windowID = SDL_GetWindowID(window);
    resized.window.data1 = width;
    resized.window.data2 = height;
    if (!SDL_PushEvent(&resized))
        log_error("Test hook: the size event was not pushed\n%s", SDL_GetError());
}

// A function only the headless harness builds: with STREAMFLEX_TEST_DISPLAY_EVENT set, once the file it
// names exists, SDL_EVENT_WINDOW_DISPLAY_CHANGED is pushed once for the window's own display, as a move
// to another display would send: Xvfb has one display
static void test_display_event_update()
{
    static bool pushed = false;
    const char *path = getenv("STREAMFLEX_TEST_DISPLAY_EVENT");
    if (path == NULL || pushed || !file_exists(path))
        return;
    pushed = true;
    log_debug("Test hook: the window moved to a display");
    SDL_Event moved;
    SDL_zero(moved);
    moved.type = SDL_EVENT_WINDOW_DISPLAY_CHANGED;
    moved.window.windowID = SDL_GetWindowID(window);
    moved.window.data1 = (Sint32) SDL_GetDisplayForWindow(window);
    if (!SDL_PushEvent(&moved))
        log_error("Test hook: the display event was not pushed\n%s", SDL_GetError());
}

// A function only the headless harness builds: with STREAMFLEX_TEST_RENDER_EVENT set, each time what
// the file it names holds changes, the render events it names are pushed in one batch, in its order:
// "targets" (SDL_EVENT_RENDER_TARGETS_RESET), "reset" (SDL_EVENT_RENDER_DEVICE_RESET) and "lost"
// (SDL_EVENT_RENDER_DEVICE_LOST), so the real handlers run on events no software renderer sends. A
// size STREAMFLEX_TEST_PIXEL_SIZE's file took since test_pixel_size_update() read it this pass is
// pushed first, so a size and a reset that a check writes together always arrive in one batch.
static void test_render_event_update()
{
    static char last[64] = "";
    const char *path = getenv("STREAMFLEX_TEST_RENDER_EVENT");
    if (path == NULL)
        return;
    size_t length = 0;
    char *text = fileio_read_all(path, &length);
    char now[64] = "";
    if (text != NULL)
        copy_string(now, text, sizeof(now));
    alloc_free(text);
    if (strcmp(now, last) == 0)
        return;
    copy_string(last, now, sizeof(last));
    test_pixel_size_update();
    char *rest = NULL;
    for (char *word = strtok_r(now, " \n", &rest); word != NULL; word = strtok_r(NULL, " \n", &rest)) {
        SDL_Event render;
        SDL_zero(render);
        render.type = strcmp(word, "targets") == 0 ? SDL_EVENT_RENDER_TARGETS_RESET
                    : strcmp(word, "reset") == 0 ? SDL_EVENT_RENDER_DEVICE_RESET
                    : strcmp(word, "lost") == 0 ? SDL_EVENT_RENDER_DEVICE_LOST : 0;
        if (render.type == 0) {
            log_error("Test hook: '%s' names no render event", word);
            continue;
        }
        render.render.windowID = SDL_GetWindowID(window);
        log_debug("Test hook: pushed the render event '%s'", word);
        if (!SDL_PushEvent(&render))
            log_error("Test hook: the render event was not pushed\n%s", SDL_GetError());
    }
}

// A function only the headless harness builds: copy a renderer list without one of its names
static void test_list_without(const char *list, const char *name, char *out, size_t size)
{
    size_t used = 0;
    out[0] = '\0';
    for (const char *at = list; *at != '\0'; ) {
        size_t length = strcspn(at, ",");
        bool skipped = length == strlen(name) && strncmp(at, name, length) == 0;
        if (!skipped && used + length + 2 <= size) {
            if (used > 0)
                out[used++] = ',';
            memcpy(out + used, at, length);
            used += length;
            out[used] = '\0';
        }
        at += length;
        if (*at == ',')
            at++;
    }
}
#endif

// A function to tell whether the gamepad subsystem is running
bool gamepad_running()
{
    return gamepad_on;
}

// A function to tell whether a pad is open to read, as a pad's capture needs
bool gamepad_connected()
{
    for (Gamepad *pad = gamepads; pad != NULL; pad = pad->next) {
        if (pad->controller != NULL)
            return true;
    }
    return false;
}

// A function to open every game controller present that the Device setting allows, by listing them.
// Starting the subsystem also queues a connect event for each, which then finds the pad open already.
static void connect_present_pads()
{
    int count = joystick_count();
    for (int i = 0; i < count; i++) {
        if (SDL_IsGamepad(joystick_at(i)) && (config.gamepad_device < 0 || config.gamepad_device == i))
            connect_gamepad(i, !state.application_running, true);
    }
}

// A function to start the gamepad, when it is on: the game controller subsystem, the mappings file
// (once: SDL can add mappings but not remove them), the default controls and the pads present
static void start_gamepad()
{
    static bool mappings_loaded = false;
    if (!config.gamepad_enabled || gamepad_on)
        return;
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        log_error("Could not start the gamepad\n%s", SDL_GetError());
        return;
    }
    gamepad_on = true;
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: its virtual pad is attached here, as a pad plugged in
    // before the launcher started is present, so the listing below finds it while its connect
    // event is still queued
    test_pad_attach();
#endif
    if (!mappings_loaded && config.gamepad_mappings_file != NULL) {
        mappings_loaded = true;
        int added = SDL_AddGamepadMappingsFromFile(config.gamepad_mappings_file);
        if (added < 0)
            log_error("Could not load gamepad mappings from %s\n%s", config.gamepad_mappings_file, SDL_GetError());
        else
            log_debug("Gamepad mappings loaded from %s (%i added)", config.gamepad_mappings_file, added);
    }
    add_default_gamepad_controls();
    connect_present_pads();
    log_debug("Gamepad started");
}

// A function to stop the gamepad: close every pad and the subsystem, and forget any press in progress
static void stop_gamepad()
{
    if (!gamepad_on)
        return;
#ifdef STREAMFLEX_TEST_HOOKS
    test_pad_stop();
#endif
    disconnect_gamepad(-1, true, true);
    for (GamepadControl *i = gamepad_controls; i != NULL; i = i->next)
        i->hold.held = false;
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    gamepad_on = false;
    log_debug("Gamepad stopped");
}

// A function to restart the gamepad after On or Device changed
void reload_gamepad()
{
    stop_gamepad();
    start_gamepad();
}

// A function to find the first control held on any open pad, as a gamepad label's index (util.c's
// table's order, which bindings.c shares); -1 for none
int gamepad_pressed_label()
{
    for (int i = 0; i < gamepad_label_count(); i++) {
        const struct gamepad_info *control = gamepad_label_info(i);
        for (Gamepad *pad = gamepads; pad != NULL; pad = pad->next) {
            if (pad->controller == NULL)
                continue;
            bool held = control->type == TYPE_BUTTON
                        ? SDL_GetGamepadButton(pad->controller, (SDL_GamepadButton) control->index)
                        : (control->type == TYPE_AXIS_POS ? 1 : -1) * SDL_GetGamepadAxis(pad->controller, (SDL_GamepadAxis) control->index) > GAMEPAD_DEADZONE;
            if (held)
                return i;
        }
    }
    return -1;
}

// A function to time every control still held from now, after a gap in which the pad was not polled (an
// application launched or ran, or every pad was gone): it counts as pressed now, not pressed again, so
// its first repeat is a whole delay away. Left as it was, the gap would have made its repeat due at once.
static void rebase_held_controls()
{
    for (GamepadControl *i = gamepad_controls; i != NULL; i = i->next) {
        if (!i->hold.held)
            continue;
        i->hold.pressed_at = ticks.main;
        i->hold.repeat_due = GAMEPAD_REPEAT_DELAY;
#ifdef STREAMFLEX_TEST_HOOKS
        log_debug("Test hook: pad %s held through a gap in polling, timed from now", i->label);   // Only the headless harness builds this
#endif
    }
}

// A function to poll the connected gamepad for commands. A command can rebuild the controls under
// this loop (the settings screen's bindings), freeing the one it is on: the loop then stops, for this
// frame.
static void poll_gamepad()
{
    int value_multiplier; // Handles positive or negative axis
    bool pressed;
    unsigned int version = gamepad_controls_version();
    for (GamepadControl *i = gamepad_controls; i != NULL; i = i->next) {
        pressed = false;
        for (Gamepad *gamepad = gamepads; gamepad != NULL; gamepad = gamepad->next) {

            // Check if axis value exceeds dead zone
            if (i->type == TYPE_AXIS_POS || i->type == TYPE_AXIS_NEG) {
                value_multiplier = i->type == TYPE_AXIS_POS ? 1 : -1;
                if (value_multiplier*SDL_GetGamepadAxis(gamepad->controller, i->index) > GAMEPAD_DEADZONE) {
                    pressed = true;
                    break;
                }
            }

            // Check buttons
            else if (i->type == TYPE_BUTTON) {
                if (SDL_GetGamepadButton(gamepad->controller, i->index)) {
                    pressed = true;
                    break;
                }
            }
        }
        if (!pressed) {
            i->hold.held = false;
            continue;
        }

        // Execute the command at the press, and repeat it GAMEPAD_REPEAT_DELAY ms later, then every
        // GAMEPAD_REPEAT_INTERVAL ms (at most once a frame): wall time, so the frame rate cannot change
        // either. The hold is set before the command runs, since the command can free this control.
        if (!i->hold.held) {
            i->hold = (ControlHold) { true, ticks.main, GAMEPAD_REPEAT_DELAY };
            log_debug("Gamepad %s detected", i->label);
            ticks.last_input = ticks.main;
            execute_command(i->cmd);
        }
        else if (derive_repeat_due(ticks.main, i->hold.pressed_at, i->hold.repeat_due)) {
            ticks.last_input = ticks.main;
#ifdef STREAMFLEX_TEST_HOOKS
            log_debug("Test hook: pad %s repeated", i->label);   // Only the headless harness builds this
#endif
            i->hold.repeat_due = derive_next_repeat(ticks.main, i->hold.pressed_at, i->hold.repeat_due,
                                                    GAMEPAD_REPEAT_INTERVAL);

            // :settings acts on the first press only: repeating it would strobe settings open and shut
            if (strcmp(i->cmd, SCMD_SETTINGS) != 0)
                execute_command(i->cmd);
        }
        if (gamepad_controls_version() != version)
            return;
    }
}

// A function to update the slideshow
static void update_slideshow()
{
    // If image duration time has elapsed, load the next image and start the transition
    if (!state.slideshow_transition && (ticks.main - ticks.slideshow_load > config.slideshow_image_duration) &&
    !state.slideshow_paused) {
        
        // Render the new background image in a separate thread so we don't block the main thread.
        // It is marked as rendering before it starts: a thread that finished first would find
        // the mark still unset, and leave it set for good once the main thread set it.
        if (!SDL_GetAtomicInt(&state.slideshow_background_rendering) && !SDL_GetAtomicInt(&state.slideshow_background_ready)) {
            SDL_SetAtomicInt(&state.slideshow_background_rendering, 1);
            Slideshowhread = SDL_CreateThread(load_next_slideshow_background_async, "Slideshow Thread", (void*) slideshow);
        }

        // Convert background to texture after the rendering thread has completed
        else if (SDL_GetAtomicInt(&state.slideshow_background_ready)) {
            SDL_WaitThread(Slideshowhread, NULL);
            Slideshowhread = NULL;

            // The loader found no image that loads, or only the one on show: stop the slideshow
            if (slideshow->transition_surface == NULL || slideshow->only_one) {
                SDL_SetAtomicInt(&state.slideshow_background_ready, 0);
                fall_back_from_slideshow(slideshow->transition_surface);
                return;
            }
            if (config.slideshow_transition_time > 0) {
                slideshow->transition_texture = load_texture(slideshow->transition_surface);
                SDL_SetTextureAlphaMod(slideshow->transition_texture, 0);
                state.slideshow_transition = true;
                slideshow->transition_start = ticks.main;
                log_debug("Slideshow: fading in the next image");
#ifdef STREAMFLEX_TEST_HOOKS
                log_debug("Test hook: the slideshow's fade began at tick %u, to take %u ms",
                          slideshow->transition_start, config.slideshow_transition_time);
#endif
            }
            else {
                SDL_DestroyTexture(background_texture);
                background_texture = load_texture(slideshow->transition_surface);
                background_luminance = slideshow->transition_luminance;
                log_luminance();
                ticks.slideshow_load = ticks.main;
            }
        slideshow->transition_surface = NULL;
        SDL_SetAtomicInt(&state.slideshow_background_ready, 0);
        }
    }
    else if (state.slideshow_transition) {

        // Fade the next image in by the share of its fade time that has passed
        int alpha = derive_fade_alpha(ticks.main, slideshow->transition_start, config.slideshow_transition_time, 0xFF);

        // If transition is done, destroy old background and replace it with the new one
        if (alpha >= 0xFF) {
#ifdef STREAMFLEX_TEST_HOOKS
            log_debug("Test hook: the slideshow's fade ended at tick %u, %u ms after it began",
                      ticks.main, ticks.main - slideshow->transition_start);
#endif
            SDL_SetTextureAlphaMod(slideshow->transition_texture, 0xFF);
            SDL_DestroyTexture(background_texture);
            background_texture = slideshow->transition_texture;
            slideshow->transition_texture = NULL;
            background_luminance = slideshow->transition_luminance;
            log_luminance();
            state.slideshow_transition = false;
            ticks.slideshow_load = ticks.main;
        }
        else
            SDL_SetTextureAlphaMod(slideshow->transition_texture, (Uint8) alpha);
    }
}

// A function to update the screensaver
static void update_screensaver()
{
    // Activate the screensaver if the launcher has been idle for the required time
    if (!state.screensaver_active && ticks.main - ticks.last_input > config.screensaver_idle_time) {
        log_debug("Screensaver on");
        state.screensaver_active = true;
        state.screensaver_transition = true;
        screensaver->transition_start = ticks.main;
#ifdef STREAMFLEX_TEST_HOOKS
        log_debug("Test hook: the screensaver's dim began at tick %u, to take %u ms",
                  screensaver->transition_start, (Uint32) SCREENSAVER_TRANSITION_TIME);
#endif
        if (background_shown == BACKGROUND_SLIDESHOW && config.screensaver_pause_slideshow)
            state.slideshow_paused = true;
    }
    else {

        // Transition the screen to dark, by the share of the dim's time that has passed
        if (state.screensaver_transition) {
            int end = (int) screensaver->alpha_end_value;
            int alpha = derive_fade_alpha(ticks.main, screensaver->transition_start, SCREENSAVER_TRANSITION_TIME, end);
            screensaver->alpha = (float) alpha;
            if (alpha >= end) {
                SDL_SetTextureAlphaMod(screensaver->texture, (Uint8) end);
                state.screensaver_transition = false;
#ifdef STREAMFLEX_TEST_HOOKS
                log_debug("Test hook: the screensaver's dim ended at tick %u, %u ms after it began",
                          ticks.main, ticks.main - screensaver->transition_start);
#endif
            }
            else
                SDL_SetTextureAlphaMod(screensaver->texture, (Uint8) alpha);
        }

        // User has pressed input, deactivate the screensaver
        if (state.screensaver_active && ticks.last_input == ticks.main) {
            log_debug("Screensaver off");
            SDL_SetTextureAlphaMod(screensaver->texture, 0);
            screensaver->alpha = 0.0f;
            state.screensaver_active = false;
            state.screensaver_transition = false;
            if (background_shown == BACKGROUND_SLIDESHOW) {
                state.slideshow_paused = false;
                
                // Reset the slideshow time so we don't have a transition immediately 
                // after coming out of screensaver mode
                ticks.slideshow_load = ticks.main;
            }
        }
    }
}

// A function to start the clock's render thread; NULL when it cannot start
static SDL_Thread *start_clock_thread()
{
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: the thread fails to start
    if (getenv("STREAMFLEX_TEST_CLOCK_THREAD_FAIL") != NULL) {
        SDL_SetError("Test hook: the clock's thread does not start");
        return NULL;
    }
#endif
    return SDL_CreateThread(render_clock_async, "Clock Thread", (void*) clk);
}

// A function to update the clock display
static void update_clock(bool block)
{
    if (ticks.main - ticks.clock_update > CLOCK_UPDATE_PERIOD) {
        if (!SDL_GetAtomicInt(&state.clock_rendering)) {

            // Check to see if the time has changed
            get_time(clk);
#ifdef STREAMFLEX_TEST_HOOKS
            // Only the headless harness builds this: with a slow render asked for, render every second
            if (getenv("STREAMFLEX_TEST_CLOCK_DELAY_MS") != NULL)
                clk->render_time = true;
#endif
            if (clk->render_time) {
                SDL_SetAtomicInt(&state.clock_rendering, 1);
                if (block)
                    render_clock(clk);
                else {
                    clock_thread = start_clock_thread();

                    // A thread that cannot start leaves the render to this one, as the blocking
                    // render does, and the handoff below takes it at once; the next one tries again
                    if (clock_thread == NULL) {
                        log_error("Could not start the clock's render thread, rendering on the main thread\n%s", SDL_GetError());
                        render_clock(clk);
                    }
#ifdef STREAMFLEX_TEST_HOOKS
                    // Only the headless harness builds this: a check waits for a render to be in flight
                    else if (getenv("STREAMFLEX_TEST_CLOCK_DELAY_MS") != NULL)
                        log_debug("Clock: rendering on its thread");
#endif
                }
            }
            else
                ticks.clock_update = ticks.main;
        }

        // Render texture
        if (SDL_GetAtomicInt(&state.clock_ready)) {
            SDL_WaitThread(clock_thread, NULL);
            clock_thread = NULL;
            clk->time_rect = clk->next_time_rect;   // The render placed them where the main thread
            clk->date_rect = clk->next_date_rect;   // does not draw from until now
            SDL_DestroyTexture(clk->time_texture);
            clk->time_texture = load_texture(clk->time_surface);
            clk->time_surface = NULL;
            if (clk->render_date) {
                SDL_DestroyTexture(clk->date_texture);
                clk->date_texture = load_texture(clk->date_surface);
                clk->date_surface = NULL;
            }
            ticks.clock_update = ticks.main;
            clk->render_time = false;
            clk->render_date = false;
            SDL_SetAtomicInt(&state.clock_rendering, 0);
            SDL_SetAtomicInt(&state.clock_ready, 0);
        }
    }
}

static int reload_pending = RELOAD_NONE;   // The ReloadReasons marked since the last reload
static bool size_deferred = false;         // A size change came while an app was in front: post_launch() reads it again

// A function to tell whether an app is launching or running: nothing is laid out for a new size then
bool app_in_front(void)
{
    return state.application_launching || state.application_running;
}

// A function to mark a reload, with its reason. The handlers only mark: one reload runs after the
// events are drained (reload_run_pending()), whatever came in the batch.
void reload_mark(ReloadReason why)
{
    reload_pending |= (int) why;
}

// A function to name a reload's reasons for the log, in a fixed order
static void describe_reload(int why, char *out, size_t size)
{
    static const struct { int reason; const char *name; } names[] = {
        { RELOAD_SIZE, "the window's size" }, { RELOAD_TARGETS, "a render target reset" },
        { RELOAD_DEVICE, "a device reset or loss" }, { RELOAD_RECREATE, "a new window" }
    };
    out[0] = '\0';
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        if (!(why & names[i].reason))
            continue;
        size_t used = strlen(out);
        snprintf(out + used, size - used, "%s%s", used > 0 ? " and " : "", names[i].name);
    }
}

// A function to run the one reload the marks ask for, after the events are drained and before
// anything is drawn: the screen geometry first, then every texture in the order Discard uses
// (REFRESH_ORDER), the gamepad left running and the screensaver's dim kept. While an app is launching
// or running, geo stays as it is and a size change waits for post_launch(), so a game's temporary
// resolution never becomes the layout; a reset in the same batch still rebuilds, at the size laid out.
// A reload with no renderer waits for the new one.
void reload_run_pending(void)
{
    if (reload_pending == RELOAD_NONE || renderer == NULL)
        return;
    int why = reload_pending;
    reload_pending = RELOAD_NONE;
    if (app_in_front() && (why & RELOAD_SIZE)) {
        size_deferred = true;   // The size is read again when the app is done, and reloads then if still new
        why &= ~RELOAD_SIZE;
        if (why == RELOAD_NONE)
            return;
    }
    bool resized = !app_in_front() && read_screen_geometry();
    if (why == RELOAD_SIZE && !resized) {
        log_debug("Screen: %ix%i px again; nothing to reload", geo.screen_width, geo.screen_height);
        return;
    }
    if (resized) {
        refresh_effective();      // The percentages of the screen, worked out again
        calculate_layout_area();
    }
    settings_refresh_for_reload();   // REFRESH_ORDER, the gamepad kept running and the screensaver's dim kept
    if (settings_is_open() && resized)
        settings_reflow();
    else if (settings_is_open())
        settings_remake_textures();   // A reset or a new renderer at the same size: the page as it is
    else if (resized)
        trim_title_fonts();
    char reasons[128];
    describe_reload(why, reasons, sizeof(reasons));
    log_debug("Reload: done at %ix%i px, for %s", geo.screen_width, geo.screen_height, reasons);
}

// A function to act on a change of the window's pixel size (SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED): the
// size laid out already does nothing (the event SDL sends as it makes the window, a Wayland scale
// change that keeps the native size); a new one marks the reload, or waits while an app is in front
static void pixel_size_changed(void)
{
    int width, height;
    window_pixel_size(&width, &height);
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: every size event, so a check can wait for one that changes nothing
    log_debug("Test hook: a size event found the window %ix%i px", width, height);
#endif
    SizeDecision decision = derive_size_decision(geo.screen_width, geo.screen_height, width, height, app_in_front());
    if (decision == SIZE_RELOAD) {
        log_debug("Screen: %ix%i px, was %ix%i; reloading", width, height, geo.screen_width, geo.screen_height);
        reload_mark(RELOAD_SIZE);
    }
    else if (decision == SIZE_WAIT) {
        log_debug("Screen: %ix%i px, was %ix%i; reloading when the app is done", width, height, geo.screen_width, geo.screen_height);
        size_deferred = true;
    }
}

// A function to read the size again once an app is done (post_launch(), or a launch given up on),
// when a size change came while it was in front: the reload runs only if the size is still new, so a
// game that changes the resolution and puts it back costs nothing
static void recheck_deferred_size(void)
{
    if (!size_deferred)
        return;
    size_deferred = false;
    int width, height;
    window_pixel_size(&width, &height);
    if (derive_size_decision(geo.screen_width, geo.screen_height, width, height, false) == SIZE_RELOAD) {
        log_debug("Screen: %ix%i px, was %ix%i; reloading", width, height, geo.screen_width, geo.screen_height);
        reload_mark(RELOAD_SIZE);
    }
    else
        log_debug("Screen: %ix%i px again; nothing to reload", width, height);
}

// A function to follow the window to another display (SDL_EVENT_WINDOW_DISPLAY_CHANGED): its refresh
// rate, and the frame timing worked out from it again (whose Frame timing: line follows this one's).
// A new size comes as its own event.
static void display_changed(SDL_DisplayID display)
{
    read_refresh_rate();
    log_debug("Screen: now on display %u, %i Hz", (unsigned int) display, display_rate);
    apply_frame_timing();
}

// A function to log a change of the OS's scale (SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED), which is
// never applied; one that changed the pixel size too is the size event's to report
static void display_scale_changed(void)
{
    float scale = SDL_GetWindowDisplayScale(window);
    int width, height;
    window_pixel_size(&width, &height);
    if (scale != display_scale && width == geo.screen_width && height == geo.screen_height)
        log_debug("Screen: display scale %.2f, was %.2f; nothing to redo", (double) scale, (double) display_scale);
    display_scale = scale;
}

// A function to mark the reload a render target or device reset needs (SDL_EVENT_RENDER_TARGETS_RESET,
// SDL_EVENT_RENDER_DEVICE_RESET): every texture made again once the events are drained, the menu and
// the screen kept. The log says so once for each reload, however many resets the batch held.
static void render_reset(ReloadReason why)
{
    if (!(reload_pending & (RELOAD_TARGETS | RELOAD_DEVICE)))
        log_debug("Renderer: device lost; reloading");
    reload_mark(why);
}

// A function to destroy a texture and forget it
static void destroy_texture(SDL_Texture **texture, int *count)
{
    if (*texture == NULL)
        return;
    SDL_DestroyTexture(*texture);
    *texture = NULL;
    (*count)++;
}

// A function to destroy every texture StreamFlex holds and forget it, while the renderer that made
// them still exists: what holds each (the highlight, the clock, a menu's entries) stays, so the
// reload's stops and starts find nothing to destroy and make every texture again. Returns how many.
static int drop_textures(void)
{
    int count = 0;
    destroy_texture(&background_texture, &count);
    destroy_texture(&background_overlay, &count);
    if (slideshow != NULL)
        destroy_texture(&slideshow->transition_texture, &count);
    if (highlight != NULL) {
        destroy_texture(&highlight->texture, &count);
        highlight->button = 0;   // So the next layout renders it again
    }
    if (scroll != NULL)
        destroy_texture(&scroll->texture, &count);
    if (clk != NULL) {
        destroy_texture(&clk->time_texture, &count);
        destroy_texture(&clk->date_texture, &count);
    }
    if (screensaver != NULL)
        destroy_texture(&screensaver->texture, &count);
    for (Menu *menu = config.first_menu; menu != NULL; menu = menu->next) {
        for (unsigned int i = 0; i < menu->num_entries; i++) {
            destroy_texture(&menu->items[i]->icon, &count);
            destroy_texture(&menu->items[i]->icon_selected, &count);
            destroy_texture(&menu->items[i]->title_texture, &count);
        }
        menu->rendered_size = 0;
    }
    count += settings_drop_textures();   // The settings screen's own, and the browsed image
    return count;
}

// A function to let go of a renderer SDL could not recover (SDL_EVENT_RENDER_DEVICE_LOST: SDL has
// already tried once, and the renderer is unusable): every texture first, then the renderer. A new
// one is owed, at once, or once a launched app is done: a GPU reset is often the launched game's own,
// a renderer made under it would likely fail the same way, and nothing is drawn while it runs.
// A second loss in the same batch finds no renderer, and does nothing.
static void renderer_lost(void)
{
    if (renderer == NULL)
        return;
    int dropped = drop_textures();
    SDL_DestroyRenderer(renderer);
    renderer = NULL;
    log_debug("Renderer: dropped %i textures before the renderer went", dropped);
    log_debug("Renderer: device lost and not recovered; creating a new renderer");
    renderer_owed = true;
    renderer_attempt = 0;
    renderer_next_try = SDL_GetTicks();
}

// A function to make the renderer a lost device owes, when it is due: at once after the loss, or
// once a launched app is done, then RENDERER_RETRY_MS apart, up to RENDERER_RETRIES attempts (spec
// section 3). It is called once a pass of the main loop, so events are pumped between the attempts:
// the window never stops answering, and every quit path stays live. The new renderer is made as at
// startup, for the window in force; on Windows its handle is read again. The last attempt's failure
// takes the startup error path, but on a Transparent window, which falls back to an opaque one.
static void renderer_retry(void)
{
    if (!renderer_owed || app_in_front() || SDL_GetTicks() < renderer_next_try)
        return;
    renderer_attempt++;
    log_debug("Renderer: creating a new renderer, attempt %i of %i", renderer_attempt, RENDERER_RETRIES);
    // The window's kind picks the list, never the Mode setting, which may have moved on while
    // settings wait for a rest, or after a fallback
    WindowPlan plan = plan_in_force();
    if (create_renderer(&plan)) {
        renderer_owed = false;
        renderer_attempt = 0;
#ifdef _WIN32
        platform_window_changed(window, window);
#endif
        reload_mark(RELOAD_DEVICE);
        return;
    }
    if (renderer_attempt >= RENDERER_RETRIES) {
        if (window_kind != WINDOW_TRANSPARENT_SHORT)
            quit(EXIT_FAILURE);   // create_renderer() logged SDL's reason, as at startup
        // A Transparent window never ends StreamFlex: an opaque one takes its place, whose renderer
        // ends the schedule (recreate_window() owes no other)
        char why[512];
        snprintf(why, sizeof(why), "%i attempts at a new renderer failed: %s", RENDERER_RETRIES, SDL_GetError());
        fall_back_to_opaque(why);
        return;
    }
    renderer_next_try = SDL_GetTicks() + RENDERER_RETRY_MS;
}

static inline void pre_launch()
{
    if (gamepads != NULL)
        disconnect_gamepad(-1, true, false);

// Initialize exit hotkey for Windows
#ifdef _WIN32
    if (has_exit_hotkey())
        listen_exit_hotkey(true);
#endif
}

static inline void post_launch()
{
    // Rebaseline the timing after the program is done, a held pad control's too
    ticks.main = ticks_ms();
    ticks.last_input = ticks.main;
    rebase_held_controls();

    // Post-application updates
    if (gamepad_on)
        connect_gamepad(-1, true, false);
    // A device lost while the app ran: its new renderer is due now, and is made in this pass of the
    // main loop, after the app is said to be finished; the reload then makes the clock again too
    if (renderer_owed) {
        renderer_next_try = SDL_GetTicks();
        log_debug("Renderer: the app is done, so the new renderer is made now");
    }
    else if (clk != NULL)
        update_clock(true);
    if (background_shown == BACKGROUND_SLIDESHOW)
        resume_slideshow();
    if (config.on_launch == ON_LAUNCH_BLANK)
        set_draw_color();
    recheck_deferred_size();

#ifdef _WIN32
    listen_exit_hotkey(false);
    if (background_shown == BACKGROUND_TRANSPARENT)
        hide_cursor(current_entry);
#endif
}

#ifdef _WIN32
// A function to try the exit hotkey Windows refused again, now or when it is due. Taken while an
// application runs, its key must reach the loop at once, as pre_launch() lets it when the hotkey was
// there at launch: post_launch() may never follow, when focus comes and goes in one drain
static void retry_exit_hotkey_events(bool at_once)
{
    if (retry_exit_hotkey(at_once) && state.application_running)
        listen_exit_hotkey(true);
}
#endif

// A function to quit the launcher
void quit(int status)
{
    log_debug("Quitting program");
    bool message_box = status != EXIT_SUCCESS;
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: STREAMFLEX_TEST_NO_MESSAGE_BOX leaves the message box
    // out, which some SDLs show and wait on
    if (getenv("STREAMFLEX_TEST_NO_MESSAGE_BOX") != NULL)
        message_box = false;
#endif
    if (message_box)
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, 
            PROJECT_NAME, 
            "A critical error occurred. Check the log file for details.", 
            NULL
        );
    // Close settings first, saving nothing: while they are open they would swallow the QuitCmd
    settings_close_now();
    if (config.quit_cmd != NULL) {
        execute_command(config.quit_cmd);
        free(config.quit_cmd);
        config.quit_cmd = NULL;   // cleanup() frees it too
    }
    cleanup();
    release_last();
    exit(status);
}

// A function to close the log file if it is open; a log on stderr (no home folder) is not ours to close
static void close_log()
{
    if (log_file != NULL && log_file != stderr)
        fclose(log_file);
    log_file = NULL;
}

// A function to release what cleanup() leaves, the last thing quit() and a restart do before they exit
static void release_last(void)
{
    close_log();
    free(config.exe_path);   // The log's folder on Windows, so it goes after the log
    alloc_free(start_argv);
}

// A function to restart StreamFlex, to apply the settings named (as the restart prompt words them)
// that wait for the next start. A restart is not a quit: QuitCmd does not run, and the fresh copy,
// started with --restarted, runs no StartupCmd. The program is found before anything is torn down
// (on Windows its fresh copy is made then too, waiting, while this one can still give it the
// foreground), and when it cannot be, this comes back with the reason logged and StreamFlex running
// on. Then everything quit() tears down goes but the log, which says what became of the start: on
// Linux the program takes this process's place (exec), on Windows the fresh copy runs once this
// one's window and :exit hotkey are gone, and this one ends. A start that fails ends StreamFlex
// with an error, to be started by hand.
void restart_streamflex(const char *names)
{
    if (start_argv == NULL) {
        log_error("Cannot restart StreamFlex: there was no memory to keep its arguments when it started");
        return;
    }
    if (!find_self(start_argv[0]))
        return;
#ifdef _WIN32
    if (!make_self())
        return;
#endif
    log_debug("Restarting StreamFlex to apply %s", names);
#ifdef _WIN32
    clear_exit_hotkey();
#endif
    cleanup();
    if (log_file != NULL)
        fflush(log_file);   // Before the fresh copy goes on with the log
    bool started = start_self(start_argv);
    release_last();
    exit(started ? EXIT_SUCCESS : EXIT_FAILURE);
}

// A function to print the version and other info to command line
void print_version(FILE *stream)
{
    int sdl_version = SDL_GetVersion();
    int img_version = IMG_Version();
    int ttf_version = TTF_Version();
    fprintf(stream, PROJECT_NAME " version " PROJECT_VERSION ", using:" endline);
    fprintf(stream, "  SDL       %d.%d.%d" endline, SDL_VERSIONNUM_MAJOR(sdl_version), SDL_VERSIONNUM_MINOR(sdl_version),
            SDL_VERSIONNUM_MICRO(sdl_version));
    fprintf(stream, "  SDL_image %d.%d.%d" endline, SDL_VERSIONNUM_MAJOR(img_version), SDL_VERSIONNUM_MINOR(img_version),
            SDL_VERSIONNUM_MICRO(img_version));
    fprintf(stream, "  SDL_ttf   %d.%d.%d" endline, SDL_VERSIONNUM_MAJOR(ttf_version), SDL_VERSIONNUM_MINOR(ttf_version),
            SDL_VERSIONNUM_MICRO(ttf_version));
}

int main(int argc, char *argv[]) 
{
    int error;
    char *config_file_path = NULL;
    const char *base_path = SDL_GetBasePath();   // SDL3's own copy, freed at SDL_Quit; release_last() frees ours
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: SDL_GetBasePath() failing, as it can (05-sdl-results.sh)
    if (getenv("STREAMFLEX_TEST_NO_BASE_PATH") != NULL)
        base_path = NULL;
#endif
    config.exe_path = base_path != NULL ? strdup(base_path) : NULL;
    config_apply_defaults();

    // Keep the arguments as they are now, before getopt reorders them, for a restart to start with
    test_fail("restart", true);
    start_argv = alloc_calloc((size_t) argc + 2, sizeof(char*));
    test_fail("restart", false);
    for (int i = 0; start_argv != NULL && i < argc; i++)
        start_argv[i] = argv[i];

    // Handle command line arguments, find config file
    handle_arguments(argc, argv, &config_file_path);
#ifdef __linux__
    if (config.restarted)
        keep_name(argv[0]);   // Started through /proc/self/exe, which names the process "exe"
#endif
    if (start_argv != NULL && !config.restarted)
        start_argv[argc] = "--restarted";

    // Parse config file for settings and menu entries
    parse_config_file(config_file_path);
    config.config_path = config_file_path;   // The settings screen saves here
    build_menu_items();
    resolve_library_icons();

    // Get default menu
    if (config.default_menu == NULL)
        log_fatal("No default menu defined in config file");
    default_menu = get_menu(config.default_menu);
    if (default_menu == NULL)
        log_fatal("Default menu %s not found in config file", config.default_menu);

    // Initialize SDL and make the window first: the screen's size comes from the window (spec
    // section 6), and the title font's minimum and the derived values read that size
    init_sdl();
    create_window();
    if (config.restarted)
        raise_restarted_window();
    ticks.program_start = ticks_ms();   // The window is open: Timing: startup and the focus re-grab count from here
#ifdef _WIN32
    log_debug("DPI awareness: %s", dpi_awareness());
#endif
    prime_image_formats();
    init_sdl_ttf();
    refresh_effective();

    // Initialize Nanosvg
    init_svg();

    // Initialize timing
    ticks.main = ticks_ms();
    ticks.last_input = ticks.main;

    // Set the background up
    reload_background();

    // Start every feature that is on: the settings screen starts and stops them the same way
    start_gamepad();
    apply_os_screensaver();
    start_screensaver();
    start_clock();
    start_highlight();
    start_scroll();

    // Work out where the buttons may go, now that the clock's size is known
    calculate_layout_area();

    // Register exit hotkey with Windows
#ifdef _WIN32
    if (has_exit_hotkey())
        register_exit_hotkey();
#endif

    // Print debug info to log
    if (config.debug) {
        debug_video(renderer, geo.screen_width, geo.screen_height, display_rate);
        debug_settings();
        debug_gamepad(gamepad_controls);
        debug_hotkeys(hotkeys);    
        debug_menu_entries(config.first_menu, config.num_menus);
    }

    // Load the default menu and display it
    error = load_menu(default_menu, false, true);
    if (error)
        log_fatal("Could not load default menu %s", config.default_menu);

    // Execute startup command; a restart's fresh copy runs none, as the start before it ran it
    if (config.startup_cmd != NULL && config.restarted)
        log_debug("Restarted, so the StartupCmd does not run again");
    else if (config.startup_cmd != NULL)
        execute_command(config.startup_cmd);
    
    // Main program loop
    log_debug("Begin program loop");
    log_debug("Timing: startup: first menu shown %u ms after the window opened", (unsigned) (ticks_ms() - ticks.program_start));
    while (1) {
        ticks.main = ticks_ms();
#ifdef STREAMFLEX_TEST_HOOKS
        test_pad_update();
        test_app_update();
        test_pixel_size_update();
        test_display_event_update();
        test_render_event_update();
#endif
        while (SDL_PollEvent(&event)) {
            // A window event names its window: one from a window a recreation replaced (its focus lost as
            // the new one took the foreground) is not the window's, and is never read as an app's launch
            // or return, or as a size or display change
            if (event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST &&
                event.window.windowID != window_id)
                continue;
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    quit(EXIT_SUCCESS);
                    break;

                case SDL_EVENT_KEY_DOWN:
                    ticks.last_input = ticks.main;
                    // Nothing is on screen to act on until a lost device's new renderer exists
                    if (renderer == NULL) {
                        log_debug("Key ignored: there is no renderer yet");
#ifdef STREAMFLEX_TEST_HOOKS
                        test_key_undrawn = true;   // The first frame drawn once there is one is the key's
#endif
                        break;
                    }
                    // Every key code StreamFlex holds is SDL2's, as config.ini stores it (keycodes.h)
                    handle_keypress((SDL_Keycode) keycode_to_stored(event.key.key, KEYCODES_WINDOWS), event.key.repeat);
#ifdef STREAMFLEX_TEST_HOOKS
                    test_key_undrawn = true;
#endif
                    break;

                case SDL_EVENT_KEY_UP:
                    if (settings_is_open())
                        settings_raw_release((int) keycode_to_stored(event.key.key, KEYCODES_WINDOWS));
                    break;

                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    if (renderer != NULL && config.mouse_select && !settings_is_open() && event.button.button == SDL_BUTTON_LEFT) {
                        ticks.last_input = ticks.main;
                        execute_command(current_entry->cmd);
                    }
                    break;

                case SDL_EVENT_JOYSTICK_ADDED: {
                    // `which` is the instance id in SDL3; the Device setting counts device indexes
                    int index = joystick_index(event.jdevice.which);
                    if (gamepad_on && index >= 0 && SDL_IsGamepad(event.jdevice.which) &&
                        (config.gamepad_device < 0 || config.gamepad_device == index)) {
                        // With no pad before it, nothing was polled since the last one went
                        bool none = gamepads == NULL;
                        connect_gamepad(index, !state.application_running, true);
                        if (none && gamepads != NULL)
                            rebase_held_controls();
                    }
                    settings_pads_changed();   // The Device row names every pad present, while settings are open
                    break;
                }

                case SDL_EVENT_JOYSTICK_REMOVED:
                    // `which` is the instance id here: only a pad in the list is removed, whatever Device says
                    log_debug("Gamepad disconnected");
                    disconnect_gamepad((int) event.jdevice.which, true, true);
                    settings_pads_changed();
                    break;

                case SDL_EVENT_JOYSTICK_BATTERY_UPDATED:
                    gamepad_power_changed(event.jbattery.which, event.jbattery.state, event.jbattery.percent);
                    break;

                case SDL_EVENT_WINDOW_FOCUS_LOST:
                    log_debug("Lost keyboard focus");
                    state.has_focus = false;
                    if (state.application_launching) {
                        log_debug("Application detected");
                        state.application_launching = false;
                        state.application_running = true;
                        pre_launch();
                    }
#ifdef _WIN32
                    // Sometimes the launcher will lose focus on Windows when autostarting
                    // So if we lose the window focus within 10 seconds of the launcher starting
                    // we will grab back th Window focus. This is a bit of a hack
                    else if (ticks.main - ticks.program_start < 10000)
                        set_foreground_window();
#endif
                    break;

                case SDL_EVENT_WINDOW_FOCUS_GAINED:
                    log_debug("Gained keyboard focus");
                    state.has_focus = true;
#ifdef _WIN32
                    retry_exit_hotkey_events(true);   // The application that held its key may have just quit
#endif
                    break;

                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                    pixel_size_changed();
                    break;

                case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
                    display_changed((SDL_DisplayID) event.window.data1);
                    refit_transparent_window();
                    break;

                case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:
                case SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED:
                    refit_transparent_window();
                    break;

                case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
                    display_scale_changed();
                    break;

                case SDL_EVENT_RENDER_TARGETS_RESET:
                    render_reset(RELOAD_TARGETS);
                    break;

                case SDL_EVENT_RENDER_DEVICE_RESET:
                    render_reset(RELOAD_DEVICE);
                    break;

                case SDL_EVENT_RENDER_DEVICE_LOST:
                    renderer_lost();
                    break;

                case SDL_EVENT_WINDOW_MOUSE_LEAVE:
                    log_debug("Lost mouse focus");
                    break;
            }
        }

#ifdef _WIN32
        // Try the exit hotkey Windows refused again when it is due
        retry_exit_hotkey_events(false);
#endif

        // Update application state
        if (state.application_running && state.has_focus) {
            state.application_running = false;
            post_launch();
            log_debug("Application finished");
        }

        // A launch that never took the screen is given up on; a size change it held back is read again
        if (state.application_launching &&
        ticks.main - ticks.application_launched > config.application_timeout) {
            state.application_launching = false;
            rebase_held_controls();   // The pad was not polled while the application launched
            recheck_deferred_size();
            if (config.on_launch == ON_LAUNCH_BLANK)
                set_draw_color();
        }

        // A lost device's new renderer when it is due, then one reload for every reason the events
        // above marked, geometry first, before anything is drawn
        renderer_retry();
        // A window of another kind, asked for by settings, is made once no app is in front and a
        // renderer exists (a lost device's new renderer comes first, from renderer_retry()), before the
        // reload it marks
        if (recreate_wanted && !app_in_front() && renderer != NULL)
            run_recreation();
        reload_run_pending();

        // While a new renderer is owed, nothing is drawn or updated; the events above still run
        if (renderer == NULL) {
            SDL_Delay(APPLICATION_WAIT_PERIOD);
            continue;
        }

        // Post-event loop updates
        if (!(state.application_running || state.application_launching)) {
            // A capture or the 10 s take the pad before its controls run
            if (gamepads != NULL && !(settings_is_open() && settings_raw_pad(gamepad_pressed_label())))
                poll_gamepad();
            if (background_shown == BACKGROUND_SLIDESHOW)
                update_slideshow();
            // Settings never start the screensaver, but the key that opened them must still end it
            if (screensaver != NULL && (!settings_is_open() || state.screensaver_active))
                update_screensaver();
            if (clk != NULL)
                update_clock(false);
        }
        if (settings_is_open())
            settings_draw();
        else if (state.application_running)
            SDL_Delay(APPLICATION_WAIT_PERIOD);
        else
            draw_screen();
    }
    quit(EXIT_SUCCESS);
}
