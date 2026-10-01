#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include "launcher.h"
#include <launcher_config.h>
#include "settings.h"
#include "settings_pickers.h"
#include "listpick.h"
#include "colourpick.h"
#include "test_hooks.h"
#include "util.h"
#include "debug.h"

extern Config config;
extern SDL_Renderer *renderer;

#define DOT " \xC2\xB7 "       // U+00B7 with a space either side
#define ALPHA_MARK 230         // The outline round the swatch under the cursor

typedef enum {
    PICKER_NONE,
    PICKER_LIST,
    PICKER_COLOUR
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

// A function to close the picker on show, if any
static void close_picker(void)
{
    if (kind != PICKER_NONE)
        log_debug("Settings: closed the picker for [%s] %s", slot->def->section, slot->def->key);
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
    log_debug("Settings: the command picker lists %i rows", listpick_count(to));
    return true;
}

// A function to fill a list for a menu, device or command setting; false when out of memory
static bool fill_list(ListPick *to, const SettingSlot *s)
{
    char text[LISTPICK_TEXT_MAX];
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

// A function to write a setting's value as the list picker's value text
static void list_value(const SettingSlot *s, char *out, size_t size)
{
    if (s->def->type == SET_TYPE_DEVICE)
        snprintf(out, size, "%d", s->value.number);
    else
        snprintf(out, size, "%s", s->value.inherit ? "" : s->value.text);
}

// A function to make the list for a menu, device or command setting, with the cursor on the row
// that gives `at`. A value no row gives is pinned first, named as its row would name it: a device
// index with no pad as the Device row does, anything else "Custom: ...". NULL when out of memory,
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

// A function to open the list picker for a menu, device or command setting. Out of memory it does
// not open, and says so: the keys stay with the page.
static void open_list(SettingSlot *s)
{
    char value[LISTPICK_TEXT_MAX];
    list_value(s, value, sizeof(value));
    list = make_list(s, value);
    if (list == NULL) {
        log_error("Settings: the list cannot open: out of memory");
        return;
    }
    forget_list_log();
    kind = PICKER_LIST;
}

// A function to open the picker a setting's row asks for: the colour picker, or the list picker
// for the default menu, the device or a command. A font's row opens nothing yet.
void pickers_open(SettingSlot *s)
{
    close_picker();
    SettingType type = s->def->type;
    if (type != SET_TYPE_COLOR && type != SET_TYPE_MENU && type != SET_TYPE_DEVICE && type != SET_TYPE_COMMAND)
        return;
    slot = s;
    original = s->value;
    if (type == SET_TYPE_COLOR) {
        colourpick_open(&colour, s->value.color);
        cells_logged = false;
        custom_shown[0] = '\0';
        kind = PICKER_COLOUR;
    }
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
    int target = -1;
    for (int i = 0; i < listpick_count(list) && target < 0; i++) {
        if (strcmp(listpick_row(list, i)->value, was) == 0)
            target = i;
    }
    while (target >= 0 && listpick_cursor(list) != target) {
        ListPickCommand toward = listpick_cursor(list) < target ? LISTPICK_DOWN : LISTPICK_UP;
        if (listpick_command(list, toward, 1) != LISTPICK_MOVED)
            break;
    }
    log_debug("Settings: the pads changed, so the list was made again");
}

// A function to hand a chosen value to the pages, which apply and log it, and close the picker.
// A value the setting already had changes nothing: the preview already shows it.
static void choose(SettingValue value)
{
    SettingSlot *s = slot;
    s->value = original;   // The colour picker's preview changed it; the choice is made from the original
    close_picker();
    SettingsEvent event = settings_choose_value(host.model, s, &value);
    if (event.kind == SETTINGS_EVENT_NONE)
        log_debug("Settings: [%s] %s is unchanged", s->def->section, s->def->key);
    else
        host.event(&event);
}

// A function to show a colour in the preview, live, without choosing it
static void preview_colour(SettingColor shown)
{
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

// A function to act on a key in the list picker
static void list_command(const char *command)
{
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
    if (kind == PICKER_LIST)
        list_command(command);
    else if (kind == PICKER_COLOUR)
        colour_command(command);
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
        const ListPickRow *row = listpick_row(list, i);
        SettingsRow shown;
        memset(&shown, 0, sizeof(shown));
        shown.kind = SETTINGS_ROW_ACTION;
        shown.enabled = row->enabled;
        copy_string(shown.label, row->label, sizeof(shown.label));
        y += host.row(&shown, i == cursor, x, y, host.column_width, 0);
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
    int current = colourpick_find(colour.original);
    if (!cells_logged) {
        cells_logged = true;
        log_debug("Settings: the colour picker draws %i px cells from %i,%i", cell, x + pad, top);
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

// A function to draw the picker on show in the column, between two heights
void pickers_draw(int x, int top, int bottom)
{
    if (kind == PICKER_LIST)
        draw_list(x, top, bottom);
    else if (kind == PICKER_COLOUR)
        draw_colour(x, top, bottom);
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
