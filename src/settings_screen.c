// The settings screen: a narrow column of settings on the left and a live preview of the launcher
// on the right, driven by the remote alone. settings.c holds the pages and the values; this file
// draws them, puts each change into the running launcher, and saves on the way out.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "launcher.h"
#include <launcher_config.h>
#include "settings.h"
#include "config_fields.h"
#include "settings_screen.h"
#include "settings_pickers.h"
#include "settings_fonts.h"
#include "config_save.h"
#include "browser.h"
#include "fileio.h"
#include "alloc.h"
#include "inidoc.h"
#include "image.h"
#include "fontset.h"
#include "util.h"
#include "debug.h"
#include "test_hooks.h"
#include "platform/platform.h"
#include "library.h"
#include "iconmatch.h"
#include "plate.h"
#include "iconfind.h"
#ifndef _WIN32
#include <unistd.h>
#endif
#include "appscan.h"

extern Config config;
extern Geometry geo;
extern SDL_Renderer *renderer;
extern Menu *current_menu;
extern Menu *default_menu;
extern LayoutGeometry layout;
extern TextInfo title_info;

#define MARGIN_RATIO 0.03F         // Of the screen height
#define HEADER_FONT_RATIO 0.045F
#define ROW_FONT_RATIO 0.028F
#define SMALL_FONT_RATIO 0.02F
#define ROWS_TOP_RATIO 0.17F
#define ROW_HEIGHT_RATIO 1.6F      // Of the row font's line height
#define MIN_COLUMN_RATIO 0.20F     // Of the screen width
#define MAX_COLUMN_RATIO 0.32F
#define TEXT_CACHE_SIZE 96
#define NOTE_TEXT_MAX 1600         // Longest note a row shows: a failed save's path and reason
#define PREVIEW_REST_MS 300        // How long a cursor rests before the screen follows it: the Menus list's preview, Mode's window
#define KEYBOARD_TOP_RATIO 0.6F    // The on-screen keyboard covers the screen's bottom 40%, right of the column
#define ELLIPSIS "\xE2\x80\xA6"           // U+2026
#define CHOOSE_APP "\x1F" "app"           // A tile's command list's rows that open something else: the app list...
#define CHOOSE_PROGRAM "\x1F" "program"   // ...the program browser...
#define CHOOSE_TYPED "\x1F" "typed"       // ...and the keyboard
#ifdef _WIN32
#define PROGRAM_WINDOWS true    // The program browser lists .exe and .lnk files, and quotes a path with a space
#else
#define PROGRAM_WINDOWS false   // It lists executables and .desktop files, and quotes what the shell would split
#define ICON_DATA_DIRS 32       // The data folders the Linux icon lookup reads, at most
#endif
#define OWN_ICON_ASKED 256      // The size Windows is asked for an app's image (IShellItemImageFactory)
#define SLOW_KEY_MS 50             // A key that keeps the screen waiting this long is logged
#define ALPHA_VALUE 180            // A row's value
#define ALPHA_DIM 110              // Grayed rows, the page path and the key hint
#define ALPHA_FILL 40              // The highlighted row
#define ALPHA_OUTLINE 220
#define ALPHA_DIVIDER 46
#define ALPHA_FRAME 77             // The preview's outline
#define LEFT_ARROW "\xE2\x80\xB9"  // U+2039, a single left-pointing angle quote
#define RIGHT_ARROW "\xE2\x80\xBA" // U+203A, a single right-pointing angle quote

static const SDL_Color BACKDROP = { 0x0B, 0x16, 0x20, 0xFF };
static const SDL_Color WHITE = { 0xFF, 0xFF, 0xFF, 0xFF };

// A line of text already rendered, kept while it is still being drawn
typedef struct {
    TTF_Font *font;
    char *text;
    SDL_Texture *texture;
    int w;
    int h;
    Uint32 used;
} CachedText;

static SettingsState *model = NULL;   // NULL while settings are closed
static Bindings *bindings = NULL;     // The key and gamepad bindings while settings are open; NULL: unread
static Menu **menus = NULL;           // The launcher's menus, in the model's order
static int menu_count = 0;
static Menu *origin = NULL;           // The menu settings opened over
static MenuEdit *menu_edit = NULL;    // The menus model, read from config.ini as settings open; NULL: unread
static MenuEdit *menus_logged = NULL;   // The menus model as the log last described it; each change is logged against it
static MenuEdit *menus_opened = NULL;   // The menus model as settings opened, which the save's steps are told against
static void log_menu_save(void);
static MenuEdit *menu_snapshot = NULL;   // The menus model as settings opened: what Discard puts back
static MenuEdit *menu_taken = NULL;      // The menus model as the launcher last took it: what a change the
                                         // launcher cannot take (out of memory) goes back to; NULL: none
static char **plated = NULL;             // The icon files plated in this visit (settings_note_plated())...
static int plated_count = 0;             // ...and how many

// The tile whose title the keyboard edits (a new submenu's name needs none: settings_add_submenu() finds its
// menu from the Add page)
static struct {
    int menu;        // The settings' menu the tile is in
    int tile;
    bool submenu;    // The tile opens a menu: its title is the menu's name, renamed at Done only
} typing;
static bool go_home = false;          // Go to the default menu after closing (:home)
static char restart_names[256];       // What the restart prompt offers to apply: "the mappings file"
static SDL_Texture *preview = NULL;   // The scene at full size; NULL when the renderer has no targets
static TTF_Font *font_header = NULL;
static TTF_Font *font_row = NULL;
static TTF_Font *font_small = NULL;
static FontSet *set_header = NULL;    // The three fonts' sets: each font above is its set's, fallbacks attached
static FontSet *set_row = NULL;
static FontSet *set_small = NULL;
static CachedText text_cache[TEXT_CACHE_SIZE];
static Uint32 text_clock = 0;
static int margin = 0;
static int column_width = 0;
static int row_height = 0;
static int first_row = 0;             // The first row shown when a list is longer than the column
static SDL_Rect preview_rect;
static char counted_folder[SETTING_TEXT_MAX]; // The folder the Folder row last counted; "" counts again
static int counted_images = -1;               // Its images; -1 when it could not be listed
static char fitted_source[NOTE_TEXT_MAX];     // The note fit_note() fitted last...
static int fitted_width = -1;                 // ...to this width...
static int fitted_height = -1;                // ...and this height...
static char fitted_note[NOTE_TEXT_MAX];       // ...and what it came to...
static int fitted_note_h = 0;                 // ...and how tall that wraps
#ifdef STREAMFLEX_TEST_HOOKS
static int measures = 0;                      // Paragraphs measured while settings are open
#endif
static char drawn_note[512];                  // The note under the preview last drawn, for the log
static char drawn_cursor[480];                // The row under the cursor as last drawn, for the log
static char logged_path[512];                 // The page path last logged
static char logged_hint[160];                 // The key hint last logged
static Menu *preview_wanted = NULL;           // The menu the preview switches to once the cursor rests...
static Uint32 preview_asked = 0;              // ...since when it has rested
static bool kind_waiting = false;             // Mode changed: the window may need another kind...
static Uint32 kind_asked = 0;                 // ...once Mode's cursor has rested since this step, on Mode
static int shown_first = -1;                  // The rows last on show, for the log
static int shown_last = -1;
static int shown_count = -1;

static Browser *browser = NULL;          // The folder browser, while it is open
static SettingSlot *browser_slot = NULL; // The setting it chooses for: Image, Folder or Mappings file
static BrowserMode browser_mode = BROWSER_IMAGE;   // The mode it was opened in
static int browser_first = 0;            // Its first row on show
static int browser_page = 1;             // How many of its rows fit: Left and Right move this far
static char browser_note[128] = "";      // Why the last OK did nothing, for the caption
static bool browsing_icon = false;       // It chooses the Icon page's tile's image, not a setting's path

// The Icon page (spec "Icons"): the tile it is about, and that app's own icon on the plate, made as the
// page opens and written to a file only when it is chosen (where it came from is Task 10b's icon_from)
static int icon_menu = -1;                      // The tile's menu, the settings' index (menus[]); -1 with no Icon page open
static int icon_tile = -1;                      // ...and its place there, the menus model's
static SDL_Surface *icon_plated = NULL;         // Its own icon on the plate; NULL when it has none
static char icon_previewed[SETTING_TEXT_MAX + 8];   // What the preview's tile shows in place of its icon
                                                    // ("library:<name>", "own", "file:<path>"); "" for its icon
static void show_on_tile(SDL_Surface *surface, const char *what);
static void restore_tile_icon(void);
static void close_icon_page(void);
static void (*browser_chose)(const char *path) = NULL;   // A browser for no setting (programs): its choice goes here
static char program_last[BROWSER_PATH_MAX];              // The program last chosen: the program browser opens on it next
static char icon_from[SETTING_TEXT_MAX + 32];            // Where the last own icon read came from, for the log

// What the app list, the program browser and a tile's command list choose for: a new tile on the Add page,
// or the command of tile `tile` of menu `menu`
static struct {
    bool adding;
    int menu;
    int tile;
} choosing;

// The installed apps, listed on their own thread at most once per visit, from its first Add > App (spec
// "Installed apps")
static AppScan *app_scan = NULL;         // From settings_apps_start() until the list is taken
static AppList *apps = NULL;             // The list, for Add > App; NULL before the start, while listing, or when there is none
static int apps_found = 0;               // Apps listed so far, while the scan runs
static char apps_why[1024] = "";         // Why there is no list, or it is empty
static Uint32 apps_started = 0;          // When the scan started, for the log

// A function to start listing the installed apps, unless this visit has started already: a scan under
// way, a list, or a reason there is none (see settings_screen.h). One that cannot start leaves Add's other
// choices working, and says why.
void settings_apps_start(void)
{
    if (app_scan != NULL || apps != NULL || apps_why[0] != '\0')
        return;
    apps_found = 0;
    apps_started = ticks_ms();
    app_scan = appscan_start();
    if (app_scan == NULL) {
        snprintf(apps_why, sizeof(apps_why), "The apps could not be listed: out of memory, or no thread");
        log_error("Apps: the apps cannot be listed: out of memory, or no thread");
        return;
    }
    log_debug("Apps: listing the installed apps");
}

// A function to see each frame whether the scan is done, and once it is, take its list and say what it
// found. The hook build lists each app, in the list's order.
static void poll_apps(void)
{
    if (app_scan == NULL || !appscan_poll(app_scan, &apps_found))
        return;
    apps = appscan_take(app_scan);
    const char *why = appscan_why(app_scan);
    snprintf(apps_why, sizeof(apps_why), "%s", why != NULL ? why : "");
    appscan_free(app_scan);
    app_scan = NULL;
    Uint32 took = ticks_ms() - apps_started;
    if (apps == NULL)
        log_error("Apps: none listed: %s (%u ms)", apps_why, took);
    else if (applist_count(apps) == 0)
        log_debug("Apps: none listed: %s (%u ms)", apps_why, took);
    else
        log_debug("Apps: found %i apps (%u ms)", applist_count(apps), took);
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: each app, so a check can read the list
    for (int i = 0; apps != NULL && i < applist_count(apps); i++)
        log_debug("Test hook: app %i is %s, run by %s", i + 1, applist_item(apps, i)->name, applist_item(apps, i)->command);
#endif
}

// A function to let go of the apps as settings close: a scan under way is stopped and waited for before
// anything it fills is freed (appscan_free()), then the list goes
static void stop_apps(void)
{
    if (app_scan != NULL) {
        appscan_free(app_scan);
        app_scan = NULL;
        log_debug("Apps: stopped the app scan as settings closed (%u ms)", ticks_ms() - apps_started);
    }
    applist_free(apps);
    apps = NULL;
    apps_found = 0;
    apps_why[0] = '\0';
}

// A function to give the installed apps this visit's scan listed (see settings_screen.h)
const AppList *settings_apps(int *found, const char **why)
{
    if (found != NULL)
        *found = apps_found;
    if (why != NULL)
        *why = apps_why[0] != '\0' ? apps_why : NULL;
    return apps;
}

// The preview's image, decoded on its own thread so moving through a folder never stalls
static SDL_Thread *decode_thread = NULL;
static SDL_AtomicInt decode_done;
static char decode_path[BROWSER_PATH_MAX];  // What the thread is decoding
static SDL_Surface *decode_surface = NULL;  // Its result; NULL when it failed
static char decode_error[256];              // Why it failed, copied on the thread (SDL's error is per thread)
static char wanted_path[BROWSER_PATH_MAX];  // What the preview should show; "" = the real background
static char shown_path[BROWSER_PATH_MAX];   // What background_override holds
static char broken_path[BROWSER_PATH_MAX];  // The last image that could not be decoded
static const char CANNOT_OPEN[] = "This image cannot be opened";

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

// A function to tell whether settings are open
bool settings_is_open(void)
{
    return model != NULL;
}

// A function to get the set a settings font belongs to; NULL for any other font
static FontSet *set_of(TTF_Font *font)
{
    if (font == NULL)
        return NULL;
    if (font == font_row)
        return set_row;
    if (font == font_small)
        return set_small;
    return font == font_header ? set_header : NULL;
}

// A function to measure a line of text, the fallbacks of the characters its font lacks attached first
static int text_width(TTF_Font *font, const char *text)
{
    int w = 0;
    int h = 0;
    fontset_prepare(set_of(font), text);
    if (text[0] != '\0')
        TTF_GetStringSize(font, text, 0, &w, &h);
    return w;
}

// A function to get a line of text as a texture, from the cache when it was drawn recently; its text is prepared, as draw_text() measures it first
static CachedText *cached_text(TTF_Font *font, const char *text)
{
    text_clock++;
    int oldest = 0;
    for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
        CachedText *entry = &text_cache[i];
        if (entry->texture != NULL && entry->font == font && strcmp(entry->text, text) == 0) {
            entry->used = text_clock;
            return entry;
        }
        if (entry->used < text_cache[oldest].used)
            oldest = i;
    }
    CachedText *entry = &text_cache[oldest];
    if (entry->texture != NULL) {
        SDL_DestroyTexture(entry->texture);
        free(entry->text);
        memset(entry, 0, sizeof(*entry));
    }
    SDL_Surface *surface = TTF_RenderText_Blended(font, text, 0, WHITE);
    if (surface == NULL)
        return NULL;
    entry->texture = surface_texture(surface);
    entry->w = surface->w;
    entry->h = surface->h;
    SDL_DestroySurface(surface);
    if (entry->texture == NULL)
        return NULL;
    entry->font = font;
    entry->text = strdup(text);
    if (entry->text == NULL) {
        SDL_DestroyTexture(entry->texture);
        memset(entry, 0, sizeof(*entry));
        return NULL;
    }
    entry->used = text_clock;
    return entry;
}

// A function to empty the text cache; returns how many textures it destroyed
static int clear_text_cache(void)
{
    int count = 0;
    for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
        if (text_cache[i].texture != NULL) {
            SDL_DestroyTexture(text_cache[i].texture);
            count++;
        }
        free(text_cache[i].text);
    }
    memset(text_cache, 0, sizeof(text_cache));
    return count;
}

// A function to draw a line of text cut with "..." to fit; `right` puts its right edge at x
static void draw_text(TTF_Font *font, const char *text, int x, int y, int max_width, Uint8 alpha, bool right)
{
    if (text == NULL || text[0] == '\0' || max_width <= 0)
        return;
    char buffer[SETTING_TEXT_MAX + 16];
    copy_string(buffer, text, sizeof(buffer));
    int w = text_width(font, buffer);
    if (w > max_width)
        utf8_truncate(buffer, w, max_width);
    CachedText *line = cached_text(font, buffer);
    if (line == NULL)
        return;
    SDL_SetTextureAlphaMod(line->texture, alpha);
    SDL_Rect rect = { right ? x - line->w : x, y, line->w, line->h };
    draw_texture(renderer, line->texture, NULL, &rect);
}

// A function to draw a paragraph wrapped to a width; returns its height
static int draw_wrapped(TTF_Font *font, const char *text, int x, int y, int width, Uint8 alpha)
{
    if (text == NULL || text[0] == '\0' || width <= 0)
        return 0;
    fontset_prepare(set_of(font), text);
    SDL_Surface *surface = TTF_RenderText_Blended_Wrapped(font, text, 0, WHITE, width);
    if (surface == NULL)
        return 0;
    int h = surface->h;
    SDL_Texture *texture = surface_texture(surface);
    SDL_Rect rect = { x, y, surface->w, surface->h };
    SDL_DestroySurface(surface);
    if (texture != NULL) {
        SDL_SetTextureAlphaMod(texture, alpha);
        draw_texture(renderer, texture, NULL, &rect);
        SDL_DestroyTexture(texture);
    }
    return h;
}

// A function to name the section a setting lives in: its own, or its menu's
static const char *section_of(const SettingSlot *slot)
{
    return slot->def->section != NULL ? slot->def->section : menus[slot->menu]->name;
}

// A function to read a setting's value from the running launcher
static SettingValue read_value(SettingId id, int menu_index)
{
    return config_read(id, menu_index >= 0 ? menus[menu_index] : NULL);
}

// Each refresh group's name for the log, in the enum's order
static const char *const REFRESH_NAMES[] = { "nothing", "the layout", "the titles", "the background",
    "the title font", "the highlight", "the scroll indicators", "the clock", "the screensaver",
    "the gamepad", "the frame timing" };

// The order Discard runs the groups in (every group but SET_REFRESH_NONE): the title font first, as
// it renders the titles and lays the menu out, and the layout last
static const SettingRefresh REFRESH_ORDER[] = { SET_REFRESH_TITLE_FONT, SET_REFRESH_TITLES, SET_REFRESH_BACKGROUND,
    SET_REFRESH_HIGHLIGHT, SET_REFRESH_SCROLL, SET_REFRESH_CLOCK, SET_REFRESH_SCREENSAVER,
    SET_REFRESH_GAMEPAD, SET_REFRESH_FRAME, SET_REFRESH_LAYOUT };

// The groups end with SET_REFRESH_FRAME, and both lists know every one of them: a new group fails
// these until it is named and ordered
COMPILE_CHECK(refresh_frame_is_the_last_group, SET_REFRESH_FRAME + 1 == SET_REFRESH_COUNT);
COMPILE_CHECK(refresh_names_name_every_group, sizeof(REFRESH_NAMES) / sizeof(REFRESH_NAMES[0]) == SET_REFRESH_COUNT);
COMPILE_CHECK(refresh_order_runs_every_group, sizeof(REFRESH_ORDER) / sizeof(REFRESH_ORDER[0]) == SET_REFRESH_COUNT - 1);

// A function to name a refresh group for the log
static const char *refresh_name(SettingRefresh refresh)
{
    return REFRESH_NAMES[refresh];
}

