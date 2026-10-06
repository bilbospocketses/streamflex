#include "layout.h"
#include "derive.h"

// Color masking bit logic
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define RMASK 0xff000000
#define GMASK 0x00ff0000
#define BMASK 0x0000ff00
#define AMASK 0x000000ff
#else
#define RMASK 0x000000ff
#define GMASK 0x0000ff00
#define BMASK 0x00ff0000
#define AMASK 0xff000000
#endif
#define COLOR_MASKS RMASK, GMASK, BMASK, AMASK

// Launcher parameters
#define DEFAULT_REFRESH_RATE 60
#define MIN_FPS_LIMIT 10
#define GAMEPAD_DEADZONE 10000
#define GAMEPAD_REPEAT_DELAY 500
#define GAMEPAD_REPEAT_INTERVAL 25
#define CLOCK_UPDATE_PERIOD 1000
#define SCROLL_INDICATOR_HEIGHT 0.11F
#define SCREEN_MARGIN 0.05F
#define SCREENSAVER_TRANSITION_TIME 1500
#define APPLICATION_WAIT_PERIOD 100
#define TITLE_MIN_SIZE 0.02F       // The readable minimum title size: 2% of the screen height
#define TITLE_MEASURE_SIZE 1000    // Point size a title font's line height is measured at

// Special commands
#define SCMD_SELECT ":select"
#define SCMD_SUBMENU ":submenu"
#define SCMD_FORK ":fork"
#define SCMD_EXIT ":exit"
#define SCMD_LEFT ":left"
#define SCMD_RIGHT ":right"
#define SCMD_UP ":up"
#define SCMD_DOWN ":down"
#define SCMD_HOME ":home"
#define SCMD_BACK ":back"
#define SCMD_QUIT ":quit"
#define SCMD_SHUTDOWN ":shutdown"
#define SCMD_RESTART ":restart"
#define SCMD_SLEEP ":sleep"
#define SCMD_SETTINGS ":settings"

typedef enum {
    MODE_SETTING_BACKGROUND,
    MODE_SETTING_ON_LAUNCH,
    MODE_SETTING_OVERSIZE,
    MODE_SETTING_ALIGNMENT,
    MODE_SETTING_TIME_FORMAT,
    MODE_SETTING_DATE_FORMAT
} ModeSettingType;

typedef enum {
    BACKGROUND_COLOR,
    BACKGROUND_IMAGE,
    BACKGROUND_SLIDESHOW,
    BACKGROUND_TRANSPARENT
} ModeBackground;

typedef enum {
    ON_LAUNCH_BLANK,
    ON_LAUNCH_NONE,
    ON_LAUNCH_QUIT
} ModeOnLaunch;

typedef enum {
    OVERSIZE_TRUNCATE,
    OVERSIZE_SHRINK,
    OVERSIZE_NONE
} ModeOversize;

typedef enum {
    ALIGNMENT_LEFT,
    ALIGNMENT_RIGHT,
} Alignment;

typedef enum {
    FORMAT_TIME_24HR,
    FORMAT_TIME_12HR,
    FORMAT_TIME_AUTO
} TimeFormat;

typedef enum {
    FORMAT_DATE_BIG,
    FORMAT_DATE_LITTLE,
    FORMAT_DATE_AUTO
} DateFormat;

typedef enum {
    TYPE_BUTTON,
    TYPE_AXIS_POS,
    TYPE_AXIS_NEG,
} ControlType;

// Program states
typedef struct {
    bool application_launching;
    bool application_running;
    bool has_focus;
    bool slideshow_transition;
    SDL_atomic_t slideshow_background_rendering; // Both also written by the slideshow's loader thread
    SDL_atomic_t slideshow_background_ready;
    bool slideshow_paused;
    bool screensaver_active;
    bool screensaver_transition;
    SDL_atomic_t clock_rendering;   // Both also written by the clock's render thread
    SDL_atomic_t clock_ready;
} State;

