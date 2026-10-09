#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bindings.h"
#include "alloc.h"
#include <launcher_config.h>

// The gamepad's control labels, in util.c's table's order (its add_gamepad_control() info)
static const char *const LABELS[BINDINGS_LABELS] = {
    SETTING_GAMEPAD_LSTICK_XM, SETTING_GAMEPAD_LSTICK_XP, SETTING_GAMEPAD_LSTICK_YM, SETTING_GAMEPAD_LSTICK_YP,
    SETTING_GAMEPAD_RSTICK_XM, SETTING_GAMEPAD_RSTICK_XP, SETTING_GAMEPAD_RSTICK_YM, SETTING_GAMEPAD_RSTICK_YP,
    SETTING_GAMEPAD_LTRIGGER, SETTING_GAMEPAD_RTRIGGER, SETTING_GAMEPAD_BUTTON_A, SETTING_GAMEPAD_BUTTON_B,
    SETTING_GAMEPAD_BUTTON_X, SETTING_GAMEPAD_BUTTON_Y, SETTING_GAMEPAD_BUTTON_BACK, SETTING_GAMEPAD_BUTTON_GUIDE,
    SETTING_GAMEPAD_BUTTON_START, SETTING_GAMEPAD_BUTTON_LEFT_STICK, SETTING_GAMEPAD_BUTTON_RIGHT_STICK,
    SETTING_GAMEPAD_BUTTON_LEFT_SHOULDER, SETTING_GAMEPAD_BUTTON_RIGHT_SHOULDER, SETTING_GAMEPAD_BUTTON_DPAD_UP,
    SETTING_GAMEPAD_BUTTON_DPAD_DOWN, SETTING_GAMEPAD_BUTTON_DPAD_LEFT, SETTING_GAMEPAD_BUTTON_DPAD_RIGHT
};

// The commands the floor keeps a way to, and why it refuses a change that would leave one with none,
// on the keyboard and on the gamepad
#define FLOOR_COUNT 7
static const char *const FLOOR[FLOOR_COUNT] = { ":left", ":right", ":up", ":down", ":select", ":back", ":settings" };
static const char *const NO_KEY[FLOOR_COUNT] = {
    "That would leave no key for Left", "That would leave no key for Right", "That would leave no key for Up",
    "That would leave no key for Down", "That would leave no key for OK", "That would leave no key for Back",
    "That would leave no key for Settings"
};
static const char *const NO_BUTTON[FLOOR_COUNT] = {
    "That would leave no button for Left", "That would leave no button for Right", "That would leave no button for Up",
    "That would leave no button for Down", "That would leave no button for OK", "That would leave no button for Back",
    "That would leave no button for Settings"
};

typedef struct {
    Binding *items;
    int count;
    int capacity;
    Binding *entry;       // The list as loaded: Discard's target
    int entry_count;
} List;

struct Bindings {
    bool windows;         // The exit hotkey's rule applies
    bool gamepad_on;      // The gamepad's list has a floor only while the gamepad runs
    List lists[2];
};

// A list as a change would leave it: `changed` in place of the binding at `index`, which is `count - 1`
// for a new binding; with `index` -1, the list as it is
typedef struct {
    const List *list;
    int index;
    const Binding *changed;
    int count;
} View;

// A function to make an empty model; NULL when out of memory
Bindings *bindings_create(bool windows, bool gamepad_on)
{
    Bindings *b = alloc_calloc(1, sizeof(Bindings));
    if (b != NULL) {
        b->windows = windows;
        b->gamepad_on = gamepad_on;
    }
    return b;
}

// A function to follow the gamepad switched on or off while the lists are open: its list has a floor
// only while it runs
void bindings_set_gamepad_on(Bindings *b, bool gamepad_on)
{
    b->gamepad_on = gamepad_on;
}

// A function to free the model
void bindings_free(Bindings *b)
{
    if (b == NULL)
        return;
    for (int d = 0; d < 2; d++) {
        alloc_free(b->lists[d].items);
        alloc_free(b->lists[d].entry);
    }
    alloc_free(b);
}

// A function to get a gamepad label's name by its index; "" out of range
const char *bindings_label(int index)
{
    return index >= 0 && index < BINDINGS_LABELS ? LABELS[index] : "";
}

// A function to find a gamepad label's index; -1 for none
int bindings_label_index(const char *label)
{
    for (int i = 0; i < BINDINGS_LABELS; i++) {
        if (strcmp(LABELS[i], label) == 0)
            return i;
    }
    return -1;
}