// A function to name the gamepads present for the Device row, by their device index now (the
// launcher opens them by instance id, so an index is only good at the moment it is listed): every
// joystick SDL lists while the gamepad runs, as SDL names it. A nameless one, or a failed list, is
// the model's to describe ("Pad N", or none).
static void list_pads(void)
{
    const char *names[SETTINGS_MAX_PADS] = { NULL };
    int count = 0;
    SDL_JoystickID *ids = gamepad_running() ? SDL_GetJoysticks(&count) : NULL;
    if (ids == NULL)
        count = 0;
    if (count > SETTINGS_MAX_PADS)
        count = SETTINGS_MAX_PADS;
    for (int i = 0; i < count; i++)
        names[i] = SDL_GetJoystickNameForID(ids[i]);
    settings_set_pads(model, names, count);   // It copies the names
    SDL_free(ids);
}

// A function to name the pads again after one was plugged in or pulled out, while settings are
// open: the Device row, and the Device list when it is open
void settings_pads_changed(void)
{
    if (model == NULL)
        return;
    list_pads();
    pickers_pads_changed();
}

// A function to refresh what a group of settings affects in the running launcher
static void run_refresh(SettingRefresh refresh)
{
    switch (refresh) {
        case SET_REFRESH_NONE:
            return;
        case SET_REFRESH_LAYOUT:
            refresh_layout();
            break;
        case SET_REFRESH_TITLES:
            reload_titles();
            break;
        case SET_REFRESH_BACKGROUND:
            reload_background();
            break;
        case SET_REFRESH_TITLE_FONT:
            reload_title_font();
            break;
        case SET_REFRESH_HIGHLIGHT:
            reload_highlight();
            break;
        case SET_REFRESH_SCROLL:
            reload_scroll();
            break;
        case SET_REFRESH_CLOCK:
            reload_clock();
            break;
        case SET_REFRESH_SCREENSAVER:
            reload_screensaver();
            break;
        case SET_REFRESH_GAMEPAD:
            reload_gamepad();
            list_pads();
            if (bindings != NULL)
                bindings_set_gamepad_on(bindings, gamepad_running());   // Its floor follows it
            break;
        case SET_REFRESH_FRAME:
            apply_frame_timing();
            break;
        case SET_REFRESH_COUNT:   // Not a group
            return;
    }
    log_debug("Settings: refreshed %s", refresh_name(refresh));
}

// A function to put a setting's value into the running launcher, then refresh what it affects.
// The clock is stopped before one of its settings is written. That is defense in depth, not a need:
// the clock's thread reads only its own snapshot (clk), never config, so a render in flight could
// not see the write; reload_clock() would stop it anyway. Three settings act beyond any group: the OS
// screensaver block, the menu :home goes to, and Mode, any value but Transparent letting a Transparent
// that failed be tried again, whether or not the cursor rests on it (the window follows on the rest).
static void apply_slot(const SettingSlot *slot, bool refresh)
{
    if (slot->def->refresh == SET_REFRESH_CLOCK)
        stop_clock();
    config_store(slot->def->id, slot->menu >= 0 ? menus[slot->menu] : NULL, &slot->value);
    refresh_effective();
    if (slot->def->id == SET_ID_INHIBIT_OS_SCREENSAVER)
        apply_os_screensaver();
    else if (slot->def->id == SET_ID_DEFAULT_MENU)
        apply_default_menu();
    else if (slot->def->id == SET_ID_BACKGROUND_MODE)
        window_mode_chosen();
    if (refresh)
        run_refresh(slot->def->refresh);
}

// A function to put every value back into the launcher after Discard, refreshing once each group
// whose settings changed back, in REFRESH_ORDER
static void apply_all(void)
{
    bool due[SET_REFRESH_COUNT];
    memset(due, 0, sizeof(due));
    for (int i = 0; i < settings_slot_count(model); i++) {
        SettingSlot *slot = settings_slot_at(model, i);
        SettingValue now = config_read(slot->def->id, slot->menu >= 0 ? menus[slot->menu] : NULL);
        if (setting_equal(slot->def, &now, &slot->value))
            continue;
        apply_slot(slot, false);
        due[slot->def->refresh] = true;
    }
    for (size_t i = 0; i < sizeof(REFRESH_ORDER) / sizeof(REFRESH_ORDER[0]); i++) {
        if (due[REFRESH_ORDER[i]])
            run_refresh(REFRESH_ORDER[i]);
    }
}

// A function to refresh every group, in REFRESH_ORDER, as Discard would if every setting had changed
void settings_refresh_all(void)
{
    for (size_t i = 0; i < sizeof(REFRESH_ORDER) / sizeof(REFRESH_ORDER[0]); i++)
        run_refresh(REFRESH_ORDER[i]);
}

// A function to refresh what a reload of the whole launcher must make again (a new screen size, a reset
// or new renderer), in REFRESH_ORDER, keeping the screen's state (spec section 3): the gamepad keeps
// running, its pads open, and the screensaver keeps its dim and its transition, only its screen-sized
// layer made again at the new size. The title font stays open (spec section 4): fonts are not the
// renderer's, and the titles group renders the titles again from the sets open, a percentage size a
// new screen needs opened as the layout asks for it. Whether settings are open or not.
void settings_refresh_for_reload(void)
{
    for (size_t i = 0; i < sizeof(REFRESH_ORDER) / sizeof(REFRESH_ORDER[0]); i++) {
        if (REFRESH_ORDER[i] == SET_REFRESH_GAMEPAD || REFRESH_ORDER[i] == SET_REFRESH_TITLE_FONT)
            continue;
        if (REFRESH_ORDER[i] == SET_REFRESH_SCREENSAVER)
            remake_screensaver_texture();
        else
            run_refresh(REFRESH_ORDER[i]);
    }
}

// A function to log a change for -d: "[Section] Key old -> new"
static void log_change(const SettingSlot *slot, const SettingValue *before)
{
    char old_text[SETTING_TEXT_MAX];
    char new_text[SETTING_TEXT_MAX];
    setting_format(slot->def, before, old_text, sizeof(old_text));
    setting_format(slot->def, &slot->value, new_text, sizeof(new_text));
    log_debug("Settings: [%s] %s %s -> %s", section_of(slot), slot->def->key,
        old_text[0] != '\0' ? old_text : "(none)", new_text[0] != '\0' ? new_text : "(none)");
}

// A function to open the screen's own fonts: the bundled default, sized from the screen height.
// An install without it still opens settings, in a font the launcher has already opened: the
// file the titles opened. A bundled font that is there but is not a regular file (a pipe, say, whose
// read would wait for good) is taken as missing, and whichever file is used is opened only if it is
// still a regular file.
static bool open_fonts(void)
{
    char *bundled = find_default_font(FILENAME_DEFAULT_FONT);
    if (bundled != NULL && fileio_not_a_file(bundled)) {
        log_error("Settings: the font %s is %s", bundled, fileio_last_error());
        free(bundled);
        bundled = NULL;
    }
    const char *path = bundled;
    if (path == NULL) {
        path = title_info.font_path;
        if (path == NULL) {
            log_error("Settings cannot open: the font %s is missing, and there is no other font", FILENAME_DEFAULT_FONT);
            return false;
        }
        log_error("Settings: the font %s is missing, so they use %s", FILENAME_DEFAULT_FONT, path);
    }
    float height = (float) geo.screen_height;
    set_header = fontset_open(path, (float) max_int(8, (int) (HEADER_FONT_RATIO * height)), 0, "settings headers");
    set_row = fontset_open(path, (float) max_int(8, (int) (ROW_FONT_RATIO * height)), 0, "settings rows");
    set_small = fontset_open(path, (float) max_int(8, (int) (SMALL_FONT_RATIO * height)), 0, "settings notes");
    font_header = fontset_font(set_header);
    font_row = fontset_font(set_row);
    font_small = fontset_font(set_small);
    if (font_header == NULL || font_row == NULL || font_small == NULL)
        log_error("Settings cannot open: could not open the font %s\n%s", path, SDL_GetError());
    free(bundled);
    return font_header != NULL && font_row != NULL && font_small != NULL;
}

// A function to close the screen's three font sets, each font before its fallbacks (fontset_close()).
// A set that never opened is NULL, which fontset_close() passes over.
static void close_fonts(void)
{
    fontset_close(set_header);
    fontset_close(set_row);
    fontset_close(set_small);
    set_header = NULL;
    set_row = NULL;
    set_small = NULL;
    font_header = NULL;
    font_row = NULL;
    font_small = NULL;
}

// A function to size the column and the preview for this screen. The column is as wide as its
// widest row needs, within limits, and keeps that width while settings are open, so the preview
// never jumps. The preview keeps the screen's shape.
static void measure_layout(void)
{
    static const char *const labels[] = {
        "General", "Background", "Menus", "Titles", "Highlight", "Scroll indicators", "Clock", "Screensaver",
        "Controls", "Discard changes", "Mode", "Color", "Image", "Folder", "Change every", "Fade", "Rows",
        "Columns", "Largest button", "Size", "All menus", "Try again", "Leave without saving", "Use this folder",
        "Default menu", "Wrap around", "Reset on Back", "Mouse select", "Block the OS screensaver", "VSync",
        "FPS limit", "After launching an app", "App timeout", "Startup command", "Quit command",
        "See-through color", "Overlay", "Overlay color", "Overlay opacity", "Icon spacing", "Vertical center",
        "Show titles", "Font", "Opacity", "Shadows", "Shadow color", "Too long", "Padding", "Show",
        "Fill color", "Fill opacity", "Outline size", "Outline color", "Outline opacity", "Corner radius",
        "Vertical padding", "Horizontal padding", "Show date", "Weekday", "Alignment", "Margin", "Time", "Date",
        "On", "Idle time", "Dim level", "Pause slideshow", "Gamepad", "Device", "Mappings file", "Keyboard layout"
    };
    static const char *const values[] = {
        LEFT_ARROW " Transparent " RIGHT_ARROW, LEFT_ARROW " Custom #000000 " RIGHT_ARROW,
        LEFT_ARROW " All menus (1024 px) " RIGHT_ARROW, LEFT_ARROW " Fixed 512 " RIGHT_ARROW,
        "12 \xC3\x97 10 " RIGHT_ARROW, LEFT_ARROW " Keep showing " RIGHT_ARROW,
        LEFT_ARROW " Pad 15 (not connected) " RIGHT_ARROW, LEFT_ARROW " 12.5% " RIGHT_ARROW
    };
    int w = geo.screen_width;
    int h = geo.screen_height;
    margin = (int) (MARGIN_RATIO * (float) h);
    row_height = (int) (ROW_HEIGHT_RATIO * (float) TTF_GetFontHeight(font_row));
    int widest_label = 0;
    int widest_value = 0;
    for (size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); i++)
        widest_label = max_int(widest_label, text_width(font_row, labels[i]));
    for (int i = 0; i < menu_count; i++)
        widest_label = max_int(widest_label, text_width(font_row, menus[i]->name));
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++)
        widest_value = max_int(widest_value, text_width(font_row, values[i]));
    column_width = widest_label + widest_value + 3 * margin;
    column_width = max_int(column_width, (int) (MIN_COLUMN_RATIO * (float) w));
    column_width = min_int(column_width, (int) (MAX_COLUMN_RATIO * (float) w));

    int x = 2 * margin + column_width;
    int available_w = w - x - margin;
    int caption = 2 * TTF_GetFontHeight(font_small);
    int available_h = h - 2 * margin - caption;
    int preview_w = available_w;
    int preview_h = preview_w * h / w;
    if (preview_h > available_h) {
        preview_h = available_h;
        preview_w = preview_h * w / h;
    }
    preview_rect.x = x + (available_w - preview_w) / 2;
    preview_rect.y = (h - preview_h - caption) / 2;
    preview_rect.w = preview_w;
    preview_rect.h = preview_h;
}

// A function to show a menu in the preview, and log how long laying it out (and, the first time
// at a size, rasterizing its icons) took
static void show_preview(Menu *menu)
{
    Uint32 start = ticks_ms();
    show_menu(menu);
    log_debug("Settings: the preview shows menu '%s' (%u ms)", menu->name, ticks_ms() - start);
}

// A function to highlight in the preview the tile the page is about (settings_preview_tile())
static void select_preview_tile(void)
{
    int tile = settings_preview_tile(model);
    if (tile < 0 || current_menu == NULL || tile >= (int) current_menu->num_entries ||
        tile == current_menu->position.selected)
        return;
    select_entry(tile);
}

// A function to show in the preview the menu the page is about: a menu being edited or highlighted, its tile
// highlighted on a Tiles page, else the one settings opened over. An empty menu shows too, with its note
// (plan ruling 2). In the Menus list the preview waits for the cursor to rest (settings_draw() switches it),
// so moving down the list never waits for the icons of every menu on the way.
static void follow_preview(void)
{
    int index = settings_preview_menu(model);
    Menu *want = index >= 0 && index < menu_count ? menus[index] : origin;
    preview_wanted = NULL;
    if (want != current_menu) {
        if (settings_page(model) == SETTINGS_PAGE_MENUS) {
            preview_wanted = want;
            preview_asked = ticks_ms();
            return;
        }
        show_preview(want);
    }
    select_preview_tile();
    outline_entry(settings_moving_tile(model));
}

// A function run on its own thread: decode one image for the preview
static int decode_image(void *data)
{
    UNUSED(data);
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: it slows the decode down, so a key can land during it
    const char *delay = getenv("STREAMFLEX_TEST_DECODE_DELAY_MS");
    if (delay != NULL)
        SDL_Delay((Uint32) atoi(delay));
#endif
    decode_surface = load_image_file(decode_path);   // Other formats than PNG, JPEG and WebP one at a time
    if (decode_surface == NULL)
        copy_string(decode_error, SDL_GetError(), sizeof(decode_error));
    SDL_SetAtomicInt(&decode_done, 1);
    return 0;
}

// A function to start decoding the wanted image, unless another is being decoded already, or it
// is on show, or it already failed
static void start_decode(void)
{
    if (decode_thread != NULL || wanted_path[0] == '\0' || strcmp(wanted_path, shown_path) == 0 || strcmp(wanted_path, broken_path) == 0)
        return;
    copy_string(decode_path, wanted_path, sizeof(decode_path));
    SDL_SetAtomicInt(&decode_done, 0);
    decode_thread = SDL_CreateThread(decode_image, "Preview image", NULL);
}

// A function to ask for an image as the preview's background; "" goes back to the real one
static void want_preview_image(const char *path)
{
    copy_string(wanted_path, path, sizeof(wanted_path));
    if (wanted_path[0] == '\0' && browsing_icon)
        restore_tile_icon();   // The Icon page's browser off an image: the tile shows its own icon again
    if (wanted_path[0] == '\0' && background_override != NULL) {
        SDL_DestroyTexture(background_override);
        background_override = NULL;
        shown_path[0] = '\0';
    }
    start_decode();
}

// A function to say what the caption says while the browser is open: why the last OK did nothing,
// else that the highlighted image cannot be opened, else why the highlighted row is disabled
static const char *browser_caption(void)
{
    const BrowserRow *row = browser_row(browser, browser_cursor(browser));
    if (browser_note[0] != '\0')
        return browser_note;
    if (row != NULL && row->kind == BROWSER_ROW_IMAGE && strcmp(row->path, broken_path) == 0)
        return CANNOT_OPEN;
    return row != NULL && row->why != NULL ? row->why : "";
}

// A function to pick up a finished decode (with `wait`, the one in flight, waiting for it): show
// it if it is still wanted, then start the next
static void poll_decode(bool wait)
{
    if (decode_thread == NULL || (!wait && !SDL_GetAtomicInt(&decode_done)))
        return;
    SDL_WaitThread(decode_thread, NULL);
    decode_thread = NULL;
    if (decode_surface == NULL) {
        copy_string(broken_path, decode_path, sizeof(broken_path));
        log_debug("Settings: could not open %s: %s", decode_path, decode_error);
        if (browser != NULL && strcmp(browser_caption(), CANNOT_OPEN) == 0)
            log_debug("Settings: the caption says %s for %s", CANNOT_OPEN, decode_path);
    }
    else if (strcmp(decode_path, wanted_path) == 0 && browsing_icon) {
        // The Icon page's browser: the image goes on the tile, not behind the menu
        show_on_tile(decode_surface, decode_path);
        snprintf(icon_previewed, sizeof(icon_previewed), "file:%s", decode_path);
    }
    else if (strcmp(decode_path, wanted_path) == 0) {
        SDL_Texture *texture = surface_texture(decode_surface);
        if (texture != NULL) {
            if (background_override != NULL)
                SDL_DestroyTexture(background_override);
            background_override = texture;
            copy_string(shown_path, decode_path, sizeof(shown_path));
            log_debug("Settings: the preview shows %s", shown_path);
        }
    }
    if (decode_surface != NULL)
        SDL_DestroySurface(decode_surface);
    decode_surface = NULL;
    start_decode();
}

// A function to wait out a decode in flight and drop the preview's image
static void stop_decoding(void)
{
    if (decode_thread != NULL) {
        SDL_WaitThread(decode_thread, NULL);
        decode_thread = NULL;
    }
    if (decode_surface != NULL)
        SDL_DestroySurface(decode_surface);
    decode_surface = NULL;
    wanted_path[0] = '\0';
    shown_path[0] = '\0';
    broken_path[0] = '\0';
    if (background_override != NULL)
        SDL_DestroyTexture(background_override);
    background_override = NULL;
}

// A function to list a folder for the browser
static int list_folder(const char *folder, FileioEntry **entries, void *context)
{
    UNUSED(context);
    return fileio_list(folder, entries);
}

// config.ini as it was when the browser opened, for check_path; NULL when it could not be read
static IniDoc *browser_doc = NULL;

// A function to tell the browser whether config.ini can hold a path for its setting, as the save
// will find: the key's own line may keep a comment that leaves less room
static const char *check_path(const char *path, void *context)
{
    const SettingSlot *slot = context;
    if (browser_doc == NULL)
        return inidoc_check(slot->def->key, path);
    return inidoc_check_in(browser_doc, section_of(slot), slot->def->key, path);
}

// A function to preview what the browser's cursor is on: an image, or a folder's first image. File
// mode (a mappings file) previews nothing, not even a folder's first image, so the real background
// shows.
static void preview_highlighted(void)
{
    const BrowserRow *row = browser_row(browser, browser_cursor(browser));
    char path[BROWSER_PATH_MAX] = "";
    if (row != NULL && row->kind == BROWSER_ROW_IMAGE)
        copy_string(path, row->path, sizeof(path));
    else if (row != NULL && browser_mode != BROWSER_FILE &&
             (row->kind == BROWSER_ROW_FOLDER || row->kind == BROWSER_ROW_USE_FOLDER))
        browser_first_image(browser, row->path, path, sizeof(path));
    want_preview_image(path);
}

// A function to log the folder the browser is showing, so the log follows the user through it
static void log_browsing(void)
{
    log_debug("Settings: browsing %s", browser_folder(browser) != NULL ? browser_folder(browser) : "the places");
}

