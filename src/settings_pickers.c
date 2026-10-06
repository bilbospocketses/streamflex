#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include "launcher.h"
#include <launcher_config.h>
#include "settings.h"
#include "settings_pickers.h"
#include "listpick.h"
#include "colourpick.h"
#include "fontlist.h"
#include "fontscan.h"
#include "image.h"
#include "clock.h"
#include "test_hooks.h"
#include "util.h"
#include "debug.h"

extern Config config;
extern SDL_Renderer *renderer;
extern TextInfo title_info;
extern Clock *clk;

#define DOT " \xC2\xB7 "       // U+00B7 with a space either side
#define ALPHA_MARK 230         // The outline round the swatch under the cursor
#define FONT_SAMPLE_PERCENT 70 // A family's sample: its size in points, as a share of the row font's line height
#define FONT_FILES_PER_FRAME 6 // Files read per frame while the list loads
#define FONT_CACHE_SIZE 32     // Families drawn in their own face, kept while the picker is open
#define FONT_VALUE_MAX (SETTING_TEXT_MAX + 16)   // A font row's value: "<face>|<path>"

typedef enum {
    PICKER_NONE,
    PICKER_LIST,
    PICKER_COLOUR,
    PICKER_FONT               // A list picker whose list is NULL while the fonts load
} PickerKind;

static PickerHost host;
static PickerKind kind = PICKER_NONE;
static SettingSlot *slot = NULL;        // The setting the picker chooses for
static SettingValue original;           // Its value when the picker opened: Back puts it back
static ListPick *list = NULL;
static int list_first = 0;              // The list's first row on show
static int list_page = 1;               // How many of its rows fit: Left and Right page this far
static ColourPick colour;
static char note[512];                  // What the caption says, built as it is asked for
static int shown_first = -1;            // The list's rows last on show, for the log...
static int shown_last = -1;
static char shown_cursor[LISTPICK_TEXT_MAX]; // ...and the label of the row under its cursor
static bool cells_logged = false;       // The colour picker's cells were logged since it opened
static char custom_shown[8];            // The Custom row's colour last drawn, for the log
static SettingColor previewed;          // The colour the launcher shows while the colour picker is open

static FontScan *font_scan = NULL;      // The listing thread, until its files are read
static FontList *fonts = NULL;          // The families, kept for the session once read
static int font_files_read = 0;
static int font_files_skipped = 0;
static bool font_read_failed = false;   // A face could not be added (out of memory): the list is let go
static Uint32 font_scan_start = 0;
static bool fonts_ready = false;
static int loading_logged = -1;         // The count "Loading fonts... (N)" last logged
static struct {
    char *path;                         // The face drawn; NULL for an empty place
    int face;
    SDL_Texture *texture;               // The family's name drawn in its own face; NULL when it would not draw
    int w;
    int h;
    Uint32 used;
} font_cache[FONT_CACHE_SIZE];
static Uint32 font_clock = 0;

// The special commands the command picker offers, in its order, after None
static const char *const SPECIALS[] = {
    SCMD_LEFT, SCMD_RIGHT, SCMD_UP, SCMD_DOWN, SCMD_SELECT, SCMD_BACK, SCMD_HOME, SCMD_SETTINGS,
    SCMD_QUIT, SCMD_SHUTDOWN, SCMD_RESTART, SCMD_SLEEP
};

// A function to set the pickers up as settings open
void pickers_begin(const PickerHost *given)
{
    host = *given;
    kind = PICKER_NONE;
}

// A function to get a family's name drawn in its own face, from the cache. A face that cannot be
// opened or drawn is kept as such, so it is tried once: NULL, and its row draws in the settings'
// font. Each draw is logged, as it happens once per family on show.
static SDL_Texture *font_sample(const char *path, int face, const char *name, int *w, int *h)
{
    font_clock++;
    int oldest = 0;
    for (int i = 0; i < FONT_CACHE_SIZE; i++) {
        if (font_cache[i].path != NULL && font_cache[i].face == face && strcmp(font_cache[i].path, path) == 0) {
            font_cache[i].used = font_clock;
            *w = font_cache[i].w;
            *h = font_cache[i].h;
            return font_cache[i].texture;
        }
        if (font_cache[i].used < font_cache[oldest].used)
            oldest = i;
    }
    if (font_cache[oldest].texture != NULL)
        SDL_DestroyTexture(font_cache[oldest].texture);
    free(font_cache[oldest].path);
    memset(&font_cache[oldest], 0, sizeof(font_cache[oldest]));
    font_cache[oldest].path = strdup(path);
    font_cache[oldest].face = face;
    font_cache[oldest].used = font_clock;
    TTF_Font *font = TTF_OpenFontIndex(path, TTF_FontHeight(host.font_row) * FONT_SAMPLE_PERCENT / 100, face);
    SDL_Surface *surface = NULL;
    if (font != NULL) {
        SDL_Color white = { 0xFF, 0xFF, 0xFF, 0xFF };
        surface = TTF_RenderUTF8_Blended(font, name, white);
        TTF_CloseFont(font);
    }
    if (surface != NULL) {
        font_cache[oldest].texture = SDL_CreateTextureFromSurface(renderer, surface);
        font_cache[oldest].w = surface->w;
        font_cache[oldest].h = surface->h;
        SDL_FreeSurface(surface);
    }
    if (font_cache[oldest].texture == NULL) {
        log_debug("Settings: the font picker could not draw %s in its own face", name);
        return NULL;
    }
    log_debug("Settings: the font picker drew %s in its own face", name);
    *w = font_cache[oldest].w;
    *h = font_cache[oldest].h;
    return font_cache[oldest].texture;
}

