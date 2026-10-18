#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "launcher.h"
#include <launcher_config.h>
#include "settings.h"
#include "settings_pickers.h"
#include "settings_fonts.h"
#include "bindings.h"
#include "keycodes.h"
#include "listpick.h"
#include "settings_screen.h"
#include "colorpick.h"
#include "icongrid.h"
#include "library.h"
#include "fileio.h"
#include "image.h"
#include "test_hooks.h"
#include "util.h"
#include "debug.h"
#ifdef _WIN32
#include "platform/platform.h"
#endif

extern Config config;
extern SDL_Renderer *renderer;
extern Hotkey *hotkeys;
extern GamepadControl *gamepad_controls;

// The keys bindings.c knows by value, checked against SDL's own
SDL_COMPILE_TIME_ASSERT(key_return, BIND_KEY_RETURN == SDLK_RETURN);
SDL_COMPILE_TIME_ASSERT(key_backspace, BIND_KEY_BACKSPACE == SDLK_BACKSPACE);
SDL_COMPILE_TIME_ASSERT(key_left, BIND_KEY_LEFT == SDLK_LEFT);
SDL_COMPILE_TIME_ASSERT(key_right, BIND_KEY_RIGHT == SDLK_RIGHT);
SDL_COMPILE_TIME_ASSERT(key_up, BIND_KEY_UP == SDLK_UP);
SDL_COMPILE_TIME_ASSERT(key_down, BIND_KEY_DOWN == SDLK_DOWN);
SDL_COMPILE_TIME_ASSERT(key_application, BIND_KEY_APPLICATION == SDLK_APPLICATION);
SDL_COMPILE_TIME_ASSERT(key_menu, BIND_KEY_MENU == SDLK_MENU);
SDL_COMPILE_TIME_ASSERT(key_f1, BIND_KEY_F1 == SDLK_F1);
SDL_COMPILE_TIME_ASSERT(key_f12, BIND_KEY_F12 == SDLK_F12);
SDL_COMPILE_TIME_ASSERT(key_f13, BIND_KEY_F13 == SDLK_F13);
SDL_COMPILE_TIME_ASSERT(key_f24, BIND_KEY_F24 == SDLK_F24);
SDL_COMPILE_TIME_ASSERT(osk_scancode_mask, OSK_SCANCODE_MASK == SDLK_SCANCODE_MASK);
// The keypad's keys osk_key_char() types (user ruling 2026-10-16), in SDL's order
SDL_COMPILE_TIME_ASSERT(osk_keypad_first, OSK_KEYPAD_FIRST == SDLK_KP_DIVIDE);
SDL_COMPILE_TIME_ASSERT(osk_keypad_enter, OSK_KEYPAD_FIRST + 4 == SDLK_KP_ENTER);
SDL_COMPILE_TIME_ASSERT(osk_keypad_one, OSK_KEYPAD_FIRST + 5 == SDLK_KP_1);
SDL_COMPILE_TIME_ASSERT(osk_keypad_zero, OSK_KEYPAD_FIRST + 14 == SDLK_KP_0);
SDL_COMPILE_TIME_ASSERT(osk_keypad_last, OSK_KEYPAD_LAST == SDLK_KP_PERIOD);
// The modifiers osk_key_mods() reads, as SDL has them
SDL_COMPILE_TIME_ASSERT(osk_mod_shift, OSK_MOD_SHIFT == SDL_KMOD_SHIFT);
SDL_COMPILE_TIME_ASSERT(osk_mod_ctrl, OSK_MOD_CTRL == SDL_KMOD_CTRL);
SDL_COMPILE_TIME_ASSERT(osk_mod_alt, OSK_MOD_ALT == SDL_KMOD_ALT);
SDL_COMPILE_TIME_ASSERT(osk_mod_ralt, OSK_MOD_RALT == SDL_KMOD_RALT);
SDL_COMPILE_TIME_ASSERT(osk_mod_caps, OSK_MOD_CAPS == SDL_KMOD_CAPS);
SDL_COMPILE_TIME_ASSERT(osk_mod_mode, OSK_MOD_MODE == SDL_KMOD_MODE);

#ifdef _WIN32
#define ALTGR_IS_RIGHT_ALT true    // SDL on Windows reports AltGr as Right Alt (osk_key_mods())
#else
#define ALTGR_IS_RIGHT_ALT false
#endif

#define DOT " \xC2\xB7 "       // U+00B7 with a space either side
#define ELLIPSIS "\xE2\x80\xA6" // U+2026
#define ALPHA_MARK 230         // The outline round the swatch under the cursor
#define ALPHA_GRID_TEXT 180    // The library grid's group headers and titles, but for the cursor's
#define ICONS_COLUMNS 4        // The library grid's icons across the column
#define TYPE_VALUE "\x1Ftype"  // The command picker's Type... row: a value no command in config.ini is
#define ALPHA_KEY 26           // A key's face on the keyboard...
#define ALPHA_KEY_LIT 90       // ...the highlighted one, and Shift while it is on
#define ALPHA_FIELD 30         // The keyboard's field
#define ALPHA_EDGE 220         // The outline round the highlighted key and the field
#define ALPHA_PANEL_EDGE 77    // The keyboard's own outline
#define ALPHA_LABEL 180        // The field's label
#define HOTKEY_PREFIX "#4000003A;"   // A typed binding's command is weighed as a hotkey's line holds it

static const SDL_Color PANEL = { 0x0B, 0x16, 0x20, 0xF2 };   // The keyboard's backing: the settings' backdrop

typedef enum {
    PICKER_NONE,
    PICKER_LIST,
    PICKER_COLOR,
    PICKER_FONT,              // A list picker whose list is NULL while the fonts load
    PICKER_APPS,              // A list picker whose list is NULL while the apps are found, or when none were
    PICKER_ICONS              // The library grid, for a tile's icon
} PickerKind;

static PickerHost host;
static PickerKind kind = PICKER_NONE;
static SettingSlot *slot = NULL;        // The setting the picker chooses for
static SettingValue original;           // Its value when the picker opened: Back puts it back
static ListPick *list = NULL;
static int list_first = 0;              // The list's first row on show
static int list_page = 1;               // How many of its rows fit: Left and Right page this far
static ColorPick color_pick;
static char note[512];                  // What the caption says, built as it is asked for
static int shown_first = -1;            // The list's rows last on show, for the log...
static int shown_last = -1;
static char shown_cursor[LISTPICK_TEXT_MAX]; // ...and the label of the row under its cursor
static bool cells_logged = false;       // The color picker's cells were logged since it opened
static char custom_shown[8];            // The Custom row's color last drawn, for the log
static SettingColor previewed;          // The color the launcher shows while the color picker is open

static int loading_logged = -1;         // The count "Loading fonts... (N)" last logged

static Capture capture;
static int capture_device = -1;         // The device being captured from; -1 when no capture runs
static int pad_held = -1;               // The pad's control held last frame, for its releases
static bool pad_swallowed = false;      // A pad held as its capture ended: its controls wait until let go
static int key_held = -1;               // The key last pressed and not yet let go: a capture's starting key
static int keys_swallowed[2] = { -1, -1 }; // Keys held as a keyboard capture ended: they wait until let go
static Probation probation;
static int probation_device = BINDINGS_KEYBOARD;
static char probation_command[BINDINGS_COMMAND_MAX];
static bool picking_command = false;    // The list picker chooses a binding's command
static unsigned int clock_now = 0;      // When the clocks last ran: the countdowns count from it, so they
                                        // never show a time the clocks have not acted on yet
static IconGrid *icon_grid = NULL;               // The library grid, while it is open
static void (*icon_shown)(const char *name) = NULL;   // Told the icon under the grid's cursor, for the preview
static void (*icon_done)(const char *name) = NULL;    // Told the icon chosen, or NULL when Back left the grid
static SDL_Texture **icon_textures = NULL;       // Each library icon's, by its place in the library, made at
static int icon_texture_count = 0;               // icon_size px once a settings visit; NULL where it failed
static int icon_size = 0;
static int icon_first_line = 0;                  // The grid's first line on show: a group's header or a row of icons

// The on-screen keyboard while it is open (NULL: closed), over a page or over the list it filters
static Osk *keyboard = NULL;
static char keyboard_initial[SETTING_TEXT_MAX];   // The text it opened with, which a cancel gives its follower
static void (*keyboard_done)(const char *text, void *context) = NULL;
static void *keyboard_context = NULL;
static void (*keyboard_follower)(const char *text, void *context) = NULL;
static unsigned int keyboard_rest = 0;            // How long the text rests before the follower hears it
static unsigned int keyboard_changed_at = 0;
static bool keyboard_pending = false;             // A change the follower has not heard yet
static bool keyboard_told = false;                // The follower heard a change
static bool keyboard_typed = false;               // A real keyboard typed into this field: its Enter is Done, its Backspace deletes
static void (*finishing_done)(const char *text, void *context) = NULL;   // The `done` Done is running, and its
static void *finishing_context = NULL;            // context: a keyboard it opens with both is the same field again
static bool finishing_typed = false;              // (a refused path), which keeps keyboard_typed as it was
static const char *keyboard_why = NULL;           // Why Done is off for the text now (the validator's, kept per change)
static bool keyboard_drawn = false;               // Where it is drawn was logged since it opened
static char keyboard_logged[SETTING_TEXT_MAX + 32];   // What the log last said of the text...
static char keyboard_key_logged[32];              // ...of the highlighted key...
static char keyboard_note_logged[320];            // ...and of the note
static char field_text[SETTING_TEXT_MAX];         // The field's text, cursor and room when draw_field() last
static size_t field_cursor = 0;                   // found where to draw the text from (room -1: not since the
static int field_room = -1;                       // keyboard opened, in whatever font it has then)...
static size_t field_start = 0;                    // ...and that place
static void (*typing_opens)(const char *first) = NULL;   // The open picker filters by typing; NULL: none

// A command typed in place of the command picker's rows: for a setting, or the binding page's binding
typedef struct {
    SettingSlot *slot;   // NULL: the binding
    char key[64];        // The key config.ini holds it under, for its limits
    char why[320];       // Why Done is off, written for the note
} TypedCommand;
static TypedCommand typed_command;

// The list the screen made (pickers_open_list()), and the installed apps' list (pickers_open_apps()), whose
// apps Task 7's scan finds (settings_apps(); it keeps them until settings close)
static char list_label[96];                       // The list's name in the path line; "" for a setting's
static void (*list_chosen)(const char *value) = NULL;
static bool apps_listed = false;                  // The app list's rows were made from the scan's list
static char app_filter[MENUEDIT_TITLE_MAX + 1];   // The app list's filter, as typed
static void (*app_chosen)(const AppItem *item) = NULL;

static void draw_list(int x, int top, int bottom);
static void make_app_list(void);   // pickers_tick() makes the rows once the scan has its list

static void close_keyboard(void);
static void cancel_keyboard(const char *why);