// A function to open the folder browser in a mode at a start, listing folders through `lister` and saying
// why a path cannot be chosen through `check`; the places are fileio's. False, and said, when it cannot open.
static bool start_browser(BrowserMode mode, const char *start, BrowserList lister, BrowserCheck check, void *context)
{
    FileioPlace *places = NULL;
    test_fail("places", true);
    int count = fileio_places(&places);
    test_fail("places", false);
    if (count < 0)
        log_error("Settings: the folder browser has no places: %s", fileio_last_error());
    BrowserPlace *list = calloc((size_t) (count > 0 ? count : 1), sizeof(BrowserPlace));
    for (int i = 0; list != NULL && i < count; i++) {
        list[i].label = places[i].label;
        list[i].path = places[i].path;
        list[i].network = places[i].network;
    }
    const char *why = "out of memory";
    test_fail("browser", true);
    browser = list != NULL ? browser_open(mode, start, list, count, lister, check, context, &why) : NULL;
    test_fail("browser", false);
    free(list);
    fileio_free_places(places, count);
    if (browser == NULL) {
        log_error("Settings: the folder browser cannot open: %s", why);
        return false;
    }
    browser_mode = mode;
    browser_first = 0;
    browser_note[0] = '\0';
    log_browsing();
    preview_highlighted();
    return true;
}

// A function to open the folder browser for an Image, Folder or Mappings file setting, at its current path
static void open_browser(SettingSlot *slot)
{
    // Paths are checked against config.ini as the save will find it; unread, by the key alone
    size_t length = 0;
    char *text = fileio_read_all(config.config_path, &length);
    browser_doc = text != NULL ? inidoc_parse(text, length) : NULL;
    alloc_free(text);
    BrowserMode mode = slot->def->id == SET_ID_BACKGROUND_IMAGE ? BROWSER_IMAGE
                     : slot->def->id == SET_ID_GAMEPAD_MAPPINGS ? BROWSER_FILE : BROWSER_FOLDER;
    if (!start_browser(mode, slot->value.text, list_folder, check_path, slot)) {
        inidoc_free(browser_doc);
        browser_doc = NULL;
        return;
    }
    browser_slot = slot;
    browser_offer_typing(browser);   // 3c: every listing ends with Type a path...
}

