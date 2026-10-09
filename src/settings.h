// The settings screen's model: the table of settings it can change, how each one's value is read
// from and written to config.ini, stepped with Left and Right and described on screen, and the
// pages the remote moves through. Pure: no SDL, no globals, so tests/test_settings.c builds it on
// its own; memory comes from alloc.h. settings_screen.c draws it and applies what it changes.
#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include "bindings.h"

#define SETTING_TEXT_MAX 1024  // Longest path a setting holds
#define SETTINGS_MAX_ROWS 64   // Most rows one page shows; Menus counts the rest in a note
#define SETTINGS_MAX_DEPTH 8   // Deepest the pages go
#define SETTINGS_MAX_PADS 16   // Most gamepads the Device row names

typedef enum {
    SET_TYPE_COUNT,       // A whole number: Rows, Columns
    SET_TYPE_CHOICE,      // One of the setting's names (def->names): Mode, OnLaunch, OversizeMode
    SET_TYPE_ICON_SIZE,   // IconSize: Fill (no key), or px
    SET_TYPE_COLOR,       // #RRGGBB, stepped through the presets
    SET_TYPE_PATH,        // A file or folder, chosen in the browser, never stepped
    SET_TYPE_SECONDS,     // Whole seconds, stepped through def->steps
    SET_TYPE_MILLIS,      // Seconds with decimals in the file, ms here: SlideshowTransitionTime
    SET_TYPE_TITLE_SIZE,  // FontSize: a percentage of the button, or a fixed size
    SET_TYPE_BOOL,        // true or false, shown On or Off
    SET_TYPE_NUMBER,      // A whole number with limits: OutlineSize, FPSLimit, FontFace
    SET_TYPE_PERCENT,     // "N%" with up to two decimals (number in hundredths), or px where allowed
    SET_TYPE_COMMAND,     // A command, chosen in the command picker
    SET_TYPE_FONT,        // A font file, chosen in the font picker
    SET_TYPE_MENU,        // A menu's name: DefaultMenu, stepped through the menus by the pages
    SET_TYPE_DEVICE       // A gamepad's device index, -1 for any, stepped through the pads by the pages
} SettingType;

#define SET_FLAG_PX 1          // PERCENT: a plain whole number in the file is px
#define SET_FLAG_WHOLE 2       // PERCENT: whole percentages only (Padding, which layout.c reads whole)
#define SET_FLAG_HIDDEN 4      // No row of its own: FontFace, set by the font picker with its font
#define SET_FLAG_NEXT_START 8  // Applies at next start: ControllerMappingsFile

typedef enum {
    SET_REFRESH_NONE,        // The launcher reads the value when it next needs it
    SET_REFRESH_LAYOUT,      // Lay the menu on show out again
    SET_REFRESH_TITLES,      // Render every menu's titles again
    SET_REFRESH_BACKGROUND,  // Set the background up again, its overlay included
    SET_REFRESH_TITLE_FONT,  // Open the title font again, then render the titles
    SET_REFRESH_HIGHLIGHT,   // Render the highlight again
    SET_REFRESH_SCROLL,      // Render the scroll indicators again
    SET_REFRESH_CLOCK,       // Restart the clock, then lay the menu out again around it
    SET_REFRESH_SCREENSAVER, // Restart the screensaver
    SET_REFRESH_GAMEPAD,     // Restart the gamepad
    SET_REFRESH_FRAME,       // Work the frame timing out again: VSync and FPSLimit
    SET_REFRESH_COUNT        // Not a group: how many there are. A new group goes before it.
} SettingRefresh;