// The special commands the command picker offers, in its order, after None
static const char *const SPECIALS[] = {
    SCMD_LEFT, SCMD_RIGHT, SCMD_UP, SCMD_DOWN, SCMD_SELECT, SCMD_BACK, SCMD_HOME, SCMD_SETTINGS,
    SCMD_QUIT, SCMD_SHUTDOWN, SCMD_RESTART, SCMD_SLEEP
};

// A function to set the pickers up as settings open, or over a model made again
void pickers_begin(const PickerHost *given)
{
    host = *given;
    kind = PICKER_NONE;
    close_keyboard();
    typing_opens = NULL;
}

// A function to close the picker on show, if any. No picker open means the slot holds the color
// the color picker opened with: whatever the preview put in it goes, however the picker closes (a
// choice is made from that original). It is not applied here: as settings close, the fonts and the
// screen it would be drawn with may be gone, and a Back or Home applies it before closing. The
// font picker's samples go with it: they are drawn again when it next opens.
static void close_picker(void)
{
    if (kind == PICKER_COLOR)
        slot->value = original;
    if (kind != PICKER_NONE && slot != NULL)
        log_debug("Settings: closed the picker for [%s] %s", slot->def->section, slot->def->key);
    else if (picking_command)
        log_debug("Settings: closed the command picker for the binding");
    else if (kind == PICKER_ICONS)
        log_debug("Settings: closed the library grid");
    icongrid_free(icon_grid);
    icon_grid = NULL;
    fonts_clear_samples();
    listpick_free(list);
    list = NULL;
    kind = PICKER_NONE;
    slot = NULL;
    picking_command = false;
    typing_opens = NULL;
    list_chosen = NULL;
    list_label[0] = '\0';
    app_chosen = NULL;
    note[0] = '\0';
}

// A function to let go of everything as settings close: the picker, the keyboard (nothing said to its
// caller), a capture and the 10 s
void pickers_end(void)
{
    close_picker();
    pickers_drop_icon_textures();
    close_keyboard();
    capture_device = -1;
    probation.active = false;
    pad_swallowed = false;
    key_held = -1;
    keys_swallowed[0] = keys_swallowed[1] = -1;
}

// A function to tell whether a picker or the keyboard is open: either takes the remote's keys
bool pickers_active(void)
{
    return kind != PICKER_NONE || keyboard != NULL;
}

// A function to tell whether a picker draws the column: the keyboard alone leaves the page there
bool pickers_owns_column(void)
{
    return kind != PICKER_NONE;
}

// A function to tell whether the keyboard is open
bool pickers_keyboard_open(void)
{
    return keyboard != NULL;
}

// A function to forget what the log last said of the list, so a list made anew says it again
static void forget_list_log(void)
{
    list_first = 0;
    shown_first = -1;
    shown_last = -1;
    shown_cursor[0] = '\0';
}

// A function to add a command to the command picker once, labeled as the screen describes it;
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
// Windows the exit hotkey's command, every command the menus' entries run, by entry title, then Type...;
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
    // Last, any command at all, typed on the keyboard (3c): a :fork line, a path with its arguments
    return listpick_add(to, "Type" ELLIPSIS, TYPE_VALUE, true, NULL);
}

// A function to fill a list for a menu, device, command or font setting (or a binding's command);
// false when out of memory
static bool fill_list(ListPick *to, SettingType type)
{
    char text[LISTPICK_TEXT_MAX];
    if (type == SET_TYPE_FONT)
        return fonts_fill(to);
    if (type == SET_TYPE_MENU) {
        for (int m = 0; m < host.menu_count; m++) {
            if (!listpick_add(to, host.menus[m]->name, host.menus[m]->name, true, NULL))
                return false;
        }
        return true;
    }
    if (type == SET_TYPE_DEVICE) {
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
        fonts_value_in_use(s->def->id, out, size);
    else
        snprintf(out, size, "%s", s->value.inherit ? "" : s->value.text);
}

// A function to make the list for a menu, device, command or font setting, with the cursor on the
// row that gives `at`. A value no row gives is pinned first, named as its row would name it: a device
// index with no pad as the Device row does, a font by its file's name, anything else "Custom: ...".
// NULL when out of memory,
// whichever step ran out: a list short of rows, or without the file's value, would misstate it.
// Each step can be made to fail by the harness (test_fail(), STREAMFLEX_TEST_FAIL).
static ListPick *make_list(SettingType type, const char *at)
{
    char custom[LISTPICK_TEXT_MAX + 32];
    test_fail("list", true);
    ListPick *made = listpick_create();
    test_fail("list", false);
    if (made == NULL)
        return NULL;
    test_fail("rows", true);
    bool filled = fill_list(made, type);
    test_fail("rows", false);
    if (type == SET_TYPE_DEVICE)
        snprintf(custom, sizeof(custom), "Pad %s (not connected)", at);
    else if (type == SET_TYPE_FONT) {
        char name[LISTPICK_TEXT_MAX];
        fileio_base_name(fonts_value_path(at), name, sizeof(name));
        snprintf(custom, sizeof(custom), "Custom: %s", name);
    }
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
    list = make_list(s->def->type, value);
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
    if (fonts_ready()) {
        open_list(s);
        return;
    }
    if (!fonts_start())
        return;
    loading_logged = -1;
    kind = PICKER_FONT;
}

// A function to open the picker a setting's row asks for: the color picker, the font picker, or
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
        colorpick_open(&color_pick, s->value.color);
        previewed = s->value.color;
        cells_logged = false;
        custom_shown[0] = '\0';
        kind = PICKER_COLOR;
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
    if (kind != PICKER_LIST || slot == NULL || slot->def->type != SET_TYPE_DEVICE)
        return;   // No slot: a binding's command picker
    char was[LISTPICK_TEXT_MAX];
    char value[LISTPICK_TEXT_MAX];
    const ListPickRow *row = listpick_row(list, listpick_cursor(list));
    copy_string(was, row != NULL ? row->value : "", sizeof(was));
    list_value(slot, value, sizeof(value));
    test_fail("pads", true);
    ListPick *made = make_list(slot->def->type, value);
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
    if (fonts_ready()) {
        list_value(slot, value, sizeof(value));
        list = make_list(slot->def->type, value);
    }
    if (list == NULL) {
        log_error("Settings: the list cannot open: out of memory");
        close_picker();
        return;
    }
    forget_list_log();
}

// A function to name a key or button for the screen: a key by SDL's name for the key its stored code
// is ("Menu" for both of the Menu key's codes), a pad's control by its label; "#<HEX>" for a key SDL
// cannot name, or one SDL3 never sends
void pickers_key_name(int device, int code, char *out, size_t size)
{
    if (device == BINDINGS_GAMEPAD) {
        snprintf(out, size, "%s", bindings_label(code));
        return;
    }
    if (code == BIND_KEY_APPLICATION || code == BIND_KEY_MENU) {
        snprintf(out, size, "Menu");
        return;
    }
    const char *name = SDL_GetKeyName((SDL_Keycode) keycode_from_stored((uint32_t) code));
    if (name != NULL && name[0] != '\0')
        snprintf(out, size, "%s", name);
    else
        snprintf(out, size, "#%X", (unsigned int) code);
}

// A function to start capturing for a device. The key or pad control that started it (OK) is held
// now: the key last pressed and not let go, or the first control held on a pad; its repeats and its
// release are not captured. A pad's capture with no pad to capture from (the gamepad off, or no pad
// open) does not start: the binding page says why, once for each press.
void pickers_capture(int device)
{
    const char *why = NULL;
    if (device == BINDINGS_GAMEPAD && settings_slot(host.model, SET_ID_GAMEPAD_ENABLED, -1)->value.number == 0)
        why = "Turn the gamepad on to capture a button";
    else if (device == BINDINGS_GAMEPAD && !gamepad_connected())
        why = "No gamepad is connected";
    if (why != NULL) {
        // As when a pad's capture ends: the key held to start it waits until let go, so its repeats
        // do not refuse again
        keys_swallowed[1] = key_held;
        log_debug("Settings: the capture cannot start: %s", why);
        settings_capture_ended(host.model, why);
        return;
    }
    int starting = device == BINDINGS_KEYBOARD ? key_held : gamepad_pressed_label();
    capture_device = device;
    capture_begin(&capture, ticks_ms(), starting);
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: STREAMFLEX_TEST_CAPTURE_SECONDS=<s> listens that long
    // instead of 5 s, so a held control can run past the repeat's delay on a slow runner
    const char *seconds = getenv("STREAMFLEX_TEST_CAPTURE_SECONDS");
    if (seconds != NULL && atoi(seconds) > 0) {
        capture.window = (unsigned int) atoi(seconds) * 1000u;
        log_debug("Test hook: the capture listens for %s s", seconds);
    }
#endif
    pad_held = device == BINDINGS_GAMEPAD ? starting : -1;
    log_debug("Settings: capturing a %s", device == BINDINGS_KEYBOARD ? "key" : "button");
}

// A function to end a capture, with what it caught (`code`, -1 for nothing). Whatever is still held
// as it ends waits until let go, so its repeats never act on the page that comes next: the key caught
// and the starting key if not yet let go, any pad control, and after a pad's capture the key held
// down through it. The OK that started a capture may be on the other device.
static void end_capture(int code)
{
    if (capture_device == BINDINGS_KEYBOARD) {
        keys_swallowed[0] = code;
        keys_swallowed[1] = capture.starting_held ? capture.starting_code : -1;
        pad_swallowed = true;   // A pad control held through the keyboard's capture
    }
    else {
        keys_swallowed[1] = key_held;   // A key held through the pad's capture
        pad_swallowed = true;
    }
    capture_device = -1;
}

// A function to hand a capture's end to the pages: the key caught
static void captured(int code)
{
    char name[64];
    pickers_key_name(capture_device, code, name, sizeof(name));
    if (capture_device == BINDINGS_KEYBOARD)
        log_debug("Settings: capture got %s (#%X)", name, (unsigned int) code);
    else
        log_debug("Settings: capture got %s", name);
    end_capture(code);
    SettingsEvent event = settings_captured(host.model, code);
    host.event(&event);
}

// A function to start the 10 s for a change that took a key's navigation away
void pickers_probation(int device, int code, const char *command)
{
    char name[64];
    probation_begin(&probation, ticks_ms(), code);
    probation_device = device;
    snprintf(probation_command, sizeof(probation_command), "%s", command);
    pickers_key_name(device, code, name, sizeof(name));
    log_debug("Settings: press %s again within 10 s to keep it", name);
}

// A function to put back the change the 10 s were for, saying why
static void revert(const char *why)
{
    char name[64];
    pickers_key_name(probation_device, probation.code, name, sizeof(name));
    log_debug("Settings: the binding went back: %s %s", name, why);
    SettingsEvent event = settings_revert_binding(host.model);
    host.event(&event);
}