// A function to list a folder for the program browser: as the folder browser lists one, with every file that
// is not a program hidden (browser_is_program()), so its rows are folders and programs alone
static int list_programs(const char *folder, FileioEntry **entries, void *context)
{
    UNUSED(context);
    int count = fileio_list(folder, entries);
    for (int i = 0; i < count; i++) {
        FileioEntry *entry = &(*entries)[i];
        if (!entry->is_file || entry->hidden)
            continue;
        bool executable = false;
#ifndef _WIN32
        char path[BROWSER_PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", folder, entry->name);
        executable = access(path, X_OK) == 0;
#endif
        if (!browser_is_program(entry->name, executable, PROGRAM_WINDOWS)) {
            entry->hidden = true;
            log_debug("Settings: the program browser leaves out %s", entry->name);
        }
    }
    return count;
}

// A function to find the library icon a new tile's title matches (iconmatch.c); NULL for none, or when out of
// memory (the tile then takes its own icon or apps, and the Icon page offers the match later)
static const char *matched_icon(const char *title)
{
    int count = library_count();
    LibraryInfo *icons = count > 0 ? calloc((size_t) count, sizeof(LibraryInfo)) : NULL;
    for (int i = 0; icons != NULL && i < count; i++)
        icons[i] = *library_info(i);
    int found = icons != NULL ? iconmatch_find(title, icons, count) : -1;
    free(icons);
    return found >= 0 ? library_info(found)->name : NULL;
}

// A function to say why a program cannot be a tile's command, or NULL: its command cannot be written, or the
// line it would be written in would not fit config.ini (Fable review M13): a tile's Command keeps the tile's
// own title and icon; a new tile takes the file's name as its title and its library match or apps as its
// icon (an own icon's path is weighed again as it is written, and new_tile_icon() takes apps when the line
// cannot hold it). The reasons are the menus model's, which outlive the rows.
static const char *check_program(const char *path, void *context)
{
    UNUSED(context);
    char command[INIDOC_MAX_LINE + 1];
    char name[BROWSER_PATH_MAX];
    char title[MENUEDIT_TITLE_MAX + 1];
    const char *why = browser_program_why(path, PROGRAM_WINDOWS, INIDOC_MAX_LINE);
    if (why != NULL || !browser_program_command(path, PROGRAM_WINDOWS, command, sizeof(command)))
        return why != NULL ? why : "This path can't be written as a command";
    if (!choosing.adding)
        return settings_command_problem(model, choosing.menu, choosing.tile, command);
    browser_program_title(path, name, sizeof(name));
    settings_tile_title(name, title, sizeof(title));
    const char *match = matched_icon(title);
    return menuedit_check_tile(title, match != NULL ? match : LIBRARY_FALLBACK_ICON, command);
}

// A function to open the program browser (spec "Browse for a program ›"): file mode, programs alone, at
// `start` (a program's path, or a folder; "" as the folder browser opens)
static void open_program_browser(const char *start, void (*chosen)(const char *path))
{
    if (!start_browser(BROWSER_FILE, start, list_programs, check_program, NULL))
        return;
    browser_slot = NULL;
    browser_chose = chosen;
}

// A function to close the folder browser, back to the page it opened from (Background, or Gamepad)
static void close_browser(void)
{
    browser_free(browser);
    browser = NULL;
    browser_slot = NULL;
    browser_chose = NULL;
    inidoc_free(browser_doc);
    browser_doc = NULL;
    want_preview_image("");
    browsing_icon = false;
}

// A function to count a folder's images as the browser does (regular files with an image's
// extension, hidden ones left out); -1 when it cannot be listed
static int count_images(const char *folder)
{
    FileioEntry *entries = NULL;
    int count = fileio_list(folder, &entries);
    int images = count < 0 ? -1 : 0;
    for (int i = 0; i < count; i++) {
        if (browser_is_image_file(&entries[i]))
            images++;
    }
    fileio_free_list(entries, count > 0 ? count : 0);
    return images;
}

// A function to add the folder's image count to the Folder row's value, after a middle dot. The
// folder is counted again only when it changes or the Background page opens, never every frame.
static void describe_folder_row(SettingsRow *row)
{
    const char *folder = row->slot->value.text;
    if (folder[0] == '\0')
        return;
    bool recounted = strcmp(folder, counted_folder) != 0;
    if (recounted) {
        copy_string(counted_folder, folder, sizeof(counted_folder));
        counted_images = count_images(folder);
    }
    if (counted_images >= 0) {
        size_t used = strlen(row->value);
        snprintf(row->value + used, sizeof(row->value) - used,
            counted_images == 1 ? " \xC2\xB7 %i image" : " \xC2\xB7 %i images", counted_images);
    }
    if (recounted)
        log_debug("Settings: the Folder row shows %s", row->value);
}

// A function to free what the screen holds while open
static void free_screen(void)
{
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: how many paragraphs were measured to fit them
    log_debug("Test hook: %i paragraphs were measured while settings were open", measures);
    measures = 0;
#endif
    pickers_end();
    stop_apps();
    if (browser != NULL)
        close_browser();
    close_icon_page();
    stop_decoding();
    preview_wanted = NULL;
    kind_waiting = false;
    if (preview != NULL)
        SDL_DestroyTexture(preview);
    preview = NULL;
    clear_text_cache();
    close_fonts();
    settings_free(model);
    model = NULL;
    menuedit_free(menu_edit);
    menu_edit = NULL;
    menuedit_free(menus_logged);
    menus_logged = NULL;
    menuedit_free(menus_opened);
    menus_opened = NULL;
    menuedit_free(menu_snapshot);
    menu_snapshot = NULL;
    for (int i = 0; i < plated_count; i++)
        free(plated[i]);
    free(plated);
    plated = NULL;
    plated_count = 0;
    program_last[0] = '\0';
    menuedit_free(menu_taken);
    menu_taken = NULL;
    bindings_free(bindings);
    bindings = NULL;
    free(menus);
    menus = NULL;
    menu_count = 0;
}

// A function to close settings, back to the menu they opened over, or for :home straight to the
// default menu: one menu is loaded either way
static void close_settings(void)
{
    if (!go_home || show_home() != 0) {
        if (origin != NULL && current_menu != origin)
            show_menu(origin);
    }
    outline_entry(-1);
    free_screen();
    log_debug("Settings closed");
    trim_title_fonts();
    window_follow_background();   // Saved, left or put back, the Mode in force may want another window
#ifdef _WIN32
    retry_exit_hotkey(true);   // Whether or not anything changed: its key may have been let go meanwhile
#endif
}

// The save's notes the menus model gave (apply_menus()), by their place among the result's notes: a tile's
// "changed meanwhile" note says nothing of keys and buttons (log_save_notes())
static bool menu_notes[CONFIG_SAVE_NOTES_SIZE / 2];

// A function to log what the save could not make as asked (config_save's notes, one a line). A change
// written as a new line beside a line changed by hand is not said to have taken effect: the first line
// on a key or button is the one that runs. That is said of the bindings' such notes only, by the kind
// config_save gives each line, not of a removal skipped, nor of a tile's (3c): a tile written as a new
// line beside the hand's is one more tile, which the menu shows.
static void log_save_notes(ConfigSaveResult *result)
{
    int index = 0;
    for (char *line = result->notes; line[0] != '\0'; index++) {
        char *end = strchr(line, '\n');
        if (end != NULL)
            *end = '\0';
        // A line past the kinds config_save gave (none can be, by how it counts) gets no words
        bool from_menus = index < (int) (sizeof(menu_notes) / sizeof(menu_notes[0])) && menu_notes[index];
        if (index < result->note_count && result->note_kinds[index] == CONFIG_NOTE_CHANGED_MEANWHILE && !from_menus)
            log_debug("Settings: not saved as asked: %s; where two lines bind one key or button, the first in the file is the one that runs", line);
        else
            log_debug("Settings: not saved as asked: %s", line);
        if (end == NULL)
            break;
        line = end + 1;
    }
}

// A function to write the menus model into the fresh read of config.ini, as config_save_with() calls it,
// marking the notes it adds as the menus model's
static bool apply_menus(IniDoc *doc, void *context, ConfigSaveResult *result)
{
    int first = result->note_count;
    bool ok = menuedit_apply(context, doc, result);
    for (int i = first; i < result->note_count && i < (int) (sizeof(menu_notes) / sizeof(menu_notes[0])); i++)
        menu_notes[i] = true;
    return ok;
}

// A function to save every changed setting and binding into config.ini; on failure, show why. The
// bindings' list edits are sized to their lists, which holds every edit they can make.
static bool save_changes(void)
{
    log_menu_save();
    int count = settings_slot_count(model);
    bool menus_edited = menu_edit != NULL && menuedit_changed(menu_edit);
    memset(menu_notes, 0, sizeof(menu_notes));   // This save's notes alone
    int list_max = bindings != NULL ? bindings_count(bindings, BINDINGS_KEYBOARD) + bindings_count(bindings, BINDINGS_GAMEPAD) : 0;
    ConfigEdit *edits = calloc((size_t) count, sizeof(ConfigEdit));
    char (*values)[SETTING_TEXT_MAX] = calloc((size_t) count, SETTING_TEXT_MAX);
    ConfigListEdit *lists = calloc((size_t) (list_max > 0 ? list_max : 1), sizeof(ConfigListEdit));
    char (*list_values)[BINDINGS_VALUE_MAX] = calloc((size_t) (list_max > 0 ? list_max : 1), BINDINGS_VALUE_MAX);
    if (edits == NULL || values == NULL || lists == NULL || list_values == NULL) {
        free(edits);
        free(values);
        free(lists);
        free(list_values);
        settings_show_save_failed(model, "Couldn't save: out of memory");
        return false;
    }
    int list_count = 0;
    if (bindings != NULL) {
        list_count = bindings_edits(bindings, BINDINGS_KEYBOARD, lists, list_values, bindings_count(bindings, BINDINGS_KEYBOARD));
        list_count += bindings_edits(bindings, BINDINGS_GAMEPAD, lists + list_count, list_values + list_count,
                                     bindings_count(bindings, BINDINGS_GAMEPAD));
    }
    int n = 0;
    for (int i = 0; i < count; i++) {
        SettingSlot *slot = settings_slot_at(model, i);
        if (!settings_changed(slot))
            continue;
        setting_format(slot->def, &slot->value, values[n], SETTING_TEXT_MAX);
        edits[n].section = section_of(slot);
        edits[n].key = slot->def->key;
        edits[n].alias = slot->def->alias;
        edits[n].value = slot->value.inherit ? NULL : values[n];
        edits[n].placement = slot->def->section != NULL ? INIDOC_AFTER_LAST_KEY : INIDOC_UNDER_HEADER;
        n++;
    }
    ConfigSaveResult result;
    bool ok;
    test_fail("keep", true);
#ifdef __unix__
    // The packaged config cannot be written; the user's own goes where the launcher looks first
    char user_config[MAX_PATH_CHARS + 1];
    char home[MAX_PATH_CHARS + 1];
    bool has_home = home_directory(home, sizeof(home));
    if (has_home)
        join_paths(user_config, sizeof(user_config), 4, home, ".config", EXECUTABLE_TITLE, FILENAME_DEFAULT_CONFIG);
    ok = config_save_with(config.config_path, PATH_CONFIG_SYSTEM, has_home ? user_config : NULL, edits, n, lists, list_count,
                          menus_edited ? apply_menus : NULL, menu_edit, &result);
#else
    ok = config_save_with(config.config_path, NULL, NULL, edits, n, lists, list_count,
                          menus_edited ? apply_menus : NULL, menu_edit, &result);
#endif
    test_fail("keep", false);
    free(edits);
    free(values);
    free(lists);
    free(list_values);
    if (ok) {
        log_debug("Settings saved %i change(s) to %s (backup: %s)", n + list_count, result.path,
            result.backup[0] != '\0' ? result.backup : "none");
        if (result.warning[0] != '\0')
            log_error("Settings saved to %s, but %s", result.path, result.warning);
        log_save_notes(&result);
        if (menus_edited)
            log_debug("Settings: the menus were written to %s", result.path);
        if (strcmp(result.path, config.config_path) != 0) {
            free(config.config_path);
            config.config_path = strdup(result.path);
        }
        return true;
    }
    char message[CONFIG_SAVE_PATH_MAX + 600];
    snprintf(message, sizeof(message), "Couldn't save to %s: %s", result.path, result.why);
    log_error("%s", message);
    settings_show_save_failed(model, message);
    return false;
}

// A function to save and close; nothing is written when nothing changed. A save that wrote a setting
// that applies at next start asks first whether to restart StreamFlex now.
static void save_and_close(void)
{
    if (!settings_any_changed(model) && !settings_menus_edited(model)) {
        log_debug("Settings: nothing changed");
        close_settings();
    }
    else if (save_changes()) {
        if (settings_next_start(model, restart_names, sizeof(restart_names)) == 0)
            close_settings();
        else {
            log_debug("Settings: asking to restart StreamFlex to apply %s", restart_names);
            settings_show_restart(model, restart_names);
        }
    }
}

static void handle_event(const SettingsEvent *event);
static void keep_taken(void);

// A function to tell whether a tile of the menus model uses a file as its icon
static bool icon_in_use(const char *path)
{
    for (int m = 0; menu_edit != NULL && m < menuedit_count(menu_edit); m++) {
        const MenuEditMenu *menu = menuedit_menu(menu_edit, m);
        for (int t = 0; t < menu->count; t++) {
            if (strcmp(menu->tiles[t].icon, path) == 0)
                return true;
        }
    }
    return false;
}

// A function to keep an icon file plated in this visit, which Discard deletes when no tile uses it (spec
// "Icons": StreamFlex deletes a plated file only so). Out of memory it is kept, and the log says so.
void settings_note_plated(const char *path)
{
    char **grown = realloc(plated, (size_t) (plated_count + 1) * sizeof(char*));
    char *copy = strdup(path);
    if (grown != NULL)
        plated = grown;
    if (grown == NULL || copy == NULL) {
        free(copy);
        log_error("Settings: out of memory, so Discard will not delete %s", path);
        return;
    }
    plated[plated_count++] = copy;
}

// A function to delete each icon file plated in this visit that no tile uses now
static void delete_unused_plated(void)
{
    for (int i = 0; i < plated_count; i++) {
        if (plated[i] == NULL || icon_in_use(plated[i]))
            continue;
        if (fileio_remove(plated[i]))
            log_debug("Settings: deleted %s, an icon plated in this visit that no tile uses", plated[i]);
        else
            log_error("Settings: could not delete %s: %s", plated[i], fileio_last_error());
        free(plated[i]);
        plated[i] = NULL;
    }
}

// A function to put the menus back as they were read, for Discard (spec "Saving and applying"): the menus
// model becomes a copy of the snapshot taken as settings opened, and the launcher and the settings follow it.
// Out of memory the menus stay as they are, and the log says so.
static void discard_menus(void)
{
    if (menu_edit != NULL && menu_snapshot != NULL && menuedit_changed(menu_edit)) {
        MenuEdit *copy = menuedit_copy(menu_snapshot);
        MenuEdit *now = menu_edit;
        if (copy == NULL) {
            log_error("Settings: out of memory, so Discard leaves the menus as they are");
            return;
        }
        menu_edit = copy;
        settings_set_menus(model, copy);
        if (!settings_menus_changed()) {
            menu_edit = now;
            settings_set_menus(model, now);
            menuedit_free(copy);
            return;   // settings_menus_changed() said why
        }
        menuedit_free(now);
        keep_taken();
        log_debug("Settings: the menus went back to how they were read");
    }
    delete_unused_plated();
}

// A function to keep a copy of the menus model as the launcher has just taken it, which a later change the
// launcher cannot take goes back to (menus_taken()); out of memory there is none, and the log says so
static void keep_taken(void)
{
    menuedit_free(menu_taken);
    menu_taken = menu_edit != NULL ? menuedit_copy(menu_edit) : NULL;
    if (menu_edit != NULL && menu_taken == NULL)
        log_error("Settings: out of memory, so a menus change the launcher cannot take cannot be undone");
}

// A function to put a change to the menus model into the launcher (settings_menus_changed()). One the
// launcher cannot take (out of memory) is undone in the menus model as well, back to the copy taken when the
// launcher last followed it, so the two never part: a rename living in the menus model alone would file the
// slot edits under the old section with a false "gone" note (Task 4's review). True when the launcher took it.
static bool menus_taken(void)
{
    if (settings_menus_changed()) {
        keep_taken();
        return true;
    }
    if (menu_taken != NULL) {
        menuedit_free(menu_edit);
        menu_edit = menu_taken;
        menu_taken = NULL;
        settings_set_menus(model, menu_edit);
        keep_taken();
        log_error("Settings: the menus change is undone, as the launcher could not take it");
    }
    return false;
}

// A function to put a change to the menus model into the launcher (menus_taken()); after a
// submenu's rename, the bindings and the Startup and Quit commands the model rewrote too
static void menus_follow(bool rebind)
{
    if (!menus_taken())
        return;
    log_debug("Settings: the menus changed");
    if (!rebind)
        return;
    if (bindings != NULL)
        apply_bindings(bindings);
    apply_slot(settings_slot(model, SET_ID_STARTUP_CMD, -1), false);
    apply_slot(settings_slot(model, SET_ID_QUIT_CMD, -1), false);
    log_debug("Settings: the bindings and the Startup and Quit commands follow the renamed menu");
}

// A function to get a tile of the menus model by the settings' menu and the tile's index; NULL for none
static const MenuTile *screen_tile(int menu, int tile)
{
    const MenuEditMenu *m = menu >= 0 && menu < menu_count && menu_edit != NULL
                          ? menuedit_menu(menu_edit, menuedit_find(menu_edit, menus[menu]->name)) : NULL;
    return m != NULL && tile >= 0 && tile < m->count ? &m->tiles[tile] : NULL;
}

#ifndef _WIN32
// A function to read the icon a Linux app's .desktop file names, from a tile's command: the file (any
// ";<action>" after it dropped, as unix.c launches it), read whole, its Icon= whatever its NoDisplay or
// Hidden say (browser_desktop_icon(), Fable review M18). False for a command that is no .desktop file, `why`
// left "", and for a file that cannot be read or names no icon, with why. 13b's Icon page reads it too.
static bool desktop_icon_name(const char *command, char *out, size_t size, char *why, size_t why_size)
{
    char file[SETTING_TEXT_MAX];
    snprintf(file, sizeof(file), "%s", command);
    file[strcspn(file, ";")] = '\0';
    size_t length = strlen(file);
    if (length <= 8 || strcmp(file + length - 8, ".desktop") != 0)
        return false;
    size_t text_length = 0;
    char *text = fileio_read_all(file, &text_length);
    if (text == NULL) {
        snprintf(why, why_size, "%s could not be read: %s", file, fileio_last_error());
        return false;
    }
    bool named = browser_desktop_icon(text, text_length, out, size);
    alloc_free(text);
    if (!named)
        snprintf(why, why_size, "%s names no icon", file);
    return named;
}
#endif

// A function to read an app's own icon by what names it (Windows: its AppsFolder app ID, appicon_image();
// Linux: its Icon= value, looked up as iconfind_path() does over the data folders applist_data_dirs() names:
// $XDG_DATA_HOME, else ~/.local/share; each of $XDG_DATA_DIRS, else /usr/local/share and /usr/share; then
// the Flatpak exports' and Snap's, in that order; an SVG read at 512 px by load_icon_surface(), so it plates
// sharp) and center it on the library's plate (plate_icon(), spec "Icons", choice 2), in memory: it is
// written to a file only when it is used. NULL, with why, when it cannot be found, read or plated (the test
// hook STREAMFLEX_TEST_FAIL=plate fails the plating, which 13b documents with check 85); icon_from says
// where it came from. 13b's Icon page reads its own icon through it too.
static SDL_Surface *plated_icon(const char *named, char *why, size_t size)
{
    SDL_Surface *image = NULL;
#ifdef _WIN32
    snprintf(icon_from, sizeof(icon_from), "the AppsFolder app %s", named);
    image = appicon_image(named, OWN_ICON_ASKED);
#else
    char path[SETTING_TEXT_MAX];
    char home[MAX_PATH_CHARS + 1];
    char folders[4 * SETTING_TEXT_MAX];
    const char *dirs[ICON_DATA_DIRS];
    int count = applist_data_dirs(getenv("XDG_DATA_HOME"), getenv("XDG_DATA_DIRS"),
                                  home_directory(home, sizeof(home)) ? home : NULL, folders, sizeof(folders),
                                  dirs, ICON_DATA_DIRS);
    if (!iconfind_path(named, dirs, count < ICON_DATA_DIRS ? count : ICON_DATA_DIRS, path, sizeof(path))) {
        snprintf(why, size, "its icon %s is in none of the icon folders", named);
        return NULL;
    }
    snprintf(icon_from, sizeof(icon_from), "%s", path);
    image = load_icon_surface(path, PLATE_SIZE);
#endif
    if (image == NULL) {
        snprintf(why, size, "%s could not be read: %s", icon_from, SDL_GetError());
        return NULL;
    }
    bool failing = test_failing("plate");
    SDL_Surface *on_plate = failing ? NULL : plate_icon(image, PLATE_SIZE);
    if (on_plate == NULL)
        snprintf(why, size, "%s could not be put on a plate: %s", icon_from, failing ? "the test hook failed it" : SDL_GetError());
    SDL_DestroySurface(image);
    return on_plate;
}

// A function to choose a new tile's icon as it is added (user ruling 2026-10-16, Fable review I5; spec
// "Icons", in its order): the library icon its title matches; else the app's own icon, plated and written
// as a new PNG in the icons folder (named from the title, never over a file that is there) and listed for
// Discard, when the tile's line can hold its path (weighed first, so a refusal leaves no file); else the
// library's apps. `named` names the app's own icon (the app list's AppItem.icon: Linux its Icon= value,
// Windows its app ID), or is NULL to read it from a .desktop command (Linux: a browsed .desktop file; a
// Windows program browsed has none). `out` gets a library name or the PNG's path; the log says which, and
// why an own icon was not used.
static void new_tile_icon(const char *title, const char *named, const char *command, char *out, size_t size)
{
    const char *match = matched_icon(title);
    if (match != NULL) {
        snprintf(out, size, "%s", match);
        log_debug("Menus: the new tile '%s' takes the library icon %s, its match", title, match);
        return;
    }
    snprintf(out, size, "%s", LIBRARY_FALLBACK_ICON);
    char why[2 * SETTING_TEXT_MAX] = "";
    char value[SETTING_TEXT_MAX] = "";
    if (named != NULL)
        snprintf(value, sizeof(value), "%s", named);
#ifndef _WIN32
    else if (!desktop_icon_name(command, value, sizeof(value), why, sizeof(why)))
        value[0] = '\0';
#endif
    SDL_Surface *on_plate = value[0] != '\0' ? plated_icon(value, why, sizeof(why)) : NULL;
    if (on_plate != NULL) {
        char dir[SETTING_TEXT_MAX];
        char path[SETTING_TEXT_MAX];
        const char *refused = NULL;
        if (!icons_dir(dir, sizeof(dir)))
            snprintf(why, sizeof(why), "the icons folder could not be made: %s", fileio_last_error());
        else if (!icons_new_path(dir, title, path, sizeof(path)))
            snprintf(why, sizeof(why), "no new icon file could be named in %s", dir);
        else if ((refused = menuedit_check_tile(title, path, command)) != NULL)
            snprintf(why, sizeof(why), "its line could not hold %s: %s", path, refused);
        else if (!plate_save(on_plate, path))
            snprintf(why, sizeof(why), "it could not be saved to %s: %s", path, SDL_GetError());
        else {
            settings_note_plated(path);
            snprintf(out, size, "%s", path);
            log_debug("Menus: the new tile '%s' takes its own icon from %s, saved to %s", title, icon_from, path);
        }
        SDL_DestroySurface(on_plate);
        if (why[0] == '\0')
            return;
    }
    if (why[0] != '\0')
        log_debug("Menus: the new tile '%s' has no icon of its own: %s", title, why);
    log_debug("Menus: the new tile '%s' takes the generic icon %s", title, LIBRARY_FALLBACK_ICON);
}

// A function to take an app the app list chose: a new tile (its name the title; its library match, else
// its own icon, else apps, new_tile_icon()), or a tile's new command
static void app_chosen(const AppItem *item)
{
    SettingsEvent event;
    if (choosing.adding) {
        char title[MENUEDIT_TITLE_MAX + 1];
        char icon[SETTING_TEXT_MAX];
        settings_tile_title(item->name, title, sizeof(title));
        new_tile_icon(title, item->icon, item->command, icon, sizeof(icon));
        event = settings_add_tile(model, title, icon, item->command);
    }
    else
        event = settings_set_command(model, choosing.menu, choosing.tile, item->command);
    handle_event(&event);
}

// A function to take a program the program browser chose: a new tile (its file's name the title, its icon
// as new_tile_icon() chooses it), or a tile's new command; the browser opens on it next time
static void program_chosen(const char *path)
{
    char command[INIDOC_MAX_LINE + 1];
    char name[BROWSER_PATH_MAX];
    char title[MENUEDIT_TITLE_MAX + 1];
    char icon[SETTING_TEXT_MAX];
    copy_string(program_last, path, sizeof(program_last));
    if (!browser_program_command(path, PROGRAM_WINDOWS, command, sizeof(command)))
        return;   // check_program() refused every such row
    SettingsEvent event;
    if (choosing.adding) {
        browser_program_title(path, name, sizeof(name));
        settings_tile_title(name, title, sizeof(title));
        new_tile_icon(title, NULL, command, icon, sizeof(icon));
        event = settings_add_tile(model, title, icon, command);
    }
    else
        event = settings_set_command(model, choosing.menu, choosing.tile, command);
    handle_event(&event);
}

// A function to say why a tile cannot run a command as typed (the keyboard's validator)
static const char *command_problem(const char *text, void *context)
{
    UNUSED(context);
    return settings_command_problem(model, choosing.menu, choosing.tile, text);
}

// A function to take a typed command
static void command_typed(const char *text, void *context)
{
    UNUSED(context);
    SettingsEvent event = settings_set_command(model, choosing.menu, choosing.tile, text);
    handle_event(&event);
}

// A function to act on the row a tile's command list chose: the app list, the program browser (at the
// tile's own program when its command names a file, as it is or inside the quotes a browsed path was
// written in), the keyboard, or a special command
static void tile_command_chosen(const char *value)
{
    const MenuTile *tile = screen_tile(choosing.menu, choosing.tile);
    if (tile == NULL)
        return;
    if (strcmp(value, CHOOSE_APP) == 0)
        pickers_open_apps(app_chosen);
    else if (strcmp(value, CHOOSE_PROGRAM) == 0) {
        char path[BROWSER_PATH_MAX];
        bool own = browser_program_path(tile->command, path, sizeof(path)) && fileio_is_file(path);
        open_program_browser(own ? path : program_last, program_chosen);
    }
    else if (strcmp(value, CHOOSE_TYPED) == 0) {
        char label[MENUEDIT_TITLE_MAX + 32];
        snprintf(label, sizeof(label), "Command for %s", tile->title);
        pickers_open_keyboard(label, tile->command, INIDOC_MAX_LINE, command_problem, NULL, command_typed);
    }
    else {
        SettingsEvent event = settings_set_command(model, choosing.menu, choosing.tile, value);
        handle_event(&event);
    }
}

// A function to open a tile's command list (spec "Command ›"): the app list, a browse, the special
// commands (never :select, which no tile may run), then Type...; the cursor on the tile's command, pinned
// first as Custom when no row gives it
static void open_tile_commands(int menu, int tile)
{
    static const char *const specials[] = { SCMD_HOME, SCMD_BACK, SCMD_SETTINGS, SCMD_SLEEP, SCMD_RESTART,
                                            SCMD_SHUTDOWN, SCMD_QUIT };
    const MenuTile *t = screen_tile(menu, tile);
    if (t == NULL)
        return;
    choosing.adding = false;
    choosing.menu = menu;
    choosing.tile = tile;
    char label[LISTPICK_TEXT_MAX];
    ListPick *made = listpick_create();
    bool ok = made != NULL && listpick_add(made, "App", CHOOSE_APP, true, NULL) &&
              listpick_add(made, "Browse for a program", CHOOSE_PROGRAM, true, NULL);
    for (size_t i = 0; ok && i < sizeof(specials) / sizeof(specials[0]); i++) {
        setting_command_label(specials[i], label, sizeof(label));
        ok = listpick_add(made, label, specials[i], true, NULL);
    }
    ok = ok && listpick_add(made, "Type" ELLIPSIS, CHOOSE_TYPED, true, NULL);
    snprintf(label, sizeof(label), "Custom: %s", t->command);
    ok = ok && listpick_select(made, t->command, label);
    if (!ok) {
        listpick_free(made);
        made = NULL;
    }
    pickers_open_list("Command", made, tile_command_chosen);
}

// A function to say why a tile's title, as typed, cannot be written (the keyboard's validator)
static const char *title_problem(const char *text, void *context)
{
    UNUSED(context);
    return settings_title_problem(model, typing.menu, typing.tile, text);
}

// A function to take a typed title
static void title_typed(const char *text, void *context)
{
    UNUSED(context);
    SettingsEvent event = settings_set_title(model, typing.menu, typing.tile, text);
    handle_event(&event);
}

// A function to show a plain tile's title in the preview as it is typed (spec "the preview updates as the
// user types"), once it has rested, and the title it had once the keyboard is canceled. A submenu tile's
// title is its menu's name, which changes at Done only: a rename makes the settings model again.
static void title_follows(const char *text, void *context)
{
    UNUSED(context);
    if (typing.submenu || settings_title_problem(model, typing.menu, typing.tile, text) != NULL)
        return;
    SettingsEvent event = settings_set_title(model, typing.menu, typing.tile, text);
    if (event.kind == SETTINGS_EVENT_MENUS && menus_taken()) {
        log_debug("Settings: the preview shows the title '%s'", text);
        follow_preview();
    }
}

// A function to open the keyboard on a tile's title: "Title for <title>"; a submenu tile's is its menu's
// name, held to a section name's limit
static void open_title_keyboard(int menu, int tile)
{
    const MenuEditMenu *m = menu >= 0 && menu < menu_count && menu_edit != NULL
                          ? menuedit_menu(menu_edit, menuedit_find(menu_edit, menus[menu]->name)) : NULL;
    if (m == NULL || tile < 0 || tile >= m->count)
        return;
    const MenuTile *t = &m->tiles[tile];
    const char *opens = menuedit_submenu_of(t);
    typing.menu = menu;
    typing.tile = tile;
    typing.submenu = opens != NULL && menuedit_find(menu_edit, opens) >= 0;
    char label[MENUEDIT_TITLE_MAX + 32];
    snprintf(label, sizeof(label), "Title for %s", t->title);
    pickers_open_keyboard(label, t->title, typing.submenu ? INIDOC_MAX_SECTION : MENUEDIT_TITLE_MAX, title_problem,
                          NULL, title_typed);
    pickers_keyboard_follow(title_follows, PREVIEW_REST_MS);
}

// A function to say why a new submenu's name, as typed, cannot be written (the keyboard's validator)
static const char *name_problem(const char *text, void *context)
{
    UNUSED(context);
    return settings_name_problem(model, text);
}

// A function to take a typed name: the submenu is made
static void name_typed(const char *text, void *context)
{
    UNUSED(context);
    SettingsEvent event = settings_add_submenu(model, text);
    handle_event(&event);
}

// A function to give the Icon page's tile, in the menus model, found by its menu's name (Task 10b's
// screen_tile(): a name is what the settings and the menus model are both sure of); NULL while no Icon page
// is open
static const MenuTile *icon_page_tile(void)
{
    return icon_menu >= 0 ? screen_tile(icon_menu, icon_tile) : NULL;
}

// A function to give the launcher's menu the Icon page's tile is in, which the preview shows; NULL when
// there is none
static Menu *icon_page_menu(void)
{
    if (icon_page_tile() == NULL)
        return NULL;
    return menu_for(menuedit_menu(menu_edit, menuedit_find(menu_edit, menus[icon_menu]->name)));
}

// A function to show an icon on the Icon page's tile in the preview without choosing it (spec "Icons":
// the preview shows each highlighted icon on the tile): the launcher's entry gets a texture of it, its
// selected icon dropped so this one draws under the highlight too, until the menu's buttons are rendered
// again. `what` names it for the log; the surface stays the caller's.
static void show_on_tile(SDL_Surface *surface, const char *what)
{
    Menu *menu = icon_page_menu();
    if (menu == NULL || surface == NULL || icon_tile >= (int) menu->num_entries)
        return;
    SDL_Texture *texture = surface_texture(surface);
    if (texture == NULL) {
        log_error("Settings: the preview cannot show %s: %s", what, SDL_GetError());
        return;
    }
    Entry *entry = menu->items[icon_tile];
    if (entry->icon != NULL)
        SDL_DestroyTexture(entry->icon);
    if (entry->icon_selected != NULL)
        SDL_DestroyTexture(entry->icon_selected);
    entry->icon = texture;
    entry->icon_selected = NULL;
    log_debug("Menus: the preview shows %s on '%s'", what, entry->title);
}

// A function to show the tile's own icon in the preview again: the menu's buttons are rendered again from
// the launcher's entries, at once when it is on show, else when it next is
static void restore_tile_icon(void)
{
    if (icon_previewed[0] == '\0')
        return;
    icon_previewed[0] = '\0';
    Menu *menu = icon_page_menu();
    if (menu == NULL)
        return;
    menu->rendered_size = 0;
    if (menu == current_menu)
        refresh_layout();
}

// A function to show a library icon on the tile in the preview, rendered from the library's file at the
// button's size
static void preview_library_icon(const char *name)
{
    char key[SETTING_TEXT_MAX + 8];
    snprintf(key, sizeof(key), "library:%s", name);
    if (strcmp(key, icon_previewed) == 0)
        return;
    const char *file = library_lookup(name);
    SDL_Surface *surface = file != NULL ? load_icon_surface(file, layout.button > 0 ? layout.button : PLATE_SIZE) : NULL;
    if (surface == NULL) {
        log_error("Settings: the preview cannot show the library icon %s: %s", name,
            file != NULL ? SDL_GetError() : "the library has no such icon");
        return;
    }
    char what[96];
    snprintf(what, sizeof(what), "the library icon %s", name);
    show_on_tile(surface, what);
    SDL_DestroySurface(surface);
    snprintf(icon_previewed, sizeof(icon_previewed), "%s", key);
}

// A function to show in the preview the icon of the Icon page's row under the cursor: the library icon
// it offers, or the app's own icon; on Library and Browse, the tile's own icon, until the grid's or the
// folder's cursor rests on one
static void preview_icon_row(void)
{
    SettingsRow rows[SETTINGS_MAX_ROWS];
    int count = settings_rows(model, rows, SETTINGS_MAX_ROWS);
    int cursor = settings_cursor(model);
    SettingsAction action = cursor >= 0 && cursor < count ? rows[cursor].action : SETTINGS_ACTION_NONE;
    if (action == SETTINGS_ACTION_ICON_SUGGESTED)
        preview_library_icon(settings_icon_choices(model)->suggested);
    else if (action == SETTINGS_ACTION_ICON_OWN && strcmp(icon_previewed, "own") != 0) {
        show_on_tile(icon_plated, "its own icon");
        snprintf(icon_previewed, sizeof(icon_previewed), "own");
    }
    else if (action != SETTINGS_ACTION_ICON_OWN)
        restore_tile_icon();
}

// A function to find a library icon by its name; NULL when the library has none of that name
static const LibraryInfo *library_named(const char *name)
{
    for (int i = 0; i < library_count(); i++) {
        if (strcmp(library_info(i)->name, name) == 0)
            return library_info(i);
    }
    return NULL;
}

// A function to read a tile's app's own icon and put it on the library's plate (spec "Icons", choice 2),
// in memory: it is written to a file only when it is chosen. What names it comes from the tile's command:
// on Windows the app ID an AppsFolder command launches (applist_appsfolder_id()); on Linux the Icon= of
// the .desktop file it runs, whatever its NoDisplay or Hidden say (Task 10b's desktop_icon_name()). Task
// 10b's plated_icon() then finds, reads and plates it, as it does for a new tile. NULL for a tile that is
// no such app, `why` left "", and for an icon that cannot be found, read or plated, with why. icon_from
// says where it came from.
static SDL_Surface *own_icon(const MenuTile *tile, char *why, size_t size)
{
    char named[SETTING_TEXT_MAX];
#ifdef _WIN32
    if (!applist_appsfolder_id(tile->command, named, sizeof(named)))
        return NULL;
#else
    if (!desktop_icon_name(tile->command, named, sizeof(named), why, size))
        return NULL;
#endif
    return plated_icon(named, why, size);
}

// A function to leave the Icon page: the preview's tile shows its own icon again, and its own icon on the
// plate, never chosen, goes. Nothing happens with no Icon page open.
static void close_icon_page(void)
{
    if (icon_menu < 0)
        return;
    restore_tile_icon();
    if (icon_plated != NULL)
        SDL_DestroySurface(icon_plated);
    icon_plated = NULL;
    icon_menu = -1;
    icon_tile = -1;
}

// A function to find what the Icon page offers for its tile as it opens (spec "Icons"): the library icon
// its title matches (iconmatch_find()), its app's own icon on a plate, and with neither the library's
// generic icon. The model's rows follow, the cursor on the first. The preview's highlight goes to the
// tile, so it is in view while its icons are tried on it. `menu` is the settings' index, as
// settings_icon_tile() gives it.
static void open_icon_page(int menu, int tile)
{
    close_icon_page();
    icon_menu = menu;
    icon_tile = tile;
    const MenuTile *at = icon_page_tile();
    if (at == NULL) {
        icon_menu = -1;
        icon_tile = -1;
        return;
    }
    const char *menu_name = menus[menu]->name;
    char why[2 * SETTING_TEXT_MAX] = "";
    icon_plated = own_icon(at, why, sizeof(why));
    int found = iconmatch_find(at->title, library_info(0), library_count());
    const LibraryInfo *offered = found >= 0 ? library_info(found)
                               : icon_plated == NULL ? library_named(LIBRARY_FALLBACK_ICON) : NULL;
    SettingsIconChoices choices;
    memset(&choices, 0, sizeof(choices));
    choices.own = icon_plated != NULL;
    choices.matched = found >= 0;
    if (offered != NULL) {
        snprintf(choices.suggested, sizeof(choices.suggested), "%s", offered->name);
        snprintf(choices.suggested_title, sizeof(choices.suggested_title), "%s", offered->title);
    }
    if (found >= 0)
        log_debug("Menus: '%s' in '%s': the library match is %s", at->title, menu_name, offered->name);
    else
        log_debug("Menus: '%s' in '%s': no library match", at->title, menu_name);
    if (icon_plated != NULL)
        log_debug("Menus: '%s' in '%s': its own icon is %s", at->title, menu_name, icon_from);
    else if (why[0] != '\0')
        log_debug("Menus: '%s' in '%s': its own icon cannot be used: %s", at->title, menu_name, why);
    if (found < 0 && icon_plated == NULL && offered != NULL)
        log_debug("Menus: '%s' in '%s': no match and no icon of its own, so the generic %s is offered", at->title,
            menu_name, offered->name);
    settings_set_icon_choices(model, &choices);
    Menu *live = icon_page_menu();
    if (live != NULL && live == current_menu && tile < (int) live->num_entries && live->position.selected != tile) {
        live->position.selected = tile;
        refresh_layout();   // apply_layout() scrolls the tile into view
    }
}

// A function to give the Icon page's tile an icon (a library name or an image's path) and go back to its
// page, through the model (13a's settings_set_icon(), fix pass R12): its MENUS event goes to handle_event(),
// whose Task 10a case puts the change into the launcher (settings_menus_changed()), so the preview shows it
// from its file; nothing here calls settings_menus_changed() again. One its line in config.ini could not
// hold is refused: the page stays, and the caption says why (settings_icon_done()). `source` names where it
// came from, for the log.
static bool set_tile_icon(const char *icon, const char *source)
{
    const MenuTile *tile = icon_page_tile();
    if (tile == NULL)
        return false;
    // Copies: `icon` may be the model's, and the tile's strings go when the model changes
    char chosen[SETTING_TEXT_MAX];
    char title[SETTING_TEXT_MAX];
    char menu_name[SETTING_TEXT_MAX];
    snprintf(chosen, sizeof(chosen), "%s", icon);
    snprintf(title, sizeof(title), "%s", tile->title);
    snprintf(menu_name, sizeof(menu_name), "%s", menus[icon_menu]->name);
    SettingsEvent event = settings_set_icon(model, icon_menu, icon_tile, chosen);
    if (event.kind != SETTINGS_EVENT_MENUS) {
        char why[SETTING_TEXT_MAX + 512];   // A copy: settings_icon_done() writes the notice it is given
        snprintf(why, sizeof(why), "%s", settings_notice(model));
        log_debug("Menus: '%s' in '%s' cannot take the icon %s: %s", title, menu_name, chosen, why);
        settings_icon_done(model, why);
        return false;
    }
    log_debug("Menus: '%s' in '%s' now has the icon %s, from %s", title, menu_name, chosen, source);
    icon_previewed[0] = '\0';   // The launcher's menus are made again from the model, this icon with them
    handle_event(&event);
    if (model != NULL)
        settings_icon_done(model, NULL);
    return true;
}

// A function to say why the Icon page's tile cannot take `icon`, or NULL: its line weighed as config.ini has
// it, its comment and spacing counted (Task 3's menuedit_check_set(), Fable review M2), so a row the save
// would refuse is refused before anything is written
static const char *icon_problem(const MenuTile *tile, const char *icon)
{
    return menuedit_check_set(menu_edit, menuedit_find(menu_edit, menus[icon_menu]->name), icon_tile, tile->title, icon,
        tile->command);
}

// A function to tell whether a tile's icon is a plated icon already written to the icons folder `dir`: a
// file there, directly in it, where only an app's own icon is ever written
static bool icon_in_folder(const char *icon, const char *dir)
{
    size_t length = strlen(dir);
    if (strncmp(icon, dir, length) != 0 || (icon[length] != '/' && icon[length] != '\\'))
        return false;
    const char *name = icon + length + 1;
    return name[0] != '\0' && strpbrk(name, "/\\") == NULL && fileio_is_file(icon);
}

// A function to tell whether an image file holds the own icon the Icon page plated as it opened, pixel for
// pixel: its size, and each pixel's color and alpha, a clear pixel by its alpha alone, as Task 9's
// png_roundtrip reads a plated icon back from its PNG (plate_icon() makes ARGB8888). A file that cannot be
// read, or holds another picture (the tile's Command changed since it was written, the app's icon changed,
// or the tile was given another tile's file), does not.
static bool holds_own_icon(const char *path)
{
    if (icon_plated == NULL || icon_plated->format != SDL_PIXELFORMAT_ARGB8888)
        return false;
    SDL_Surface *loaded = load_image_file(path);
    SDL_Surface *argb = loaded != NULL ? SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_ARGB8888) : NULL;
    SDL_DestroySurface(loaded);
    bool same = argb != NULL && argb->w == icon_plated->w && argb->h == icon_plated->h;
    for (int y = 0; same && y < argb->h; y++) {
        const Uint32 *on_file = (const Uint32 *) ((const Uint8 *) argb->pixels + (size_t) y * (size_t) argb->pitch);
        const Uint32 *on_page = (const Uint32 *) ((const Uint8 *) icon_plated->pixels +
                                                  (size_t) y * (size_t) icon_plated->pitch);
        for (int x = 0; same && x < argb->w; x++)
            same = (on_page[x] >> 24) == 0 ? (on_file[x] >> 24) == 0 : on_file[x] == on_page[x];
    }
    SDL_DestroySurface(argb);
    return same;
}

