#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <SDL_thread.h>
#include "launcher.h"
#include <launcher_config.h>
#include "image.h"
#include "util.h"
#include "library.h"
#include "debug.h"
#include "clock.h"
#include "config_fields.h"
#include "settings_screen.h"
#include "settings_pickers.h"
#include "platform/platform.h"

static void init_sdl(void);
static void init_sdl_image(void);
static void create_window(void);
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
#ifdef STREAMFLEX_TEST_HOOKS
static void test_pad_attach(void);
static int test_pad_device(SDL_JoystickID id);
static void test_pad_swap(void);
static void test_pad_update(void);
static void test_pad_stop(void);
#endif
static SDL_Thread *start_clock_thread(void);
static inline void pre_launch(void);
static inline void post_launch(void);
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
static void handle_keypress(SDL_Keysym *key, bool repeat);
static bool hotkey_bound(SDL_Keycode keycode);
static bool menu_key(SDL_Keycode keycode);
static void execute_command(const char *command);
static void poll_gamepad(void);
static void init_gamepad(Gamepad **gamepad, int device_index);
static void connect_gamepad(int device_index, bool open, bool raise_error);
static void disconnect_gamepad(int id, bool disconnect, bool remove);
static int gamepad_device_index(const Gamepad *gamepad);
static void open_controller(Gamepad *gamepad, bool raise_error);
static void cleanup(void);

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
    .highlight_fill_color.a           = DEFAULT_HIGHLIGHT_FILL_COLOR_A,
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
ModeBackground background_shown       = BACKGROUND_COLOR; // What is on screen: the colour when the chosen background failed
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
SDL_SysWMinfo wm_info;
SDL_DisplayMode display_mode;
TextInfo title_info;
static TTF_Font *fixed_title_font = NULL; // The title font at the fixed FontSize
Ticks ticks;
Geometry geo;
LayoutGeometry layout;                     // The current menu's layout
LayoutArea layout_area;                    // The part of the screen the buttons may use
Uint32 refresh_period;
Uint32 delay_period;
Uint32 repeat_period;
Effective eff;                        // The values drawn with, derived from config (derive.h)
SDL_Color title_color;                // eff's title colours as SDL colours, for the titles (the clock keeps its own)
SDL_Color title_shadow_color;
static bool gamepad_on = false;   // The game controller subsystem is running
static bool vsync_wanted = true; // VSync and FPSLimit ask the renderer for VSync
static bool vsync_on = false;    // The renderer really gives it; unless both are true present_frame() paces each frame


// A function to initialize SDL
static void init_sdl()
{    
    // Set flags, hints
    Uint32 sdl_flags = SDL_INIT_VIDEO;
#ifdef __unix__
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#endif
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, config.inhibit_os_screensaver ? "0" : "1");

    // Initialize SDL
    if (SDL_Init(sdl_flags) < 0)
        log_fatal("Could not initialize SDL\n%s", SDL_GetError());

    SDL_GetDesktopDisplayMode(0, &display_mode);
    geo.screen_width = display_mode.w;
    geo.screen_height = display_mode.h;

    // SDL reports 0 when the display does not say (Xvfb, some VMs and remote desktops), and
    // every timing below divides by the rate: create_window() reads it again from display_mode
    if (display_mode.refresh_rate <= 0) {
        log_debug("The display reports no refresh rate, using %i Hz", DEFAULT_REFRESH_RATE);
        display_mode.refresh_rate = DEFAULT_REFRESH_RATE;
    }
    refresh_period = 1000 / (Uint32) display_mode.refresh_rate;
    geo.screen_margin = (int) (SCREEN_MARGIN * (float) geo.screen_height);
    geo.title_min_size = (int) (TITLE_MIN_SIZE * (float) geo.screen_height + 0.5F);
}

// A function to tell whether the renderer really presents with VSync. Before SDL 2.26 nothing
// stands in for a VSync the driver refuses: the OpenGL renderer then reports none, and the software
// renderer reports VSync it does not give.
static bool renderer_vsync()
{
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: its SDL simulates VSync, so a refusal is made up here.
    // "off" is a renderer that keeps VSync on when it is not wanted; any other value never gives it.
    const char *refused = getenv("STREAMFLEX_TEST_VSYNC_REFUSED");
    if (refused != NULL)
        return strcmp(refused, "off") == 0;
#endif
    SDL_RendererInfo info;
    if (renderer == NULL || SDL_GetRendererInfo(renderer, &info) != 0 || !(info.flags & SDL_RENDERER_PRESENTVSYNC))
        return false;
    SDL_version version;
    SDL_GetVersion(&version);
    if (version.major == 2 && version.minor < 26 && strcmp(info.name, "software") == 0)
        return false;
    return true;
}

// A function to find out whether the renderer gives the VSync wanted, and say so; without it,
// present_frame() paces each frame to refresh_period
static void check_vsync()
{
    vsync_on = renderer_vsync();
    log_debug("Frame timing: VSync wanted %s, the renderer gives %s", vsync_wanted ? "on" : "off", vsync_on ? "on" : "off");
    if (vsync_wanted && !vsync_on)
        log_error("The renderer refused VSync: each frame is paced to %u ms instead", refresh_period);
    else if (!vsync_wanted && vsync_on)
        log_error("The renderer would not turn VSync off: each frame is still paced to %u ms", refresh_period);
}