// A function to run the clocks: a capture that caught nothing in 5 s ends with the reason, and a
// change not confirmed in 10 s goes back. Each frame they run first, before that frame's keys and pad
// (probation_press() has no clock of its own).
static void run_clocks(void)
{
    unsigned int now = ticks_ms();
    clock_now = now;
    if (capture_expired(&capture, now)) {
        end_capture(-1);
        log_debug("Settings: the capture caught nothing in 5 s");
        settings_capture_ended(host.model, "Nothing was pressed in 5 s");
    }
    if (probation_expired(&probation, now))
        revert("was not pressed again within 10 s");
}

// A function to settle the 10 s as settings close: a change not yet confirmed goes back first
void pickers_settle(void)
{
    if (!probation.active)
        return;
    probation.active = false;
    revert("was not pressed again before settings closed");
}

// A function to end the 10 s without putting anything back: Discard put everything back already
void pickers_end_probation(void)
{
    probation.active = false;
}

// A function to tell whether a capture or the 10 s are running, for the caption
bool pickers_busy(void)
{
    return capture_device >= 0 || probation.active;
}

// A function to take a key while settings are open: during a keyboard capture every key is the
// capture's (a pad's capture keeps them waiting); a key held as a capture ended waits until let go;
// during the 10 s, the new key (pressed, not repeated) confirms them. True when the key was taken.
bool pickers_raw_key(int code, bool repeat)
{
    char name[64];
    run_clocks();
    key_held = code;   // A repeat is of the key last pressed, which is this one already
    if (capture_device == BINDINGS_KEYBOARD) {
        if (capture_press(&capture, ticks_ms(), code, repeat))
            captured(code);
        return true;
    }
    if (capture_device == BINDINGS_GAMEPAD)
        return true;
    if (code == keys_swallowed[0] || code == keys_swallowed[1])
        return true;
    if (!repeat && probation_press(&probation, code)) {
        pickers_key_name(BINDINGS_KEYBOARD, code, name, sizeof(name));
        log_debug("Settings: kept %s for %s", name, probation_command);
        settings_keep_binding(host.model);
        return true;
    }
    return false;
}

// A function to take a key's release while settings are open: a held key let go, and during a capture
// the starting key's release
void pickers_raw_release(int code)
{
    if (code == key_held)
        key_held = -1;
    if (code == keys_swallowed[0])
        keys_swallowed[0] = -1;
    if (code == keys_swallowed[1])
        keys_swallowed[1] = -1;
    if (capture_device == BINDINGS_KEYBOARD)
        capture_release(&capture, code);
}

#ifdef STREAMFLEX_TEST_HOOKS
// A function only the headless harness builds: it times how long settings take one held pad control for
// one reason (`taken`; TAKEN_NONE when not taken), from the first frame they took it, and logs once that
// reaches the repeat's delay, when the control would have repeated had the launcher's controls had it
enum { TAKEN_NONE, TAKEN_CAPTURE, TAKEN_SWALLOWED };
static void test_pad_taken(int label, int taken)
{
    static const char *const reasons[] = { "", "the capture", "waiting to be let go" };
    static int last_label = -1;
    static int last_taken = TAKEN_NONE;
    static Uint32 since = 0;
    static bool said = false;
    Uint32 now = ticks_ms();
    if (taken == TAKEN_NONE || label < 0 || label != last_label || taken != last_taken) {
        since = now;
        said = false;
    }
    last_label = label;
    last_taken = taken;
    if (taken != TAKEN_NONE && label >= 0 && !said && now - since >= GAMEPAD_REPEAT_DELAY) {
        said = true;
        log_debug("Test hook: pad %s held while settings took it (%s): %u ms, the repeat's delay",
            bindings_label(label), reasons[taken], (unsigned int) (now - since));
    }
}
#else
#define test_pad_taken(label, taken)
#endif

static void keyboard_key(OskCommand key);

// A function to tell a pad control that is one of the keyboard's shortcuts, and which: X, Y or Start
static bool pad_shortcut(int label, OskCommand *out)
{
    const char *name = label >= 0 ? bindings_label(label) : "";
    if (strcmp(name, SETTING_GAMEPAD_BUTTON_X) == 0)
        *out = OSK_BACKSPACE;
    else if (strcmp(name, SETTING_GAMEPAD_BUTTON_Y) == 0)
        *out = OSK_SPACE;
    else if (strcmp(name, SETTING_GAMEPAD_BUTTON_START) == 0)
        *out = OSK_DONE;
    else
        return false;
    return true;
}

// A function to take the pad's state each frame (`label`, the first control held; -1 for none): during
// a pad capture a newly held control is the capture's, and a keyboard capture keeps the pad waiting; a
// pad held as its capture ended waits until every control is let go. True when the pad's input was
// taken. (The 10 s are the keyboard's alone: bindings_takes_navigation() is false for the pad.)
bool pickers_raw_pad(int label)
{
    run_clocks();
    int was = pad_held;
    pad_held = label;
    if (capture_device == BINDINGS_GAMEPAD) {
        // A control held on (the starting one) is ignored by the capture until let go; any other is
        // caught on its first frame
        if (was >= 0 && was != label)
            capture_release(&capture, was);
        if (label >= 0 && capture_press(&capture, ticks_ms(), label, false))
            captured(label);
        test_pad_taken(label, TAKEN_CAPTURE);
        return true;
    }
    if (capture_device == BINDINGS_KEYBOARD) {
        test_pad_taken(label, TAKEN_CAPTURE);
        return true;
    }
    // The keyboard's shortcuts on the pad (spec "Shortcuts"): X is backspace, Y a space, Start Done. Each acts
    // as it is pressed, then its control waits until let go, as after a capture: a Start held past Done must
    // never reach :settings, which would close settings
    OskCommand shortcut;
    if (keyboard != NULL && pad_shortcut(label, &shortcut)) {
        if (label != was) {
            log_debug("Settings: the pad's %s %s", bindings_label(label),
                shortcut == OSK_BACKSPACE ? "deletes" : shortcut == OSK_SPACE ? "types a space" : "is Done");
            keyboard_key(shortcut);
        }
        pad_swallowed = true;
        test_pad_taken(label, TAKEN_SWALLOWED);
        return true;
    }
    pad_swallowed = pad_swallowed && label >= 0;
    test_pad_taken(label, pad_swallowed ? TAKEN_SWALLOWED : TAKEN_NONE);
    return pad_swallowed;
}

// A function to rebuild the launcher's hotkeys and gamepad controls from the bindings, as they now are:
// the exit hotkey let go and taken again on Windows (the first :exit on a key it can register, as the
// model's floor assumes), the gamepad's defaults added again. A control held as they are rebuilt keeps
// its press and its next repeat, so it does not press again.
void apply_bindings(const Bindings *bindings)
{
    ControlHold holds[BINDINGS_LABELS];
    memset(holds, 0, sizeof(holds));
    for (GamepadControl *control = gamepad_controls; control != NULL; control = control->next) {
        int label = bindings_label_index(control->label);
        if (label >= 0 && control->hold.held && !holds[label].held)
            holds[label] = control->hold;
    }
#ifdef _WIN32
    clear_exit_hotkey();
#endif
    clear_hotkeys();
    clear_gamepad_controls();
    int keys = 0;
    int controls = 0;
    char code[16];
    test_fail("apply", true);   // The harness can make every addition run out of memory
    for (int i = 0; i < bindings_count(bindings, BINDINGS_KEYBOARD); i++) {
        const Binding *b = bindings_at(bindings, BINDINGS_KEYBOARD, i);
        if (b->removed)
            continue;
        snprintf(code, sizeof(code), "#%X", (unsigned int) b->code);
        add_hotkey(code, b->command);
        keys++;
    }
    for (int i = 0; i < bindings_count(bindings, BINDINGS_GAMEPAD); i++) {
        const Binding *b = bindings_at(bindings, BINDINGS_GAMEPAD, i);
        if (b->removed)
            continue;
        add_gamepad_control(bindings_label(b->code), b->command);
        controls++;
    }
    if (gamepad_running())
        add_default_gamepad_controls();
    test_fail("apply", false);
    for (GamepadControl *control = gamepad_controls; control != NULL; control = control->next) {
        int label = bindings_label_index(control->label);
        if (label >= 0)
            control->hold = holds[label];
    }
#ifdef _WIN32
    if (has_exit_hotkey())
        register_exit_hotkey();
#endif
    log_debug("Settings: the bindings now hold %i %s and %i controls", keys, keys == 1 ? "hotkey" : "hotkeys", controls);
    if (config.debug) {
        debug_hotkeys(hotkeys);
        debug_gamepad(gamepad_controls);
    }
}

// A function to open the command picker for the binding page's binding, with the cursor on its
// command. Out of memory it does not open, and says so: the keys stay with the page.
void pickers_open_binding_command(void)
{
    close_picker();
    list = make_list(SET_TYPE_COMMAND, settings_binding_command(host.model));
    if (list == NULL) {
        log_error("Settings: the list cannot open: out of memory");
        return;
    }
    picking_command = true;
    log_debug("Settings: opened the command picker for the binding");
    log_debug("Settings: the command picker lists %i rows", listpick_count(list));
    forget_list_log();
    kind = PICKER_LIST;
}

// A function to let go of the library grid's textures, before the renderer that made them goes or as
// settings close; how many it destroyed, for a lost device's count. The grid makes them again when it
// next draws.
int pickers_drop_icon_textures(void)
{
    int count = 0;
    for (int i = 0; i < icon_texture_count; i++) {
        if (icon_textures[i] != NULL) {
            SDL_DestroyTexture(icon_textures[i]);
            count++;
        }
    }
    free(icon_textures);
    icon_textures = NULL;
    icon_texture_count = 0;
    icon_size = 0;
    return count;
}

// A function to make every library icon's texture at `size` px for the grid, decoded on the icon threads
// as a menu's icons are (iconload_batch()): once a settings visit, and again only at another size. An
// icon that cannot be read shows its title alone, and is logged.
static void load_icon_textures(int size)
{
    if (icon_textures != NULL && size == icon_size)
        return;
    pickers_drop_icon_textures();
    int count = library_count();
    Uint64 start = SDL_GetPerformanceCounter();
    icon_textures = calloc((size_t) (count > 0 ? count : 1), sizeof(SDL_Texture*));
    IconJob *jobs = calloc((size_t) (count > 0 ? count : 1), sizeof(IconJob));
    int *from = calloc((size_t) (count > 0 ? count : 1), sizeof(int));
    if (icon_textures == NULL || jobs == NULL || from == NULL) {
        free(icon_textures);
        free(jobs);
        free(from);
        icon_textures = NULL;
        log_error("Settings: the library grid's icons cannot be made: out of memory, so it shows their titles");
        return;
    }
    icon_texture_count = count;
    icon_size = size;
    int work = 0;
    for (int i = 0; i < count; i++) {
        const char *path = library_lookup(library_info(i)->name);
        if (path == NULL || fileio_not_a_file(path))
            continue;
        size_t length = strlen(path);
        jobs[work].path = path;
        jobs[work].size = size;
        jobs[work].svg = length > 4 && SDL_strcasecmp(path + length - 4, ".svg") == 0;
        from[work++] = i;
    }
    iconload_batch(jobs, work, iconload_workers(), decode_icon_job, icon_worker_start, icon_worker_end);
    int made = 0;
    for (int j = 0; j < work; j++) {
        if (jobs[j].surface == NULL) {
            log_error("Settings: the library grid cannot show %s: %s", library_info(from[j])->name, jobs[j].error);
            continue;
        }
        icon_textures[from[j]] = load_texture(jobs[j].surface);   // Frees the surface
        made += icon_textures[from[j]] != NULL ? 1 : 0;
    }
    free(jobs);
    free(from);
    log_debug("Settings: the library grid made %i of %i icons at %i px in %u ms", made, count, size,
        timing_ms(SDL_GetPerformanceCounter() - start));
}