typedef enum {
    SET_ID_BACKGROUND_MODE,
    SET_ID_BACKGROUND_COLOR,
    SET_ID_BACKGROUND_IMAGE,
    SET_ID_SLIDESHOW_DIRECTORY,
    SET_ID_SLIDESHOW_DURATION,
    SET_ID_SLIDESHOW_FADE,
    SET_ID_LAYOUT_ROWS,
    SET_ID_LAYOUT_COLUMNS,
    SET_ID_LAYOUT_ICON_SIZE,
    SET_ID_TITLE_SIZE,
    // [General]
    SET_ID_DEFAULT_MENU,
    SET_ID_WRAP_ENTRIES,
    SET_ID_RESET_ON_BACK,
    SET_ID_MOUSE_SELECT,
    SET_ID_INHIBIT_OS_SCREENSAVER,
    SET_ID_VSYNC,
    SET_ID_FPS_LIMIT,
    SET_ID_ON_LAUNCH,
    SET_ID_APPLICATION_TIMEOUT,
    SET_ID_STARTUP_CMD,
    SET_ID_QUIT_CMD,
    // [Background], beyond 3a's
    SET_ID_CHROMA_KEY_COLOR,
    SET_ID_OVERLAY,
    SET_ID_OVERLAY_COLOR,
    SET_ID_OVERLAY_OPACITY,
    // [Layout], beyond 3a's
    SET_ID_ICON_SPACING,
    SET_ID_VCENTER,
    // [Titles], beyond 3a's
    SET_ID_TITLES_ENABLED,
    SET_ID_TITLE_FONT,
    SET_ID_TITLE_FONT_FACE,
    SET_ID_TITLE_COLOR,
    SET_ID_TITLE_OPACITY,
    SET_ID_TITLE_SHADOWS,
    SET_ID_TITLE_SHADOW_COLOR,
    SET_ID_TITLE_OVERSIZE,
    SET_ID_TITLE_PADDING,
    // [Highlight]
    SET_ID_HIGHLIGHT_ENABLED,
    SET_ID_HIGHLIGHT_FILL_COLOR,
    SET_ID_HIGHLIGHT_FILL_OPACITY,
    SET_ID_HIGHLIGHT_OUTLINE_SIZE,
    SET_ID_HIGHLIGHT_OUTLINE_COLOR,
    SET_ID_HIGHLIGHT_OUTLINE_OPACITY,
    SET_ID_HIGHLIGHT_CORNER_RADIUS,
    SET_ID_HIGHLIGHT_VPADDING,
    SET_ID_HIGHLIGHT_HPADDING,
    // [Scroll Indicators]
    SET_ID_SCROLL_ENABLED,
    SET_ID_SCROLL_FILL_COLOR,
    SET_ID_SCROLL_OUTLINE_SIZE,
    SET_ID_SCROLL_OUTLINE_COLOR,
    SET_ID_SCROLL_OPACITY,
    // [Clock]
    SET_ID_CLOCK_ENABLED,
    SET_ID_CLOCK_SHOW_DATE,
    SET_ID_CLOCK_ALIGNMENT,
    SET_ID_CLOCK_FONT,
    SET_ID_CLOCK_FONT_FACE,
    SET_ID_CLOCK_COLOR,
    SET_ID_CLOCK_SHADOWS,
    SET_ID_CLOCK_SHADOW_COLOR,
    SET_ID_CLOCK_OPACITY,
    SET_ID_CLOCK_FONT_SIZE,
    SET_ID_CLOCK_MARGIN,
    SET_ID_CLOCK_TIME_FORMAT,
    SET_ID_CLOCK_DATE_FORMAT,
    SET_ID_CLOCK_WEEKDAY,
    // [Screensaver]
    SET_ID_SCREENSAVER_ENABLED,
    SET_ID_SCREENSAVER_IDLE_TIME,
    SET_ID_SCREENSAVER_INTENSITY,
    SET_ID_SCREENSAVER_PAUSE,
    // [Gamepad]
    SET_ID_GAMEPAD_ENABLED,
    SET_ID_GAMEPAD_DEVICE,
    SET_ID_GAMEPAD_MAPPINGS,
    SET_ID_MENU_ROWS,        // The per-menu settings come last: one of each for every menu
    SET_ID_MENU_COLUMNS,
    SET_ID_MENU_ICON_SIZE,
    SET_ID_COUNT
} SettingId;