// Timing information
typedef struct {
    Uint32 main;
    Uint32 program_start;
    Uint32 application_launched;
    Uint32 slideshow_load;
    Uint32 last_input;
    Uint32 clock_update;
    Uint32 application_exited;
} Ticks;

// Linked list for menu entries
typedef struct entry {
    char           *title;
    char           *icon_path;
    char           *icon_selected_path;
    char           *cmd;
    SDL_Texture    *icon;
    SDL_Texture    *icon_selected;
    SDL_Rect       icon_rect;
    SDL_Texture    *title_texture;
    SDL_Rect       text_rect;
    int            title_offset;
    struct entry   *next;
    struct entry   *previous;
} Entry;

// Linked list for menus
typedef struct menu {
    char            *name;
    unsigned int    num_entries;
    Entry           *first_entry;
    Entry           **items;          // Entries by index, for the layout maths
    LayoutOverrides overrides;        // Per-menu Rows/Columns/IconSize; 0 = from [Layout]
    LayoutPosition  position;         // Selected entry and scroll position
    int             rendered_size;    // Button size the textures were rendered at; 0 = not yet
    bool            fixed_titles;     // Its percentage title size's font failed to open: the fixed FontSize stands in
    struct menu     *next;
    struct menu     *back;
} Menu;

typedef struct gamepad {
    SDL_GameController *controller;
    int id;                          // The instance id; its device index is looked up when it opens
    struct gamepad *previous;
    struct gamepad *next;
} Gamepad;

// Linked list of gamepad controls
typedef struct gamepad_control {
    ControlType            type;
    int                    index;
    Uint32                 repeat;
    const char             *label;
    char                   *cmd;
    struct gamepad_control *next;
} GamepadControl;

// Linked list of hotkeys
typedef struct hotkey {
    SDL_Keycode   keycode;
    char          *cmd;
    struct hotkey *next;
} Hotkey;

// Struct for the screen geometry every menu shares
typedef struct {
    int screen_width;
    int screen_height;
    int screen_margin;
    int font_height;    // The fixed FontSize's line height
    int title_min_size; // The readable minimum title size, in points
    int title_line_pm;  // The title font's line height per point, in thousandths
    int vcenter; // The VCenter setting in px from the top of the screen
} Geometry;

// Struct for highlight, with the button size and padding its texture was rendered for
typedef struct {
    SDL_Texture *texture;
    SDL_Rect rect;
    int button;
    int hpad;
    int vpad;
    int title_block;
} Highlight;

// Struct for scroll indicators: left and right for a strip, up and down for a grid
typedef struct {
    SDL_Texture *texture;
    SDL_Rect rect_right;
    SDL_Rect rect_left;
    SDL_Rect rect_up;
    SDL_Rect rect_down;
} Scroll;

// Slideshow
typedef struct {
    char **images;
    int *order;
    int i;
    int num_images;
    float transition_alpha;
    float transition_change_rate;
    SDL_Surface *transition_surface;
    SDL_Texture *transition_texture;
    bool only_one;   // Set by the loader: the only image that loads is the one already on show
    double transition_luminance;   // The next image's mean luminance, measured on the loader thread
} Slideshow;

// Screensaver
typedef struct {
    float alpha;
    float alpha_end_value;
    float transition_change_rate;
    SDL_Texture *texture;
} Screensaver;