// A function to empty the family samples' cache
static void clear_font_cache(void)
{
    for (int i = 0; i < FONT_CACHE_SIZE; i++) {
        if (font_cache[i].texture != NULL)
            SDL_DestroyTexture(font_cache[i].texture);
        free(font_cache[i].path);
    }
    memset(font_cache, 0, sizeof(font_cache));
}

// A function to close the picker on show, if any. No picker open means the slot holds the colour
// the colour picker opened with: whatever the preview put in it goes, however the picker closes (a
// choice is made from that original). It is not applied here: as settings close, the fonts and the
// screen it would be drawn with may be gone, and a Back or Home applies it before closing. The
// font picker's samples go with it: they are drawn again when it next opens.
static void close_picker(void)
{
    if (kind == PICKER_COLOUR)
        slot->value = original;
    if (kind != PICKER_NONE)
        log_debug("Settings: closed the picker for [%s] %s", slot->def->section, slot->def->key);
    clear_font_cache();
    listpick_free(list);
    list = NULL;
    kind = PICKER_NONE;
    slot = NULL;
    note[0] = '\0';
}

// A function to let go of everything as settings close
void pickers_end(void)
{
    close_picker();
}

// A function to tell whether a picker is open
bool pickers_active(void)
{
    return kind != PICKER_NONE;
}

// A function to forget what the log last said of the list, so a list made anew says it again
static void forget_list_log(void)
{
    list_first = 0;
    shown_first = -1;
    shown_last = -1;
    shown_cursor[0] = '\0';
}

// A function to add a command to the command picker once, labelled as the screen describes it;
// false when out of memory
static bool add_command(ListPick *to, const char *command, const char *label)
{
    char text[LISTPICK_TEXT_MAX];
    if (listpick_has(to, command))
        return true;
    if (label == NULL) {
        setting_command_label(command, text, sizeof(text));
        label = text;
    }
    return listpick_add(to, label, command, true, NULL);
}

// A function to fill the command picker: None, the special commands, each menu as a submenu, on
// Windows the exit hotkey's command, then every command the menus' entries run, by entry title;
// false when out of memory
static bool fill_commands(ListPick *to)
{
    char text[LISTPICK_TEXT_MAX];
    if (!listpick_add(to, "None", "", true, NULL))
        return false;
    for (size_t i = 0; i < sizeof(SPECIALS) / sizeof(SPECIALS[0]); i++) {
        if (!add_command(to, SPECIALS[i], NULL))
            return false;
    }
    for (int m = 0; m < host.menu_count; m++) {
        snprintf(text, sizeof(text), SCMD_SUBMENU " %s", host.menus[m]->name);
        if (!add_command(to, text, NULL))
            return false;
    }
#ifdef _WIN32
    if (!add_command(to, SCMD_EXIT, NULL))
        return false;
#endif
    for (int m = 0; m < host.menu_count; m++) {
        for (Entry *e = host.menus[m]->first_entry; e != NULL; e = e->next) {
            if (!add_command(to, e->cmd, e->title))
                return false;
        }
    }
    return true;
}