#define SET_ID_GLOBAL_COUNT SET_ID_MENU_ROWS
#define SET_ID_PER_MENU_COUNT (SET_ID_COUNT - SET_ID_MENU_ROWS)

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} SettingColor;

typedef struct {
    bool inherit;                // The key is absent: a menu follows [Layout], IconSize is Fill, FPSLimit is Off
    int number;                  // COUNT, CHOICE (index), ICON_SIZE (px), SECONDS (s), MILLIS (ms), TITLE_SIZE,
                                 // BOOL (1/0), NUMBER, DEVICE (-1 any), PERCENT (hundredths, or px)
    bool percent;                // TITLE_SIZE, PERCENT: `number` is a percentage (PERCENT: in hundredths)
    SettingColor color;          // COLOR
    char text[SETTING_TEXT_MAX]; // PATH, FONT, COMMAND, MENU; "" when none is chosen
} SettingValue;

typedef struct {
    SettingId id;
    const char *label;           // The row's label on screen
    const char *section;         // NULL: the section of the menu being edited
    const char *key;
    const char *alias;           // An older key the parser reads as this one (MaxButtons); NULL if none
    SettingType type;
    int min;                     // What the file may hold: COUNT, NUMBER, DEVICE, SECONDS, MILLIS;
    int max;                     // PERCENT's percentages, in hundredths
    bool can_inherit;            // The lowest step removes the key
    SettingRefresh refresh;
    int lo;                      // The steps: NUMBER and PERCENT from lo to hi by `step`, CHOICE the
    int hi;                      // names from index lo to hi
    int step;
    const int *steps;            // The steps as a list instead (SECONDS, NUMBER); NULL for none
    int step_count;
    const char *const *names;    // CHOICE: the file's names, NULL-terminated...
    const char *const *labels;   // ...and the screen's, one for each
    const char *legacy;          // CHOICE: an older spelling the parser also reads; NULL if none...
    int legacy_index;            // ...and the name it stands for
    const char *unit;            // NUMBER: after the number on screen (" px"); NULL for none
    int max_px;                  // PERCENT with SET_FLAG_PX: the largest px the file may hold
    int flags;                   // SET_FLAG_*
    const char *fallback;        // The built-in value as the file would write it, for Config's start; NULL:
                                 // launcher.c's initializer holds it
    const char *inherit_label;   // What the lowest step shows when it removes the key ("Off", "None")
} SettingDef;

typedef struct {
    const SettingDef *def;
    int menu;                    // The menu's index for a per-menu setting; -1 otherwise
    SettingValue value;          // What the launcher shows now
    SettingValue entry;          // What it was when settings opened: Discard's target, and the changed test
} SettingSlot;

typedef enum {
    SETTINGS_PAGE_TOP,
    SETTINGS_PAGE_GENERAL,
    SETTINGS_PAGE_BACKGROUND,
    SETTINGS_PAGE_MENUS,
    SETTINGS_PAGE_MENU,          // One menu's grid, or All menus ([Layout])
    SETTINGS_PAGE_TITLES,
    SETTINGS_PAGE_HIGHLIGHT,
    SETTINGS_PAGE_SCROLL,
    SETTINGS_PAGE_CLOCK,
    SETTINGS_PAGE_SCREENSAVER,
    SETTINGS_PAGE_CONTROLS,
    SETTINGS_PAGE_GAMEPAD,
    SETTINGS_PAGE_KEYBOARD,      // The hotkeys
    SETTINGS_PAGE_BINDING,       // One binding: its key or button, its command, Remove
    SETTINGS_PAGE_CAPTURE,       // Press the key or button
    SETTINGS_PAGE_CONFIRM,       // Keep the key captured, try again, or cancel
    SETTINGS_PAGE_SAVE_FAILED,
    SETTINGS_PAGE_RESTART        // Saved: restart StreamFlex now to apply what waits for the next start?
} SettingsPage;