// A function to tell the screen which icon is under the grid's cursor, for the preview, and log it
static void tell_shown(void)
{
    const LibraryInfo *icon = icongrid_icon(icon_grid, icongrid_cursor(icon_grid));
    if (icon == NULL)
        return;
    log_debug("Settings: the library grid's cursor is on %s", icon->name);
    if (icon_shown != NULL)
        icon_shown(icon->name);
}

// A function to open the library grid over the library's icons, filtered by `query`, the cursor on the
// icon `at` when it is shown (else the first). Out of memory it does not open, and says so.
static bool open_grid(const char *at, const char *query)
{
    close_picker();
    icon_grid = icongrid_create(library_info(0), library_count());
    if (icon_grid == NULL || !icongrid_filter(icon_grid, query)) {
        icongrid_free(icon_grid);
        icon_grid = NULL;
        log_error("Settings: the library grid cannot open: out of memory");
        return false;
    }
    if (at != NULL)
        icongrid_select(icon_grid, at);
    icon_first_line = 0;
    kind = PICKER_ICONS;
    return true;
}

// A function to filter the open library grid in place as the keyboard's text changes (Task 12's follower,
// at once), the cursor kept on its icon while that is still shown; a cancel after a change gives it the
// text the keyboard opened with, which puts the old filter back. It never opens the grid again: the grid
// stays open under the keyboard, so Back lands on it. Out of memory the grid closes, and says so.
static void icons_filter_changed(const char *text, void *context)
{
    UNUSED(context);
    if (kind != PICKER_ICONS || icon_grid == NULL)
        return;
    if (!icongrid_filter(icon_grid, text)) {
        log_error("Settings: the library grid cannot be filtered: out of memory");
        close_picker();
        return;
    }
    log_debug("Settings: the library grid shows %i icons for '%s'", icongrid_count(icon_grid), icongrid_query(icon_grid));
    tell_shown();
}

// A function to open the keyboard on the library grid's filter, over the grid (Task 12's way into it for a
// filtered list, as Task 10b's app list does): `first` is the character a real keyboard's key typed to
// open it, or NULL (Up from the grid's first row). Done keeps the filter, already applied as it was typed.
static void open_icon_filter(const char *first)
{
    log_debug("Settings: the keyboard filters the library grid");
    pickers_open_keyboard("Filter the icons", icongrid_query(icon_grid), ICONGRID_QUERY_MAX - 1, NULL, NULL,
                          icons_filter_changed);
    pickers_keyboard_follow(icons_filter_changed, 0);
    if (first != NULL)
        pickers_keyboard_type(first);
}

// A function to open the library grid for a tile's icon (spec "Icons", choice 3): every library icon with
// its title, grouped, the cursor on `current` when it names one. Typing on a real keyboard opens its filter.
void pickers_open_icons(const char *current, void (*shown)(const char *name), void (*done)(const char *name))
{
    icon_shown = shown;
    icon_done = done;
    if (!open_grid(current, ""))
        return;
    pickers_set_typing(open_icon_filter);   // After open_grid(): its close_picker() clears the opener
    log_debug("Settings: opened the library grid: %i icons in %i groups", icongrid_count(icon_grid), icongrid_groups(icon_grid));
    tell_shown();
}

// A function to act on a key in the library grid: the arrows move (Up from the first row opens the
// keyboard to filter it), OK chooses and Back leaves, each telling the screen
static void icons_command(const char *command)
{
    IconGridCommand key;
    if (MATCH(command, SCMD_UP))
        key = ICONGRID_UP;
    else if (MATCH(command, SCMD_DOWN))
        key = ICONGRID_DOWN;
    else if (MATCH(command, SCMD_LEFT))
        key = ICONGRID_LEFT;
    else if (MATCH(command, SCMD_RIGHT))
        key = ICONGRID_RIGHT;
    else if (MATCH(command, SCMD_SELECT))
        key = ICONGRID_OK;
    else
        key = ICONGRID_BACK;
    IconGridResult result = icongrid_command(icon_grid, key, ICONS_COLUMNS);
    void (*done)(const char *name) = icon_done;
    if (result == ICONGRID_MOVED)
        tell_shown();
    else if (result == ICONGRID_FILTER)
        open_icon_filter(NULL);
    else if (result == ICONGRID_CANCELED) {
        close_picker();
        if (done != NULL)
            done(NULL);
    }
    else if (result == ICONGRID_CHOSEN) {
        char name[LIBRARY_NAME_MAX + 1];   // Copied: the grid goes before `done` runs
        snprintf(name, sizeof(name), "%s", icongrid_chosen(icon_grid)->name);
        close_picker();
        log_debug("Settings: chose the library icon %s", name);
        if (done != NULL)
            done(name);
    }
}

// A function to name a group for its header: its name with a capital first letter; "Other" for none
static void group_label(const char *group, char *out, size_t size)
{
    if (group == NULL || group[0] == '\0') {
        snprintf(out, size, "Other");
        return;
    }
    snprintf(out, size, "%s", group);
    if (out[0] >= 'a' && out[0] <= 'z')
        out[0] = (char) (out[0] - 'a' + 'A');
}

// A function to tell whether the grid's line `line` is a group's header (else a row of its icons)
static bool header_line(int line)
{
    for (int g = 0, at = 0; g < icongrid_groups(icon_grid); g++) {
        if (line == at)
            return true;
        at += 1 + (icongrid_group_size(icon_grid, g) + ICONS_COLUMNS - 1) / ICONS_COLUMNS;
        if (line < at)
            return false;
    }
    return false;
}

// A function to draw one icon of the grid in its cell: the icon, its title under it, and an outline round
// the cell under the cursor
static void draw_icon_cell(int shown, int x, int y, int cell, int title_height)
{
    const LibraryInfo *icon = icongrid_icon(icon_grid, shown);
    bool cursor = shown == icongrid_cursor(icon_grid);
    int pad = host.margin / 2;
    int index = (int) (icon - library_info(0));
    SDL_Rect box = { x + pad / 2, y + pad / 2, cell - pad, cell - pad };
    if (index >= 0 && index < icon_texture_count && icon_textures[index] != NULL)
        draw_texture(renderer, icon_textures[index], NULL, &box);
    host.text(host.font_small, icon->title, x + 2, y + cell - pad / 2, cell - 4, (Uint8) (cursor ? 255 : ALPHA_GRID_TEXT), false);
    if (!cursor)
        return;
    SDL_Rect outline = { x, y, cell, cell + title_height };
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_MARK);
    outline_rect(renderer, &outline);
    SDL_Rect inner = { outline.x + 1, outline.y + 1, outline.w - 2, outline.h - 2 };
    outline_rect(renderer, &inner);
}

// A function to draw the library grid: each group's header, then its icons four across, each with its
// title, the one under the cursor outlined; scrolled so the cursor's row (with its group's header, for a
// group's first row) is on show. With no icon shown it says so.
static void draw_icons(int x, int top, int bottom)
{
    int pad = host.margin / 2;
    int cell = (host.column_width - 2 * pad) / ICONS_COLUMNS;
    int title_height = TTF_GetFontHeight(host.font_small);
    int row_height = cell + title_height;
    load_icon_textures(cell - pad);
    int cursor = icongrid_cursor(icon_grid);
    if (cursor < 0) {
        host.text(host.font_row, "No icon matches. Up types another filter.", x + pad, top, host.column_width - 2 * pad, 255, false);
        return;
    }

    // The cursor's line: each group a header, then its rows
    int groups = icongrid_groups(icon_grid);
    int cursor_group = icongrid_group_of(icon_grid, cursor);
    int cursor_place = cursor - icongrid_group_first(icon_grid, cursor_group);
    int cursor_line = 0;
    for (int g = 0; g < cursor_group; g++)
        cursor_line += 1 + (icongrid_group_size(icon_grid, g) + ICONS_COLUMNS - 1) / ICONS_COLUMNS;
    cursor_line += 1 + cursor_place / ICONS_COLUMNS;
    int from = cursor_place < ICONS_COLUMNS ? cursor_line - 1 : cursor_line;   // A first row shows its header
    if (from < icon_first_line)
        icon_first_line = from;
    for (;;) {
        int y = top;
        for (int line = icon_first_line; line <= cursor_line; line++)
            y += header_line(line) ? host.row_height : row_height;
        if (y <= bottom || icon_first_line >= cursor_line)
            break;
        icon_first_line++;
    }

    int y = top;
    int line = 0;
    for (int g = 0; g < groups && y < bottom; g++) {
        int first = icongrid_group_first(icon_grid, g);
        int size = icongrid_group_size(icon_grid, g);
        int rows = (size + ICONS_COLUMNS - 1) / ICONS_COLUMNS;
        if (line >= icon_first_line) {
            if (y + host.row_height > bottom)
                break;
            char label[64];
            group_label(icongrid_group(icon_grid, g), label, sizeof(label));
            host.text(host.font_row, label, x + pad, y + (host.row_height - TTF_GetFontHeight(host.font_row)) / 2,
                      host.column_width - 2 * pad, ALPHA_GRID_TEXT, false);
            y += host.row_height;
        }
        line++;
        for (int r = 0; r < rows; r++, line++) {
            if (line < icon_first_line)
                continue;
            if (y + row_height > bottom) {
                y = bottom;
                break;
            }
            for (int c = 0; c < ICONS_COLUMNS && r * ICONS_COLUMNS + c < size; c++)
                draw_icon_cell(first + r * ICONS_COLUMNS + c, x + pad + c * cell, y, cell, title_height);
            y += row_height;
        }
    }
}

// A function to name a key as it shows now: a character key's shifted text while Shift or caps lock is on;
// the shapes' keys by what they stand for
static const char *key_face(const OskKey *key)
{
    if (key == NULL)
        return "";
    switch (key->kind) {
        case OSK_KEY_BACKSPACE:
            return "Backspace";
        case OSK_KEY_CURSOR_LEFT:
            return "Cursor left";
        case OSK_KEY_CURSOR_RIGHT:
            return "Cursor right";
        case OSK_KEY_CHAR:
            return (osk_shift(keyboard) || osk_caps(keyboard)) && key->shifted != NULL ? key->shifted : key->label;
        default:
            return key->label;
    }
}