// A function to tell a path's separator: '/', and on Windows '\' too
static bool is_separator(char c)
{
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

// A function to find a path's file name
static const char *base_name(const char *path)
{
    const char *base = path;
    for (const char *p = path; *p != '\0'; p++) {
        if (is_separator(*p))
            base = p + 1;
    }
    return base;
}

// A function to find the bundled fonts' folder (caller frees): the one the bundled title font is
// in, beside the executable or where the packages put it; NULL when it is not there
static char *bundled_fonts_folder(void)
{
    char *font = find_default_font(FILENAME_DEFAULT_FONT);
    if (font == NULL)
        return NULL;
    size_t length = strlen(font);
    while (length > 0 && !is_separator(font[length - 1]))
        length--;
    font[length > 0 ? length - 1 : 0] = '\0';
    return font;
}

// A function to write a face as a font row's value: "<face>|<path>"
static void font_value(const char *path, int face, char *out, size_t size)
{
    snprintf(out, size, "%d|%s", face, path);
}

// A function to read the file from a font row's value
static const char *font_value_path(const char *value)
{
    const char *bar = strchr(value, '|');
    return bar != NULL ? bar + 1 : value;
}

// A function to write the value of the font a setting uses now: the file and face the titles or
// the clock opened (the configured one, or the bundled font they fell back to), as the family that
// holds that face writes it; a face no family holds (a file outside the folders listed) as itself.
// The clock's row is greyed while the clock is off, and a clock that is on has opened its font
// (init_clock() failing quits through log_fatal), so clk is there.
static void font_list_value(const SettingSlot *s, char *out, size_t size)
{
    const TextInfo *info = s->def->id == SET_ID_TITLE_FONT ? &title_info : &clk->text_info;
    const char *path = info->font_path != NULL ? info->font_path : "";
    int family = fontlist_find(fonts, path, info->font_face);
    if (family >= 0)
        font_value(fontlist_path(fonts, family), fontlist_face(fonts, family), out, size);
    else
        font_value(path, info->font_face, out, size);
}

// A function to fill the font picker from the font list, one row a family; false when out of memory
static bool fill_fonts(ListPick *to)
{
    char value[FONT_VALUE_MAX];
    for (int i = 0; i < fontlist_count(fonts); i++) {
        font_value(fontlist_path(fonts, i), fontlist_face(fonts, i), value, sizeof(value));
        if (!listpick_add(to, fontlist_family(fonts, i), value, true, NULL))
            return false;
    }
    return true;
}

// A function to read one file's faces into the font list, on the main thread (SDL_ttf's one
// FreeType library is not safe on two): every face with a family name, and a glyph for each of
// "Aa0" (symbol and emoji fonts have none), is added. A face that could not be added for want of
// memory marks the list as failed.
static void read_font_file(const char *path, bool bundled)
{
    TTF_Font *font = TTF_OpenFontIndex(path, 12, 0);
    if (font == NULL) {
        font_files_skipped++;
        return;
    }
    long faces = TTF_FontFaces(font);
    TTF_CloseFont(font);
    for (long i = 0; i < faces && i < 64; i++) {
        TTF_Font *face = TTF_OpenFontIndex(path, 12, i);
        if (face == NULL)
            continue;
        const char *family = TTF_FontFaceFamilyName(face);
        const char *style = TTF_FontFaceStyleName(face);
        if (family != NULL && TTF_GlyphIsProvided(face, 'A') && TTF_GlyphIsProvided(face, 'a') &&
            TTF_GlyphIsProvided(face, '0')) {
            test_fail("faces", true);
            bool added = fontlist_add(fonts, path, (int) i, family, style != NULL ? style : "", bundled);
            test_fail("faces", false);
            if (!added)
                font_read_failed = true;
        }
        TTF_CloseFont(face);
    }
}

// A function to fill a list for a menu, device, command or font setting; false when out of memory
static bool fill_list(ListPick *to, const SettingSlot *s)
{
    char text[LISTPICK_TEXT_MAX];
    if (s->def->type == SET_TYPE_FONT)
        return fill_fonts(to);
    if (s->def->type == SET_TYPE_MENU) {
        for (int m = 0; m < host.menu_count; m++) {
            if (!listpick_add(to, host.menus[m]->name, host.menus[m]->name, true, NULL))
                return false;
        }
        return true;
    }
    if (s->def->type == SET_TYPE_DEVICE) {
        if (!listpick_add(to, "Any", "-1", true, NULL))
            return false;
        for (int i = 0; i < settings_pad_count(host.model); i++) {
            snprintf(text, sizeof(text), "%d", i);
            if (!listpick_add(to, settings_pad_name(host.model, i), text, true, NULL))
                return false;
        }
        return true;
    }
    return fill_commands(to);
}

// A function to write a setting's value as the list picker's value text: for a font, the font in use
static void list_value(const SettingSlot *s, char *out, size_t size)
{
    if (s->def->type == SET_TYPE_DEVICE)
        snprintf(out, size, "%d", s->value.number);
    else if (s->def->type == SET_TYPE_FONT)
        font_list_value(s, out, size);
    else
        snprintf(out, size, "%s", s->value.inherit ? "" : s->value.text);
}

// A function to make the list for a menu, device, command or font setting, with the cursor on the
// row that gives `at`. A value no row gives is pinned first, named as its row would name it: a device
// index with no pad as the Device row does, a font by its file's name, anything else "Custom: ...".
// NULL when out of memory,
// whichever step ran out: a list short of rows, or without the file's value, would misstate it.
// Each step can be made to fail by the harness (test_fail(), STREAMFLEX_TEST_FAIL).
static ListPick *make_list(const SettingSlot *s, const char *at)
{
    char custom[LISTPICK_TEXT_MAX + 32];
    test_fail("list", true);
    ListPick *made = listpick_create();
    test_fail("list", false);
    if (made == NULL)
        return NULL;
    test_fail("rows", true);
    bool filled = fill_list(made, s);
    test_fail("rows", false);
    if (s->def->type == SET_TYPE_DEVICE)
        snprintf(custom, sizeof(custom), "Pad %s (not connected)", at);
    else if (s->def->type == SET_TYPE_FONT)
        snprintf(custom, sizeof(custom), "Custom: %s", base_name(font_value_path(at)));
    else
        snprintf(custom, sizeof(custom), "Custom: %s", at);
    test_fail("select", true);
    bool selected = filled && listpick_select(made, at, custom);
    test_fail("select", false);
    if (!selected) {
        listpick_free(made);
        return NULL;
    }
    return made;
}

// A function to open the list picker for a menu, device, command or font setting (the fonts read).
// Out of memory it does not open, and says so: the keys stay with the page.
static void open_list(SettingSlot *s)
{
    char value[FONT_VALUE_MAX];
    list_value(s, value, sizeof(value));
    list = make_list(s, value);
    if (list == NULL) {
        log_error("Settings: the list cannot open: out of memory");
        return;
    }
    if (s->def->type == SET_TYPE_COMMAND)
        log_debug("Settings: the command picker lists %i rows", listpick_count(list));
    forget_list_log();
    kind = s->def->type == SET_TYPE_FONT ? PICKER_FONT : PICKER_LIST;
}

// A function to open the font picker: its list at once when the fonts were read before, else
// "Loading fonts..." while they are listed, on a thread (a listing under way goes on) and then read
// a few files a frame. A listing that cannot start does not open the picker, and says so.
static void open_fonts(SettingSlot *s)
{
    if (fonts_ready) {
        open_list(s);
        return;
    }
    if (font_scan == NULL) {
        char *folder = bundled_fonts_folder();
        test_fail("fontlist", true);
        fonts = fontlist_create();
        test_fail("fontlist", false);
        test_fail("fontscan", true);
        font_scan = fonts != NULL ? fontscan_start(folder) : NULL;
        test_fail("fontscan", false);
        free(folder);
        if (font_scan == NULL) {
            fontlist_free(fonts);
            fonts = NULL;
            log_error("Settings: the fonts cannot be listed: out of memory, or no thread");
            return;
        }
        font_scan_start = SDL_GetTicks();
        font_files_read = 0;
        font_files_skipped = 0;
        font_read_failed = false;
        log_debug("Fonts: listing the font files");
    }
    loading_logged = -1;
    kind = PICKER_FONT;
}

// A function to open the picker a setting's row asks for: the colour picker, the font picker, or
// the list picker for the default menu, the device or a command
void pickers_open(SettingSlot *s)
{
    close_picker();
    SettingType type = s->def->type;
    if (type != SET_TYPE_COLOR && type != SET_TYPE_MENU && type != SET_TYPE_DEVICE && type != SET_TYPE_COMMAND &&
        type != SET_TYPE_FONT)
        return;
    slot = s;
    original = s->value;
    if (type == SET_TYPE_COLOR) {
        colourpick_open(&colour, s->value.color);
        previewed = s->value.color;
        cells_logged = false;
        custom_shown[0] = '\0';
        kind = PICKER_COLOUR;
    }
    else if (type == SET_TYPE_FONT)
        open_fonts(s);
    else
        open_list(s);
    if (kind == PICKER_NONE) {
        slot = NULL;
        return;
    }
    log_debug("Settings: opened the picker for [%s] %s", s->def->section, s->def->key);
}

// A function to make an open Device list again after a pad was plugged in or pulled out, so it
// names the pads present now. The cursor stays on the value it was on while a row still gives it,
// else goes back to the setting's value. Out of memory, the list closes: a stale one would offer
// pads that are gone.
void pickers_pads_changed(void)
{
    if (kind != PICKER_LIST || slot->def->type != SET_TYPE_DEVICE)
        return;
    char was[LISTPICK_TEXT_MAX];
    char value[LISTPICK_TEXT_MAX];
    const ListPickRow *row = listpick_row(list, listpick_cursor(list));
    copy_string(was, row != NULL ? row->value : "", sizeof(was));
    list_value(slot, value, sizeof(value));
    test_fail("pads", true);
    ListPick *made = make_list(slot, value);
    test_fail("pads", false);
    if (made == NULL) {
        log_error("Settings: the pads changed, and the list could not be made again: out of memory");
        close_picker();
        return;
    }
    listpick_free(list);
    list = made;
    forget_list_log();
    listpick_move_to(list, was);   // Else the cursor stays on the setting's value, where make_list() put it
    log_debug("Settings: the pads changed, so the list was made again");
}

// A function to fill the font picker on show once the fonts are read. Out of memory (the faces read,
// or the list made) it closes, and says so: the keys go back to the page.
static void fill_open_fonts(void)
{
    char value[FONT_VALUE_MAX];
    if (fonts_ready) {
        list_value(slot, value, sizeof(value));
        list = make_list(slot, value);
    }
    if (list == NULL) {
        log_error("Settings: the list cannot open: out of memory");
        close_picker();
        return;
    }
    forget_list_log();
}

// A function to read more of the font files each frame once the thread has listed them, then put the
// families in order, keep them for the session, and fill the font picker if it is open. A list a
// face could not be added to is let go, so the next opening lists the files anew.
void pickers_tick(void)
{
    if (font_scan == NULL || !fontscan_done(font_scan))
        return;
    int total = fontscan_count(font_scan);
    for (int n = 0; n < FONT_FILES_PER_FRAME && font_files_read < total; n++, font_files_read++)
        read_font_file(fontscan_file(font_scan, font_files_read), fontscan_bundled(font_scan, font_files_read));
    if (font_files_read < total)
        return;
    fontscan_free(font_scan);
    font_scan = NULL;
    if (font_read_failed) {
        fontlist_free(fonts);
        fonts = NULL;
        log_error("Fonts: out of memory while reading the faces, so the list was let go");
    }
    else {
        fontlist_finish(fonts);
        fonts_ready = true;
        log_debug("Fonts: found %i families in %i files, skipped %i (%u ms)", fontlist_count(fonts), total,
            font_files_skipped, SDL_GetTicks() - font_scan_start);
    }
    if (kind == PICKER_FONT)
        fill_open_fonts();
}

// A function to let go of the font list and its listing at quit: they are kept while the launcher
// runs. A listing under way is waited for. The log says what went, as LeakSanitizer cannot see
// memory a static still points to.
void pickers_quit(void)
{
    close_picker();
    if (font_scan != NULL) {
        fontscan_free(font_scan);
        font_scan = NULL;
        log_debug("Fonts: waited for the font scan at quit");
    }
    if (fonts != NULL) {
        fontlist_free(fonts);
        fonts = NULL;
        log_debug("Fonts: let go of the font list at quit");
    }
    fonts_ready = false;
}

// A function to hand a chosen value to the pages, which apply and log it, and close the picker.
// A value the setting already had changes nothing: the preview already shows it.
static void choose(SettingValue value)
{
    SettingSlot *s = slot;
    close_picker();   // It puts back the original the colour picker's preview changed: the choice is made from it
    SettingsEvent event = settings_choose_value(host.model, s, &value);
    if (event.kind == SETTINGS_EVENT_NONE)
        log_debug("Settings: [%s] %s is unchanged", s->def->section, s->def->key);
    else
        host.event(&event);
}

// A function to show a colour in the preview, live, without choosing it. A key that leaves the
// colour as it is (a hex digit chosen, the editor opened or left) applies nothing: for the titles,
// each apply is a reload.
static void preview_colour(SettingColor shown)
{
    if (shown.r == previewed.r && shown.g == previewed.g && shown.b == previewed.b)
        return;
    previewed = shown;
    slot->value = original;
    slot->value.color = shown;
    host.apply(slot, true);
    log_debug("Settings: previewing #%02X%02X%02X", shown.r, shown.g, shown.b);
}

// A function to put back the colour the colour picker opened with, which the preview may have changed
static void put_colour_back(void)
{
    slot->value = original;
    host.apply(slot, true);
    log_debug("Settings: the colour picker put [%s] %s back", slot->def->section, slot->def->key);
}

// A function to choose a font row's family: its face is stored quietly first, then its file with
// the refresh that opens it (the pages store the font and run the title font's or the clock's group
// once). The same file's other face leaves the font's own value as it was, so the refresh is run
// here. `chosen` is copied before the list goes.
static void choose_font(const char *chosen)
{
    SettingValue font = original;
    int face = atoi(chosen);
    snprintf(font.text, sizeof(font.text), "%s", font_value_path(chosen));
    SettingId face_id = slot->def->id == SET_ID_TITLE_FONT ? SET_ID_TITLE_FONT_FACE : SET_ID_CLOCK_FONT_FACE;
    SettingSlot *face_slot = settings_slot(host.model, face_id, -1);
    SettingValue face_value = face_slot->value;
    face_value.inherit = face == 0;
    face_value.number = face;
    SettingsEvent quiet = settings_choose_value(host.model, face_slot, &face_value);
    host.quiet(&quiet);
    if (quiet.kind != SETTINGS_EVENT_NONE && strcmp(font.text, original.text) == 0) {
        SettingSlot *s = slot;
        close_picker();
        host.apply(s, true);
        return;
    }
    choose(font);
}

// A function to act on a key in the list picker, or the font picker. While the fonts load, the font
// picker has no list: Back closes it, and every other key does nothing.
static void list_command(const char *command)
{
    if (list == NULL) {
        if (MATCH(command, SCMD_BACK))
            close_picker();
        return;
    }
    ListPickCommand key;
    if (MATCH(command, SCMD_UP))
        key = LISTPICK_UP;
    else if (MATCH(command, SCMD_DOWN))
        key = LISTPICK_DOWN;
    else if (MATCH(command, SCMD_LEFT))
        key = LISTPICK_PAGE_UP;
    else if (MATCH(command, SCMD_RIGHT))
        key = LISTPICK_PAGE_DOWN;
    else if (MATCH(command, SCMD_SELECT))
        key = LISTPICK_OK;
    else
        key = LISTPICK_BACK;
    ListPickResult result = listpick_command(list, key, list_page);
    if (result == LISTPICK_CANCELLED)
        close_picker();
    else if (result == LISTPICK_CHOSEN && kind == PICKER_FONT)
        choose_font(listpick_chosen(list));
    else if (result == LISTPICK_CHOSEN) {
        // The chosen text lives in the list, which choose() frees: it is copied first
        SettingValue value = original;
        const char *chosen = listpick_chosen(list);
        value.inherit = false;
        if (slot->def->type == SET_TYPE_DEVICE)
            value.number = atoi(chosen);
        else {
            snprintf(value.text, sizeof(value.text), "%s", chosen);
            value.inherit = slot->def->can_inherit && chosen[0] == '\0';
        }
        choose(value);
    }
}

// A function to act on a key in the colour picker
static void colour_command(const char *command)
{
    ColourPickCommand key;
    if (MATCH(command, SCMD_UP))
        key = COLOURPICK_UP;
    else if (MATCH(command, SCMD_DOWN))
        key = COLOURPICK_DOWN;
    else if (MATCH(command, SCMD_LEFT))
        key = COLOURPICK_LEFT;
    else if (MATCH(command, SCMD_RIGHT))
        key = COLOURPICK_RIGHT;
    else if (MATCH(command, SCMD_SELECT))
        key = COLOURPICK_OK;
    else
        key = COLOURPICK_BACK;
    ColourPickResult result = colourpick_command(&colour, key);
    if (result == COLOURPICK_MOVED)
        preview_colour(colourpick_shown(&colour));
    else if (result == COLOURPICK_CANCELLED) {
        put_colour_back();
        close_picker();
    }
    else if (result == COLOURPICK_CHOSEN) {
        SettingValue value = original;
        value.color = colour.chosen;
        choose(value);
    }
}

// A function to act on a key while a picker is open. Home and the key that opened settings leave
// the picker without choosing (the colour picker puts its colour back first, even from the hex
// editor, where Back would only leave the editor), then close settings as the pages would.
void pickers_command(const char *command)
{
    if (MATCH(command, SCMD_HOME) || MATCH(command, SCMD_SETTINGS)) {
        if (kind == PICKER_COLOUR)
            put_colour_back();
        close_picker();
        SettingsEvent event = settings_command(host.model, MATCH(command, SCMD_HOME) ? SETTINGS_HOME : SETTINGS_CLOSE);
        host.event(&event);
        return;
    }
    if (!MATCH(command, SCMD_UP) && !MATCH(command, SCMD_DOWN) && !MATCH(command, SCMD_LEFT) &&
        !MATCH(command, SCMD_RIGHT) && !MATCH(command, SCMD_SELECT) && !MATCH(command, SCMD_BACK)) {
        log_debug("Settings: ignoring '%s' while a picker is open", command);
        return;
    }
    if (kind == PICKER_LIST || kind == PICKER_FONT)
        list_command(command);
    else if (kind == PICKER_COLOUR)
        colour_command(command);
}

// A function to draw a list row: in the font picker, a family's name in its own face over an empty
// row's highlight (the Custom row, and a face that would not draw, in the settings' font); returns
// the height it took
static int draw_list_row(const ListPickRow *row, bool highlighted, int x, int y)
{
    SettingsRow shown;
    memset(&shown, 0, sizeof(shown));
    shown.kind = SETTINGS_ROW_ACTION;
    shown.enabled = row->enabled;
    int w = 0;
    int h = 0;
    SDL_Texture *sample = kind == PICKER_FONT && !row->custom
        ? font_sample(font_value_path(row->value), atoi(row->value), row->label, &w, &h) : NULL;
    if (sample == NULL)
        copy_string(shown.label, row->label, sizeof(shown.label));
    int drawn = host.row(&shown, highlighted, x, y, host.column_width, 0);
    if (sample != NULL) {
        int room = host.column_width - host.margin;
        SDL_Rect rect = { x + host.margin / 2, y + (drawn - h) / 2, w < room ? w : room, h };
        SDL_Rect from = { 0, 0, rect.w, h };
        SDL_RenderCopy(renderer, sample, &from, &rect);
    }
    return drawn;
}

// A function to draw the list picker's rows, scrolled to keep the cursor in view, and log the rows
// on show and the one under the cursor when they change
static void draw_list(int x, int top, int bottom)
{
    int count = listpick_count(list);
    int cursor = listpick_cursor(list);
    list_page = (bottom - top) / host.row_height > 1 ? (bottom - top) / host.row_height : 1;
    if (cursor < list_first)
        list_first = cursor;
    if (cursor >= list_first + list_page)
        list_first = cursor - list_page + 1;
    int y = top;
    int last = list_first - 1;
    for (int i = list_first; i < count && y + host.row_height <= bottom; i++) {
        y += draw_list_row(listpick_row(list, i), i == cursor, x, y);
        last = i;
    }
    if (list_first != shown_first || last != shown_last) {
        shown_first = list_first;
        shown_last = last;
        log_debug("Settings: the list shows rows %i to %i of %i", list_first, last, count);
    }
    const ListPickRow *at = listpick_row(list, cursor);
    if (at != NULL && strcmp(at->label, shown_cursor) != 0) {
        copy_string(shown_cursor, at->label, sizeof(shown_cursor));
        log_debug("Settings: the list's cursor reads %s", shown_cursor);
    }
}

// A function to draw the colour picker: the swatches, the current one marked and the one under the
// cursor outlined, then the Custom row, and in the hex editor each digit with the chosen one boxed
static void draw_colour(int x, int top, int bottom)
{
    int pad = host.margin / 2;
    int cell = (host.column_width - 2 * pad) / COLOURPICK_COLUMNS;
    // The swatches, the Custom row and the hex editor's row all end above the key hint, so a short,
    // wide screen gets smaller swatches
    int fits = (bottom - top - pad - 2 * host.row_height) / COLOURPICK_ROWS;
    if (cell > fits)
        cell = fits;
    int current = colourpick_find(colour.original);
    if (!cells_logged) {
        cells_logged = true;
        log_debug("Settings: the colour picker draws %i px cells from %i,%i, down to %i of %i", cell, x + pad, top,
            top + COLOURPICK_ROWS * cell + pad + 2 * host.row_height, bottom);
    }
    for (int i = 0; i < COLOURPICK_SWATCHES; i++) {
        SettingColor c = colourpick_swatch(i);
        SDL_Rect box = { x + pad + (i % COLOURPICK_COLUMNS) * cell + 3, top + (i / COLOURPICK_COLUMNS) * cell + 3,
                         cell - 6, cell - 6 };
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 0xFF);
        SDL_RenderFillRect(renderer, &box);
        if (i == current) {
            SDL_Rect mark = { box.x + box.w / 3, box.y + box.h / 3, box.w / 3, box.h / 3 };
            SDL_SetRenderDrawColor(renderer, (Uint8) (0xFF - c.r), (Uint8) (0xFF - c.g), (Uint8) (0xFF - c.b), 0xFF);
            SDL_RenderFillRect(renderer, &mark);
        }
        if (i == colour.cursor) {
            SDL_Rect outline = { box.x - 3, box.y - 3, box.w + 6, box.h + 6 };
            SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_MARK);
            SDL_RenderDrawRect(renderer, &outline);
            SDL_Rect inner = { outline.x + 1, outline.y + 1, outline.w - 2, outline.h - 2 };
            SDL_RenderDrawRect(renderer, &inner);
        }
    }
    int y = top + COLOURPICK_ROWS * cell + pad;
    SettingColor shown = colour.editing || colour.cursor == COLOURPICK_CUSTOM ? colour.hex : colour.original;
    SettingsRow custom;
    memset(&custom, 0, sizeof(custom));
    custom.kind = SETTINGS_ROW_ACTION;
    custom.enabled = true;
    snprintf(custom.label, sizeof(custom.label), "Custom");
    snprintf(custom.value, sizeof(custom.value), "#%02X%02X%02X", shown.r, shown.g, shown.b);
    if (strcmp(custom.value, custom_shown) != 0) {
        copy_string(custom_shown, custom.value, sizeof(custom_shown));
        log_debug("Settings: the Custom row reads %s", custom_shown);
    }
    y += host.row(&custom, colour.cursor == COLOURPICK_CUSTOM && !colour.editing, x, y, host.column_width, 0);
    if (!colour.editing || y + host.row_height > bottom)
        return;

    // The hex editor: # and six digits in cells as wide as the widest digit, the chosen one boxed
    char digits[8];
    snprintf(digits, sizeof(digits), "%02X%02X%02X", colour.hex.r, colour.hex.g, colour.hex.b);
    int w = 0;
    int h = 0;
    TTF_SizeUTF8(host.font_row, "W", &w, &h);
    int step = w + pad;
    host.text(host.font_row, "#", x + pad, y, step, 255, false);
    for (int i = 0; i < 6; i++) {
        char one[2] = { digits[i], '\0' };
        int cx = x + pad + (i + 1) * step;
        host.text(host.font_row, one, cx, y, step, 255, false);
        if (i == colour.digit) {
            SDL_Rect box = { cx - pad / 2, y, step, h };
            SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_MARK);
            SDL_RenderDrawRect(renderer, &box);
        }
    }
}

