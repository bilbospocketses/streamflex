// The settings screen: a narrow column of settings on the left and a live preview of the launcher
// on the right, driven by the remote alone. settings.c holds the pages and the values; this file
// draws them, puts each change into the running launcher, and saves on the way out.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include "launcher.h"
#include <launcher_config.h>
#include "settings.h"
#include "config_fields.h"
#include "settings_screen.h"
#include "settings_pickers.h"
#include "config_save.h"
#include "browser.h"
#include "fileio.h"
#include "alloc.h"
#include "inidoc.h"
#include "image.h"
#include "util.h"
#include "debug.h"
#include "test_hooks.h"
#include "platform/platform.h"

extern Config config;
extern Geometry geo;
extern SDL_Renderer *renderer;
extern Menu *current_menu;
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
#define PREVIEW_REST_MS 300        // How long the Menus list's cursor rests before the preview follows it
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
static bool go_home = false;          // Go to the default menu after closing (:home)
static char restart_names[256];       // What the restart prompt offers to apply: "the mappings file"
static SDL_Texture *preview = NULL;   // The scene at full size; NULL when the renderer has no targets
static TTF_Font *font_header = NULL;
static TTF_Font *font_row = NULL;
static TTF_Font *font_small = NULL;
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
static int shown_first = -1;                  // The rows last on show, for the log
static int shown_last = -1;
static int shown_count = -1;

static Browser *browser = NULL;          // The folder browser, while it is open
static SettingSlot *browser_slot = NULL; // The setting it chooses for: Image, Folder or Mappings file
static BrowserMode browser_mode = BROWSER_IMAGE;   // The mode it was opened in
static int browser_first = 0;            // Its first row on show
static int browser_page = 1;             // How many of its rows fit: Left and Right move this far
static char browser_note[128] = "";      // Why the last OK did nothing, for the caption

// The preview's image, decoded on its own thread so moving through a folder never stalls
static SDL_Thread *decode_thread = NULL;
static SDL_atomic_t decode_done;
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

// A function to measure a line of text
static int text_width(TTF_Font *font, const char *text)
{
    int w = 0;
    int h = 0;
    if (text[0] != '\0')
        TTF_SizeUTF8(font, text, &w, &h);
    return w;
}

// A function to get a line of text as a texture, from the cache when it was drawn recently
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
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, WHITE);
    if (surface == NULL)
        return NULL;
    entry->texture = SDL_CreateTextureFromSurface(renderer, surface);
    entry->w = surface->w;
    entry->h = surface->h;
    SDL_FreeSurface(surface);
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

// A function to empty the text cache
static void clear_text_cache(void)
{
    for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
        if (text_cache[i].texture != NULL)
            SDL_DestroyTexture(text_cache[i].texture);
        free(text_cache[i].text);
    }
    memset(text_cache, 0, sizeof(text_cache));
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
    SDL_RenderCopy(renderer, line->texture, NULL, &rect);
}