// A function to write the tile's own icon, plated as the page opened it, as a new PNG in the icons folder
// (named from its title, never over a file that is there), list it for Discard, and make it the tile's
// icon. A tile whose icon is already that icon's file (a file directly in that folder holding the same
// plated picture: written as the tile was added, or chosen here before) keeps it: nothing is written and
// nothing changes (the controller's ruling, 2026-10-16). A file there holding another picture is not this
// icon, so a new file is written (Opus re-review P3). A path the tile's line could not hold is refused
// before anything is written (Fable review M15: a file left behind would wait for Discard), and a file
// that cannot be written leaves the tile's icon as it was; the caption says why.
static void save_own_icon(void)
{
    const MenuTile *tile = icon_page_tile();
    char dir[SETTING_TEXT_MAX];
    char path[SETTING_TEXT_MAX];
    char why[SETTING_TEXT_MAX + 512];
    const char *refused = NULL;
    if (tile == NULL || icon_plated == NULL)
        return;
    if (!icons_dir(dir, sizeof(dir)))
        snprintf(why, sizeof(why), "Couldn't make the icons folder: %s", fileio_last_error());
    else if (icon_in_folder(tile->icon, dir) && holds_own_icon(tile->icon)) {
        log_debug("Menus: '%s' in '%s' keeps its own icon %s, written already", tile->title, menus[icon_menu]->name,
            tile->icon);
        settings_icon_done(model, NULL);
        return;
    }
    else if (!icons_new_path(dir, tile->title, path, sizeof(path)))
        snprintf(why, sizeof(why), "Couldn't name a new icon file in %s", dir);
    else if ((refused = icon_problem(tile, path)) != NULL)
        snprintf(why, sizeof(why), "Couldn't use %s as its icon: %s", path, refused);
    else if (!plate_save(icon_plated, path))
        snprintf(why, sizeof(why), "Couldn't save its icon to %s: %s", path, SDL_GetError());
    else {
        log_debug("Menus: saved the own icon of '%s' to %s", tile->title, path);
        settings_note_plated(path);
        set_tile_icon(path, "its own icon");
        return;
    }
    log_debug("Menus: %s", why);
    settings_icon_done(model, why);
}

// The library grid's cursor moved: its icon on the tile in the preview
static void icon_grid_shown(const char *name)
{
    preview_library_icon(name);
}

// The library grid closed: the icon chosen becomes the tile's; none (Back) leaves the Icon page as it was
static void icon_grid_done(const char *name)
{
    if (name != NULL)
        set_tile_icon(name, "the library");
}

// A function to tell the folder browser whether the Icon page's tile can take an image's path as its
// icon: the rule its line in config.ini must keep (icon_problem())
static const char *check_icon_path(const char *path, void *context)
{
    UNUSED(context);
    const MenuTile *tile = icon_page_tile();
    return tile != NULL ? icon_problem(tile, path) : NULL;
}

// The Icon page's folder browser chose an image (Task 10b's browser_chose, called once the browser has
// closed, after handle_browser_command() waited for its decode): it becomes the tile's icon
static void icon_browser_chose(const char *path)
{
    set_tile_icon(path, "an image file");
}

// A function to open the folder browser for the Icon page's tile (*Browse for an image ›*), in image mode,
// at its icon when that is an image's path, else at the first place; each image the cursor rests on is
// shown on the tile in the preview. Task 10b's start_browser() opens it with no slot (fix pass R2): it sets
// the mode, logs the folder and previews the highlighted image.
static void open_icon_browser(void)
{
    const MenuTile *tile = icon_page_tile();
    const char *start = tile != NULL && !library_is_name(tile->icon) ? tile->icon : NULL;
    browsing_icon = true;   // Before it opens: the first image it highlights goes on the tile
    if (!start_browser(BROWSER_IMAGE, start, list_folder, check_icon_path, NULL)) {
        browsing_icon = false;
        return;
    }
    browser_slot = NULL;
    browser_chose = icon_browser_chose;
}

// A function to act on the Icon page's row chosen: the library icon it offers and the app's own icon
// become the tile's; Library and Browse open the grid or the folder browser over the page
static void choose_icon(SettingsAction action)
{
    const SettingsIconChoices *choices = settings_icon_choices(model);
    const MenuTile *tile = icon_page_tile();
    if (action == SETTINGS_ACTION_ICON_SUGGESTED)
        set_tile_icon(choices->suggested, choices->matched ? "the library match" : "the library's generic icon");
    else if (action == SETTINGS_ACTION_ICON_OWN)
        save_own_icon();
    else if (action == SETTINGS_ACTION_ICON_GRID)
        pickers_open_icons(tile != NULL ? tile->icon : NULL, icon_grid_shown, icon_grid_done);
    else if (action == SETTINGS_ACTION_ICON_BROWSE)
        open_icon_browser();
}

// A function to keep the Icon page's tile in step with the pages after every key: the page opened (find
// what it offers), its cursor moved (its row's icon on the tile), or the page left (the tile's own icon
// again, its own icon on the plate let go). While the grid or the browser is open over the page, they
// show their own icons on the tile.
static void follow_icon_page(void)
{
    int menu = -1;
    int tile = -1;
    if (model == NULL || !settings_icon_tile(model, &menu, &tile)) {
        close_icon_page();
        return;
    }
    if (menu != icon_menu || tile != icon_tile || !settings_icon_choices(model)->ready)
        open_icon_page(menu, tile);
    if (!pickers_active() && browser == NULL)
        preview_icon_row();
}

// A function to act on what a key did in the model
static void handle_event(const SettingsEvent *event)
{
    switch (event->kind) {
        case SETTINGS_EVENT_CHANGED:
            log_change(event->slot, &event->before);
            apply_slot(event->slot, true);
            // Each step of Mode starts the rest again, so stepping through Transparent recreates nothing
            if (event->slot->def->id == SET_ID_BACKGROUND_MODE) {
                kind_waiting = true;
                kind_asked = ticks_ms();   // Task 1.2a's 32-bit milliseconds, as preview_asked takes them
            }
            break;
        case SETTINGS_EVENT_DISCARD:
            log_debug("Settings: discarded the changes");
            discard_menus();   // First: the slots are applied over the menus as they were
            apply_all();
            kind_waiting = false;
            window_follow_background();   // The Mode put back may want the window it had
            if (bindings != NULL)
                apply_bindings(bindings);
            pickers_end_probation();   // The change the 10 s were for went with the rest
            break;
        case SETTINGS_EVENT_BROWSE:
            open_browser(event->slot);
            return;
        case SETTINGS_EVENT_CAPTURE:
            pickers_capture(event->device);
            return;
        case SETTINGS_EVENT_PICK_COMMAND:
            pickers_open_binding_command();
            return;
        case SETTINGS_EVENT_BINDINGS:
            apply_bindings(bindings);
            if (event->confirm)
                pickers_probation(event->device, event->code, settings_binding_command(model));
            break;
        case SETTINGS_EVENT_CLOSE:
        case SETTINGS_EVENT_CLOSE_HOME:
            if (event->slot != NULL) {
                log_change(event->slot, &event->before);
                apply_slot(event->slot, true);
            }
            pickers_settle();   // A change still waiting for its key goes back before the save
            go_home = event->kind == SETTINGS_EVENT_CLOSE_HOME;
            save_and_close();
            return;
        case SETTINGS_EVENT_RETRY:
            save_and_close();
            return;
        case SETTINGS_EVENT_LEAVE:
            log_debug("Settings: leaving without saving");
            close_settings();
            return;
        case SETTINGS_EVENT_RESTART:
            restart_streamflex(restart_names);   // Comes back only when the program cannot be found
            close_settings();
            return;
        case SETTINGS_EVENT_CLOSE_SAVED:
            log_debug("Settings: no restart now, so %s waits for the next start", restart_names);
            close_settings();
            return;
        case SETTINGS_EVENT_PICK:
            // OK on a picker row opens its picker (settings_pickers.c): the color picker for a
            // color, the font picker for a font, the list picker for the default menu, the device
            // and a command. The mappings file is a browse row, which
            // SETTINGS_EVENT_BROWSE opens. The rows that step still step with Left and Right.
            pickers_open(event->slot);
            return;
        case SETTINGS_EVENT_MENUS:
            menus_follow(event->rebind);
            break;
        case SETTINGS_EVENT_TYPE_TITLE:
            open_title_keyboard(event->menu, event->tile);
            return;
        case SETTINGS_EVENT_TYPE_NAME:
            pickers_open_keyboard("Name for the new submenu", "", INIDOC_MAX_SECTION, name_problem, NULL, name_typed);
            return;
        case SETTINGS_EVENT_PICK_APP:
            choosing.adding = true;
            pickers_open_apps(app_chosen);
            return;
        case SETTINGS_EVENT_BROWSE_PROGRAM:
            choosing.adding = true;
            open_program_browser(program_last, program_chosen);
            return;
        case SETTINGS_EVENT_PICK_TILE_COMMAND:
            open_tile_commands(event->menu, event->tile);
            return;
        case SETTINGS_EVENT_ICON:
            choose_icon(event->action);
            if (pickers_active() || browser != NULL)
                return;   // The grid or the browser is open over the page: they show their own icons
            break;
        case SETTINGS_EVENT_MOVED:
        case SETTINGS_EVENT_NONE:
            break;
    }
    follow_preview();
}

// A function to turn a special command into one of the screen's keys
static bool to_settings_command(const char *command, SettingsCommand *out)
{
    static const struct {
        const char *name;
        SettingsCommand command;
    } keys[] = {
        { SCMD_UP, SETTINGS_UP }, { SCMD_DOWN, SETTINGS_DOWN }, { SCMD_LEFT, SETTINGS_LEFT },
        { SCMD_RIGHT, SETTINGS_RIGHT }, { SCMD_SELECT, SETTINGS_OK }, { SCMD_BACK, SETTINGS_BACK },
        { SCMD_HOME, SETTINGS_HOME }, { SCMD_SETTINGS, SETTINGS_CLOSE }
    };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        if (MATCH(command, keys[i].name)) {
            *out = keys[i].command;
            return true;
        }
    }
    return false;
}

static char path_why[320];                     // Why a typed path cannot be chosen, written for the keyboard's note
static char path_refused[BROWSER_PATH_MAX];    // The typed path Done last refused, which the keyboard reopened on...
static char path_refused_why[128];             // ...and why: its note says so until the text changes

// A function to say why a typed path cannot be chosen for a path row, or NULL when it can, as it is typed: it
// must not be empty, and config.ini must hold it, as the browser's rows are checked. It never reads the disk
// (Fable review I6): it runs on every change, on the main thread, where a partial network path's server or
// a hung mount could block for seconds; what the path names is checked at Done (path_typed()). The text
// Done just refused keeps its reason here until it is changed.
static const char *path_problem(const char *text, void *context)
{
    SettingSlot *slot = context;
    if (text[0] == '\0')
        return "Type a path";
    const char *why = check_path(text, slot);
    if (why != NULL) {
        snprintf(path_why, sizeof(path_why), "config.ini can't hold this path: %s", why);
        return path_why;
    }
    return path_refused[0] != '\0' && strcmp(text, path_refused) == 0 ? path_refused_why : NULL;
}