// A function to draw the font picker: "Loading fonts... (N)" while the files are listed and read,
// logged as N changes; then its list
static void draw_fonts(int x, int top, int bottom)
{
    if (list != NULL) {
        draw_list(x, top, bottom);
        return;
    }
    char text[64];
    snprintf(text, sizeof(text), "Loading fonts\xE2\x80\xA6 (%i)", font_files_read);
    host.text(host.font_row, text, x + host.margin / 2, top, host.column_width - host.margin, 255, false);
    if (font_files_read != loading_logged) {
        loading_logged = font_files_read;
        log_debug("Settings: the font picker reads %s", text);
    }
}

// A function to draw the picker on show in the column, between two heights
void pickers_draw(int x, int top, int bottom)
{
    if (kind == PICKER_LIST)
        draw_list(x, top, bottom);
    else if (kind == PICKER_COLOUR)
        draw_colour(x, top, bottom);
    else if (kind == PICKER_FONT)
        draw_fonts(x, top, bottom);
}

// A function to write the column's path line: the page's path, then the setting being chosen
void pickers_path(char *out, size_t size)
{
    settings_path(host.model, out, size);
    size_t used = strlen(out);
    if (slot != NULL)
        snprintf(out + used, size - used, " \xE2\x80\xBA %s", slot->def->label);
}

