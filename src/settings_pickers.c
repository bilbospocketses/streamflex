#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include "launcher.h"
#include <launcher_config.h>
#include "settings.h"
#include "settings_pickers.h"
#include "settings_fonts.h"
#include "bindings.h"
#include "listpick.h"
#include "colorpick.h"
#include "fileio.h"
#include "image.h"
#include "test_hooks.h"
#include "util.h"
#include "debug.h"
#ifdef _WIN32
#include <SDL_syswm.h>
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

#define DOT " \xC2\xB7 "       // U+00B7 with a space either side
#define ALPHA_MARK 230         // The outline round the swatch under the cursor

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

// A function to close the picker on show, if any. No picker open means the slot holds the colour
// the colour picker opened with: whatever the preview put in it goes, however the picker closes (a
// choice is made from that original). It is not applied here: as settings close, the fonts and the
// screen it would be drawn with may be gone, and a Back or Home applies it before closing. The
// font picker's samples go with it: they are drawn again when it next opens.
static void close_picker(void)
{
    if (kind == PICKER_COLOUR)
        slot->value = original;
    if (kind != PICKER_NONE && slot != NULL)
        log_debug("Settings: closed the picker for [%s] %s", slot->def->section, slot->def->key);
    else if (picking_command)
        log_debug("Settings: closed the command picker for the binding");
    fonts_clear_samples();
    listpick_free(list);
    list = NULL;
    kind = PICKER_NONE;
    slot = NULL;
    picking_command = false;
    note[0] = '\0';
}

// A function to let go of everything as settings close: the picker, a capture and the 10 s
void pickers_end(void)
{
    close_picker();
    capture_device = -1;
    probation.active = false;
    pad_swallowed = false;
    key_held = -1;
    keys_swallowed[0] = keys_swallowed[1] = -1;
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
        colorpick_open(&colour, s->value.color);
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

// A function to name a key or button for the screen: a key by SDL's name ("Menu" for both of the
// Menu key's codes), a pad's control by its label; "#<HEX>" for a key SDL cannot name
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
    const char *name = SDL_GetKeyName((SDL_Keycode) code);
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
    capture_begin(&capture, SDL_GetTicks(), starting);
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
    probation_begin(&probation, SDL_GetTicks(), code);
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
    unsigned int now = SDL_GetTicks();
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
        if (capture_press(&capture, SDL_GetTicks(), code, repeat))
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
// A function only the headless harness builds: it counts the frames on which settings take one held pad
// control for one reason (`taken`; TAKEN_NONE when not taken), and logs when the count reaches the
// repeat's delay, the frame the control would have repeated on had the launcher's controls had it
enum { TAKEN_NONE, TAKEN_CAPTURE, TAKEN_SWALLOWED };
static void test_pad_taken(int label, int taken)
{
    static const char *const reasons[] = { "", "the capture", "waiting to be let go" };
    static int last_label = -1;
    static int last_taken = TAKEN_NONE;
    static Uint32 frames = 0;
    if (taken == TAKEN_NONE || label < 0 || label != last_label || taken != last_taken)
        frames = 0;
    last_label = label;
    last_taken = taken;
    if (taken != TAKEN_NONE && label >= 0 && ++frames == test_pad_repeat_delay())
        log_debug("Test hook: pad %s held while settings took it (%s): %u frames, the repeat's delay",
            bindings_label(label), reasons[taken], (unsigned int) frames);
}
#else
#define test_pad_taken(label, taken)
#endif

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
        if (label >= 0 && capture_press(&capture, SDL_GetTicks(), label, false))
            captured(label);
        test_pad_taken(label, TAKEN_CAPTURE);
        return true;
    }
    if (capture_device == BINDINGS_KEYBOARD) {
        test_pad_taken(label, TAKEN_CAPTURE);
        return true;
    }
    pad_swallowed = pad_swallowed && label >= 0;
    test_pad_taken(label, pad_swallowed ? TAKEN_SWALLOWED : TAKEN_NONE);
    return pad_swallowed;
}