typedef enum {
    SETTINGS_ROW_SETTING,        // Left and Right step its value
    SETTINGS_ROW_LINK,           // OK opens another page
    SETTINGS_ROW_BROWSE,         // OK opens the folder browser for its path
    SETTINGS_ROW_PICK,           // OK opens a picker for its slot; Left and Right step it when its type steps
    SETTINGS_ROW_ACTION,         // OK does something
    SETTINGS_ROW_BINDING,        // OK opens a binding's page
    SETTINGS_ROW_DIVIDER,
    SETTINGS_ROW_NOTE            // Text only
} SettingsRowKind;

typedef enum {
    SETTINGS_ACTION_NONE,
    SETTINGS_ACTION_DISCARD,
    SETTINGS_ACTION_RETRY,
    SETTINGS_ACTION_LEAVE,
    SETTINGS_ACTION_ADD_BINDING,
    SETTINGS_ACTION_CAPTURE,
    SETTINGS_ACTION_BIND_COMMAND,
    SETTINGS_ACTION_REMOVE_BINDING,
    SETTINGS_ACTION_KEEP,
    SETTINGS_ACTION_TRY_AGAIN,
    SETTINGS_ACTION_CANCEL,
    SETTINGS_ACTION_RESTART,
    SETTINGS_ACTION_NO_RESTART
} SettingsAction;

typedef struct {
    SettingsRowKind kind;
    char label[128];
    char value[256];             // What the row shows on the right
    const char *note;            // NOTE rows: the text, owned by the model
    SettingSlot *slot;           // SETTING, BROWSE and PICK rows
    SettingsPage target;         // LINK rows
    int menu;                    // LINK rows to a menu's page: its index; -1 for All menus
    SettingsAction action;       // ACTION rows
    bool enabled;                // False: shown grayed, and the cursor skips it unless `why` gives a reason
    const char *why;             // A grayed row's reason (the cursor may rest on it); NULL for none
    bool steps;                  // Left and Right step its value while it is enabled: a setting, or a
                                 // picker for a color, the default menu or the device
    int binding;                 // BINDING rows: the binding's index
} SettingsRow;

typedef enum {
    SETTINGS_UP,
    SETTINGS_DOWN,
    SETTINGS_LEFT,
    SETTINGS_RIGHT,
    SETTINGS_OK,
    SETTINGS_BACK,
    SETTINGS_HOME,               // Save and close, then go to the default menu
    SETTINGS_CLOSE               // Save and close (the key that opened settings)
} SettingsCommand;

typedef enum {
    SETTINGS_EVENT_NONE,
    SETTINGS_EVENT_MOVED,        // The cursor or the page changed: redraw
    SETTINGS_EVENT_CHANGED,      // `slot`'s value changed from `before`: apply it
    SETTINGS_EVENT_BROWSE,       // Open the folder browser for `slot`
    SETTINGS_EVENT_PICK,         // Open the picker for `slot`
    SETTINGS_EVENT_DISCARD,      // Every value went back to its entry: apply them all
    SETTINGS_EVENT_CLOSE,        // Save and close; apply `slot` first when it is set
    SETTINGS_EVENT_CLOSE_HOME,   // The same, then go to the default menu
    SETTINGS_EVENT_RETRY,        // Try the failed save again
    SETTINGS_EVENT_LEAVE,        // Close without saving
    SETTINGS_EVENT_CAPTURE,      // Start capturing a key or button for the binding page's device
    SETTINGS_EVENT_PICK_COMMAND, // Open the command picker for the binding page's binding
    SETTINGS_EVENT_BINDINGS,     // The binding lists changed: apply them; with `confirm`, start the 10 s for `code`
    SETTINGS_EVENT_RESTART,      // Saved: restart StreamFlex now
    SETTINGS_EVENT_CLOSE_SAVED   // Saved: close, with no restart and nothing saved again
} SettingsEventKind;