// A function to make room for one more binding
static bool grow(List *list)
{
    if (list->count < list->capacity)
        return true;
    int capacity = list->capacity ? list->capacity * 2 : 8;
    Binding *items = alloc_realloc(list->items, (size_t) capacity * sizeof(Binding));
    if (items == NULL)
        return false;
    list->items = items;
    list->capacity = capacity;
    return true;
}

// A function to read a hotkey's value as config_handler() and add_hotkey() do: semicolons before it
// skipped (its strtok_r() skips them), then "#<hex>" up to the next ';', and the rest, which must not
// be empty, as the command; false for a value the launcher would not read
static bool read_hotkey(const char *value, int *code, char *command, size_t size)
{
    value += strspn(value, ";");
    const char *semicolon = strchr(value, ';');
    if (value[0] != '#' || semicolon == NULL || semicolon[1] == '\0')
        return false;
    *code = (int) strtol(value + 1, NULL, 16);
    snprintf(command, size, "%s", semicolon + 1);
    return true;
}

// A function to empty a list that could not be loaded, with nothing for Discard to bring back
static bool load_failed(List *list)
{
    list->count = 0;
    list->entry_count = 0;
    return false;
}

// A function to load a device's bindings from its section's lines (inidoc_list()), in order. Lines
// the launcher would not read (no keycode, an unknown label, no command, or longer than inih reads
// whole) are left out: settings never edit them. A [Hotkeys] line with an empty name is kept, since
// inih reads it as a hotkey. False when out of memory, with the list empty.
bool bindings_load(Bindings *b, BindingsDevice device, const IniDocItem *items, int count)
{
    List *list = &b->lists[device];
    list->count = 0;
    for (int i = 0; i < count; i++) {
        Binding binding;
        memset(&binding, 0, sizeof(binding));
        if (strlen(items[i].text) > INIDOC_MAX_LINE)
            continue;
        if (device == BINDINGS_KEYBOARD) {
            if (!read_hotkey(items[i].value, &binding.code, binding.command, sizeof(binding.command)))
                continue;
        }
        else {
            binding.code = bindings_label_index(items[i].key);
            if (binding.code < 0 || items[i].value[0] == '\0')
                continue;
            snprintf(binding.command, sizeof(binding.command), "%s", items[i].value);
        }
        snprintf(binding.key, sizeof(binding.key), "%s", items[i].key);
        snprintf(binding.original, sizeof(binding.original), "%s", items[i].text);
        if (!grow(list))
            return load_failed(list);
        list->items[list->count++] = binding;
    }
    alloc_free(list->entry);
    list->entry = alloc_calloc((size_t) (list->count > 0 ? list->count : 1), sizeof(Binding));
    if (list->entry == NULL)
        return load_failed(list);
    if (list->count > 0)
        memcpy(list->entry, list->items, (size_t) list->count * sizeof(Binding));
    list->entry_count = list->count;
    return true;
}

// A function to count a device's bindings, removed ones included (their rows go; their places stay)
int bindings_count(const Bindings *b, BindingsDevice device)
{
    return b->lists[device].count;
}

// A function to get a binding; NULL out of range
const Binding *bindings_at(const Bindings *b, BindingsDevice device, int index)
{
    const List *list = &b->lists[device];
    return index >= 0 && index < list->count ? &list->items[index] : NULL;
}

// A function to tell whether a command is a special command, read as execute_command() reads it: by
// its first word, up to a space (":up now" is :up)
static bool command_is(const char *command, const char *name)
{
    size_t length = strcspn(command, " ");
    return strlen(name) == length && strncmp(command, name, length) == 0;
}

// A function to find a command among the floor's; -1 for another
static int floor_index(const char *command)
{
    for (int i = 0; i < FLOOR_COUNT; i++) {
        if (command_is(command, FLOOR[i]))
            return i;
    }
    return -1;
}

// A function to tell whether Windows can register a key as the exit hotkey: the keys
// platform/keycode_convert.h converts (F1-F11 and F13-F24), mirrored here because that table needs
// SDL's and Windows' headers
static bool registrable(int code)
{
    return (code >= BIND_KEY_F1 && code < BIND_KEY_F12) || (code >= BIND_KEY_F13 && code <= BIND_KEY_F24);
}

// A function to tell whether a keyboard key is one the dispatcher always takes first
static bool dispatcher_key(int code)
{
    return code == BIND_KEY_LEFT || code == BIND_KEY_RIGHT || code == BIND_KEY_RETURN || code == BIND_KEY_BACKSPACE;
}