// A function to work out the frame timing from VSync and FPSLimit, live: VSync, or the FPS limit's
// own frame time when it is off and the limit lies between the minimum and the display's rate
// (otherwise VSync, as always). The settings stay as written; the renderer follows (SDL 2.0.18).
// The gamepad's repeat, the slideshow's fade and the screensaver's dim are timed in frames, so
// they are worked out again too.
void apply_frame_timing()
{
    int rate = display_mode.refresh_rate;
    bool wanted = config.vsync || config.fps_limit < MIN_FPS_LIMIT || config.fps_limit > rate;
    bool changed = wanted != vsync_wanted;
    vsync_wanted = wanted;

    // With VSync wanted the frame is the display's, which also paces a renderer that refused it
    refresh_period = 1000 / (Uint32) (wanted ? rate : config.fps_limit);
    delay_period = GAMEPAD_REPEAT_DELAY / refresh_period;
    repeat_period = GAMEPAD_REPEAT_INTERVAL / refresh_period;
    if (!repeat_period)
        repeat_period = 1;
    update_slideshow_timing();
    if (screensaver != NULL)
        screensaver->transition_change_rate = screensaver->alpha_end_value / ((float) SCREENSAVER_TRANSITION_TIME / (float) refresh_period);
    if (wanted)
        log_debug("Frame timing: VSync at %i Hz, %u ms a frame", rate, refresh_period);
    else
        log_debug("Frame timing: FPS limit %i, %u ms a frame", config.fps_limit, refresh_period);

    // The renderer is asked only when the wish changes, so one that refused VSync is not asked, or
    // logged, again for each FPSLimit change. A failure to switch counts as a refusal.
    if (renderer != NULL && changed) {
        if (SDL_RenderSetVSync(renderer, wanted ? 1 : 0) != 0)
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

// A function to create the window and renderer
static void create_window()
{
    window = SDL_CreateWindow(PROJECT_NAME,
                 SDL_WINDOWPOS_UNDEFINED,
                 SDL_WINDOWPOS_UNDEFINED,
                 0,
                 0,
                 SDL_WINDOW_FULLSCREEN_DESKTOP
             );
    if (window == NULL)
        log_fatal("Could not create SDL Window\n%s", SDL_GetError());
    SDL_ShowCursor(SDL_DISABLE);

    // Create HW accelerated renderer, get screen resolution for geometry calculations
    apply_frame_timing();
    Uint32 renderer_flags = SDL_RENDERER_ACCELERATED;
    if (vsync_wanted)
        renderer_flags |= SDL_RENDERER_PRESENTVSYNC;
    renderer = SDL_CreateRenderer(window, -1, renderer_flags);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    if (renderer == NULL)
        log_fatal("Could not initialize renderer\n%s", SDL_GetError());

    // Which video driver and renderer SDL chose: X11 or Wayland, OpenGL or software
    SDL_RendererInfo renderer_info;
    if (SDL_GetRendererInfo(renderer, &renderer_info) == 0)
        log_debug("Video: SDL's %s driver, the %s renderer", SDL_GetCurrentVideoDriver(), renderer_info.name);
    check_vsync();

    // Set background color
    set_draw_color();

#ifdef _WIN32
    SDL_VERSION(&wm_info.version);
    SDL_GetWindowWMInfo(window, &wm_info);
#endif
}

// A function to initialize the SDL_image library
static void init_sdl_image()
{
    int img_flags = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP;
    if (!(IMG_Init(img_flags) & img_flags))
        log_fatal("Could not initialize SDL_image\n%s", IMG_GetError());
}

// A function to set the color of the renderer
void set_draw_color()
{
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
    if (TTF_Init() == -1)
        log_fatal("Could not initialize SDL_ttf\n%s", TTF_GetError());

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
    if (load_font(&title_info, config.title_font_path, config.title_font_face, FILENAME_DEFAULT_FONT))
        log_fatal("Could not load title font");
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: STREAMFLEX_TEST_RELOAD_FONTS opens the font again, as a
    // reload will, so the leak pass shows whether the one it replaces is closed
    if (getenv("STREAMFLEX_TEST_RELOAD_FONTS") != NULL) {
        log_debug("Test hook: the title font opens again");
        if (load_font(&title_info, config.title_font_path, config.title_font_face, FILENAME_DEFAULT_FONT))
            log_fatal("Could not load title font");
    }
#endif
    log_debug("Title font: %s (face %i)", title_info.font_path, title_info.font_face);
    fixed_title_font = title_info.font;
    geo.font_height = config.titles_enabled ? TTF_FontHeight(title_info.font) : 0;

    // A percentage FontSize sizes each menu's titles from its buttons, so measure the font's line
    // height per point once, at a large size, for the layout to reserve room for any size
    TTF_Font *probe = TTF_OpenFontIndex(title_info.font_path, TITLE_MEASURE_SIZE, title_info.font_face);
    geo.title_line_pm = probe != NULL ? TTF_FontHeight(probe) * 1000 / TITLE_MEASURE_SIZE : 1500;
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
    IMG_Quit();

    // Close every title font while SDL_ttf is still open (stop_clock() closed the clock's)
    title_fonts_free();
    if (fixed_title_font != NULL)
        TTF_CloseFont(fixed_title_font);
    fixed_title_font = NULL;
    title_info.font = NULL;
    free(title_info.font_path);
    title_info.font_path = NULL;
    TTF_Quit();
    quit_svg();

    // Close log file if open; a log on stderr (no home folder) is not ours to close
    if (log_file != NULL && log_file != stderr)
        fclose(log_file);

    // Free dynamically allocated memory
    free(config.default_menu);
    free(config.background_image);
    free(config.title_font_path);
    free(config.exe_path);
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

// A function to handle key presses from keyboard; `repeat` marks the keyboard's own repeats of a
// held key
static void handle_keypress(SDL_Keysym *key, bool repeat)
{
    if (config.debug)
        log_debug("Key %s (#%X) detected", SDL_GetKeyName(key->sym), key->sym);

    // A capture takes every key, and the 10 s the key that confirms them, before any other meaning
    if (settings_is_open() && settings_raw_key(key->sym, repeat))
        return;

    // A held Menu key opens (or closes) settings once: its repeats would strobe them
    if (repeat && menu_key(key->sym))
        return;

    // While settings are open, the built-in keys are their commands; other keys run their hotkey,
    // which execute_command() hands to settings (and settings ignore unless it is one of theirs)
    if (settings_is_open()) {
        const char *command = NULL;
        switch (key->sym) {
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
                if (menu_key(key->sym))
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
    if (key->sym == SDLK_LEFT)
        move_selection(LAYOUT_LEFT);
    else if (key->sym == SDLK_RIGHT)
        move_selection(LAYOUT_RIGHT);
    else if ((key->sym == SDLK_UP || key->sym == SDLK_DOWN) && !hotkey_bound(key->sym))
        move_selection(key->sym == SDLK_UP ? LAYOUT_UP : LAYOUT_DOWN);
    else if (key->sym == SDLK_RETURN) {
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
    else if (key->sym == SDLK_BACKSPACE)
        load_back_menu(current_menu);

    // The Menu key opens settings, unless a hotkey has it
    else if (menu_key(key->sym))
        execute_command(SCMD_SETTINGS);

    //Check hotkeys
    else {
        for (Hotkey *i = hotkeys; i != NULL; i = i->next) {
            if (key->sym == i->keycode) {
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
// one image that still loads (surface, the same image as the one on show), or the colour when none
// does. The Mode setting stays Slideshow, so the folder is tried again when the background is next
// set up.
static void fall_back_from_slideshow(SDL_Surface *surface)
{
    if (surface != NULL) {
        log_error("Could only load one image from slideshow directory %s, showing it as a single image",
            config.slideshow_directory
        );
        SDL_FreeSurface(surface);
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

// A function to scan the slideshow folder. What is shown falls back to the colour, or to a single
// image, when the folder is missing or holds fewer than two images; the settings are left alone.
static void init_slideshow()
{
    // While settings are open, a mode with no folder chosen yet is not a config problem
    if (config.slideshow_directory == NULL && settings_is_open()) {
        log_debug("Settings: no slideshow folder chosen yet, the preview shows the colour");
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
        .transition_alpha = 0.f,
        .transition_change_rate = 0.f,
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

// A function to start the screensaver, when it is on and not already running
static void start_screensaver()
{
    if (!config.screensaver_enabled || screensaver != NULL)
        return;
    if (eff.screensaver_alpha < 1) {
        log_error("Invalid screensaver intensity value, so the screensaver was not started");
        return;
    }
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, geo.screen_width, geo.screen_height, 32, SDL_PIXELFORMAT_ARGB8888);
    if (surface == NULL) {
        log_error("Could not start the screensaver\n%s", SDL_GetError());
        return;
    }
    SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 0, 0, 0, 0xFF));
    screensaver = malloc(sizeof(Screensaver));
    screensaver->alpha_end_value = (float) eff.screensaver_alpha;
    screensaver->transition_change_rate = screensaver->alpha_end_value / ((float) SCREENSAVER_TRANSITION_TIME / (float) refresh_period);
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

// A function to stop the overlay, freeing its texture
static void stop_overlay()
{
    if (background_overlay == NULL)
        return;
    SDL_DestroyTexture(background_overlay);
    background_overlay = NULL;
    log_debug("Overlay stopped");
}

// A function to start the overlay, when it is on: a screen-sized texture of its colour and opacity
static void start_overlay()
{
    if (!config.background_overlay || background_overlay != NULL)
        return;
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, geo.screen_width, geo.screen_height, 32, SDL_PIXELFORMAT_ARGB8888);
    if (surface == NULL) {
        log_error("Could not start the overlay\n%s", SDL_GetError());
        return;
    }
    SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format,
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
    SDL_AtomicSet(&state.clock_rendering, 0);
    SDL_AtomicSet(&state.clock_ready, 0);
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
    SDL_AtomicSet(&state.clock_rendering, 0);
    SDL_AtomicSet(&state.clock_ready, 0);
    if (clk->time_surface != NULL)
        SDL_FreeSurface(clk->time_surface);
    if (clk->date_surface != NULL)
        SDL_FreeSurface(clk->date_surface);
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

// A function to work out the slideshow's fade speed from its fade time and the frame rate
void update_slideshow_timing()
{
    if (slideshow != NULL && config.slideshow_transition_time > 0)
        slideshow->transition_change_rate = 255.0f / ((float) config.slideshow_transition_time / (float) refresh_period);
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
            SDL_FreeSurface(slideshow->transition_surface);
        if (slideshow->transition_texture != NULL)
            SDL_DestroyTexture(slideshow->transition_texture);
        quit_slideshow();
    }
}

// A function to set the background up for config.background_mode: at startup, and whenever the
// settings screen changes it. What is shown (background_shown) falls back to the colour when an
// image or slideshow cannot be used; the setting itself stays as it was chosen.
void reload_background()
{
    stop_slideshow();
    state.slideshow_transition = false;
    SDL_AtomicSet(&state.slideshow_background_rendering, 0);
    SDL_AtomicSet(&state.slideshow_background_ready, 0);
    if (background_texture != NULL) {
        SDL_DestroyTexture(background_texture);
        background_texture = NULL;
    }
    background_luminance = -1.0;

    background_shown = config.background_mode;
    if (config.background_mode == BACKGROUND_IMAGE) {
        // While settings are open, a mode with no image chosen yet is not a config problem; an
        // image that was chosen and fails to load is one, and says so as it does at startup
        if (config.background_image == NULL && settings_is_open())
            log_debug("Settings: no image chosen yet, the preview shows the colour");
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
            SDL_Surface *surface = load_next_slideshow_background(slideshow, false);
            if (surface != NULL) {
                background_luminance = surface_luminance(surface);
                background_texture = load_texture(surface);
                ticks.slideshow_load = ticks.main;
            }
            else
                fall_back_from_slideshow(NULL);
        }
    }
    if (background_shown == BACKGROUND_IMAGE || background_shown == BACKGROUND_SLIDESHOW)
        log_luminance();
    update_slideshow_timing();
#ifdef _WIN32
    if (background_shown == BACKGROUND_TRANSPARENT)
        make_window_transparent();
    else
        make_window_opaque();
#endif
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
    log_debug("Layout area: from y %i, %i px tall, centred at %i px", layout_area.y, layout_area.h, layout_area.vcenter);
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
    int size = geometry->button;
    title_info.font = fixed_title_font;
    title_info.font_size = (int) config.title_font_size;
    if (geometry->title_size > 0) {
        TTF_Font *font = title_font(geometry->title_size);
        if (font != NULL) {
            title_info.font = font;
            title_info.font_size = geometry->title_size;
        }
    }
    int line_height = TTF_FontHeight(title_info.font);
    title_info.max_width = layout_title_width(config.title_shadows, size, line_height);   // Room for the shadow
    for (unsigned int i = 0; i < menu->num_entries; i++) {
        Entry *entry = menu->items[i];
        if (entry->icon != NULL)
            SDL_DestroyTexture(entry->icon);
        if (entry->icon_selected != NULL)
            SDL_DestroyTexture(entry->icon_selected);
        entry->icon = load_icon(entry->icon_path, size);
        entry->icon_selected = entry->icon_selected_path != NULL ? load_icon(entry->icon_selected_path, size) : NULL;
        if (config.titles_enabled) {
            int h;
            if (entry->title_texture != NULL)
                SDL_DestroyTexture(entry->title_texture);
            entry->title_texture = render_text_texture(entry->title, &title_info, &entry->text_rect, &h);
            entry->title_offset = (config.title_oversize_mode == OVERSIZE_SHRINK && h != line_height)
                                  ? (line_height - h) / 2 : 0;
            if (config.title_oversize_mode != OVERSIZE_NONE && entry->text_rect.w > size)
                log_debug("Menu '%s': the title '%s' is %i px wide, over its %i px button",
                    menu->name, entry->title, entry->text_rect.w, size);
        }
    }

    // Keep no pointer to a cached font: another menu's title size may close it
    title_info.font = fixed_title_font;
    title_info.font_size = (int) config.title_font_size;
    log_debug("Menu '%s': rendered its buttons at %i px", menu->name, size);
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
    geo.font_height = config.titles_enabled ? TTF_FontHeight(fixed_title_font) : 0;

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
    if (fixed_title_font != NULL)
        TTF_CloseFont(fixed_title_font);
    fixed_title_font = NULL;
    title_info.font = NULL;
    title_info.font_size = (int) config.title_font_size;
    if (load_font(&title_info, config.title_font_path, config.title_font_face, FILENAME_DEFAULT_FONT))
        return;   // log_fatal has quit: not even the bundled font opens
    fixed_title_font = title_info.font;
    TTF_Font *probe = TTF_OpenFontIndex(title_info.font_path, TITLE_MEASURE_SIZE, title_info.font_face);
    geo.title_line_pm = probe != NULL ? TTF_FontHeight(probe) * 1000 / TITLE_MEASURE_SIZE : 1500;
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
// area is worked out again first, so a new vertical centre or a clock that moved reaches it
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

// A function to fill the screen with a grey checkerboard. In the settings preview it stands for a
// transparent background: a texture cannot show the desktop through.
static void draw_checkerboard()
{
    int square = geo.screen_height / 18 > 8 ? geo.screen_height / 18 : 8;
    for (int y = 0; y < geo.screen_height; y += square) {
        for (int x = 0; x < geo.screen_width; x += square) {
            Uint8 shade = ((x / square) + (y / square)) % 2 == 0 ? 0x55 : 0x88;
            SDL_Rect cell = { x, y, square, square };
            SDL_SetRenderDrawColor(renderer, shade, shade, shade, 0xFF);
            SDL_RenderFillRect(renderer, &cell);
        }
    }
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
        SDL_RenderCopy(renderer, background_override, NULL, NULL);
    else if (preview && background_shown == BACKGROUND_TRANSPARENT)
        draw_checkerboard();
    else {
        if (background_shown == BACKGROUND_IMAGE || background_shown == BACKGROUND_SLIDESHOW)
            SDL_RenderCopy(renderer, background_texture, NULL, NULL);
        if (background_shown == BACKGROUND_SLIDESHOW && state.slideshow_transition)
            SDL_RenderCopy(renderer, slideshow->transition_texture, NULL, NULL);
    }

    // Draw background overlay
    if (background_overlay != NULL)
        SDL_RenderCopy(renderer, background_overlay, NULL, NULL);

    // Draw scroll indicators: a strip's point left and right from the bottom corners,
    // a grid's are the same arrow turned to point up and down from the top and bottom margins
    if (scroll != NULL) {
        int count = (int) current_menu->num_entries;
        LayoutPosition position = current_menu->position;
        if (layout.rows == 1) {
            if (layout_can_scroll(&layout, count, position, LAYOUT_RIGHT))
                SDL_RenderCopy(renderer, scroll->texture, NULL, &scroll->rect_right);
            if (layout_can_scroll(&layout, count, position, LAYOUT_LEFT))
                SDL_RenderCopyEx(renderer, scroll->texture, NULL, &scroll->rect_left, 0, NULL, SDL_FLIP_HORIZONTAL);
        }
        else {
            if (layout_can_scroll(&layout, count, position, LAYOUT_UP))
                SDL_RenderCopyEx(renderer, scroll->texture, NULL, &scroll->rect_up, 270.0, NULL, SDL_FLIP_NONE);
            if (layout_can_scroll(&layout, count, position, LAYOUT_DOWN))
                SDL_RenderCopyEx(renderer, scroll->texture, NULL, &scroll->rect_down, 90.0, NULL, SDL_FLIP_NONE);
        }
    }

    // Draw clock
    if (clk != NULL) {
        SDL_RenderCopy(renderer, clk->time_texture, NULL, &clk->time_rect);
        if (clk->show_date)
            SDL_RenderCopy(renderer, clk->date_texture, NULL, &clk->date_rect);
    }

    // Draw highlight
    if (highlight != NULL)
        SDL_RenderCopy(renderer,
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
        SDL_RenderCopy(renderer, icon, NULL, &entry->icon_rect);
        if (config.titles_enabled)
            SDL_RenderCopy(renderer, entry->title_texture, NULL, &entry->text_rect);
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
    Uint32 now = SDL_GetTicks();
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
#endif

// A function to show the frame, and without VSync wait out the rest of its time
void present_frame()
{
    SDL_RenderPresent(renderer);

    // Paced unless the renderer gives the VSync wanted. A renderer that keeps VSync on when it is not
    // wanted still gets the delay, which holds the FPS limit: the present waits at most one refresh more.
    bool paced = !vsync_wanted || !vsync_on;
    if (paced) {
        Uint32 elapsed = SDL_GetTicks() - ticks.main;
        if (elapsed < refresh_period)
            SDL_Delay(refresh_period - elapsed);
    }
#ifdef STREAMFLEX_TEST_HOOKS
    test_frame_report(paced);
#endif
}

// A function to update the screen: the scene and the screensaver's dimming, or a blank screen
// while an application is launching
static void draw_screen()
{
    if (!(state.application_launching && config.on_launch == ON_LAUNCH_BLANK)) {
        draw_scene(false);
        if (state.screensaver_active && screensaver != NULL)
            SDL_RenderCopy(renderer, screensaver->texture, NULL, NULL);
    }
    else {
        SDL_RenderClear(renderer);
        SDL_RenderFillRect(renderer, NULL);
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

// A function to initialize the gamepad struct. Only the instance id is kept: a device index moves
// whenever a pad before it goes, so it is looked up again each time the pad is opened.
static void init_gamepad(Gamepad **gamepad, int device_index)
{
    *gamepad = malloc(sizeof(Gamepad));
    **gamepad = (Gamepad) {
        .id = (int) SDL_JoystickGetDeviceInstanceID(device_index),
        .controller = NULL,
        .next = NULL,
        .previous = NULL,
    };
}

// A function to find a pad's device index now, from its instance id; -1 when it is gone
static int gamepad_device_index(const Gamepad *gamepad)
{
    int count = SDL_NumJoysticks();
    for (int i = 0; i < count; i++) {
        if ((int) SDL_JoystickGetDeviceInstanceID(i) == gamepad->id)
            return i;
    }
    return -1;
}

// A function to open the SDL controller, at the pad's device index as it is now
static void open_controller(Gamepad *gamepad, bool raise_error)
{
    int device_index = gamepad_device_index(gamepad);
    if (device_index < 0) {
        log_debug("Gamepad with instance id %i is no longer present, so it is not opened", gamepad->id);
        return;
    }
    gamepad->controller = SDL_GameControllerOpen(device_index);
    if (gamepad->controller == NULL) {
        if (raise_error)
            log_error("Could not open gamepad at device index %i", device_index);
        return;
    }
    log_debug("Gamepad opened at device index %i, instance id %i", device_index,
        (int) SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gamepad->controller)));
    if (config.debug && raise_error) {
        char *mapping = SDL_GameControllerMapping(gamepad->controller);
        log_debug("Gamepad Mapping:\n%s", mapping);
        SDL_free(mapping);
    }
}

// A function to connect gamepad(s): the pad at a device index, added to the list unless its
// instance id is there already, or with -1 every pad in the list. A pad already open is left as it
// is: SDL counts each open, and pre_launch()'s one close would not let it go.
static void connect_gamepad(int device_index, bool open, bool raise_error)
{
    if (device_index >= 0) {
        Gamepad *gamepad = NULL;
        SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(device_index);
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
                SDL_GameControllerClose(i->controller);
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
                i = i->next;
            }
        }
        else
            i = i->next;
    }
}

#ifdef STREAMFLEX_TEST_HOOKS
static SDL_Joystick *test_pad = NULL;   // The harness's virtual gamepad, while attached
static int test_pad_index = -1;
static SDL_JoystickID test_swap[3] = { -1, -1, -1 };   // STREAMFLEX_TEST_PAD_SWAP's pads A, B and C
static SDL_JoystickID test_plug = -1;   // STREAMFLEX_TEST_PAD_PLUG's pad, while it is plugged in
static int test_swap_step = 0;
static bool test_pad_file = false;      // STREAMFLEX_TEST_PAD's file was there last frame
static int test_pad_frames = 0;         // Frames STREAMFLEX_TEST_PAD_FRAMES's press has left

// A function only the headless harness builds: with STREAMFLEX_TEST_PAD set, it attaches the
// virtual gamepad once the gamepad runs
static void test_pad_attach()
{
    if (getenv("STREAMFLEX_TEST_PAD") == NULL || !gamepad_on || test_pad_index >= 0)
        return;
    test_pad_index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                         SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
    test_pad = test_pad_index >= 0 ? SDL_JoystickOpen(test_pad_index) : NULL;
    if (test_pad == NULL)
        log_error("Test hook: no virtual gamepad\n%s", SDL_GetError());
}

// A function only the headless harness builds, to find a joystick's device index from its
// instance id; -1 when it is gone
static int test_pad_device(SDL_JoystickID id)
{
    int count = SDL_NumJoysticks();
    for (int i = 0; i < count; i++) {
        if (SDL_JoystickGetDeviceInstanceID(i) == id)
            return i;
    }
    return -1;
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
        int index = test_pad_device(test_swap[0]);
        if (index >= 0)
            SDL_JoystickDetachVirtual(index);
        test_swap[0] = -1;
        log_debug("Test hook: pad A detached from device index %i", index);
        return;
    }
    int slot = step == 3 ? 2 : step;
    int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                    SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
    if (index < 0) {
        log_error("Test hook: no virtual gamepad %s\n%s", names[slot], SDL_GetError());
        return;
    }
    test_swap[slot] = SDL_JoystickGetDeviceInstanceID(index);
    log_debug("Test hook: pad %s attached at device index %i, instance id %i", names[slot], index, (int) test_swap[slot]);
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
    if (wanted && test_plug < 0) {
        int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                        SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
        if (index < 0) {
            log_error("Test hook: no virtual gamepad to plug in\n%s", SDL_GetError());
            return;
        }
        test_plug = SDL_JoystickGetDeviceInstanceID(index);
        log_debug("Test hook: pad plugged in at device index %i", index);
    }
    else if (!wanted && test_plug >= 0) {
        int index = test_pad_device(test_plug);
        if (index >= 0)
            SDL_JoystickDetachVirtual(index);
        test_plug = -1;
        log_debug("Test hook: pad unplugged");
    }
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
    SDL_GameControllerButton button = name != NULL ? SDL_GameControllerGetButtonFromString(name) : SDL_CONTROLLER_BUTTON_START;
    SDL_GameControllerAxis axis = button == SDL_CONTROLLER_BUTTON_INVALID ? SDL_GameControllerGetAxisFromString(name)
                                                                        : SDL_CONTROLLER_AXIS_INVALID;
    if (test_pad != NULL && button != SDL_CONTROLLER_BUTTON_INVALID)
        SDL_JoystickSetVirtualButton(test_pad, button, pressed ? SDL_PRESSED : SDL_RELEASED);
    else if (test_pad != NULL && axis != SDL_CONTROLLER_AXIS_INVALID)
        SDL_JoystickSetVirtualAxis(test_pad, axis, pressed ? SDL_JOYSTICK_AXIS_MAX : 0);
}

// A function to let the virtual gamepads go before their subsystem stops
static void test_pad_stop()
{
    if (test_pad != NULL)
        SDL_JoystickClose(test_pad);
    if (test_pad_index >= 0)
        SDL_JoystickDetachVirtual(test_pad_index);
    test_pad = NULL;
    test_pad_index = -1;
    // The plugged pad goes with the subsystem; test_pad_plug() plugs it in again once it restarts
    int plugged = test_plug >= 0 ? test_pad_device(test_plug) : -1;
    if (plugged >= 0)
        SDL_JoystickDetachVirtual(plugged);
    test_plug = -1;
    for (int i = 0; i < 3; i++) {
        int index = test_swap[i] >= 0 ? test_pad_device(test_swap[i]) : -1;
        if (index >= 0)
            SDL_JoystickDetachVirtual(index);
        test_swap[i] = -1;
    }
}
#endif

// A function to tell whether the gamepad subsystem is running
bool gamepad_running()
{
    return gamepad_on;
}

// A function to open every game controller present that the Device setting allows, by listing them.
// Starting the subsystem also queues a connect event for each, which then finds the pad open already.
static void connect_present_pads()
{
    int count = SDL_NumJoysticks();
    for (int i = 0; i < count; i++) {
        if (SDL_IsGameController(i) == SDL_TRUE && (config.gamepad_device < 0 || config.gamepad_device == i))
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
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0) {
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
        if (SDL_GameControllerAddMappingsFromFile(config.gamepad_mappings_file) < 0)
            log_error("Could not load gamepad mappings from %s\n%s", config.gamepad_mappings_file, SDL_GetError());
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
        i->repeat = 0;
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
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
                        ? SDL_GameControllerGetButton(pad->controller, (SDL_GameControllerButton) control->index) != 0
                        : (control->type == TYPE_AXIS_POS ? 1 : -1) * SDL_GameControllerGetAxis(pad->controller, (SDL_GameControllerAxis) control->index) > GAMEPAD_DEADZONE;
            if (held)
                return i;
        }
    }
    return -1;
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
                if (value_multiplier*SDL_GameControllerGetAxis(gamepad->controller, i->index) > GAMEPAD_DEADZONE) {
                    i->repeat++;
                    pressed = true;
                    break;
                }
            }

            // Check buttons
            else if (i->type == TYPE_BUTTON) {
                if (SDL_GameControllerGetButton(gamepad->controller, i->index)) {
                    i->repeat++;
                    pressed = true;
                    break;
                }
            }
        }
        if (!pressed) {
            i->repeat = 0;
            continue;
        }

        // Execute command if first press or valid repeat
        if (i->repeat == 1) {
            log_debug("Gamepad %s detected", i->label);
            ticks.last_input = ticks.main;
            execute_command(i->cmd);
        }
        else if (i->repeat == delay_period) {
            ticks.last_input = ticks.main;
            i->repeat -= repeat_period;

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
        if (!SDL_AtomicGet(&state.slideshow_background_rendering) && !SDL_AtomicGet(&state.slideshow_background_ready)) {
            SDL_AtomicSet(&state.slideshow_background_rendering, 1);
            Slideshowhread = SDL_CreateThread(load_next_slideshow_background_async, "Slideshow Thread", (void*) slideshow);
        }

        // Convert background to texture after the rendering thread has completed
        else if (SDL_AtomicGet(&state.slideshow_background_ready)) {
            SDL_WaitThread(Slideshowhread, NULL);
            Slideshowhread = NULL;

            // The loader found no image that loads, or only the one on show: stop the slideshow
            if (slideshow->transition_surface == NULL || slideshow->only_one) {
                SDL_AtomicSet(&state.slideshow_background_ready, 0);
                fall_back_from_slideshow(slideshow->transition_surface);
                return;
            }
            if (config.slideshow_transition_time > 0) {
                slideshow->transition_texture = load_texture(slideshow->transition_surface);
                SDL_SetTextureAlphaMod(slideshow->transition_texture, 0);
                state.slideshow_transition = true;
                log_debug("Slideshow: fading in the next image");
            }
            else {
                SDL_DestroyTexture(background_texture);
                background_texture = load_texture(slideshow->transition_surface);
                background_luminance = slideshow->transition_luminance;
                log_luminance();
                ticks.slideshow_load = ticks.main;
            }
        slideshow->transition_surface = NULL;
        SDL_AtomicSet(&state.slideshow_background_ready, 0);
        }
    }
    else if (state.slideshow_transition) {
        
        // Increase the transparency
        slideshow->transition_alpha += slideshow->transition_change_rate;
        
        // If transition is done, destroy old background and replace it with the new one
        if (slideshow->transition_alpha >= 255.0f) {
            SDL_SetTextureAlphaMod(slideshow->transition_texture, 0xFF);
            slideshow->transition_alpha = 0.0f;
            SDL_DestroyTexture(background_texture);
            background_texture = slideshow->transition_texture;
            slideshow->transition_texture = NULL;
            background_luminance = slideshow->transition_luminance;
            log_luminance();
            state.slideshow_transition = false;
            ticks.slideshow_load = ticks.main;
        }
        else
            SDL_SetTextureAlphaMod(slideshow->transition_texture, (Uint8) slideshow->transition_alpha);
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
        if (background_shown == BACKGROUND_SLIDESHOW && config.screensaver_pause_slideshow)
            state.slideshow_paused = true;
    }
    else {

        // Transition the screen to dark
        if (state.screensaver_transition) {
            screensaver->alpha += screensaver->transition_change_rate;
            if (screensaver->alpha >= screensaver->alpha_end_value) {
                SDL_SetTextureAlphaMod(screensaver->texture, (Uint8) screensaver->alpha_end_value);
                state.screensaver_transition = false;
            }
            else
                SDL_SetTextureAlphaMod(screensaver->texture, (Uint8) screensaver->alpha);
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
        if (!SDL_AtomicGet(&state.clock_rendering)) {

            // Check to see if the time has changed
            get_time(clk);
#ifdef STREAMFLEX_TEST_HOOKS
            // Only the headless harness builds this: with a slow render asked for, render every second
            if (getenv("STREAMFLEX_TEST_CLOCK_DELAY_MS") != NULL)
                clk->render_time = true;
#endif
            if (clk->render_time) {
                SDL_AtomicSet(&state.clock_rendering, 1);
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
        if (SDL_AtomicGet(&state.clock_ready)) {
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
            SDL_AtomicSet(&state.clock_rendering, 0);
            SDL_AtomicSet(&state.clock_ready, 0);
        }
    }
}

static inline void pre_launch()
{
    if (gamepads != NULL)
        disconnect_gamepad(-1, true, false);

// Initialize exit hotkey for Windows
#ifdef _WIN32
    if (has_exit_hotkey())
        SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
#endif
}

static inline void post_launch()
{
    // Rebaseline the timing after the program is done
    ticks.main = SDL_GetTicks();
    ticks.last_input = ticks.main;

    // Post-application updates
    if (gamepad_on)
        connect_gamepad(-1, true, false);
    if (clk != NULL)
        update_clock(true);
    if (background_shown == BACKGROUND_SLIDESHOW)
        resume_slideshow();
    if (config.on_launch == ON_LAUNCH_BLANK)
        set_draw_color();

#ifdef _WIN32
    SDL_EventState(SDL_SYSWMEVENT, SDL_DISABLE);
    if (background_shown == BACKGROUND_TRANSPARENT)
        hide_cursor(current_entry);
#endif
}

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
    exit(status);
}

// A function to print the version and other info to command line
void print_version(FILE *stream)
{
    SDL_version sdl_version;
    SDL_GetVersion(&sdl_version);
    const SDL_version *img_version = IMG_Linked_Version();
    const SDL_version *ttf_version = TTF_Linked_Version();
    fprintf(stream, PROJECT_NAME " version " PROJECT_VERSION ", using:" endline);
    fprintf(stream, "  SDL       %u.%u.%u" endline, sdl_version.major, sdl_version.minor, sdl_version.patch);
    fprintf(stream, "  SDL_image %u.%u.%u" endline, img_version->major, img_version->minor, img_version->patch);
    fprintf(stream, "  SDL_ttf   %u.%u.%u" endline, ttf_version->major, ttf_version->minor, ttf_version->patch);
}

int main(int argc, char *argv[]) 
{
    int error;
    char *config_file_path = NULL;
    config.exe_path = SDL_GetBasePath();
    config_apply_defaults();

    // Handle command line arguments, find config file
    handle_arguments(argc, argv, &config_file_path);

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

    // Initialize SDL, verify all settings are in their allowable range
    init_sdl();
    init_sdl_image();
    init_sdl_ttf();
    refresh_effective();

    // Initialize Nanosvg, create window and renderer
    init_svg();
    create_window();

    // Initialize timing
    ticks.main = SDL_GetTicks();
    ticks.last_input = ticks.main;
    ticks.program_start = ticks.main;

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
        debug_video(renderer, &display_mode);
        debug_settings();
        debug_gamepad(gamepad_controls);
        debug_hotkeys(hotkeys);    
        debug_menu_entries(config.first_menu, config.num_menus);
    }

    // Load the default menu and display it
    error = load_menu(default_menu, false, true);
    if (error)
        log_fatal("Could not load default menu %s", config.default_menu);

    // Execute startup command
    if (config.startup_cmd != NULL)
        execute_command(config.startup_cmd);
    
    // Main program loop
    log_debug("Begin program loop");
    while (1) {
        ticks.main = SDL_GetTicks();
#ifdef STREAMFLEX_TEST_HOOKS
        test_pad_update();
#endif
        while (SDL_PollEvent(&event)) {
            switch(event.type) {
                case SDL_QUIT:
                    quit(EXIT_SUCCESS);
                    break;

                case SDL_KEYDOWN:
                    ticks.last_input = ticks.main;
                    handle_keypress(&event.key.keysym, event.key.repeat != 0);
                    break;

                case SDL_KEYUP:
                    if (settings_is_open())
                        settings_raw_release(event.key.keysym.sym);
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    if (config.mouse_select && !settings_is_open() && event.button.button == SDL_BUTTON_LEFT) {
                        ticks.last_input = ticks.main;
                        execute_command(current_entry->cmd);
                    }
                    break;

                case SDL_JOYDEVICEADDED:
                    if (gamepad_on && SDL_IsGameController(event.jdevice.which) == SDL_TRUE &&
                        (config.gamepad_device < 0 || config.gamepad_device == event.jdevice.which))
                        connect_gamepad(event.jdevice.which, !state.application_running, true);
                    settings_pads_changed();   // The Device row names every pad present, while settings are open
                    break;

                case SDL_JOYDEVICEREMOVED:
                    // `which` is the instance id here: only a pad in the list is removed, whatever Device says
                    log_debug("Gamepad disconnected");
                    disconnect_gamepad(event.jdevice.which, true, true);
                    settings_pads_changed();
                    break;

                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
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
                    }
                    else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                        log_debug("Gained keyboard focus");
                        state.has_focus = true;
                    }
                    else if (event.window.event == SDL_WINDOWEVENT_LEAVE)
                        log_debug("Lost mouse focus");
                    break;
#ifdef _WIN32
                case SDL_SYSWMEVENT:
                    check_exit_hotkey(event.syswm.msg);
                    break;
#endif
            }
        }

        // Update application state
        if (state.application_running && state.has_focus) {
            state.application_running = false;
            post_launch();
            log_debug("Application finished");
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
        if (state.application_launching &&
        ticks.main - ticks.application_launched > config.application_timeout) {
            state.application_launching = false;
            if (config.on_launch == ON_LAUNCH_BLANK)
                set_draw_color();
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