// A function to give the keyboard's note: "Back again to discard" while it asks, why Done is off, else
// the pad's shortcuts
static const char *keyboard_note(void)
{
    if (osk_asking(keyboard))
        return "Back again to discard";
    if (keyboard_why != NULL)
        return keyboard_why;
    return "X deletes" DOT "Y types a space" DOT "Start is Done";
}

// A function to log the keyboard as it changes: its text and cursor, the highlighted key, the note
static void log_keyboard(void)
{
    char text[sizeof(keyboard_logged)];
    snprintf(text, sizeof(text), "'%s' (cursor %i)", osk_text(keyboard), osk_cursor(keyboard));
    if (strcmp(text, keyboard_logged) != 0) {
        copy_string(keyboard_logged, text, sizeof(keyboard_logged));
        log_debug("Settings: the keyboard reads %s", text);
    }
    int row = 0;
    int column = 0;
    osk_highlight(keyboard, &row, &column);
    const char *face = key_face(osk_key(keyboard, row, column));
    if (strcmp(face, keyboard_key_logged) != 0) {
        copy_string(keyboard_key_logged, face, sizeof(keyboard_key_logged));
        log_debug("Settings: the keyboard's highlight is on %s", face);
    }
    const char *says = keyboard_note();
    if (strcmp(says, keyboard_note_logged) != 0) {
        copy_string(keyboard_note_logged, says, sizeof(keyboard_note_logged));
        log_debug("Settings: the keyboard says %s", says);
    }
}

// A function to let go of the keyboard, saying nothing to whoever opened it
static void close_keyboard(void)
{
    osk_free(keyboard);
    keyboard = NULL;
    keyboard_done = NULL;
    keyboard_context = NULL;
    keyboard_follower = NULL;
    keyboard_pending = false;
    keyboard_told = false;
    keyboard_typed = false;
    keyboard_why = NULL;
    keyboard_logged[0] = '\0';
    keyboard_key_logged[0] = '\0';
    keyboard_note_logged[0] = '\0';
    field_room = -1;
}

// A function to remember the layout the keyboard shows as KeyboardLayout (spec "The choice is remembered"):
// a change to that setting, which the save writes
static void remember_layout(void)
{
    SettingSlot *layout = settings_slot(host.model, SET_ID_KEYBOARD_LAYOUT, -1);
    int now = osk_layout(keyboard) == OSK_ABC ? OSK_ABC : OSK_QWERTY;
    if (layout->value.number == now)
        return;
    SettingValue value = layout->value;
    value.number = now;
    SettingsEvent event = settings_choose_value(host.model, layout, &value);
    host.event(&event);
}

// A function to tell the follower the text now
static void tell_follower(void)
{
    keyboard_pending = false;
    if (keyboard_follower == NULL)
        return;
    keyboard_told = true;
    keyboard_follower(osk_text(keyboard), keyboard_context);
}

// A function to hand the text on at Done: the keyboard closes first, so `done` may open what it likes. One
// it reopens on its own field (a typed path refused) is still the field a real keyboard typed into, if one
// had: its Enter stays Done and its Backspace stays delete.
static void finish_keyboard(void)
{
    char text[SETTING_TEXT_MAX];
    copy_string(text, osk_text(keyboard), sizeof(text));
    void (*done)(const char *text, void *context) = keyboard_done;
    void *context = keyboard_context;
    bool typed = keyboard_typed;
    remember_layout();
    close_keyboard();
    log_debug("Settings: the keyboard finished with '%s'", text);
    if (done == NULL)
        return;
    finishing_done = done;
    finishing_context = context;
    finishing_typed = typed;
    done(text, context);
    finishing_done = NULL;
    finishing_context = NULL;
    finishing_typed = false;
}

// A function to close the keyboard without its text: a follower that heard a change hears the text the
// field opened with, so the preview goes back
static void cancel_keyboard(const char *why)
{
    void (*follower)(const char *text, void *context) = keyboard_told ? keyboard_follower : NULL;
    void *context = keyboard_context;
    remember_layout();
    close_keyboard();
    log_debug("Settings: the keyboard was canceled%s", why);
    if (follower != NULL)
        follower(keyboard_initial, context);
}

// A function to act on what a key did: Done hands the text on, a cancel closes it, a change is followed,
// and Done refused says why in the log
static void keyboard_result(OskResult result, bool done_asked)
{
    switch (result) {
        case OSK_FINISHED:
            finish_keyboard();
            return;
        case OSK_CANCELED:
            cancel_keyboard("");
            return;
        case OSK_CHANGED:
            keyboard_why = osk_why(keyboard);
            keyboard_pending = true;
            keyboard_changed_at = ticks_ms();
            if (keyboard_rest == 0)
                tell_follower();
            break;
        case OSK_NONE:
            if (done_asked && keyboard_why != NULL)
                log_debug("Settings: Done is off: %s", keyboard_why);
            break;
        case OSK_MOVED:
        case OSK_ASKED:
            break;
    }
    if (keyboard != NULL) {
        remember_layout();
        log_keyboard();
    }
}

// A function to press one of the keyboard's commands: the remote's, or the pad's shortcuts
static void keyboard_key(OskCommand key)
{
    int row = 0;
    int column = 0;
    osk_highlight(keyboard, &row, &column);
    const OskKey *at = osk_key(keyboard, row, column);
    bool done_asked = key == OSK_DONE || (key == OSK_OK && at != NULL && at->kind == OSK_KEY_DONE);
    keyboard_result(osk_command(keyboard, key), done_asked);
}

// A function to act on one of the remote's keys while the keyboard is open
static void keyboard_command(const char *command)
{
    OskCommand key = OSK_BACK;
    if (MATCH(command, SCMD_UP))
        key = OSK_UP;
    else if (MATCH(command, SCMD_DOWN))
        key = OSK_DOWN;
    else if (MATCH(command, SCMD_LEFT))
        key = OSK_LEFT;
    else if (MATCH(command, SCMD_RIGHT))
        key = OSK_RIGHT;
    else if (MATCH(command, SCMD_SELECT))
        key = OSK_OK;
    keyboard_key(key);
}

// A function to open the keyboard on a field (spec "The on-screen keyboard"): `label` names it ("Title for
// Kodi"), `initial` is its text, `max_bytes` its limit, and `check` its validator, whose reason the note
// gives while Done is off. It opens in the layout KeyboardLayout names, over the page or the list under it,
// which stays open. `done` gets the text at Done, once the keyboard has closed; Back with no change, or
// twice after one, closes it with nothing said. Opened from `done` with that `done` and `context`, it is the
// same field again, so a real keyboard that typed into it keeps its Enter as Done and Backspace as delete.
// Out of memory it does not open, and says so: the keys stay with what is under it. The harness can make it
// fail (STREAMFLEX_TEST_FAIL=keyboard).
void pickers_open_keyboard(const char *label, const char *initial, int max_bytes, OskCheck check, void *context,
                           void (*done)(const char *text, void *context))
{
    close_keyboard();
    OskLayout layout = settings_slot(host.model, SET_ID_KEYBOARD_LAYOUT, -1)->value.number == OSK_ABC ? OSK_ABC : OSK_QWERTY;
    test_fail("keyboard", true);
    keyboard = osk_create(label, initial, max_bytes, check, context, layout);
    test_fail("keyboard", false);
    if (keyboard == NULL) {
        log_error("Settings: the keyboard cannot open: out of memory");
        return;
    }
    copy_string(keyboard_initial, initial != NULL ? initial : "", sizeof(keyboard_initial));
    keyboard_done = done;
    keyboard_context = context;
    keyboard_typed = finishing_typed && done == finishing_done && context == finishing_context;
    keyboard_why = osk_why(keyboard);
    keyboard_drawn = false;
    log_debug("Settings: opened the keyboard for %s (%s)", label, layout == OSK_ABC ? "ABC" : "QWERTY");
    log_keyboard();
}

// A function to have the keyboard's text followed as it changes (see settings_pickers.h)
void pickers_keyboard_follow(void (*changed)(const char *text, void *context), unsigned int rest_ms)
{
    keyboard_follower = keyboard != NULL ? changed : NULL;
    keyboard_rest = rest_ms;
}

// A function to put text in at the cursor as a real keyboard's key does: from then on Enter is Done
void pickers_keyboard_type(const char *utf8)
{
    if (keyboard == NULL)
        return;
    keyboard_typed = true;
    keyboard_result(osk_insert(keyboard, utf8), false);
}

// A function to say the open picker filters by typing: a printing key on a real keyboard calls `open`,
// with the key's character, while the keyboard is closed. close_picker() clears it.
void pickers_set_typing(void (*open)(const char *first))
{
    typing_opens = open;
}

// A function to take a real keyboard's key while settings are open, before handle_keypress() (spec "A real
// keyboard types into the field"). The character is the layout's own for the key's place, with Shift, Caps
// Lock and AltGr applied (SDL_GetKeyFromScancode() with only those modifiers, osk_key_mods(), which on
// Windows takes Right Alt or Ctrl+Alt as AltGr), never event.key.key, which
// SDL3 gives unshifted; the keypad's digits, point and operators type theirs too (osk_key_char()). While the
// keyboard is open, a printing key types it, Esc is Back and the keypad's Enter is Done; Enter and Backspace
// are the remote's OK and Back (KEYBOARD_NOTE) until a printing key has been typed into this field, and Done
// and delete after; every other key, the arrows always, goes on to handle_keypress(). While a picker that
// filters is open and the keyboard is not, a printing key opens it on the filter with that character. True
// when the key was taken.
bool pickers_text_key(SDL_Scancode scancode, SDL_Keymod mod, bool repeat)
{
    char typed[5];
    SDL_Keycode code = SDL_GetKeyFromScancode(scancode, (SDL_Keymod) osk_key_mods((unsigned int) mod, ALTGR_IS_RIGHT_ALT), false);
    bool printing = osk_key_char((unsigned int) code, typed);
    if (keyboard == NULL) {
        if (typing_opens == NULL || !printing || repeat)
            return false;
        log_debug("Settings: '%s' (scancode %s) opens the keyboard on the filter", typed, SDL_GetScancodeName(scancode));
        typing_opens(typed);
        return true;
    }
    switch (scancode) {
        case SDL_SCANCODE_ESCAPE:
            if (!repeat)
                keyboard_key(OSK_BACK);
            return true;
        case SDL_SCANCODE_KP_ENTER:
            if (!repeat)
                keyboard_key(OSK_DONE);
            return true;
        case SDL_SCANCODE_RETURN:
            if (!keyboard_typed)
                return false;
            if (!repeat)
                keyboard_key(OSK_DONE);
            return true;
        case SDL_SCANCODE_BACKSPACE:
            if (!keyboard_typed)
                return false;
            keyboard_key(OSK_BACKSPACE);   // Held, it deletes again, as any field's Backspace does
            return true;
        default:
            break;
    }
    if (!printing)
        return false;
    keyboard_typed = true;
    log_debug("Settings: the keyboard typed '%s' (scancode %s)", typed, SDL_GetScancodeName(scancode));
    keyboard_result(osk_insert(keyboard, typed), false);   // Held, it types again
    return true;
}