// A function to say why a key or button cannot be bound to a command at all; NULL when it can
const char *bindings_refuse_key(const Bindings *b, BindingsDevice device, int code, const char *command)
{
    if (device == BINDINGS_GAMEPAD)
        return code < 0 || code >= BINDINGS_LABELS ? "This button has no name StreamFlex can store" : NULL;
    if (code == BIND_KEY_UNKNOWN)
        return "This key has no code StreamFlex can store (a CEC remote's OK and Back arrive this way on Linux)";
    if (dispatcher_key(code))
        return "The arrows, OK and Back keep their own meaning, so a hotkey on them would never run";
    if (b->windows && strcmp(command, ":exit") == 0 && !registrable(code))
        return "The exit hotkey must be F1 to F24, but not F12";
    return NULL;
}

// A function to get a list's binding as the view has it
static const Binding *view_at(const View *view, int i)
{
    return i == view->index ? view->changed : &view->list->items[i];
}

// A function to tell whether a keyboard binding is in the launcher's hotkey list: not removed, and not
// Windows' exit hotkey, which add_hotkey() registers with Windows instead
static bool listed_hotkey(const Bindings *b, const Binding *binding)
{
    return !binding->removed && !(b->windows && strcmp(binding->command, ":exit") == 0);
}

// A function to tell whether a listed hotkey is the first listed on its key: handle_keypress() runs
// that one alone
static bool first_on_key(const Bindings *b, const View *view, int i)
{
    int code = view_at(view, i)->code;
    for (int j = 0; j < i; j++) {
        const Binding *earlier = view_at(view, j);
        if (listed_hotkey(b, earlier) && earlier->code == code)
            return false;
    }
    return true;
}

// A function to find the key Windows registers as the exit hotkey: the first :exit (add_hotkey()'s exact
// match) on a key it can register, since set_exit_hotkey() keeps the first it can convert; false off
// Windows, or when there is none
static bool registered_exit(const Bindings *b, const View *view, int *code)
{
    if (!b->windows)
        return false;
    for (int i = 0; i < view->count; i++) {
        const Binding *binding = view_at(view, i);
        if (!binding->removed && strcmp(binding->command, ":exit") == 0 && registrable(binding->code)) {
            *code = binding->code;
            return true;
        }
    }
    return false;
}

// A function to count each floor command's ways in on the keyboard: the built-in keys (Up, Down and
// the Menu keys give way to a hotkey on them), then every hotkey the dispatcher lets run. On Windows
// the exit hotkey's key is registered system-wide and never reaches SDL, so no hotkey on it runs.
static void keyboard_counts(const Bindings *b, const View *view, int *counts)
{
    int exit_code = 0;
    bool exit_registered = registered_exit(b, view, &exit_code);
    bool taken_up = false, taken_down = false, taken_application = false, taken_menu = false;
    for (int i = 0; i < view->count; i++) {
        const Binding *binding = view_at(view, i);
        if (!listed_hotkey(b, binding))
            continue;
        taken_up = taken_up || binding->code == BIND_KEY_UP;
        taken_down = taken_down || binding->code == BIND_KEY_DOWN;
        taken_application = taken_application || binding->code == BIND_KEY_APPLICATION;
        taken_menu = taken_menu || binding->code == BIND_KEY_MENU;
    }
    counts[0] = counts[1] = counts[4] = counts[5] = 1;   // Left, Right, Return, Backspace
    counts[2] = taken_up ? 0 : 1;
    counts[3] = taken_down ? 0 : 1;
    counts[6] = (taken_application ? 0 : 1) + (taken_menu ? 0 : 1);
    for (int i = 0; i < view->count; i++) {
        const Binding *binding = view_at(view, i);
        int f = floor_index(binding->command);
        if (f >= 0 && listed_hotkey(b, binding) && !dispatcher_key(binding->code) && first_on_key(b, view, i) &&
            !(exit_registered && binding->code == exit_code))
            counts[f]++;
    }
}