// A function to give the key hint for the picker on show
const char *pickers_hint(void)
{
    if (kind == PICKER_COLOUR && colour.editing)
        return "Left and right choose a digit" DOT "Up and down change it" DOT "OK keeps" DOT "Back returns";
    if (kind == PICKER_COLOUR)
        return "Arrows move" DOT "OK chooses" DOT "Back cancels";
    if (kind == PICKER_FONT && list == NULL)
        return "Back cancels";
    return "Left and right page" DOT "OK chooses" DOT "Back cancels";
}

// A function to warn when a title or clock colour stands out too little from what lies behind it:
// the background colour or the image on show (its mean luminance), under the overlay when it is on.
// It says nothing for a transparent background, or an image not yet measured.
void contrast_warning(SettingId id, SettingColor color, char *out, size_t size)
{
    out[0] = '\0';
    if (id != SET_ID_TITLE_COLOR && id != SET_ID_CLOCK_COLOR)
        return;
    double behind = 0.0;
    if (background_shown == BACKGROUND_COLOR) {
        SettingColor bg = { config.background_color.r, config.background_color.g, config.background_color.b };
        behind = colour_luminance(bg);
    }
    else if ((background_shown == BACKGROUND_IMAGE || background_shown == BACKGROUND_SLIDESHOW) && background_luminance >= 0.0)
        behind = background_luminance;
    else
        return;
    if (config.background_overlay) {
        SettingColor over = { eff.overlay_color.r, eff.overlay_color.g, eff.overlay_color.b };
        behind = colour_over(behind, over, eff.overlay_color.a);
    }
    double ratio = colour_contrast(colour_luminance(color), behind);
    if (ratio < COLOURPICK_MIN_CONTRAST)
        snprintf(out, size, "Low contrast: %.1f:1 against the background; 3:1 or more reads well", ratio);
}

// A function to say what the caption says while a picker is open: why OK did nothing in a list; in
// the colour picker, the colour under the cursor, and a contrast warning for it
const char *pickers_note(void)
{
    note[0] = '\0';
    if (kind == PICKER_LIST && listpick_why(list) != NULL)
        snprintf(note, sizeof(note), "%s", listpick_why(list));
    else if (kind == PICKER_COLOUR) {
        SettingColor shown = colourpick_shown(&colour);
        int index = colour.editing ? -1 : colourpick_find(shown);
        char warning[160];
        contrast_warning(slot->def->id, shown, warning, sizeof(warning));
        snprintf(note, sizeof(note), "%s #%02X%02X%02X%s%s", index >= 0 ? colourpick_name(index) : "Custom",
            shown.r, shown.g, shown.b, warning[0] != '\0' ? DOT : "", warning);
    }
    return note;
}