// A function to draw a paragraph wrapped to a width; returns its height
static int draw_wrapped(TTF_Font *font, const char *text, int x, int y, int width, Uint8 alpha)
{
    if (text == NULL || text[0] == '\0' || width <= 0)
        return 0;
    SDL_Surface *surface = TTF_RenderUTF8_Blended_Wrapped(font, text, WHITE, (Uint32) width);
    if (surface == NULL)
        return 0;
    int h = surface->h;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect rect = { x, y, surface->w, surface->h };
    SDL_FreeSurface(surface);
    if (texture != NULL) {
        SDL_SetTextureAlphaMod(texture, alpha);
        SDL_RenderCopy(renderer, texture, NULL, &rect);
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

// A check made when this file compiles: CONDITION false makes an array of size -1, which does not
// compile. MSVC's default C mode has no _Static_assert, so this stands in for it everywhere.
#define COMPILE_CHECK(name, condition) typedef char name[(condition) ? 1 : -1]

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
// joystick SDL lists while the gamepad runs, as SDL names it. A nameless one, or a failed count,
// is the model's to describe ("Pad N", or none).
static void list_pads(void)
{
    const char *names[SETTINGS_MAX_PADS] = { NULL };
    int count = gamepad_running() ? SDL_NumJoysticks() : 0;
    if (count > SETTINGS_MAX_PADS)
        count = SETTINGS_MAX_PADS;
    for (int i = 0; i < count; i++)
        names[i] = SDL_JoystickNameForIndex(i);
    settings_set_pads(model, names, count);
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
// not see the write; reload_clock() would stop it anyway. Two settings act beyond any group: the OS
// screensaver block, and the menu :home goes to.
static void apply_slot(const SettingSlot *slot, bool refresh)
{
    if (slot->def->refresh == SET_REFRESH_CLOCK)
        stop_clock();
    config_store(slot->def->id, slot->menu >= 0 ? menus[slot->menu] : NULL, &slot->value);
    if (slot->def->id == SET_ID_SLIDESHOW_FADE)
        update_slideshow_timing();
    refresh_effective();
    if (slot->def->id == SET_ID_INHIBIT_OS_SCREENSAVER)
        apply_os_screensaver();
    else if (slot->def->id == SET_ID_DEFAULT_MENU)
        apply_default_menu();
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
    font_header = open_font_file(path, max_int(8, (int) (HEADER_FONT_RATIO * height)), 0);
    font_row = open_font_file(path, max_int(8, (int) (ROW_FONT_RATIO * height)), 0);
    font_small = open_font_file(path, max_int(8, (int) (SMALL_FONT_RATIO * height)), 0);
    if (font_header == NULL || font_row == NULL || font_small == NULL)
        log_error("Settings cannot open: could not open the font %s\n%s", path, TTF_GetError());
    free(bundled);
    return font_header != NULL && font_row != NULL && font_small != NULL;
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
        "On", "Idle time", "Dim level", "Pause slideshow", "Gamepad", "Device", "Mappings file"
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
    row_height = (int) (ROW_HEIGHT_RATIO * (float) TTF_FontHeight(font_row));
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
    int caption = 2 * TTF_FontHeight(font_small);
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
    Uint32 start = SDL_GetTicks();
    show_menu(menu);
    log_debug("Settings: the preview shows menu '%s' (%u ms)", menu->name, SDL_GetTicks() - start);
}

// A function to show in the preview the menu the page is about: a menu being edited or
// highlighted, else the one settings opened over. A menu with no entries cannot be shown. In the
// Menus list the preview waits for the cursor to rest (settings_draw() switches it), so moving
// down the list never waits for the icons of every menu on the way.
static void follow_preview(void)
{
    int index = settings_preview_menu(model);
    Menu *want = index >= 0 && index < menu_count ? menus[index] : origin;
    if (want->num_entries == 0)
        want = origin;
    preview_wanted = NULL;
    if (want == current_menu)
        return;
    if (settings_page(model) == SETTINGS_PAGE_MENUS) {
        preview_wanted = want;
        preview_asked = SDL_GetTicks();
    }
    else
        show_preview(want);
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
    decode_surface = IMG_Load(decode_path);
    if (decode_surface == NULL)
        copy_string(decode_error, IMG_GetError(), sizeof(decode_error));
    SDL_AtomicSet(&decode_done, 1);
    return 0;
}

// A function to start decoding the wanted image, unless another is being decoded already, or it
// is on show, or it already failed
static void start_decode(void)
{
    if (decode_thread != NULL || wanted_path[0] == '\0' || strcmp(wanted_path, shown_path) == 0 || strcmp(wanted_path, broken_path) == 0)
        return;
    copy_string(decode_path, wanted_path, sizeof(decode_path));
    SDL_AtomicSet(&decode_done, 0);
    decode_thread = SDL_CreateThread(decode_image, "Preview image", NULL);
}

// A function to ask for an image as the preview's background; "" goes back to the real one
static void want_preview_image(const char *path)
{
    copy_string(wanted_path, path, sizeof(wanted_path));
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
    if (decode_thread == NULL || (!wait && !SDL_AtomicGet(&decode_done)))
        return;
    SDL_WaitThread(decode_thread, NULL);
    decode_thread = NULL;
    if (decode_surface == NULL) {
        copy_string(broken_path, decode_path, sizeof(broken_path));
        log_debug("Settings: could not open %s: %s", decode_path, decode_error);
        if (browser != NULL && strcmp(browser_caption(), CANNOT_OPEN) == 0)
            log_debug("Settings: the caption says %s for %s", CANNOT_OPEN, decode_path);
    }
    else if (strcmp(decode_path, wanted_path) == 0) {
        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, decode_surface);
        if (texture != NULL) {
            if (background_override != NULL)
                SDL_DestroyTexture(background_override);
            background_override = texture;
            copy_string(shown_path, decode_path, sizeof(shown_path));
            log_debug("Settings: the preview shows %s", shown_path);
        }
    }
    if (decode_surface != NULL)
        SDL_FreeSurface(decode_surface);
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
        SDL_FreeSurface(decode_surface);
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

// A function to open the folder browser for an Image, Folder or Mappings file setting, at its current path
static void open_browser(SettingSlot *slot)
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

    // Paths are checked against config.ini as the save will find it; unread, by the key alone
    size_t length = 0;
    char *text = fileio_read_all(config.config_path, &length);
    browser_doc = text != NULL ? inidoc_parse(text, length) : NULL;
    alloc_free(text);
    BrowserMode mode = slot->def->id == SET_ID_BACKGROUND_IMAGE ? BROWSER_IMAGE
                     : slot->def->id == SET_ID_GAMEPAD_MAPPINGS ? BROWSER_FILE : BROWSER_FOLDER;
    const char *why = "out of memory";
    test_fail("browser", true);
    browser = list != NULL ? browser_open(mode, slot->value.text, list, count, list_folder, check_path, slot, &why) : NULL;
    test_fail("browser", false);
    free(list);
    fileio_free_places(places, count);
    if (browser == NULL) {
        log_error("Settings: the folder browser cannot open: %s", why);
        inidoc_free(browser_doc);
        browser_doc = NULL;
        return;
    }
    browser_slot = slot;
    browser_mode = mode;
    browser_first = 0;
    browser_note[0] = '\0';
    log_browsing();
    preview_highlighted();
}

// A function to close the folder browser, back to the page it opened from (Background, or Gamepad)
static void close_browser(void)
{
    browser_free(browser);
    browser = NULL;
    browser_slot = NULL;
    inidoc_free(browser_doc);
    browser_doc = NULL;
    want_preview_image("");
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
    if (browser != NULL)
        close_browser();
    stop_decoding();
    preview_wanted = NULL;
    if (preview != NULL)
        SDL_DestroyTexture(preview);
    preview = NULL;
    clear_text_cache();
    if (font_header != NULL)
        TTF_CloseFont(font_header);
    if (font_row != NULL)
        TTF_CloseFont(font_row);
    if (font_small != NULL)
        TTF_CloseFont(font_small);
    font_header = NULL;
    font_row = NULL;
    font_small = NULL;
    settings_free(model);
    model = NULL;
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
    free_screen();
    log_debug("Settings closed");
    trim_title_fonts();
#ifdef _WIN32
    retry_exit_hotkey(true);   // Whether or not anything changed: its key may have been let go meanwhile
#endif
}

// A function to log what the save could not make as asked (config_save's notes, one a line). A change
// written as a new line beside a line changed by hand is not said to have taken effect: the first line
// on a key or button is the one that runs. That is said of those notes only, by the kind config_save
// gives each line, not of a removal skipped.
static void log_save_notes(ConfigSaveResult *result)
{
    int index = 0;
    for (char *line = result->notes; line[0] != '\0'; index++) {
        char *end = strchr(line, '\n');
        if (end != NULL)
            *end = '\0';
        // A line past the kinds config_save gave (none can be, by how it counts) gets no words
        if (index < result->note_count && result->note_kinds[index] == CONFIG_NOTE_CHANGED_MEANWHILE)
            log_debug("Settings: not saved as asked: %s; where two lines bind one key or button, the first in the file is the one that runs", line);
        else
            log_debug("Settings: not saved as asked: %s", line);
        if (end == NULL)
            break;
        line = end + 1;
    }
}

// A function to save every changed setting and binding into config.ini; on failure, show why. The
// bindings' list edits are sized to their lists, which holds every edit they can make.
static bool save_changes(void)
{
    int count = settings_slot_count(model);
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
    ok = config_save_all(config.config_path, PATH_CONFIG_SYSTEM, has_home ? user_config : NULL, edits, n, lists, list_count,
                         &result);
#else
    ok = config_save_all(config.config_path, NULL, NULL, edits, n, lists, list_count, &result);
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
    if (!settings_any_changed(model)) {
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

// A function to act on what a key did in the model
static void handle_event(const SettingsEvent *event)
{
    switch (event->kind) {
        case SETTINGS_EVENT_CHANGED:
            log_change(event->slot, &event->before);
            apply_slot(event->slot, true);
            break;
        case SETTINGS_EVENT_DISCARD:
            log_debug("Settings: discarded the changes");
            apply_all();
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
    const char *now = browser_folder(browser);
    if ((folder == NULL) != (now == NULL) || (folder != NULL && strcmp(folder, now) != 0))
        log_browsing();
    free(folder);
    if (result == BROWSER_CHOSEN) {
        char chosen[BROWSER_PATH_MAX];
        copy_string(chosen, browser_chosen(browser), sizeof(chosen));
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

// A function to act on a key while settings are open: the remote's keys move through them (or
// through a picker, while one is open), and every other command waits until they close
static void handle_command(const char *command)
{
    SettingsCommand key;
    if (pickers_active()) {
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
    SettingsEvent event = settings_command(model, key);
    handle_event(&event);
    if (model == NULL)
        return;
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
    Uint32 start = SDL_GetTicks();
    handle_command(text);
    Uint32 took = SDL_GetTicks() - start;
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

// A function to tell how tall a paragraph wraps to a width
static int wrapped_height(TTF_Font *font, const char *text, int width)
{
#ifdef STREAMFLEX_TEST_HOOKS
    measures++;   // Only the headless harness builds this: free_screen() logs the count
#endif
    SDL_Surface *surface = TTF_RenderUTF8_Blended_Wrapped(font, text, WHITE, (Uint32) width);
    if (surface == NULL)
        return 0;
    int h = surface->h;
    SDL_FreeSurface(surface);
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
// any row OK opens (a page, the browser, a picker, a binding, a binding's command); else the value alone
static void row_value_text(const SettingsRow *row, bool highlighted, char *out, size_t size)
{
    if (row->enabled && highlighted && row->steps)
        snprintf(out, size, LEFT_ARROW " %s " RIGHT_ARROW, row->value);
    else if (row->kind == SETTINGS_ROW_LINK || row->kind == SETTINGS_ROW_BROWSE || row->kind == SETTINGS_ROW_PICK ||
             row->kind == SETTINGS_ROW_BINDING ||
             (row->kind == SETTINGS_ROW_ACTION && row->action == SETTINGS_ACTION_BIND_COMMAND))
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
    int text_y = y + (row_height - TTF_FontHeight(font_row)) / 2;
    if (row->kind == SETTINGS_ROW_DIVIDER) {
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_DIVIDER);
        SDL_RenderDrawLine(renderer, x, y + row_height / 2, x + width, y + row_height / 2);
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
        SDL_RenderFillRect(renderer, &box);
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_OUTLINE);
        SDL_RenderDrawRect(renderer, &box);
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
        note_room = max_int(note_room, 3 * TTF_FontHeight(font_small));
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
    draw_text(font_small, path, x, margin + TTF_FontHeight(font_header), column_width, ALPHA_DIM, false);
    int top = (int) (ROWS_TOP_RATIO * (float) geo.screen_height);
    int hint_y = geo.screen_height - margin - TTF_FontHeight(font_small);
    if (pickers_active())
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
    draw_text(font_small, note, x, y + TTF_FontHeight(font_small), width, 255, false);
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
    SDL_RenderFillRect(renderer, NULL);
}

// A function to draw one frame of the screen and present it
void settings_draw(void)
{
    if (model == NULL)
        return;
    poll_decode(false);
    pickers_tick();
    log_path();   // A capture's end or the 10 s's moves no key through handle_command()

    // The Menus list's cursor has rested: the preview follows it now
    if (preview_wanted != NULL && SDL_GetTicks() - preview_asked >= PREVIEW_REST_MS) {
        Menu *menu = preview_wanted;
        preview_wanted = NULL;
        show_preview(menu);
    }

    // The page's rows, built once for the frame: the column draws them, and the caption says why the
    // one under the cursor is grayed, or warns of its low contrast. The browser and the pickers have
    // rows of their own.
    SettingsRow rows[SETTINGS_MAX_ROWS];
    int count = browser == NULL && !pickers_active() ? settings_rows(model, rows, SETTINGS_MAX_ROWS) : 0;
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
        SDL_RenderCopy(renderer, preview, NULL, &preview_rect);
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_FRAME);
        SDL_RenderDrawRect(renderer, &preview_rect);
        draw_caption(row_note, preview_rect.x, preview_rect.y + preview_rect.h + margin / 2, preview_rect.w);
    }
    else {
        // No render targets: the scene fills the screen, the column sits on a dark backing, and
        // the caption on another along the bottom of the rest of the screen
        draw_scene(true);
        dim_preview();
        SDL_SetRenderDrawColor(renderer, BACKDROP.r, BACKDROP.g, BACKDROP.b, 220);
        SDL_Rect backing = { 0, 0, column_width + 2 * margin, geo.screen_height };
        SDL_RenderFillRect(renderer, &backing);
        int caption_h = 2 * TTF_FontHeight(font_small) + margin;
        SDL_Rect strip = { backing.w, geo.screen_height - caption_h, geo.screen_width - backing.w, caption_h };
        SDL_RenderFillRect(renderer, &strip);
        draw_caption(row_note, strip.x + margin, strip.y + margin / 2, strip.w - 2 * margin);
    }
    draw_column(rows, count);
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

// A function to load the bindings from config.ini as it is now, whose lines the save finds again by
// their text. Bindings that cannot be read (the file gone, or out of memory) leave the binding pages
// out, and say so.
static void load_bindings(void)
{
    static const char *const skip[] = { SETTING_GAMEPAD_ENABLED, SETTING_GAMEPAD_DEVICE, SETTING_GAMEPAD_MAPPINGS_FILE, NULL };
    size_t length = 0;
    char *text = fileio_read_all(config.config_path, &length);
    IniDoc *doc = text != NULL ? inidoc_parse(text, length) : NULL;
    alloc_free(text);
#ifdef _WIN32
    bindings = bindings_create(true, gamepad_running());
#else
    bindings = bindings_create(false, gamepad_running());
#endif
    bool loaded = bindings != NULL && doc != NULL && load_section(doc, "Hotkeys", NULL, BINDINGS_KEYBOARD) &&
                  load_section(doc, "Gamepad", skip, BINDINGS_GAMEPAD);
    inidoc_free(doc);
    if (!loaded) {
        log_error("Settings: the bindings could not be read from %s, so the binding pages are left out", config.config_path);
        bindings_free(bindings);
        bindings = NULL;
        return;
    }
    settings_set_bindings(model, bindings, pickers_key_name);
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
    load_bindings();
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
    PickerHost host = {
        .model = model, .menus = menus, .menu_count = menu_count, .apply = apply_slot, .event = handle_event,
        .quiet = handle_quiet, .text = draw_text, .row = draw_row, .font_row = font_row, .font_small = font_small,
        .row_height = row_height, .column_width = column_width, .margin = margin
    };
    pickers_begin(&host);
    bool targets = SDL_RenderTargetSupported(renderer);
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
            log_debug("Settings: the preview is at %i,%i, %i x %i", preview_rect.x, preview_rect.y,
                preview_rect.w, preview_rect.h);
        }
    }
    log_debug("Settings opened over menu '%s'", origin->name);
}

// A function to let go of settings at quit, saving nothing
void settings_close_now(void)
{
    if (model != NULL)
        free_screen();
}