// A function to count each floor command's ways in on the gamepad: its controls, then the defaults
// util.c adds for Up, Down and Settings to the labels the config leaves free, when nothing else has them
static void gamepad_counts(const View *view, int *counts)
{
    static const struct {
        int floor;
        const char *labels[2];
    } defaults[] = {
        { 2, { SETTING_GAMEPAD_BUTTON_DPAD_UP, SETTING_GAMEPAD_LSTICK_YM } },
        { 3, { SETTING_GAMEPAD_BUTTON_DPAD_DOWN, SETTING_GAMEPAD_LSTICK_YP } },
        { 6, { SETTING_GAMEPAD_BUTTON_START, NULL } }
    };
    for (int f = 0; f < FLOOR_COUNT; f++)
        counts[f] = 0;
    for (int i = 0; i < view->count; i++) {
        const Binding *binding = view_at(view, i);
        int f = floor_index(binding->command);
        if (!binding->removed && f >= 0)
            counts[f]++;
    }
    for (size_t d = 0; d < sizeof(defaults) / sizeof(defaults[0]); d++) {
        if (counts[defaults[d].floor] > 0)
            continue;
        for (int k = 0; k < 2 && defaults[d].labels[k] != NULL; k++) {
            int label = bindings_label_index(defaults[d].labels[k]);
            bool free_label = true;
            for (int i = 0; i < view->count && free_label; i++) {
                const Binding *binding = view_at(view, i);
                free_label = binding->removed || binding->code != label;
            }
            if (free_label)
                counts[defaults[d].floor]++;
        }
    }
}

// A function to say why a change would leave a floor command with no way in, which it had before;
// NULL when it would not. `index` -1 is a new binding; `remove` removes the one at `index`. An index
// past the list changes nothing, so nothing is lost: the view after it ends where the list does, and
// never reaches it. The reason is a constant string.
const char *bindings_refuse_change(const Bindings *b, BindingsDevice device, int index, int code,
                                   const char *command, bool remove)
{
    const List *list = &b->lists[device];
    if (device == BINDINGS_GAMEPAD && !b->gamepad_on)
        return NULL;
    // The floor reads a binding's code, command and whether it is removed, and a removed one's code
    // and command not at all
    Binding changed;
    memset(&changed, 0, sizeof(changed));
    if (remove)
        changed.removed = true;
    else {
        changed.code = code;
        snprintf(changed.command, sizeof(changed.command), "%s", command);
    }
    View before = { list, -1, NULL, list->count };
    View after = { list, index >= 0 ? index : list->count, &changed, index >= 0 ? list->count : list->count + 1 };
    int before_counts[FLOOR_COUNT];
    int after_counts[FLOOR_COUNT];
    if (device == BINDINGS_KEYBOARD) {
        keyboard_counts(b, &before, before_counts);
        keyboard_counts(b, &after, after_counts);
    }
    else {
        gamepad_counts(&before, before_counts);
        gamepad_counts(&after, after_counts);
    }
    for (int f = 0; f < FLOOR_COUNT; f++) {
        if (before_counts[f] > 0 && after_counts[f] == 0)
            return device == BINDINGS_KEYBOARD ? NO_KEY[f] : NO_BUTTON[f];
    }
    return NULL;
}

// A function to tell whether a keyboard binding takes a key's own navigation away: Up, Down or a
// Menu key bound to anything but its own command (read by its first word, as the floor reads it)
bool bindings_takes_navigation(BindingsDevice device, int code, const char *command)
{
    if (device != BINDINGS_KEYBOARD)
        return false;
    if (code == BIND_KEY_UP)
        return !command_is(command, ":up");
    if (code == BIND_KEY_DOWN)
        return !command_is(command, ":down");
    if (code == BIND_KEY_APPLICATION || code == BIND_KEY_MENU)
        return !command_is(command, ":settings");
    return false;
}

// A function to set a binding's key or button and command, or add one (index -1); returns its index,
// -1 when out of memory or when the index is past the list
int bindings_set(Bindings *b, BindingsDevice device, int index, int code, const char *command)
{
    List *list = &b->lists[device];
    if (index >= list->count)
        return -1;
    if (index < 0) {
        if (!grow(list))
            return -1;
        index = list->count++;
        memset(&list->items[index], 0, sizeof(Binding));
    }
    Binding *binding = &list->items[index];
    binding->code = code;
    snprintf(binding->command, sizeof(binding->command), "%s", command);
    binding->removed = false;
    return index;
}

// A function to remove a binding; its place stays, so the indexes of the others do not move
void bindings_remove(Bindings *b, BindingsDevice device, int index)
{
    List *list = &b->lists[device];
    if (index >= 0 && index < list->count)
        list->items[index].removed = true;
}

// A function to tell whether a binding differs from how it was loaded (a new one always does)
static bool binding_changed(const List *list, int i)
{
    if (i >= list->entry_count)
        return !list->items[i].removed;
    const Binding *now = &list->items[i];
    const Binding *was = &list->entry[i];
    return now->removed || now->code != was->code || strcmp(now->command, was->command) != 0;
}