// A function to say why a typed path names nothing the row can choose, or NULL when it does: a file; a
// folder of 2 images or more for Folder; for Image an image StreamFlex shows that opens, its decode asked
// for and waited out as the browser's OK waits for the one highlighted (Fable review M10). At Done only.
static const char *path_missing(const char *text, const SettingSlot *slot)
{
    if (slot->def->id == SET_ID_SLIDESHOW_DIRECTORY)
        return !fileio_is_dir(text) ? "No folder is there" : count_images(text) < 2 ? "A slideshow needs 2 or more images" : NULL;
    if (!fileio_is_file(text))
        return "No file is there";
    if (slot->def->id != SET_ID_BACKGROUND_IMAGE)
        return NULL;
    if (!browser_is_image(text))
        return "That is not an image StreamFlex shows (.jpg, .jpeg, .png or .webp)";
    want_preview_image(text);
    if (decode_thread != NULL && strcmp(decode_path, text) != 0)
        poll_decode(true);   // Another image's decode first; this one's starts as it ends
    if (decode_thread != NULL && strcmp(decode_path, text) == 0) {
        log_debug("Settings: Done waited for the decode of %s", text);
        poll_decode(true);
    }
    return strcmp(text, broken_path) == 0 ? CANNOT_OPEN : NULL;
}

static void open_path_keyboard(SettingSlot *slot, const char *initial);

// A function to take a typed path as the browser takes a chosen one, once it names what the row chooses;
// else the keyboard reopens on the same text, its note saying why, over the browser still open
static void path_typed(const char *text, void *context)
{
    SettingSlot *slot = context;
    const char *why = path_missing(text, slot);
    if (why != NULL) {
        copy_string(path_refused, text, sizeof(path_refused));
        copy_string(path_refused_why, why, sizeof(path_refused_why));
        log_debug("Settings: the typed path %s is refused: %s", text, why);
        open_path_keyboard(slot, text);
        return;
    }
    path_refused[0] = '\0';
    close_browser();
    log_debug("Settings: chose %s", text);
    SettingsEvent event = settings_choose(model, slot, text);
    handle_event(&event);
}

// A function to open the keyboard on a path for a path row's setting, the browser staying open under it
static void open_path_keyboard(SettingSlot *slot, const char *initial)
{
    char label[96];
    snprintf(label, sizeof(label), "Path for %s", slot->def->label);
    pickers_open_keyboard(label, initial, BROWSER_PATH_MAX - 1, path_problem, slot, path_typed);
}

// A function to open the keyboard on a path for the browser's setting (3c, "Type a path..."), starting as
// the folder on show with a separator; the browser stays open under it until a path it takes
static void type_path(void)
{
    const BrowserRow *row = browser_row(browser, browser_cursor(browser));
    char initial[BROWSER_PATH_MAX] = "";
    if (row != NULL && row->path[0] != '\0') {
        size_t length = strlen(row->path);
        bool backslash = strchr(row->path, '\\') != NULL && strchr(row->path, '/') == NULL;
        bool ends = fileio_is_separator(row->path[length - 1]);
        snprintf(initial, sizeof(initial), "%s%s", row->path, ends ? "" : backslash ? "\\" : "/");
    }
    path_refused[0] = '\0';
    open_path_keyboard(browser_slot, initial);
}

// A function to act on a key while the folder browser is open: move, page, open, choose or go back
static void handle_browser_command(const char *command)
{
    BrowserCommand key;
    browser_note[0] = '\0';
    if (MATCH(command, SCMD_UP))
        key = BROWSER_UP;
    else if (MATCH(command, SCMD_DOWN))
        key = BROWSER_DOWN;
    else if (MATCH(command, SCMD_LEFT))
        key = BROWSER_PAGE_UP;
    else if (MATCH(command, SCMD_RIGHT))
        key = BROWSER_PAGE_DOWN;
    else if (MATCH(command, SCMD_SELECT))
        key = BROWSER_OK;
    else if (MATCH(command, SCMD_BACK))
        key = BROWSER_BACK;
    else if (MATCH(command, SCMD_HOME) || MATCH(command, SCMD_SETTINGS)) {
        // Leave the browser without choosing, then close settings as the pages would
        close_browser();
        SettingsEvent event = settings_command(model, MATCH(command, SCMD_HOME) ? SETTINGS_HOME : SETTINGS_CLOSE);
        handle_event(&event);
        return;
    }
    else {
        log_debug("Settings: ignoring '%s' while settings are open", command);
        return;
    }

    // The folder on show before the key (NULL for the places), to log a move into another
    char *folder = browser_folder(browser) != NULL ? strdup(browser_folder(browser)) : NULL;
    test_fail("command", true);
    BrowserResult result = browser_command(browser, key, browser_page);
    test_fail("command", false);
    if (result == BROWSER_CLOSED) {
        free(folder);
        close_browser();
        return;
    }
    if (result == BROWSER_TYPE) {
        free(folder);
        type_path();
        return;
    }
    const char *now = browser_folder(browser);
    if ((folder == NULL) != (now == NULL) || (folder != NULL && strcmp(folder, now) != 0))
        log_browsing();
    free(folder);
    if (result == BROWSER_CHOSEN) {
        char chosen[BROWSER_PATH_MAX];
        copy_string(chosen, browser_chosen(browser), sizeof(chosen));
        if (browser_slot == NULL) {
            if (browsing_icon) {
                // The Icon page's image, as the Image setting's: OK can come before its decode has
                // finished, so wait for it to know whether it opens (an image highlighted before may still
                // be decoding, with this one next); one that cannot open stays, with the browser's note
                if (decode_thread != NULL && strcmp(decode_path, chosen) != 0)
                    poll_decode(true);
                if (decode_thread != NULL && strcmp(decode_path, chosen) == 0) {
                    log_debug("Settings: OK waited for the decode of %s", chosen);
                    poll_decode(true);
                }
                if (strcmp(chosen, broken_path) == 0) {
                    snprintf(browser_note, sizeof(browser_note), "%s", CANNOT_OPEN);
                    log_debug("Settings: %s: %s", browser_note, chosen);
                    return;
                }
            }
            // A browser for no setting: what it chose goes to whoever opened it, once it has closed
            void (*done)(const char *path) = browser_chose;
            close_browser();
            log_debug("Settings: chose %s", chosen);
            if (done != NULL)
                done(chosen);
            return;
        }
        if (browser_slot->def->id == SET_ID_BACKGROUND_IMAGE) {
            // OK can come before the image's decode has finished: wait for it, to know whether it
            // opens. An image highlighted before may still be decoding, with this one next.
            if (decode_thread != NULL && strcmp(decode_path, chosen) != 0)
                poll_decode(true);
            if (decode_thread != NULL && strcmp(decode_path, chosen) == 0) {
                log_debug("Settings: OK waited for the decode of %s", chosen);
                poll_decode(true);
            }
        }
        // Only an image must open: a mappings file that is also an image which failed to decode is
        // still a file
        if (browser_slot->def->id == SET_ID_BACKGROUND_IMAGE && strcmp(chosen, broken_path) == 0) {
            snprintf(browser_note, sizeof(browser_note), "%s", CANNOT_OPEN);
            log_debug("Settings: %s: %s", browser_note, chosen);
            return;
        }
        SettingSlot *slot = browser_slot;
        close_browser();
        log_debug("Settings: chose %s", chosen);
        SettingsEvent event = settings_choose(model, slot, chosen);
        handle_event(&event);
        return;
    }
    if (key == BROWSER_OK && result == BROWSER_NONE) {
        // A disabled row says why; a folder or place that could not be listed says what went wrong
        const BrowserRow *row = browser_row(browser, browser_cursor(browser));
        if (row != NULL && row->why != NULL)
            snprintf(browser_note, sizeof(browser_note), "%s", row->why);
        else if (row != NULL && (row->kind == BROWSER_ROW_FOLDER || row->kind == BROWSER_ROW_PLACE))
            snprintf(browser_note, sizeof(browser_note), "Can't open %s: %s", row->name,
                browser_why(browser) != NULL ? browser_why(browser) : fileio_last_error());
        if (browser_note[0] != '\0')
            log_debug("Settings: %s", browser_note);
    }
    preview_highlighted();
}

// A function to log the column's path when it changes: the page's, or with a picker open, the
// setting it chooses for after it
static void log_path(void)
{
    char path[512];
    if (pickers_active())
        pickers_path(path, sizeof(path));
    else
        settings_path(model, path, sizeof(path));
    if (strcmp(path, logged_path) != 0) {
        copy_string(logged_path, path, sizeof(logged_path));
        log_debug("Settings: page %s", path);
    }
}

// A function to tell whether the cursor is on the page's Mode row
static bool cursor_on_mode(void)
{
    SettingsRow rows[SETTINGS_MAX_ROWS];
    int count = min_int(settings_rows(model, rows, SETTINGS_MAX_ROWS), SETTINGS_MAX_ROWS);
    int cursor = settings_cursor(model);
    return cursor >= 0 && cursor < count && rows[cursor].slot != NULL &&
           rows[cursor].slot->def->id == SET_ID_BACKGROUND_MODE;
}

// A function to give up the Mode row's rest once a key has taken the cursor off the row: the
// recreation the rest may bring reflows the screen, which closes an open picker or the folder browser.
// Every picker and the browser open from a row other than Mode, so none can open before the rest is
// given up. The window then follows Mode when settings close, when Discard puts Mode back, or at the
// rest after Mode's next step.
static void follow_mode_rest(void)
{
    if (!kind_waiting || cursor_on_mode())
        return;
    kind_waiting = false;
    log_debug("Settings: the cursor left Mode before it rested, so the window keeps its kind for now");
}

// A function to act on a key while settings are open: the remote's keys move through them (or
// through a picker, while one is open), and every other command waits until they close
static void handle_command(const char *command)
{
    SettingsCommand key;
    if (pickers_active()) {
        // Home or the settings key with the keyboard over the folder browser leaves the browser too, as the
        // browser's own Home does, so a restart question or a failed save shows instead of its rows
        if (browser != NULL && pickers_keyboard_open() && (MATCH(command, SCMD_HOME) || MATCH(command, SCMD_SETTINGS)))
            close_browser();
        pickers_command(command);
        if (model != NULL)
            log_path();
        return;
    }
    if (browser != NULL) {
        handle_browser_command(command);
        return;
    }
    if (!to_settings_command(command, &key)) {
        log_debug("Settings: ignoring '%s' while settings are open", command);
        return;
    }
    SettingsPage before = settings_page(model);
    settings_set_grid(model, layout.rows, layout.columns);   // Move steps through the grid as the preview lays it out
    SettingsEvent event = settings_command(model, key);
    handle_event(&event);
    if (model == NULL)
        return;
    follow_mode_rest();
    if (before != SETTINGS_PAGE_BACKGROUND && settings_page(model) == SETTINGS_PAGE_BACKGROUND)
        counted_folder[0] = '\0';   // The Background page opened: count the Folder's images again
    log_path();
}

// A function to act on a special command while settings are open, logging a key that kept the
// screen waiting. A special command is read by its first word, as execute_command() and the bindings'
// floor read it (":select now" is OK). The command is copied first: it may be a hotkey's or a
// control's, whose list a binding change rebuilds.
void settings_handle_command(const char *command)
{
    if (model == NULL)
        return;
    char text[SETTING_TEXT_MAX];
    snprintf(text, sizeof(text), "%s", command);
    if (text[0] == ':')
        text[strcspn(text, " ")] = '\0';
    Uint32 start = ticks_ms();
    handle_command(text);
    follow_icon_page();   // The Icon page opened, moved or left; it closes with settings too
    Uint32 took = ticks_ms() - start;
    if (took >= SLOW_KEY_MS)
        log_debug("Settings: '%s' kept the screen waiting %u ms", text, took);
}

// A function to take a key before anything else does while settings are open: a capture's, or the
// 10 s's; true when it was taken
bool settings_raw_key(int code, bool repeat)
{
    return model != NULL && pickers_raw_key(code, repeat);
}

// A function to take a key's release while settings are open
void settings_raw_release(int code)
{
    if (model != NULL)
        pickers_raw_release(code);
}

// A function to take the pad's state each frame while settings are open; true when it was taken
bool settings_raw_pad(int label)
{
    return model != NULL && pickers_raw_pad(label);
}

// A function to take a real keyboard's key before anything else while settings are open: the keyboard's
// typing, or a printing key that opens it on a list's filter (settings_pickers.c); true when it was taken
bool settings_text_key(SDL_Scancode scancode, SDL_Keymod mod, bool repeat)
{
    return model != NULL && pickers_text_key(scancode, mod, repeat);
}

// A function to tell how tall a paragraph wraps to a width
static int wrapped_height(TTF_Font *font, const char *text, int width)
{
#ifdef STREAMFLEX_TEST_HOOKS
    measures++;   // Only the headless harness builds this: free_screen() logs the count
#endif
    fontset_prepare(set_of(font), text);
    SDL_Surface *surface = TTF_RenderText_Blended_Wrapped(font, text, 0, WHITE, width);
    if (surface == NULL)
        return 0;
    int h = surface->h;
    SDL_DestroySurface(surface);
    return h;
}

// A function to keep `keep` bytes of a note, half from its start and half from its end, with "..."
// between them, never splitting a UTF-8 character
static void cut_middle(const char *note, size_t keep, char *out, size_t size)
{
    size_t length = strlen(note);
    size_t head = keep / 2;
    size_t tail = length - (keep - head);
    while (head > 0 && ((unsigned char) note[head] & 0xC0) == 0x80)
        head--;
    while (tail < length && ((unsigned char) note[tail] & 0xC0) == 0x80)
        tail++;
    snprintf(out, size, "%.*s...%s", (int) head, note, note + tail);
}

// A function to fit a note into a height, cutting it in the middle, where a long path sits, so its
// start (what failed) and its end (why) stay. The last note fitted is remembered with its height
// (fitted_note_h), so a note is measured once, not every frame.
static const char *fit_note(const char *note, int width, int max_height)
{
    if (strcmp(note, fitted_source) == 0 && width == fitted_width && max_height == fitted_height)
        return fitted_note;
    copy_string(fitted_source, note, sizeof(fitted_source));
    fitted_width = width;
    fitted_height = max_height;
    copy_string(fitted_note, note, sizeof(fitted_note));
    fitted_note_h = wrapped_height(font_small, note, width);
    if (fitted_note_h <= max_height)
        return fitted_note;
    size_t fits = 0;                  // Bytes kept that are known to fit...
    size_t too_many = strlen(note);   // ...and known not to
    int fits_h = 0;                   // ...and the height that fitted
    while (too_many - fits > 1) {
        size_t keep = (fits + too_many) / 2;
        cut_middle(note, keep, fitted_note, sizeof(fitted_note));
        int h = wrapped_height(font_small, fitted_note, width);
        if (h <= max_height) {
            fits = keep;
            fits_h = h;
        }
        else
            too_many = keep;
    }
    cut_middle(note, fits, fitted_note, sizeof(fitted_note));
    fitted_note_h = fits > 0 ? fits_h : wrapped_height(font_small, fitted_note, width);
    log_debug("Settings: the note was cut in the middle to fit the column");
    return fitted_note;
}

// A function to tell how tall a row draws: a note as tall as its fitted text, as draw_row() draws
// it, any other row one row_height
static int row_drawn_height(const SettingsRow *row, int note_room)
{
    if (row->kind != SETTINGS_ROW_NOTE)
        return row_height;
    fit_note(row->note, column_width - 2 * (margin / 2), note_room);
    return max_int(row_height, fitted_note_h + row_height / 2);
}

// A function to write what a row shows on its right: under the cursor, Left and Right arrows round
// the value of a row they step (the model says which) while it is not grayed; the › marker after
// any row OK opens (a page, the browser, a picker, a binding, a binding's command, the Icon page's Library
// and Browse); else the value alone
static void row_value_text(const SettingsRow *row, bool highlighted, char *out, size_t size)
{
    if (row->enabled && highlighted && row->steps)
        snprintf(out, size, LEFT_ARROW " %s " RIGHT_ARROW, row->value);
    else if (row->kind == SETTINGS_ROW_LINK || row->kind == SETTINGS_ROW_BROWSE || row->kind == SETTINGS_ROW_PICK ||
             row->kind == SETTINGS_ROW_BINDING ||
             (row->kind == SETTINGS_ROW_ACTION &&
              (row->action == SETTINGS_ACTION_BIND_COMMAND || row->action == SETTINGS_ACTION_ADD_SUBMENU ||
               row->action == SETTINGS_ACTION_ADD_APP || row->action == SETTINGS_ACTION_ADD_PROGRAM ||
               row->action == SETTINGS_ACTION_EDIT_COMMAND ||
               row->action == SETTINGS_ACTION_ICON_GRID || row->action == SETTINGS_ACTION_ICON_BROWSE)))
        snprintf(out, size, "%s " RIGHT_ARROW, row->value);
    else
        snprintf(out, size, "%s", row->value);
}

// A function to find how wide a row's value may draw: what its label leaves, and never less than
// half the row, so a long label is cut before a value is (a binding's F9 leaves its command the row)
static int value_room(const SettingsRow *row, int width)
{
    int pad = margin / 2;
    return max_int(width / 2, width - text_width(font_row, row->label) - 3 * pad);
}

// A function to draw one row; returns the height it took. A note row takes at most note_room.
static int draw_row(const SettingsRow *row, bool highlighted, int x, int y, int width, int note_room)
{
    int pad = margin / 2;
    int text_y = y + (row_height - TTF_GetFontHeight(font_row)) / 2;
    if (row->kind == SETTINGS_ROW_DIVIDER) {
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_DIVIDER);
        draw_line(renderer, x, y + row_height / 2, x + width, y + row_height / 2);
        return row_height;
    }
    if (row->kind == SETTINGS_ROW_NOTE) {
        const char *note = fit_note(row->note, width - 2 * pad, note_room);
        int h = draw_wrapped(font_small, note, x + pad, y, width - 2 * pad, ALPHA_VALUE);
        return max_int(row_height, h + row_height / 2);
    }
    if (highlighted) {
        SDL_Rect box = { x, y, width, row_height - 2 };
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_FILL);
        fill_rect(renderer, &box);
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_OUTLINE);
        outline_rect(renderer, &box);
    }
    char value[320];
    row_value_text(row, highlighted, value, sizeof(value));
    int room = value_room(row, width);
    int value_width = min_int(text_width(font_row, value), room);
    Uint8 label_alpha = row->enabled ? 255 : ALPHA_DIM;
    Uint8 value_alpha = !row->enabled ? ALPHA_DIM : highlighted ? 255 : ALPHA_VALUE;
    draw_text(font_row, row->label, x + pad, text_y, width - value_width - 3 * pad, label_alpha, false);
    draw_text(font_row, value, x + width - pad, text_y, room, value_alpha, true);
    return row_height;
}