// A function to rebuild the launcher's hotkeys and gamepad controls from the bindings, as they now are:
// the exit hotkey let go and taken again on Windows (the first :exit on a key it can register, as the
// model's floor assumes), the gamepad's defaults added again. A control held as they are rebuilt keeps
// its count, so it does not press again.
void apply_bindings(const Bindings *bindings)
{
    Uint32 repeats[BINDINGS_LABELS];
    memset(repeats, 0, sizeof(repeats));
    for (GamepadControl *control = gamepad_controls; control != NULL; control = control->next) {
        int label = bindings_label_index(control->label);
        if (label >= 0 && control->repeat > repeats[label])
            repeats[label] = control->repeat;
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
            control->repeat = repeats[label];
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

// A function to read more of the font files each frame (settings_fonts.c), and fill the font
// picker, if it is open, on the frame the reading ends; and to run the capture's and the 10 s's clocks
void pickers_tick(void)
{
    run_clocks();
    if (fonts_tick() && kind == PICKER_FONT)
        fill_open_fonts();
}

// A function to close the picker and let go of the fonts at quit: they are kept while the launcher runs
void pickers_quit(void)
{
    close_picker();
    fonts_quit();
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
    if (result == LISTPICK_CHOSEN && picking_command) {
        // A binding's command goes to the model, not to a slot; it is copied before the list goes
        char chosen[LISTPICK_TEXT_MAX];
        snprintf(chosen, sizeof(chosen), "%s", listpick_chosen(list));
        close_picker();
        SettingsEvent event = settings_bind_command(host.model, chosen);
        host.event(&event);
        return;
    }
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
    ColourPickResult result = colorpick_command(&colour, key);
    if (result == COLORPICK_MOVED)
        preview_colour(colorpick_shown(&colour));
    else if (result == COLORPICK_CANCELLED) {
        put_colour_back();
        close_picker();
    }
    else if (result == COLORPICK_CHOSEN) {
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
        ? fonts_sample(host.font_row, row->value, row->label, &w, &h) : NULL;
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
    int cell = (host.column_width - 2 * pad) / COLORPICK_COLUMNS;
    // The swatches, the Custom row and the hex editor's row all end above the key hint, so a short,
    // wide screen gets smaller swatches
    int fits = (bottom - top - pad - 2 * host.row_height) / COLORPICK_ROWS;
    if (cell > fits)
        cell = fits;
    int current = colorpick_find(colour.original);
    if (!cells_logged) {
        cells_logged = true;
        log_debug("Settings: the colour picker draws %i px cells from %i,%i, down to %i of %i", cell, x + pad, top,
            top + COLORPICK_ROWS * cell + pad + 2 * host.row_height, bottom);
    }
    for (int i = 0; i < COLORPICK_SWATCHES; i++) {
        SettingColor c = colorpick_swatch(i);
        SDL_Rect box = { x + pad + (i % COLORPICK_COLUMNS) * cell + 3, top + (i / COLORPICK_COLUMNS) * cell + 3,
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
    int y = top + COLORPICK_ROWS * cell + pad;
    SettingColor shown = colour.editing || colour.cursor == COLORPICK_CUSTOM ? colour.hex : colour.original;
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
    y += host.row(&custom, colour.cursor == COLORPICK_CUSTOM && !colour.editing, x, y, host.column_width, 0);
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

// A function to give the key hint for the picker on show, one line short enough for the column at
// 1280 x 720 and 1280 x 800 (the hex editor's keys are spelled out in docs/configuration.md)
const char *pickers_hint(void)
{
    if (kind == PICKER_COLOUR && colour.editing)
        return "Arrows edit the digits" DOT "OK keeps" DOT "Back returns";
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
    if (ratio < COLORPICK_MIN_CONTRAST)
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
        SettingColor shown = colorpick_shown(&colour);
        int index = colour.editing ? -1 : colorpick_find(shown);
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