typedef struct {
    SettingsEventKind kind;
    SettingSlot *slot;
    SettingValue before;
    int device;                  // CAPTURE and BINDINGS: the BindingsDevice captured from, or changed
    int code;                    // BINDINGS with confirm: the key whose navigation was taken
    bool confirm;
} SettingsEvent;

typedef struct SettingsState SettingsState;

// A function the screen gives to name a key or button: a key by its name, a pad's control by its label
typedef void (*SettingsKeyNamer)(int device, int code, char *out, size_t size);

const SettingDef *setting_def(SettingId id);
const SettingDef *setting_find(const char *section, const char *key);
bool setting_parse(const SettingDef *def, const char *text, SettingValue *value);
void setting_format(const SettingDef *def, const SettingValue *value, char *out, size_t size);
bool setting_equal(const SettingDef *def, const SettingValue *a, const SettingValue *b);
SettingValue setting_step(const SettingDef *def, const SettingValue *current, const SettingValue *entry, int direction);
void setting_describe(const SettingDef *def, const SettingValue *value, const SettingValue *inherited, char *out, size_t size);
void setting_command_label(const char *command, char *out, size_t size);

SettingsState *settings_create(const char *const *menu_names, int menu_count);
void settings_free(SettingsState *state);
SettingSlot *settings_slot(SettingsState *state, SettingId id, int menu);
int settings_slot_count(const SettingsState *state);
SettingSlot *settings_slot_at(SettingsState *state, int index);
void settings_set_entry(SettingsState *state, SettingId id, int menu, const SettingValue *value);
bool settings_changed(const SettingSlot *slot);
bool settings_any_changed(const SettingsState *state);
int settings_rows(SettingsState *state, SettingsRow *rows, int max);
bool settings_row_selectable(const SettingsRow *row);
int settings_cursor(const SettingsState *state);
SettingsPage settings_page(const SettingsState *state);
void settings_path(const SettingsState *state, char *out, size_t size);
int settings_preview_menu(SettingsState *state);
const char *settings_notice(const SettingsState *state);
SettingsEvent settings_command(SettingsState *state, SettingsCommand command);
SettingsEvent settings_choose(SettingsState *state, SettingSlot *slot, const char *path);
SettingsEvent settings_choose_value(SettingsState *state, SettingSlot *slot, const SettingValue *value);
// The gamepads present, by device index. A negative count names none. A NULL name, or one whose
// copy ran out of memory, is named "Pad N" as the Device row shows it; settings_pad_name() gives
// NULL only past the pads named (index < 0 or >= the count).
void settings_set_pads(SettingsState *state, const char *const *names, int count);
int settings_pad_count(const SettingsState *state);
const char *settings_pad_name(const SettingsState *state, int index);
void settings_show_save_failed(SettingsState *state, const char *message);
// The restart prompt, after a save that wrote a setting that applies at next start: its names, as
// the prompt words them ("the mappings file", "the A and the B", "the A, the B and the C")
void settings_restart_name(const char *label, char *out, size_t size);
void settings_join_names(const char *const *names, int count, char *out, size_t size);
int settings_next_start(const SettingsState *state, char *out, size_t size);
void settings_show_restart(SettingsState *state, const char *names);
// The key and gamepad bindings, while settings are open (NULL: no binding pages), and a way to name
// keys and buttons (NULL: "#<HEX>"). The caller owns the bindings and frees them after the model.
void settings_set_bindings(SettingsState *state, Bindings *bindings, SettingsKeyNamer namer);
SettingsEvent settings_captured(SettingsState *state, int code);
void settings_capture_ended(SettingsState *state, const char *why);
SettingsEvent settings_bind_command(SettingsState *state, const char *command);
SettingsEvent settings_revert_binding(SettingsState *state);
void settings_keep_binding(SettingsState *state);   // The 10 s were confirmed: nothing goes back
const char *settings_binding_command(const SettingsState *state);   // The command the binding page shows

#endif