// A function to measure the bytes of a text from its start, in the keyboard's font
static int span_width(const char *text, size_t length)
{
    int w = 0;
    int h = 0;
    if (length > 0)
        TTF_GetStringSize(host.font_row, text, length, &w, &h);
    return w;
}

// A function to step from a byte offset past the UTF-8 character there
static size_t next_character(const char *text, size_t at)
{
    do {
        at++;
    } while (text[at] != '\0' && ((unsigned char) text[at] & 0xC0) == 0x80);
    return at;
}

// A function to draw the field: its text through the settings' text cache (plan ruling 7), scrolled so the
// cursor stays in view, and the cursor. Where the text is drawn from is found again only when the text, the
// cursor or the room changes (finding it measures the text once a character, too much for every frame).
static void draw_field(const SDL_Rect *field, int pad)
{
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_FIELD);
    fill_rect(renderer, field);
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_EDGE);
    outline_rect(renderer, field);
    const char *text = osk_text(keyboard);
    size_t cursor = (size_t) osk_cursor(keyboard);
    int room = field->w - 2 * pad;
    if (room != field_room || cursor != field_cursor || strcmp(text, field_text) != 0) {
        size_t from = 0;
        while (from < cursor && span_width(text + from, cursor - from) > room)
            from = next_character(text, from);
        copy_string(field_text, text, sizeof(field_text));
        field_cursor = cursor;
        field_room = room;
        field_start = from;
    }
    size_t start = field_start;
    int line = TTF_GetFontHeight(host.font_row);
    int text_y = field->y + (field->h - line) / 2;
    host.text(host.font_row, text + start, field->x + pad, text_y, room, 255, false);
    SDL_Rect caret = { field->x + pad + span_width(text + start, cursor - start), text_y, 2, line };
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    fill_rect(renderer, &caret);
}

// A function to fill a triangle, for the cursor keys' faces
static void fill_triangle(float x1, float y1, float x2, float y2, float x3, float y3)
{
    SDL_FColor white = { 1.0F, 1.0F, 1.0F, 1.0F };
    SDL_Vertex corners[3] = {
        { { x1, y1 }, white, { 0.0F, 0.0F } }, { { x2, y2 }, white, { 0.0F, 0.0F } }, { { x3, y3 }, white, { 0.0F, 0.0F } }
    };
    SDL_RenderGeometry(renderer, NULL, corners, 3, NULL, 0);
}

// A function to draw a key's face: its text centered, or, for backspace and the text cursor's keys, a shape
// (the settings' font has no glyph for U+232B, U+25C0 or U+25B6, and a fallback font would be needed for one)
static void draw_key_face(const OskKey *key, const SDL_Rect *box)
{
    int cx = box->x + box->w / 2;
    int cy = box->y + box->h / 2;
    int half = box->h / 5;
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    if (key->kind == OSK_KEY_CURSOR_LEFT || key->kind == OSK_KEY_CURSOR_RIGHT) {
        float tip = (float) (key->kind == OSK_KEY_CURSOR_LEFT ? cx - half : cx + half);
        float back = (float) (key->kind == OSK_KEY_CURSOR_LEFT ? cx + half : cx - half);
        fill_triangle(tip, (float) cy, back, (float) (cy - half), back, (float) (cy + half));
        return;
    }
    if (key->kind == OSK_KEY_BACKSPACE) {
        // An arrow-shaped outline pointing left, with an x in it
        int left = cx - half;
        int right = cx + 2 * half;
        int tip = cx - 2 * half;
        draw_line(renderer, tip, cy, left, cy - half);
        draw_line(renderer, left, cy - half, right, cy - half);
        draw_line(renderer, right, cy - half, right, cy + half);
        draw_line(renderer, right, cy + half, left, cy + half);
        draw_line(renderer, left, cy + half, tip, cy);
        int x = cx + half / 2;
        int arm = half / 2;
        draw_line(renderer, x - arm, cy - arm, x + arm, cy + arm);
        draw_line(renderer, x - arm, cy + arm, x + arm, cy - arm);
        return;
    }
    const char *face = key_face(key);
    int w = 0;
    int h = 0;
    TTF_GetStringSize(host.font_row, face, 0, &w, &h);
    host.text(host.font_row, face, box->x + (box->w - (w < box->w ? w : box->w)) / 2, cy - h / 2, box->w, 255, false);
}

// A function to measure a row of keys, in key widths
static float row_units(int row)
{
    float units = 0.0F;
    for (int c = 0; c < osk_row_length(keyboard, row); c++)
        units += osk_key(keyboard, row, c)->width;
    return units;
}

// A function to draw the keys: each row centered on the widest, as osk.c moves between them (so Up and
// Down land where they look to), the highlighted key filled and outlined, Shift filled while it is on and
// outlined while it is caps lock
static void draw_keys(int x, int y, int w, int h)
{
    int rows = osk_rows(keyboard);
    float widest = 0.0F;
    for (int r = 0; r < rows; r++)
        widest = row_units(r) > widest ? row_units(r) : widest;
    if (widest <= 0.0F || rows == 0 || h <= 0)
        return;
    float unit = (float) w / widest;
    int key_h = h / rows;
    int lit_row = 0;
    int lit_column = 0;
    osk_highlight(keyboard, &lit_row, &lit_column);
    for (int r = 0; r < rows; r++) {
        float left = (float) x + (widest - row_units(r)) * unit / 2.0F;
        for (int c = 0; c < osk_row_length(keyboard, r); c++) {
            const OskKey *key = osk_key(keyboard, r, c);
            SDL_Rect box = { (int) left + 2, y + r * key_h + 2, (int) (key->width * unit) - 4, key_h - 4 };
            left += key->width * unit;
            bool lit = r == lit_row && c == lit_column;
            bool shift = key->kind == OSK_KEY_SHIFT && (osk_shift(keyboard) || osk_caps(keyboard));
            SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, (Uint8) (lit || shift ? ALPHA_KEY_LIT : ALPHA_KEY));
            fill_rect(renderer, &box);
            if (lit || (key->kind == OSK_KEY_SHIFT && osk_caps(keyboard))) {
                SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_EDGE);
                outline_rect(renderer, &box);
            }
            draw_key_face(key, &box);
        }
    }
}

// A function to draw the keyboard over the bottom of the preview (spec "Where it appears"), in the
// rectangle the screen gives: the field's label, the field with its cursor, the note (why Done is off,
// "Back again to discard", or the pad's shortcuts), then the keys. The log says once where it is drawn.
void pickers_draw_keyboard(int x, int y, int w, int h)
{
    if (keyboard == NULL || w <= 0 || h <= 0)
        return;
    if (!keyboard_drawn) {
        keyboard_drawn = true;
        log_debug("Settings: the keyboard is drawn at %i,%i, %i x %i", x, y, w, h);
    }
    int pad = host.margin / 2;
    int small = TTF_GetFontHeight(host.font_small);
    int line = TTF_GetFontHeight(host.font_row);
    SDL_Rect panel = { x, y, w, h };
    SDL_SetRenderDrawColor(renderer, PANEL.r, PANEL.g, PANEL.b, PANEL.a);
    fill_rect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_PANEL_EDGE);
    outline_rect(renderer, &panel);
    int top = y + pad;
    host.text(host.font_small, osk_label(keyboard), x + pad, top, w - 2 * pad, ALPHA_LABEL, false);
    top += small + pad / 2;
    SDL_Rect field = { x + pad, top, w - 2 * pad, line + pad };
    draw_field(&field, pad);
    top += field.h + pad / 2;
    host.text(host.font_small, keyboard_note(), x + pad, top, w - 2 * pad, 255, false);
    top += small + pad;
    draw_keys(x + pad, top, w - 2 * pad, y + h - pad - top);
}

// A function to say why a typed command cannot be held by config.ini, or NULL when it can: a setting's is
// weighed under its own key, a binding's as a hotkey's line holds it (its commit checks again, under the
// key it gets)
static const char *check_command(const char *text, void *context)
{
    TypedCommand *typed = context;
    char value[BINDINGS_VALUE_MAX];
    if (text[0] == '\0')
        return "Type a command";
    if (typed->slot != NULL)
        snprintf(value, sizeof(value), "%s", text);
    else
        snprintf(value, sizeof(value), HOTKEY_PREFIX "%s", text);
    const char *why = inidoc_check(typed->key, value);
    if (why == NULL)
        return NULL;
    snprintf(typed->why, sizeof(typed->why), "config.ini can't hold this command: %s", why);
    return typed->why;
}

// A function to take a typed command, as the list picker takes a chosen one
static void command_typed(const char *text, void *context)
{
    TypedCommand *typed = context;
    if (typed->slot == NULL) {
        SettingsEvent event = settings_bind_command(host.model, text);
        host.event(&event);
        return;
    }
    SettingValue value = typed->slot->value;
    value.inherit = false;
    copy_string(value.text, text, sizeof(value.text));
    SettingsEvent event = settings_choose_value(host.model, typed->slot, &value);
    if (event.kind == SETTINGS_EVENT_NONE)
        log_debug("Settings: [%s] %s is unchanged", typed->slot->def->section, typed->slot->def->key);
    else
        host.event(&event);
}

// A function to open the keyboard on the command the list picker chooses for (a setting's, or the binding
// page's), in place of its rows (3c: "Type..."), the command as it is now to start
static void type_command(void)
{
    char label[96];
    char initial[BINDINGS_COMMAND_MAX];
    typed_command.slot = slot;
    if (slot != NULL) {
        snprintf(label, sizeof(label), "Command for %s", slot->def->label);
        copy_string(initial, slot->value.inherit ? "" : slot->value.text, sizeof(initial));
        copy_string(typed_command.key, slot->def->key, sizeof(typed_command.key));
    }
    else {
        snprintf(label, sizeof(label), "Command for the binding");
        copy_string(initial, settings_binding_command(host.model), sizeof(initial));
        copy_string(typed_command.key, "Hotkey99999", sizeof(typed_command.key));
    }
    close_picker();
    pickers_open_keyboard(label, initial, BINDINGS_COMMAND_MAX - 1, check_command, &typed_command, command_typed);
}

// A function to read more of the font files each frame (settings_fonts.c), and fill the font
// picker, if it is open, on the frame the reading ends; to run the capture's and the 10 s's clocks; and
// to tell the keyboard's follower a change once it has rested
void pickers_tick(void)
{
    run_clocks();
    // The app list's rows, on the frame after Task 7's scan has its list (its poll_apps() runs after this,
    // in settings_draw())
    if (kind == PICKER_APPS && !apps_listed && settings_apps(NULL, NULL) != NULL)
        make_app_list();
    if (fonts_tick() && kind == PICKER_FONT)
        fill_open_fonts();
    if (keyboard != NULL && keyboard_pending && ticks_ms() - keyboard_changed_at >= keyboard_rest)
        tell_follower();
}