// A function to draw the page's rows (built once for the frame) between two heights, scrolled to
// keep the cursor in view
static void draw_model_rows(SettingsRow *rows, int count, int x, int top, int bottom)
{
    int cursor = settings_cursor(model);
    int visible = max_int(1, (bottom - top) / row_height);
    if (cursor < first_row)
        first_row = cursor;
    if (cursor >= first_row + visible)
        first_row = cursor - visible + 1;
    first_row = max_int(0, min_int(first_row, count - visible));

    // A note gets the room the other rows on show with it leave, so the rows under it (Try again
    // and Leave without saving, under a failed save's reason) are always on show. A page longer
    // than the column scrolls, so no more than a column's worth of rows count, and a note always
    // keeps three lines.
    int notes = 0;
    for (int i = 0; i < count; i++)
        notes += rows[i].kind == SETTINGS_ROW_NOTE ? 1 : 0;
    int note_room = 0;
    if (notes > 0) {
        int others = min_int(count - notes, visible - 1);
        note_room = ((bottom - top) - others * row_height) / notes - row_height / 2;
        note_room = max_int(note_room, 3 * TTF_GetFontHeight(font_small));
    }

    // With the cursor on the last row it can rest on, what follows it (the Menus page's note on
    // the menus it has no room for, say) comes on show too: the page scrolls to its end. Which rows
    // the cursor can rest on is the model's to say (a grayed row that says why is one).
    bool rest_after = false;
    for (int i = cursor + 1; i < count; i++) {
        if (settings_row_selectable(&rows[i]))
            rest_after = true;
    }
    if (!rest_after) {
        int room = bottom - top;
        int from = count;
        while (from > 0) {
            int h = row_drawn_height(&rows[from - 1], note_room);
            if (room < h)
                break;
            room -= h;
            from--;
        }
        first_row = max_int(first_row, min_int(from, cursor));
    }
    int y = top;
    int last = first_row - 1;
    for (int i = first_row; i < count && y + row_height <= bottom; i++) {
        if (rows[i].slot != NULL && rows[i].slot->def->id == SET_ID_SLIDESHOW_DIRECTORY)
            describe_folder_row(&rows[i]);
        y += draw_row(&rows[i], i == cursor, x, y, column_width, note_room);
        last = i;
    }
    if (cursor >= 0 && cursor < count && rows[cursor].kind != SETTINGS_ROW_NOTE && rows[cursor].kind != SETTINGS_ROW_DIVIDER) {
        char value[320];
        char drawn[sizeof(drawn_cursor)];
        row_value_text(&rows[cursor], true, value, sizeof(value));
        snprintf(drawn, sizeof(drawn), "%s: %s", rows[cursor].label, value);
        if (strcmp(drawn, drawn_cursor) != 0) {
            copy_string(drawn_cursor, drawn, sizeof(drawn_cursor));
            log_debug("Settings: the cursor's row reads %s", drawn);
#ifdef STREAMFLEX_TEST_HOOKS
            // Only the headless harness builds this: whether the value fits the room draw_row() gives it
            log_debug("Test hook: the cursor's row's value is %i px wide, with %i px to draw in",
                      text_width(font_row, value), value_room(&rows[cursor], column_width));
#endif
        }
    }
    if (first_row != shown_first || last != shown_last || count != shown_count) {
        shown_first = first_row;
        shown_last = last;
        shown_count = count;
        log_debug("Settings: rows %i to %i of %i on show", first_row, last, count);
    }
}

// A function to draw the browser's rows, scrolled to keep the cursor in view
static void draw_browser_rows(int x, int top, int bottom)
{
    int count = browser_row_count(browser);
    int cursor = browser_cursor(browser);
    browser_page = max_int(1, (bottom - top) / row_height);
    if (cursor < browser_first)
        browser_first = cursor;
    if (cursor >= browser_first + browser_page)
        browser_first = cursor - browser_page + 1;
    browser_first = max_int(0, min_int(browser_first, count - browser_page));
    int y = top;
    for (int i = browser_first; i < count && y + row_height <= bottom; i++) {
        const BrowserRow *row = browser_row(browser, i);
        bool opens = row->kind == BROWSER_ROW_PLACE || row->kind == BROWSER_ROW_FOLDER;
        SettingsRow shown;
        memset(&shown, 0, sizeof(shown));
        shown.kind = opens ? SETTINGS_ROW_LINK : SETTINGS_ROW_ACTION;
        shown.enabled = opens || row->enabled;
        copy_string(shown.label, row->name, sizeof(shown.label));
        if (row->kind == BROWSER_ROW_USE_FOLDER)
            snprintf(shown.value, sizeof(shown.value), row->image_count == 1 ? "%i image" : "%i images", row->image_count);
        y += draw_row(&shown, i == cursor, x, y, column_width, 0);
    }
}

// A function to draw the column: the title, the page path (or the folder being browsed, or the
// setting a picker chooses for), the rows (the page's, built once for the frame, the browser's or
// the picker's) and the key hint, logged when it changes
static void draw_column(SettingsRow *rows, int count)
{
    char path[512];
    int x = margin;
    draw_text(font_header, "Settings", x, margin, column_width, 255, false);
    if (pickers_active())
        pickers_path(path, sizeof(path));
    else if (browser != NULL)
        copy_string(path, browser_folder(browser) != NULL ? browser_folder(browser) : "Places", sizeof(path));
    else
        settings_path(model, path, sizeof(path));
    draw_text(font_small, path, x, margin + TTF_GetFontHeight(font_header),
              column_width, ALPHA_DIM, false);
    int top = (int) (ROWS_TOP_RATIO * (float) geo.screen_height);
    int hint_y = geo.screen_height - margin - TTF_GetFontHeight(font_small);
    if (pickers_owns_column())
        pickers_draw(x, top, hint_y - margin);
    else if (browser != NULL)
        draw_browser_rows(x, top, hint_y - margin);
    else
        draw_model_rows(rows, count, x, top, hint_y - margin);
    // The hint is one line: the top page's and the browser's fit the column at 1280 x 800 (a share of
    // the width, with text a share of the height), the top page's Back's "saves and closes" said as
    // "saves" and the browser's "OK opens or chooses" as the list picker's "OK chooses"
    const char *hint = pickers_active() ? pickers_hint()
                     : browser != NULL ? "Left and right page \xC2\xB7 OK chooses \xC2\xB7 Back goes up"
                     : settings_moving_tile(model) >= 0 ? "Arrows move it \xC2\xB7 OK drops it \xC2\xB7 Back puts it back"
                     : settings_page(model) == SETTINGS_PAGE_TOP
                       ? "Left and right change \xC2\xB7 OK opens \xC2\xB7 Back saves"
                       : "Left and right change \xC2\xB7 OK opens \xC2\xB7 Back goes back";
    draw_text(font_small, hint, x, hint_y, column_width, ALPHA_DIM, false);
    if (strcmp(hint, logged_hint) != 0) {
        copy_string(logged_hint, hint, sizeof(logged_hint));
        log_debug("Settings: the key hint reads %s", hint);
#ifdef STREAMFLEX_TEST_HOOKS
        // Only the headless harness builds this: whether the one-line hint fits the column
        log_debug("Test hook: the key hint is %i px wide, in a column %i px wide", text_width(font_small, hint), column_width);
#endif
    }
}

// A function to say why the row under the cursor is grayed, or "" when it is not
static const char *cursor_why(const SettingsRow *rows, int count)
{
    int cursor = settings_cursor(model);
    if (cursor < 0 || cursor >= count || rows[cursor].enabled || rows[cursor].why == NULL)
        return "";
    return rows[cursor].why;
}

// A function to warn in the caption when the row under the cursor is a title or clock color that
// stands out too little from the background; "" otherwise
static const char *row_warning(const SettingsRow *rows, int count)
{
    static char warning[160];
    int cursor = settings_cursor(model);
    warning[0] = '\0';
    if (cursor >= 0 && cursor < count && rows[cursor].slot != NULL)
        contrast_warning(rows[cursor].slot->def->id, rows[cursor].slot->value.color, warning, sizeof(warning));
    return warning;
}

// A function to draw the caption, two lines from (x, y) at most `width` wide: which menu, its grid
// and titles, and any note: the open picker's; the browser's; the last key's; else what the row under
// the cursor says (row_note: why it is grayed, or a contrast warning)
static void draw_caption(const char *row_note, int x, int y, int width)
{
    char caption[512];
    char titles[32];
    char why[256];
    LayoutGeometry geometry;
    describe_titles(&layout, titles, sizeof(titles));
    bool reduced = compute_menu_layout(current_menu, &geometry, why, sizeof(why)) == 0 && why[0] != '\0';
    snprintf(caption, sizeof(caption), "Preview: %s \xC2\xB7 %i \xC3\x97 %i, %i px buttons, %s%s", current_menu->name,
        layout.columns, layout.rows, layout.button, titles, reduced ? " (reduced to fit the screen)" : "");
    draw_text(font_small, caption, x, y, width, ALPHA_VALUE, false);
    const char *note = pickers_active() || pickers_busy() ? pickers_note()
                     : browser != NULL ? browser_caption()
                     : settings_notice(model)[0] != '\0' ? settings_notice(model) : row_note;
    draw_text(font_small, note, x, y + TTF_GetFontHeight(font_small), width,
              255, false);
    if (strcmp(note, drawn_note) != 0) {
        copy_string(drawn_note, note, sizeof(drawn_note));
        if (note[0] != '\0')
            log_debug("Settings: the note under the preview says %s", note);
    }
}

// A function to dim the scene as the screensaver would, while the Screensaver page is open
static void dim_preview(void)
{
    if (settings_page(model) != SETTINGS_PAGE_SCREENSAVER || eff.screensaver_alpha < 1)
        return;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, (Uint8) eff.screensaver_alpha);
    fill_rect(renderer, NULL);
}

// A function to draw one frame of the screen and present it
void settings_draw(void)
{
    if (model == NULL)
        return;
    poll_decode(false);
    pickers_tick();
    poll_apps();
    log_path();   // A capture's end or the 10 s's moves no key through handle_command()

    // The Menus list's cursor has rested: the preview follows it now
    if (preview_wanted != NULL && ticks_ms() - preview_asked >= PREVIEW_REST_MS) {
        Menu *menu = preview_wanted;
        preview_wanted = NULL;
        show_preview(menu);
    }

    // Mode's cursor has rested: the window takes the kind its background needs, when that differs
    if (kind_waiting && ticks_ms() - kind_asked >= PREVIEW_REST_MS) {
        kind_waiting = false;
#ifdef STREAMFLEX_TEST_HOOKS
        // Only the headless harness builds this: on Linux no Mode changes the window (unless the
        // harness forces the Windows plan), so this says the rest came
        log_debug("Test hook: the Mode row's cursor rested on %s", get_mode_setting(MODE_SETTING_BACKGROUND, (int) config.background_mode));
#endif
        window_follow_background();
    }

    // The page's rows, built once for the frame: the column draws them, and the caption says why the
    // one under the cursor is grayed, or warns of its low contrast. The browser and the pickers have
    // rows of their own.
    SettingsRow rows[SETTINGS_MAX_ROWS];
    int count = browser == NULL && !pickers_owns_column() ? settings_rows(model, rows, SETTINGS_MAX_ROWS) : 0;
    const char *row_note = cursor_why(rows, count);
    if (row_note[0] == '\0')
        row_note = row_warning(rows, count);
    if (preview != NULL) {
        SDL_SetRenderTarget(renderer, preview);
        draw_scene(true);
        dim_preview();
        SDL_SetRenderTarget(renderer, NULL);
        SDL_SetRenderDrawColor(renderer, BACKDROP.r, BACKDROP.g, BACKDROP.b, BACKDROP.a);
        SDL_RenderClear(renderer);
        draw_texture(renderer, preview, NULL, &preview_rect);
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_FRAME);
        outline_rect(renderer, &preview_rect);
        draw_caption(row_note, preview_rect.x, preview_rect.y + preview_rect.h + margin / 2, preview_rect.w);
    }
    else {
        // No render targets: the scene fills the screen, the column sits on a dark backing, and
        // the caption on another along the bottom of the rest of the screen
        draw_scene(true);
        dim_preview();
        SDL_SetRenderDrawColor(renderer, BACKDROP.r, BACKDROP.g, BACKDROP.b, 220);
        SDL_Rect backing = { 0, 0, column_width + 2 * margin, geo.screen_height };
        fill_rect(renderer, &backing);
        int caption_h = 2 * TTF_GetFontHeight(font_small) + margin;
        SDL_Rect strip = { backing.w, geo.screen_height - caption_h, geo.screen_width - backing.w, caption_h };
        fill_rect(renderer, &strip);
        draw_caption(row_note, strip.x + margin, strip.y + margin / 2, strip.w - 2 * margin);
    }
    draw_column(rows, count);
    // The keyboard, over the bottom of the preview and its caption while it is open (spec "Where it appears")
    if (pickers_keyboard_open()) {
        int left = column_width + 2 * margin;
        int top = (int) (KEYBOARD_TOP_RATIO * (float) geo.screen_height);
        pickers_draw_keyboard(left, top, geo.screen_width - left - margin, geo.screen_height - top - margin);
    }
    present_frame();
}

// A function to log and store a change without refreshing: the font picker's face, which the
// font's own change refreshes with it
static void handle_quiet(const SettingsEvent *event)
{
    if (event->kind != SETTINGS_EVENT_CHANGED)
        return;
    log_change(event->slot, &event->before);
    apply_slot(event->slot, false);
}

// A function to load a list section's lines into the bindings; false when they could not be listed
// or loaded (out of memory)
static bool load_section(const IniDoc *doc, const char *section, const char *const *skip, BindingsDevice device)
{
    int count = inidoc_list(doc, section, skip, NULL, 0);
    IniDocItem *items = calloc((size_t) (count > 0 ? count : 1), sizeof(IniDocItem));
    if (items == NULL)
        return false;
    inidoc_list(doc, section, skip, items, count);
    test_fail("bindings", true);
    bool loaded = bindings_load(bindings, device, items, count);
    test_fail("bindings", false);
    free(items);
    return loaded;
}

// A function to load the bindings from config.ini as it is now (`doc`; NULL when it could not be read),
// whose lines the save finds again by their text. Bindings that cannot be read (the file gone, or out
// of memory) leave the binding pages out, and say so.
static void load_bindings(const IniDoc *doc)
{
    static const char *const skip[] = { SETTING_GAMEPAD_ENABLED, SETTING_GAMEPAD_DEVICE, SETTING_GAMEPAD_MAPPINGS_FILE, NULL };
#ifdef _WIN32
    bindings = bindings_create(true, gamepad_running());
#else
    bindings = bindings_create(false, gamepad_running());
#endif
    bool loaded = bindings != NULL && doc != NULL && load_section(doc, "Hotkeys", NULL, BINDINGS_KEYBOARD) &&
                  load_section(doc, "Gamepad", skip, BINDINGS_GAMEPAD);
    if (!loaded) {
        log_error("Settings: the bindings could not be read from %s, so the binding pages are left out", config.config_path);
        bindings_free(bindings);
        bindings = NULL;
        return;
    }
    settings_set_bindings(model, bindings, pickers_key_name);
}

// A function to load the menus model from config.ini as it is now (`doc`), as the bindings are: a live
// Entry keeps neither its key nor its line (plan ruling 3). Each live menu is taken as read from the
// section of its name, as config.ini reads after any save (menu_for()). Menus that cannot be read leave
// the menus model out, and say so. So does a config.ini whose every menu was removed by hand since
// StreamFlex started: the launcher cannot show no menu (menus_apply() would leave current_menu with none to
// draw), so it keeps its own, and the hand-edit catch-up does not run. Out of memory for a live menu's
// section, the menus model is left out too: menu_for() would find that menu by no name, and the next
// change would make it again, losing its place.
static void load_menus(const IniDoc *doc)
{
    bool named = true;
    for (int m = 0; m < menu_count; m++) {
        free(menus[m]->section);
        menus[m]->section = strdup(menus[m]->name);
        named = named && menus[m]->section != NULL;
    }
    if (!named) {
        log_error("Settings: out of memory, so the menus cannot be edited");
        return;
    }
    menu_edit = doc != NULL ? menuedit_load(doc, config.default_menu) : NULL;
    if (menu_edit == NULL) {
        log_error("Settings: the menus could not be read from %s, so they cannot be edited", config.config_path);
        return;
    }
    if (menuedit_count(menu_edit) == 0) {
        log_error("Settings: %s has no menus now (edited by hand since StreamFlex started), so the launcher keeps "
                  "its menus, and they cannot be edited", config.config_path);
        menuedit_free(menu_edit);
        menu_edit = NULL;
        return;
    }
    settings_set_menus(model, menu_edit);
    log_debug("Menus: read %i menu(s) from %s", menuedit_count(menu_edit), config.config_path);
    menus_opened = menuedit_copy(menu_edit);
    menus_logged = menuedit_copy(menu_edit);
    if (menus_opened == NULL || menus_logged == NULL)
        log_error("Menus: out of memory, so menu edits and the save's steps may go unlogged");
}

// A function to tell whether the menus model and the launcher's menus are the same menus by name, in the
// same order, as they are unless config.ini's menus were edited by hand since StreamFlex started
static bool menus_in_step(void)
{
    if (menuedit_count(menu_edit) != menu_count)
        return false;
    for (int m = 0; m < menu_count; m++) {
        if (strcmp(menuedit_menu(menu_edit, m)->name, menus[m]->name) != 0)
            return false;
    }
    return true;
}

// A function to find the menu a menus model's menu is in another state of the model: the one read from
// the same section, or for a menu made in settings (read from none) the one of its name; -1 for none
static int same_menu(const MenuEdit *in, const MenuEditMenu *menu)
{
    for (int m = 0; in != NULL && m < menuedit_count(in); m++) {
        const MenuEditMenu *other = menuedit_menu(in, m);
        bool same = menu->original_name != NULL
                    ? other->original_name != NULL && strcmp(other->original_name, menu->original_name) == 0
                    : other->original_name == NULL && strcmp(other->name, menu->name) == 0;
        if (same)
            return m;
    }
    return -1;
}

// A function to tell whether two tiles say the same: their title, icon and command
static bool same_tile(const MenuTile *a, const MenuTile *b)
{
    return strcmp(a->title, b->title) == 0 && strcmp(a->icon, b->icon) == 0 && strcmp(a->command, b->command) == 0;
}