// A function to tell whether any binding changed
bool bindings_changed(const Bindings *b)
{
    for (int d = 0; d < 2; d++) {
        for (int i = 0; i < b->lists[d].count; i++) {
            if (binding_changed(&b->lists[d], i))
                return true;
        }
    }
    return false;
}

// A function to put every list back as it was loaded
void bindings_discard(Bindings *b)
{
    for (int d = 0; d < 2; d++) {
        List *list = &b->lists[d];
        if (list->entry_count > 0)
            memcpy(list->items, list->entry, (size_t) list->entry_count * sizeof(Binding));
        list->count = list->entry_count;
    }
}

// A function to write a binding's line value: "#<HEX>;<command>" for a hotkey, the command for a control
static void line_value(BindingsDevice device, const Binding *binding, char *out, size_t size)
{
    if (device == BINDINGS_KEYBOARD)
        snprintf(out, size, "#%X;%s", (unsigned int) binding->code, binding->command);
    else
        snprintf(out, size, "%s", binding->command);
}

// A function to turn a device's changes into the save's list edits, in order: a changed binding
// sets its line, a removed one removes it, a new one is added (a hotkey numbered by the save); a
// new one removed again writes nothing (binding_changed() says it is no change). One edit at most per
// binding, as config_save_all() needs: each is weighed against the list as loaded, so changes made
// one after another fold into one. `values` holds each edit's value text. Returns how many, `max`
// at most.
int bindings_edits(const Bindings *b, BindingsDevice device, ConfigListEdit *edits,
                   char (*values)[BINDINGS_VALUE_MAX], int max)
{
    const List *list = &b->lists[device];
    const char *section = device == BINDINGS_KEYBOARD ? "Hotkeys" : "Gamepad";
    int n = 0;
    for (int i = 0; i < list->count && n < max; i++) {
        const Binding *binding = &list->items[i];
        bool loaded_line = i < list->entry_count;
        if (!binding_changed(list, i))
            continue;
        ConfigListEdit *edit = &edits[n];
        memset(edit, 0, sizeof(*edit));
        edit->section = section;
        edit->original = loaded_line ? list->entry[i].original : NULL;
        edit->key = device == BINDINGS_KEYBOARD ? "Hotkey" : bindings_label(binding->code);
        edit->numbered = device == BINDINGS_KEYBOARD;
        line_value(device, binding, values[n], BINDINGS_VALUE_MAX);
        edit->value = values[n];
        edit->op = !loaded_line ? CONFIG_LIST_ADD : binding->removed ? CONFIG_LIST_REMOVE : CONFIG_LIST_SET;
        n++;
    }
    return n;
}

// A function to start a capture: the key or button that started it (OK) is held, so its repeats and
// its release are ignored until it is let go
void capture_begin(Capture *capture, unsigned int now, int starting_code)
{
    capture->state = CAPTURE_LISTENING;
    capture->started = now;
    capture->window = BINDINGS_CAPTURE_MS;
    capture->starting_code = starting_code;
    capture->starting_held = true;
    capture->code = BIND_KEY_UNKNOWN;
}

// A function to take a press while capturing; true when it is the one captured
bool capture_press(Capture *capture, unsigned int now, int code, bool repeat)
{
    if (capture->state != CAPTURE_LISTENING || now - capture->started >= capture->window)
        return false;
    if (code == capture->starting_code && (capture->starting_held || repeat))
        return false;
    capture->code = code;
    capture->state = CAPTURE_CAPTURED;
    return true;
}

// A function to take a release while capturing: the starting key's lets its next press count
void capture_release(Capture *capture, int code)
{
    if (code == capture->starting_code)
        capture->starting_held = false;
}

// A function to tell, once, that 5 s passed with nothing captured
bool capture_expired(Capture *capture, unsigned int now)
{
    if (capture->state != CAPTURE_LISTENING || now - capture->started < capture->window)
        return false;
    capture->state = CAPTURE_TIMED_OUT;
    return true;
}

// A function to start the 10 s in which a key that took navigation away must be pressed again
void probation_begin(Probation *probation, unsigned int now, int code)
{
    probation->active = true;
    probation->started = now;
    probation->code = code;
}

// A function to take a press during the 10 s; true when it is the new key, which keeps the change
bool probation_press(Probation *probation, int code)
{
    if (!probation->active || code != probation->code)
        return false;
    probation->active = false;
    return true;
}

// A function to tell, once, that the 10 s passed unconfirmed: the change reverts
bool probation_expired(Probation *probation, unsigned int now)
{
    if (!probation->active || now - probation->started < BINDINGS_CONFIRM_MS)
        return false;
    probation->active = false;
    return true;
}