// Configuration settings: what config.ini says, or the built-in default. Nothing converts them in
// place: the values the launcher draws with are derived from them into `eff` (derive.h).
typedef struct {
    char *default_menu;
    unsigned int max_buttons; // The Columns setting (MaxButtons is its older name)
    unsigned int rows;
    bool vsync;
    int fps_limit;            // -1 when FPSLimit is absent
    Uint32 application_timeout;
    ModeBackground background_mode; // Defines image or color background mode
    SDL_Color background_color; // Background color
    SDL_Color chroma_key_color;
    char *background_image; // Path to background image
    char *slideshow_directory;
    bool background_overlay;
    SDL_Color background_overlay_color; // Its alpha is eff's, from the opacity
    int background_overlay_opacity;     // Hundredths of a percent, as every opacity below
    Uint16 icon_size;
    int icon_spacing;                   // Hundredths of a percent of the screen width, or px
    bool icon_spacing_percent;
    bool titles_enabled;
    char *title_font_path;              // As configured; title_info.font_path is the file opened
    int title_font_face;
    unsigned int title_font_size;
    int title_font_size_pct;    // FontSize as a percentage of the button; 0 = the fixed title_font_size
    SDL_Color title_font_color; // Color struct for title text
    bool title_shadows;
    SDL_Color title_shadow_color;
    int title_opacity;
    ModeOversize title_oversize_mode;
    int title_padding;
    int title_padding_pct;      // Padding as a percentage of the button; 0 = the fixed title_padding
    bool highlight;
    SDL_Color highlight_fill_color;
    SDL_Color highlight_outline_color;
    int highlight_outline_size;
    int highlight_fill_opacity;
    int highlight_outline_opacity;
    int highlight_rx;
    int highlight_vpadding;
    int highlight_hpadding;
    int vcenter;                        // Hundredths of a percent of the screen height
    bool scroll_indicators;
    SDL_Color scroll_indicator_fill_color;
    int scroll_indicator_outline_size;
    SDL_Color scroll_indicator_outline_color;
    int scroll_indicator_opacity;
    bool wrap_entries;
    bool reset_on_back;
    bool mouse_select;
    bool inhibit_os_screensaver;
    char *startup_cmd;
    char *quit_cmd;
    ModeOnLaunch on_launch;
    bool screensaver_enabled;
    Uint32 screensaver_idle_time;
    int screensaver_intensity;
    bool screensaver_pause_slideshow;
    bool gamepad_enabled;
    int gamepad_device;
    char *gamepad_mappings_file;
    bool debug;
    char *exe_path;
    char *config_path; // The file the settings were read from
    Menu *first_menu;
    size_t num_menus;
    bool clock_enabled;
    bool clock_show_date;
    Alignment clock_alignment;
    char *clock_font_path;              // As configured; the clock's text_info.font_path is the file opened
    int clock_font_face;
    int clock_margin;                   // Hundredths of a percent of the screen height, or px
    bool clock_margin_percent;
    SDL_Color clock_font_color;
    int clock_opacity;
    unsigned int clock_font_size;
    bool clock_shadows;
    SDL_Color clock_shadow_color;
    TimeFormat clock_time_format;
    DateFormat clock_date_format;
    bool clock_include_weekday;
    Uint32 slideshow_image_duration;
    Uint32 slideshow_transition_time;
} Config;

void quit_slideshow(void);
void set_draw_color(void);
void quit(int status);
void print_version(FILE *stream);
int compute_menu_layout(const Menu *menu, LayoutGeometry *geometry, char *why, size_t why_size);
void describe_titles(const LayoutGeometry *geometry, char *out, size_t size);

extern ModeBackground background_shown;
extern SDL_Texture *background_override;
extern double background_luminance;              // The image on show's mean luminance; -1 unknown
void draw_scene(bool preview);
void present_frame(void);
void reload_background(void);
void update_slideshow_timing(void);
void reload_titles(void);
void trim_title_fonts(void);
void refresh_layout(void);
int show_menu(Menu *menu);
int show_home(void);
extern Effective eff;
void refresh_effective(void);
void reload_highlight(void);
void reload_scroll(void);
void reload_clock(void);        // Also lays the menu out again: the clock's size moves the buttons
void stop_clock(void);          // Waits for a render in flight; the settings screen stops it before writing a clock setting
void reload_screensaver(void);
void reload_gamepad(void);      // Also the Device setting: closes the pads and opens the chosen one
void reload_title_font(void);   // Closes the size cache and the fixed font, opens the font again, then reload_titles()
void apply_frame_timing(void);  // VSync and FPSLimit, live
void apply_os_screensaver(void);
void apply_default_menu(void);  // Points :home at config.default_menu
bool gamepad_running(void);
int gamepad_pressed_label(void);               // The first control held on any open pad; -1 for none