// A function to close the picker and let go of the fonts at quit: they are kept while the launcher runs
void pickers_quit(void)
{
    close_picker();
    pickers_drop_icon_textures();
    fonts_quit();
}

// A function to hand a chosen value to the pages, which apply and log it, and close the picker.
// A value the setting already had changes nothing: the preview already shows it.
static void choose(SettingValue value)
{
    SettingSlot *s = slot;
    close_picker();   // It puts back the original the color picker's preview changed: the choice is made from it
    SettingsEvent event = settings_choose_value(host.model, s, &value);
    if (event.kind == SETTINGS_EVENT_NONE)
        log_debug("Settings: [%s] %s is unchanged", s->def->section, s->def->key);
    else
        host.event(&event);
}

// A function to show a color in the preview, live, without choosing it. A key that leaves the
// color as it is (a hex digit chosen, the editor opened or left) applies nothing: for the titles,
// each apply is a reload.
static void preview_color(SettingColor shown)
{
    if (shown.r == previewed.r && shown.g == previewed.g && shown.b == previewed.b)
        return;
    previewed = shown;
    slot->value = original;
    slot->value.color = shown;
    host.apply(slot, true);
    log_debug("Settings: previewing #%02X%02X%02X", shown.r, shown.g, shown.b);
}

// A function to put back the color the color picker opened with, which the preview may have changed
static void put_color_back(void)
{
    slot->value = original;
    host.apply(slot, true);
    log_debug("Settings: the color picker put [%s] %s back", slot->def->section, slot->def->key);
}