// A function to log every change to the menus model since the log last described it (spec, Errors and
// limits: every menu edit), place by place: each tile added, removed or now another. A menu that came,
// went or was renamed the rebuild logs itself (menus_apply()). The model as it is now is then the one the
// next change is logged against.
static void log_menu_edits(void)
{
    for (int m = 0; menus_logged != NULL && m < menuedit_count(menu_edit); m++) {
        const MenuEditMenu *now = menuedit_menu(menu_edit, m);
        int at = same_menu(menus_logged, now);
        const MenuEditMenu *was = at >= 0 ? menuedit_menu(menus_logged, at) : NULL;
        int was_count = was != NULL ? was->count : 0;
        for (int t = 0; t < now->count || t < was_count; t++) {
            const MenuTile *a = t < was_count ? &was->tiles[t] : NULL;
            const MenuTile *b = t < now->count ? &now->tiles[t] : NULL;
            if (a == NULL)
                log_debug("Menus: '%s': tile %i added: %s;%s;%s", now->name, t, b->title, b->icon, b->command);
            else if (b == NULL)
                log_debug("Menus: '%s': tile %i removed: %s", now->name, t, a->title);
            else if (!same_tile(a, b))
                log_debug("Menus: '%s': tile %i is now %s;%s;%s, was %s;%s;%s", now->name, t, b->title, b->icon,
                    b->command, a->title, a->icon, a->command);
        }
    }
    menuedit_free(menus_logged);
    menus_logged = menuedit_copy(menu_edit);
    if (menus_logged == NULL)
        log_error("Menus: out of memory, so the next menu edit goes unlogged");
}

// A function to log the steps the save is asked to take for the menus (spec, Errors and limits: each save
// step), against the model as settings opened: each menu removed, renamed or added, and each whose tiles
// are written again. What the save could not do as asked it logs itself, a line a note.
static void log_menu_save(void)
{
    if (menu_edit == NULL || menus_opened == NULL || !menuedit_changed(menu_edit))
        return;
    for (int m = 0; m < menuedit_count(menus_opened); m++) {
        const MenuEditMenu *was = menuedit_menu(menus_opened, m);
        if (same_menu(menu_edit, was) < 0)
            log_debug("Menus: saving: [%s] is removed", was->name);
    }
    for (int m = 0; m < menuedit_count(menu_edit); m++) {
        const MenuEditMenu *now = menuedit_menu(menu_edit, m);
        int at = same_menu(menus_opened, now);
        const MenuEditMenu *was = at >= 0 ? menuedit_menu(menus_opened, at) : NULL;
        if (was == NULL) {
            log_debug("Menus: saving: [%s] is added, with %i tile(s)", now->name, now->count);
            continue;
        }
        if (strcmp(was->name, now->name) != 0)
            log_debug("Menus: saving: [%s] is renamed [%s]", was->name, now->name);
        bool changed = was->count != now->count;
        for (int t = 0; !changed && t < now->count; t++)
            changed = !same_tile(&was->tiles[t], &now->tiles[t]);
        if (changed)
            log_debug("Menus: saving: [%s]'s tiles are written again, %i of them", now->name, now->count);
    }
}

// A function to hand the pickers what they draw with: the model, the menus, the fonts, and the sizes
// measure_layout() worked out
static void begin_pickers(void)
{
    PickerHost host = {
        .model = model, .menus = menus, .menu_count = menu_count, .apply = apply_slot, .event = handle_event,
        .quiet = handle_quiet, .text = draw_text, .row = draw_row, .font_row = font_row, .font_small = font_small,
        .row_height = row_height, .column_width = column_width, .margin = margin
    };
    pickers_begin(&host);
}

// A function to put the menus model into the launcher after a change to it, and the settings with it
// (plan ruling 5). The launcher's menus are rebuilt from it (menus_apply()). When a menu came, went, was
// renamed or moved, the settings model is made again over the menus as they are now first, every unsaved
// change carried across by the menu's name (settings_remodel()), and then the screen's list of menus, the
// pickers' and the menu settings opened over follow: one now gone gives way to the default menu, which
// closing settings then shows (Review Focus 3). A picker or the folder browser open on the old model's
// slots closes first, as Back would close it. False, with nothing changed in the launcher, when out of
// memory, or when the model holds no menu (the launcher cannot show none): the change stays in the menus
// model alone.
bool settings_menus_changed(void)
{
    if (model == NULL || menu_edit == NULL)
        return false;
    log_menu_edits();
    int count = menuedit_count(menu_edit);
    if (count == 0) {
        log_error("Settings: the menus model holds no menu, so the launcher keeps its menus");
        return false;
    }
    size_t room = (size_t) (count > 0 ? count : 1);
    const char **names = calloc(room, sizeof(char*));
    const char **old_names = calloc(room, sizeof(char*));
    Menu **now = calloc(room, sizeof(Menu*));

    // Each menu of the model, the live menu it stands for, and that menu's place and name in the settings
    // model: any menu new, gone, renamed or moved makes the settings model again
    bool remake = count != menu_count;
    int origin_at = -1;
    for (int m = 0; names != NULL && old_names != NULL && m < count; m++) {
        const MenuEditMenu *menu = menuedit_menu(menu_edit, m);
        Menu *live = menu_for(menu);
        int was = -1;
        for (int k = 0; live != NULL && k < menu_count && was < 0; k++) {
            if (menus[k] == live)
                was = k;
        }
        names[m] = menu->name;
        old_names[m] = was >= 0 ? menus[was]->name : NULL;
        if (live != NULL && live == origin)
            origin_at = m;
        remake = remake || was != m || strcmp(old_names[m], names[m]) != 0;
    }
    SettingsState *remade = NULL;
    if (names != NULL && old_names != NULL && now != NULL && remake) {
        if (pickers_active())
            pickers_cancel();
        if (browser != NULL)
            close_browser();
        remade = settings_remodel(model, names, old_names, count);
    }
    if (names == NULL || old_names == NULL || now == NULL || (remake && remade == NULL)) {
        free(names);
        free(old_names);
        free(now);
        log_error("Settings: out of memory, so the menus cannot change");
        return false;
    }

    // From here on only whether each menu was in the settings model is read from old_names: the live
    // names they point at may go with menus_apply()'s renames
    for (int m = 0; m < count; m++) {
        if (old_names[m] != NULL)
            old_names[m] = "";
    }
    menus_apply(menu_edit);
    if (remade != NULL) {
        int m = 0;
        for (Menu *menu = config.first_menu; menu != NULL && m < count; menu = menu->next)
            now[m++] = menu;
        settings_free(model);
        model = remade;
        free(menus);
        menus = now;
        now = NULL;
        menu_count = count;
        // A menu new to the settings starts where its live menu is: one made from a config.ini section (the
        // hand-edit catch-up, Discard) at the section's own grid, as read for it, so a change back to All
        // menus is a change; one made in settings follows All menus, as settings_remodel() left it
        for (int n = 0; n < count; n++) {
            for (int id = SET_ID_MENU_ROWS; old_names[n] == NULL && id < SET_ID_COUNT; id++) {
                SettingValue value = read_value((SettingId) id, n);
                settings_set_entry(model, (SettingId) id, n, &value);
            }
        }
        origin = origin_at >= 0 ? menus[origin_at] : default_menu;
        preview_wanted = NULL;
        begin_pickers();
        log_debug("Settings: the menus changed, so their settings were made again over %i menu(s)", count);
    }
    free(now);
    free(names);
    free(old_names);
    return true;
}

#ifdef STREAMFLEX_TEST_HOOKS
// A function only the headless harness builds: remove a submenu with its tiles, as its confirm page does,
// through the first tile that opens it; the reason it could not, or NULL
static const char *test_remove_menu(const char *name)
{
    for (int m = 0; m < menuedit_count(menu_edit); m++) {
        const MenuEditMenu *menu = menuedit_menu(menu_edit, m);
        for (int t = 0; t < menu->count; t++) {
            const char *opens = menuedit_submenu_of(&menu->tiles[t]);
            if (opens != NULL && strcmp(opens, name) == 0)
                return menuedit_remove_menu(menu_edit, m, t, true);
        }
    }
    return "No tile opens that menu";
}

// A function only the headless harness builds: add a tile, written title;icon;command, at a menu's end
static const char *test_add_tile(const char *name, char *tile)
{
    int m = menuedit_find(menu_edit, name);
    char *icon = strchr(tile, ';');
    char *command = icon != NULL ? strchr(icon + 1, ';') : NULL;
    if (m < 0)
        return "No such menu";
    if (command == NULL)
        return "The tile is not title;icon;command";
    *icon++ = '\0';
    *command++ = '\0';
    return menuedit_add(menu_edit, m, -1, tile, icon, command);
}

// A function only the headless harness builds: STREAMFLEX_TEST_MENU_EDIT makes one change to the menus
// model as settings open, as the Tiles pages make one, and puts it into the launcher, so a check can
// watch the live rebuild: "remove-menu <name>", "rename-menu <old>=<new>", or
// "add-tile <menu>=<title>;<icon>;<command>"
static void test_menu_edit(void)
{
    const char *given = getenv("STREAMFLEX_TEST_MENU_EDIT");
    if (given == NULL || menu_edit == NULL)
        return;
    char text[256];
    snprintf(text, sizeof(text), "%s", given);
    char *name = strchr(text, ' ');
    char *value = name != NULL ? strchr(name, '=') : NULL;
    const char *why = "it is not an edit the hook knows";
    if (name != NULL) {
        *name++ = '\0';
        if (value != NULL)
            *value++ = '\0';
        int m = menuedit_find(menu_edit, name);
        if (strcmp(text, "remove-menu") == 0)
            why = test_remove_menu(name);
        else if (strcmp(text, "rename-menu") == 0 && value != NULL)
            why = m < 0 ? "No such menu" : menuedit_rename_menu(menu_edit, m, value);
        else if (strcmp(text, "add-tile") == 0 && value != NULL)
            why = test_add_tile(name, value);
    }
    if (why == NULL && !settings_menus_changed())
        why = "the settings could not follow it";
    if (why == NULL)
        log_debug("Test hook: the menu edit '%s' was made", given);
    else
        log_debug("Test hook: the menu edit '%s' was refused: %s", given, why);
}

// A function only the headless harness builds: STREAMFLEX_TEST_OPEN_TILE=<menu>:<tile> opens the pages at
// that tile's page as settings open (tile -1: the menu's Tiles page), as the hold-OK shortcut opens them,
// so a check reaches a tile's page however many rows lead there
static void test_open_tile(void)
{
    const char *given = getenv("STREAMFLEX_TEST_OPEN_TILE");
    if (given == NULL)
        return;
    char name[256];
    snprintf(name, sizeof(name), "%s", given);
    char *colon = strrchr(name, ':');
    int menu = -1;
    if (colon != NULL) {
        *colon = '\0';
        for (int m = 0; m < menu_count && menu < 0; m++) {
            if (strcmp(menus[m]->name, name) == 0)
                menu = m;
        }
    }
    if (menu < 0) {
        log_debug("Test hook: no menu for STREAMFLEX_TEST_OPEN_TILE=%s", given);
        return;
    }
    settings_show_tile(model, menu, atoi(colon + 1));
    follow_preview();
    log_path();
    log_debug("Test hook: settings opened at tile %s of menu '%s'", colon + 1, name);
}
#endif

// A function to open settings at a tile's page, for OK held on it in the launcher's menus (spec "The
// hold-OK shortcut"): over the menu named, through Menus, the menu and its Tiles page to the tile's page
// (settings_show_tile()), so Back goes to the Tiles page and then up through settings as usual; tile -1
// (an empty menu) opens the menu's Tiles page. The menu keeps its highlight through settings (the preview
// shows it, and show_menu() keeps a menu's place), so closing settings lands on the tile.
void settings_open_at_tile(const char *menu, int tile)
{
    char name[256];   // A copy: settings_open() may make the launcher's menus again, the name with them
    snprintf(name, sizeof(name), "%s", menu);
    for (Menu *over = config.first_menu; over != NULL; over = over->next) {
        if (strcmp(over->name, name) == 0 && over != current_menu)
            show_menu(over);
    }
    settings_open();
    if (model == NULL)
        return;   // settings_open() said why
    int index = -1;
    for (int m = 0; m < menu_count && index < 0; m++) {
        if (strcmp(menus[m]->name, name) == 0)
            index = m;
    }
    if (index < 0) {
        log_debug("Hold OK: the menu '%s' is not in config.ini any more, so settings open at their top", name);
        return;
    }
    settings_show_tile(model, index, tile);
    follow_preview();
    log_path();
    // The page that opened, not the one asked for (Fable review I4): a menu with no menus model stops short
    SettingsPage page = settings_page(model);
    log_debug("Hold OK: settings opened at %s of menu '%s'", page == SETTINGS_PAGE_TILE ? "a tile's page"
              : page == SETTINGS_PAGE_TILES ? "the Tiles page" : page == SETTINGS_PAGE_MENU ? "the menu's page"
              : "the Menus page", name);
}

// A function to make the preview's texture at the screen's size, or none when the renderer has no render targets (the scene is then drawn behind the column)
static void make_preview(void)
{
    if (preview != NULL)
        SDL_DestroyTexture(preview);
    preview = NULL;
    bool targets = true;   // Every SDL3 renderer has render targets (README-migration, SDL_render.h)
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: it draws as a renderer without render targets would
    if (getenv("STREAMFLEX_TEST_NO_RENDER_TARGETS") != NULL)
        targets = false;
#endif
    if (!targets)
        log_debug("Settings: the renderer has no render targets, so the menu is drawn behind them");
    else {
        preview = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, geo.screen_width, geo.screen_height);
        if (preview == NULL)
            log_error("Settings: no preview texture, the menu is drawn behind the settings instead\n%s", SDL_GetError());
        else {
            SDL_SetTextureBlendMode(preview, SDL_BLENDMODE_NONE);
            SDL_SetTextureScaleMode(preview, SDL_SCALEMODE_LINEAR);
            log_debug("Settings: the preview is at %i,%i, %i x %i", preview_rect.x, preview_rect.y,
                preview_rect.w, preview_rect.h);
        }
    }
}

// A function to open settings over the menu on show
void settings_open(void)
{
    if (model != NULL || current_menu == NULL)
        return;
    menu_count = (int) config.num_menus;
    menus = calloc((size_t) (menu_count > 0 ? menu_count : 1), sizeof(Menu*));
    const char **names = calloc((size_t) (menu_count > 0 ? menu_count : 1), sizeof(char*));
    int found = 0;
    for (Menu *menu = config.first_menu; menus != NULL && names != NULL && menu != NULL && found < menu_count; menu = menu->next) {
        menus[found] = menu;
        names[found] = menu->name;
        found++;
    }
    menu_count = found;
    model = menus != NULL && names != NULL ? settings_create(names, menu_count) : NULL;
    free(names);
    if (model == NULL) {
        log_error("Settings: out of memory");
        free_screen();
        return;
    }
    for (int id = 0; id < SET_ID_GLOBAL_COUNT; id++) {
        SettingValue value = read_value((SettingId) id, -1);
        settings_set_entry(model, (SettingId) id, -1, &value);
    }
    list_pads();
    // config.ini as it is now, for the bindings and the menus: their saves find each line again by its text
    size_t length = 0;
    char *text = fileio_read_all(config.config_path, &length);
    IniDoc *doc = text != NULL ? inidoc_parse(text, length) : NULL;
    alloc_free(text);
    load_bindings(doc);
    load_menus(doc);
    inidoc_free(doc);
    for (int m = 0; m < menu_count; m++) {
        for (int id = SET_ID_MENU_ROWS; id < SET_ID_COUNT; id++) {
            SettingValue value = read_value((SettingId) id, m);
            settings_set_entry(model, (SettingId) id, m, &value);
        }
    }
    if (!open_fonts()) {
        free_screen();
        return;
    }
    origin = current_menu;
    go_home = false;
    first_row = 0;
    counted_folder[0] = '\0';
    fitted_width = -1;
    shown_first = -1;
    shown_last = -1;
    shown_count = -1;
    drawn_note[0] = '\0';
    drawn_cursor[0] = '\0';
    logged_path[0] = '\0';
    logged_hint[0] = '\0';
    measure_layout();
    begin_pickers();
    make_preview();
    log_debug("Settings opened over menu '%s'", origin->name);
    if (menu_edit != NULL && !menus_in_step()) {
        log_debug("Menus: config.ini's menus are not the launcher's (edited by hand since StreamFlex started), so the launcher takes config.ini's");
        settings_menus_changed();
    }
    // What Discard puts the menus back to: the file's menus, as the launcher now shows them
    menu_snapshot = menu_edit != NULL ? menuedit_copy(menu_edit) : NULL;
    if (menu_edit != NULL && menu_snapshot == NULL)
        log_error("Settings: out of memory, so Discard cannot put the menus back");
#ifdef STREAMFLEX_TEST_HOOKS
    test_menu_edit();
    test_open_tile();
#endif
    keep_taken();   // The menus as the launcher shows them now, which a change it cannot take goes back to
}

// A function to let go of settings at quit, saving nothing
void settings_close_now(void)
{
    if (model != NULL)
        free_screen();
}

// A function to measure the screen again for a new size (spec section 6), keeping the page, its
// cursor and every unsaved edit: the fonts open again at the new height, the text cache empties, the
// column and the preview are measured again, and the preview is made anew. An open picker or the
// folder browser, measured when it opened, closes as Back closes it. What was logged as last drawn is
// forgotten, so the next frame logs the page and the cursor's row again.
void settings_reflow(void)
{
    if (model == NULL)
        return;
    if (pickers_active())
        pickers_cancel();
    if (browser != NULL)
        close_browser();
    clear_text_cache();
    close_fonts();
    if (!open_fonts()) {
        close_settings();   // open_fonts() said why; the fonts opened when settings did
        return;
    }
    fitted_width = -1;
    shown_first = -1;
    shown_last = -1;
    shown_count = -1;
    drawn_note[0] = '\0';
    drawn_cursor[0] = '\0';
    logged_path[0] = '\0';
    logged_hint[0] = '\0';
    measure_layout();
    begin_pickers();
    make_preview();
    log_debug("Settings: re-measured for %ix%i px", geo.screen_width, geo.screen_height);
}

// A function to let go of every texture the screen holds, before the renderer that made them goes (a
// lost device, Task 1.7b): the text cache, the preview, the font picker's samples and the browsed
// image. What the screen shows is kept: settings_remake_textures() makes them again. Returns how many
// textures it destroyed, for a lost device's count; none while settings are closed.
int settings_drop_textures(void)
{
    if (model == NULL)
        return 0;
    int count = clear_text_cache();
    count += fonts_clear_samples();
    count += pickers_drop_icon_textures();
    if (preview != NULL) {
        SDL_DestroyTexture(preview);
        count++;
    }
    preview = NULL;
    if (background_override != NULL) {
        SDL_DestroyTexture(background_override);
        count++;
    }
    background_override = NULL;
    shown_path[0] = '\0';   // The browsed image is decoded again, if it is still wanted
    return count;
}

// A function to make the screen's textures again on a reset or new renderer at the same size,
// keeping the page, its cursor, an open picker or the browser, and every unsaved edit: the preview is
// made anew and the browsed image decoded again; the cache and the font samples fill as they are drawn
void settings_remake_textures(void)
{
    if (model == NULL)
        return;
    settings_drop_textures();
    make_preview();
    start_decode();
}