// A function to choose a font row's family: its face is stored quietly first, then its file with
// the refresh that opens it (the pages store the font and run the title font's or the clock's group
// once). The same file's other face leaves the font's own value as it was, so the refresh is run
// here. `chosen` is copied before the list goes. The row of the font in use (where the cursor
// started) changes nothing, though the file names it otherwise: the Windows config's relative
// .\assets\fonts\... would become the list's full path. That row is written all the same when the
// configured file is gone: the font in use is then the bundled one the loader fell back to, and
// choosing it takes the dead path out of the file.
static void choose_font(const char *chosen)
{
    char in_use[FONT_VALUE_MAX];
    fonts_value_in_use(slot->def->id, in_use, sizeof(in_use));
    bool gone = original.text[0] != '\0' && !font_file_found(original.text);
    if (strcmp(chosen, in_use) == 0 && !gone) {
        choose(original);
        return;
    }
    SettingValue font = original;
    int face = atoi(chosen);
    snprintf(font.text, sizeof(font.text), "%s", fonts_value_path(chosen));
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

// A function to turn a remote's key into the list picker's
static ListPickCommand list_key(const char *command)
{
    if (MATCH(command, SCMD_UP))
        return LISTPICK_UP;
    if (MATCH(command, SCMD_DOWN))
        return LISTPICK_DOWN;
    if (MATCH(command, SCMD_LEFT))
        return LISTPICK_PAGE_UP;
    if (MATCH(command, SCMD_RIGHT))
        return LISTPICK_PAGE_DOWN;
    if (MATCH(command, SCMD_SELECT))
        return LISTPICK_OK;
    return LISTPICK_BACK;
}

// A function to open a list the screen made (see settings_pickers.h)
void pickers_open_list(const char *label, ListPick *made, void (*chosen)(const char *value))
{
    close_picker();
    if (made == NULL) {
        log_error("Settings: the list cannot open: out of memory");
        return;
    }
    list = made;
    list_chosen = chosen;
    copy_string(list_label, label, sizeof(list_label));
    forget_list_log();
    kind = PICKER_LIST;
    log_debug("Settings: opened the list for %s (%i rows)", label, listpick_count(list));
}

// A function to make the app list's rows from the apps Task 7's scan found, as the filter leaves them, the
// cursor on the first; nothing matching is one grayed row that says so. While the scan runs, or with no
// apps, there are no rows. Out of memory the list closes, and says so.
static void make_app_list(void)
{
    listpick_free(list);
    list = NULL;
    const AppList *apps = settings_apps(NULL, NULL);
    apps_listed = apps != NULL;
    int count = apps != NULL ? applist_count(apps) : 0;
    if (count == 0)
        return;   // The column says why there is none, or that the apps are being found
    int *rows = calloc((size_t) count, sizeof(int));
    ListPick *made = rows != NULL ? listpick_create() : NULL;
    bool ok = made != NULL;
    int shown = ok ? applist_filter(apps, app_filter, rows, count) : 0;
    char value[16];
    for (int i = 0; ok && i < shown; i++) {
        snprintf(value, sizeof(value), "%d", rows[i]);
        ok = listpick_add(made, applist_item(apps, rows[i])->name, value, true, NULL);
    }
    if (ok && shown == 0)
        ok = listpick_add(made, "No app matches", "", false, "No app's name holds what was typed");
    free(rows);
    if (!ok) {
        listpick_free(made);
        log_error("Settings: the app list cannot be made: out of memory");
        close_picker();
        return;
    }
    list = made;
    forget_list_log();
    log_debug("Settings: the app list shows %i of %i apps%s%s", shown, count, app_filter[0] != '\0' ? " for " : "", app_filter);
}

// A function to follow the app list's filter as it is typed, at once
static void app_filter_changed(const char *text, void *context)
{
    UNUSED(context);
    copy_string(app_filter, text, sizeof(app_filter));
    if (kind == PICKER_APPS)
        make_app_list();
}

// A function to open the keyboard on the app list's filter (Task 12's way into it for a filtered list):
// `first` is the character a real keyboard's key typed to open it, or NULL (Up from the top row)
static void open_app_filter(const char *first)
{
    pickers_open_keyboard("Filter the apps", app_filter, MENUEDIT_TITLE_MAX, NULL, NULL, app_filter_changed);
    pickers_keyboard_follow(app_filter_changed, 0);
    if (first != NULL)
        pickers_keyboard_type(first);
}

// A function to open the app list (see settings_pickers.h): Task 7's scan starts with its first opening in a
// visit (settings_apps_start() does nothing after that), and the rows come once it has found the apps
// (pickers_tick())
void pickers_open_apps(void (*chosen)(const AppItem *item))
{
    close_picker();
    settings_apps_start();
    app_chosen = chosen;
    app_filter[0] = '\0';
    kind = PICKER_APPS;
    pickers_set_typing(open_app_filter);
    make_app_list();
    log_debug("Settings: opened the app list");
}

// A function to act on a key in the app list: while the apps are found, or when there are none, Back alone
// closes it; Up from the top row opens the keyboard on its filter
static void apps_command(const char *command)
{
    if (list == NULL) {
        if (MATCH(command, SCMD_BACK))
            close_picker();
        return;
    }
    if (MATCH(command, SCMD_UP) && listpick_cursor(list) == 0) {
        open_app_filter(NULL);
        return;
    }
    ListPickResult result = listpick_command(list, list_key(command), list_page);
    if (result == LISTPICK_CANCELED)
        close_picker();
    else if (result == LISTPICK_CHOSEN) {
        const AppItem *item = applist_item(settings_apps(NULL, NULL), atoi(listpick_chosen(list)));
        void (*chosen)(const AppItem *item) = app_chosen;
        close_picker();
        log_debug("Settings: chose the app %s", item != NULL ? item->name : "?");
        if (item != NULL && chosen != NULL)
            chosen(item);
    }
}

// A function to draw the app list: its rows, or "Finding apps... (N)" while the scan runs, or why there are none
static void draw_apps(int x, int top, int bottom)
{
    if (list != NULL) {
        draw_list(x, top, bottom);
        return;
    }
    int found = 0;
    const char *why = NULL;
    settings_apps(&found, &why);   // No reason while the scan runs: its count so far
    char text[300];
    if (why == NULL)
        snprintf(text, sizeof(text), "Finding apps\xE2\x80\xA6 (%i)", found);
    else
        snprintf(text, sizeof(text), "%s", why);
    host.text(host.font_row, text, x + host.margin / 2, top, host.column_width - host.margin, 255, false);
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
    ListPickCommand key = list_key(command);
    ListPickResult result = listpick_command(list, key, list_page);
    if (result == LISTPICK_CHOSEN && strcmp(listpick_chosen(list), TYPE_VALUE) == 0) {
        type_command();
        return;
    }
    if (result == LISTPICK_CHOSEN && list_chosen != NULL) {
        // A list the screen made: its value goes back, copied before the list goes
        char chosen[LISTPICK_TEXT_MAX];
        snprintf(chosen, sizeof(chosen), "%s", listpick_chosen(list));
        void (*done)(const char *value) = list_chosen;
        close_picker();
        done(chosen);
        return;
    }
    if (result == LISTPICK_CHOSEN && picking_command) {
        // A binding's command goes to the model, not to a slot; it is copied before the list goes
        char chosen[LISTPICK_TEXT_MAX];
        snprintf(chosen, sizeof(chosen), "%s", listpick_chosen(list));
        close_picker();
        SettingsEvent event = settings_bind_command(host.model, chosen);
        host.event(&event);
        return;
    }
    if (result == LISTPICK_CANCELED)
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

// A function to act on a key in the color picker
static void color_command(const char *command)
{
    ColorPickCommand key;
    if (MATCH(command, SCMD_UP))
        key = COLORPICK_UP;
    else if (MATCH(command, SCMD_DOWN))
        key = COLORPICK_DOWN;
    else if (MATCH(command, SCMD_LEFT))
        key = COLORPICK_LEFT;
    else if (MATCH(command, SCMD_RIGHT))
        key = COLORPICK_RIGHT;
    else if (MATCH(command, SCMD_SELECT))
        key = COLORPICK_OK;
    else
        key = COLORPICK_BACK;
    ColorPickResult result = colorpick_command(&color_pick, key);
    if (result == COLORPICK_MOVED)
        preview_color(colorpick_shown(&color_pick));
    else if (result == COLORPICK_CANCELED) {
        put_color_back();
        close_picker();
    }
    else if (result == COLORPICK_CHOSEN) {
        SettingValue value = original;
        value.color = color_pick.chosen;
        choose(value);
    }
}

// A function to act on a key while a picker or the keyboard is open. Home and the key that opened settings
// leave the keyboard without its text and the picker without choosing (the color picker puts its color back
// first, even from the hex editor, where Back would only leave the editor), then close settings as the
// pages would. The keyboard, open over a picker or alone, takes the other keys first.
void pickers_command(const char *command)
{
    if (MATCH(command, SCMD_HOME) || MATCH(command, SCMD_SETTINGS)) {
        if (keyboard != NULL)
            cancel_keyboard(" as settings close");
        if (kind == PICKER_COLOR)
            put_color_back();
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
    if (keyboard != NULL)
        keyboard_command(command);
    else if (kind == PICKER_LIST || kind == PICKER_FONT)
        list_command(command);
    else if (kind == PICKER_APPS)
        apps_command(command);
    else if (kind == PICKER_COLOR)
        color_command(command);
    else if (kind == PICKER_ICONS)
        icons_command(command);
}

// A function to close the keyboard and the picker on show as Back closes them at their top, the color
// picker putting its color back first; settings stay open on the page they opened from. A new screen size
// does this (they were measured when they opened), and so does a remade settings model.
void pickers_cancel(void)
{
    if (keyboard != NULL)
        cancel_keyboard("");
    if (kind == PICKER_COLOR)
        put_color_back();
    close_picker();
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
        ? fonts_sample(host.font_row, row->value, row->label, &w, &h) : NULL;
    if (sample == NULL)
        copy_string(shown.label, row->label, sizeof(shown.label));
    int drawn = host.row(&shown, highlighted, x, y, host.column_width, 0);
    if (sample != NULL) {
        int room = host.column_width - host.margin;
        SDL_Rect rect = { x + host.margin / 2, y + (drawn - h) / 2, w < room ? w : room, h };
        SDL_Rect from = { 0, 0, rect.w, h };
        draw_texture(renderer, sample, &from, &rect);
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

// A function to draw the color picker: the swatches, the current one marked and the one under the
// cursor outlined, then the Custom row, and in the hex editor each digit with the chosen one boxed
static void draw_color(int x, int top, int bottom)
{
    int pad = host.margin / 2;
    int cell = (host.column_width - 2 * pad) / COLORPICK_COLUMNS;
    // The swatches, the Custom row and the hex editor's row all end above the key hint, so a short,
    // wide screen gets smaller swatches
    int fits = (bottom - top - pad - 2 * host.row_height) / COLORPICK_ROWS;
    if (cell > fits)
        cell = fits;
    int current = colorpick_find(color_pick.original);
    if (!cells_logged) {
        cells_logged = true;
        log_debug("Settings: the color picker draws %i px cells from %i,%i, down to %i of %i", cell, x + pad, top,
            top + COLORPICK_ROWS * cell + pad + 2 * host.row_height, bottom);
    }
    for (int i = 0; i < COLORPICK_SWATCHES; i++) {
        SettingColor c = colorpick_swatch(i);
        SDL_Rect box = { x + pad + (i % COLORPICK_COLUMNS) * cell + 3, top + (i / COLORPICK_COLUMNS) * cell + 3,
                         cell - 6, cell - 6 };
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 0xFF);
        fill_rect(renderer, &box);
        if (i == current) {
            SDL_Rect mark = { box.x + box.w / 3, box.y + box.h / 3, box.w / 3, box.h / 3 };
            SDL_SetRenderDrawColor(renderer, (Uint8) (0xFF - c.r), (Uint8) (0xFF - c.g), (Uint8) (0xFF - c.b), 0xFF);
            fill_rect(renderer, &mark);
        }
        if (i == color_pick.cursor) {
            SDL_Rect outline = { box.x - 3, box.y - 3, box.w + 6, box.h + 6 };
            SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_MARK);
            outline_rect(renderer, &outline);
            SDL_Rect inner = { outline.x + 1, outline.y + 1, outline.w - 2, outline.h - 2 };
            outline_rect(renderer, &inner);
        }
    }
    int y = top + COLORPICK_ROWS * cell + pad;
    SettingColor shown = color_pick.editing || color_pick.cursor == COLORPICK_CUSTOM ? color_pick.hex : color_pick.original;
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
    y += host.row(&custom, color_pick.cursor == COLORPICK_CUSTOM && !color_pick.editing, x, y, host.column_width, 0);
    if (!color_pick.editing || y + host.row_height > bottom)
        return;

    // The hex editor: # and six digits in cells as wide as the widest digit, the chosen one boxed
    char digits[8];
    snprintf(digits, sizeof(digits), "%02X%02X%02X", color_pick.hex.r, color_pick.hex.g, color_pick.hex.b);
    int w = 0;
    int h = 0;
    TTF_GetStringSize(host.font_row, "W", 0, &w, &h);
    int step = w + pad;
    host.text(host.font_row, "#", x + pad, y, step, 255, false);
    for (int i = 0; i < 6; i++) {
        char one[2] = { digits[i], '\0' };
        int cx = x + pad + (i + 1) * step;
        host.text(host.font_row, one, cx, y, step, 255, false);
        if (i == color_pick.digit) {
            SDL_Rect box = { cx - pad / 2, y, step, h };
            SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, ALPHA_MARK);
            outline_rect(renderer, &box);
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
    snprintf(text, sizeof(text), "Loading fonts\xE2\x80\xA6 (%i)", fonts_files_read());
    host.text(host.font_row, text, x + host.margin / 2, top, host.column_width - host.margin, 255, false);
    if (fonts_files_read() != loading_logged) {
        loading_logged = fonts_files_read();
        log_debug("Settings: the font picker reads %s", text);
    }
}

// A function to draw the picker on show in the column, between two heights
void pickers_draw(int x, int top, int bottom)
{
    if (kind == PICKER_LIST)
        draw_list(x, top, bottom);
    else if (kind == PICKER_COLOR)
        draw_color(x, top, bottom);
    else if (kind == PICKER_FONT)
        draw_fonts(x, top, bottom);
    else if (kind == PICKER_ICONS)
        draw_icons(x, top, bottom);
    else if (kind == PICKER_APPS)
        draw_apps(x, top, bottom);
}

// A function to write the column's path line: the page's path, then the setting being chosen, then the
// field the keyboard edits
void pickers_path(char *out, size_t size)
{
    settings_path(host.model, out, size);
    size_t used = strlen(out);
    if (slot != NULL)
        snprintf(out + used, size - used, " \xE2\x80\xBA %s", slot->def->label);
    else if (kind == PICKER_ICONS)
        snprintf(out + used, size - used, " \xE2\x80\xBA Library");
    used = strlen(out);
    if (list_label[0] != '\0')
        snprintf(out + used, size - used, " \xE2\x80\xBA %s", list_label);
    else if (kind == PICKER_APPS)
        snprintf(out + used, size - used, " \xE2\x80\xBA App");
    used = strlen(out);
    if (keyboard != NULL)
        snprintf(out + used, size - used, " \xE2\x80\xBA %s", osk_label(keyboard));
}

// A function to give the key hint for the picker on show, one line short enough for the column at
// 1280 x 720 and 1280 x 800 (the hex editor's keys are spelled out in docs/configuration.md)
const char *pickers_hint(void)
{
    if (keyboard != NULL)
        return "OK types" DOT "Back cancels";
    if (kind == PICKER_APPS && list == NULL)
        return "Back goes back";
    if (kind == PICKER_APPS)
        return "Up filters" DOT "OK adds" DOT "Back cancels";
    if (kind == PICKER_COLOR && color_pick.editing)
        return "Arrows edit the digits" DOT "OK keeps" DOT "Back returns";
    if (kind == PICKER_COLOR)
        return "Arrows move" DOT "OK chooses" DOT "Back cancels";
    if (kind == PICKER_FONT && list == NULL)
        return "Back cancels";
    if (kind == PICKER_ICONS)
        return "Arrows move" DOT "Up at the top filters" DOT "OK chooses";
    return "Left and right page" DOT "OK chooses" DOT "Back cancels";
}

// A function to warn when a title or clock color stands out too little from what lies behind it:
// the background color or the image on show (its mean luminance), under the overlay when it is on.
// It says nothing for a transparent background, or an image not yet measured.
void contrast_warning(SettingId id, SettingColor color, char *out, size_t size)
{
    out[0] = '\0';
    if (id != SET_ID_TITLE_COLOR && id != SET_ID_CLOCK_COLOR)
        return;
    double behind = 0.0;
    if (background_shown == BACKGROUND_COLOR) {
        SettingColor bg = { config.background_color.r, config.background_color.g, config.background_color.b };
        behind = color_luminance(bg);
    }
    else if ((background_shown == BACKGROUND_IMAGE || background_shown == BACKGROUND_SLIDESHOW) && background_luminance >= 0.0)
        behind = background_luminance;
    else
        return;
    if (config.background_overlay) {
        SettingColor over = { eff.overlay_color.r, eff.overlay_color.g, eff.overlay_color.b };
        behind = color_over(behind, over, eff.overlay_color.a);
    }
    double ratio = color_contrast(color_luminance(color), behind);
    if (ratio < COLORPICK_MIN_CONTRAST)
        snprintf(out, size, "Low contrast: %.1f:1 against the background; 3:1 or more reads well", ratio);
}

// A function to say what the caption says while a picker is open: why OK did nothing in a list; in
// the color picker, the color under the cursor, and a contrast warning for it
const char *pickers_note(void)
{
    note[0] = '\0';
    if (keyboard != NULL) {
        snprintf(note, sizeof(note), "%s", keyboard_note());
        return note;
    }
    if (kind == PICKER_APPS && list == NULL) {
        const char *why = NULL;
        settings_apps(NULL, &why);   // Task 7's reason; none while the apps are being found
        snprintf(note, sizeof(note), "%s", why != NULL ? why : "");
    }
    else if ((kind == PICKER_LIST || kind == PICKER_APPS) && listpick_why(list) != NULL)
        snprintf(note, sizeof(note), "%s", listpick_why(list));
    else if (kind == PICKER_ICONS && icongrid_why(icon_grid) != NULL)
        snprintf(note, sizeof(note), "%s", icongrid_why(icon_grid));
    else if (kind == PICKER_ICONS && icongrid_query(icon_grid)[0] != '\0')
        snprintf(note, sizeof(note), "%i of %i icons hold \"%s\"", icongrid_count(icon_grid), library_count(),
            icongrid_query(icon_grid));
    else if (kind == PICKER_COLOR) {
        SettingColor shown = colorpick_shown(&color_pick);
        int index = color_pick.editing ? -1 : colorpick_find(shown);
        char warning[160];
        contrast_warning(slot->def->id, shown, warning, sizeof(warning));
        snprintf(note, sizeof(note), "%s #%02X%02X%02X%s%s", index >= 0 ? colorpick_name(index) : "Custom",
            shown.r, shown.g, shown.b, warning[0] != '\0' ? DOT : "", warning);
    }
    // When nothing else is said, a capture or the 10 s count down, from when the clocks last ran (this
    // frame's, before its caption): a time at which they would have ended is never shown, so the
    // count stops at 1 s
    if (note[0] == '\0' && capture_device >= 0)
        snprintf(note, sizeof(note), "Press the key or button" "\xE2\x80\xA6" " (%u s)",
            (capture.window - (clock_now - capture.started) + 999) / 1000);
    else if (note[0] == '\0' && probation.active) {
        char name[64];
        pickers_key_name(probation_device, probation.code, name, sizeof(name));
        snprintf(note, sizeof(note), "Press %s again within %u s to keep it", name,
            ((unsigned int) BINDINGS_CONFIRM_MS - (clock_now - probation.started) + 999) / 1000);
    }
    return note;
}
