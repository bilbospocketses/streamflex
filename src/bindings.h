// The keyboard's hotkeys and the gamepad's controls as the settings screen edits them: loaded from
// the file's lines ([Hotkeys], [Gamepad]), changed, removed and added, and turned into the list edits
// the save writes. The safety floor refuses a change that would leave a navigation command or
// :settings with no key or button, and says which keyboard changes take a key's navigation away (to
// be confirmed within 10 s). Capture and that confirmation are state machines driven by the caller's
// clock. Pure: no SDL, no globals; memory comes from alloc.h.
#ifndef BINDINGS_H
#define BINDINGS_H

#include <stdbool.h>
#include "inidoc.h"
#include "config_save.h"

#define BINDINGS_COMMAND_MAX 1024
#define BINDINGS_VALUE_MAX (BINDINGS_COMMAND_MAX + 16)
#define BINDINGS_LABELS 25
#define BINDINGS_CAPTURE_MS 5000
#define BINDINGS_CONFIRM_MS 10000
#define BIND_KEY_UNKNOWN 0          // SDL keycodes by value (bindings.c is pure; settings_pickers.c checks them)
#define BIND_KEY_BACKSPACE 0x08
#define BIND_KEY_RETURN 0x0D
#define BIND_KEY_RIGHT 0x4000004F
#define BIND_KEY_LEFT 0x40000050
#define BIND_KEY_DOWN 0x40000051
#define BIND_KEY_UP 0x40000052
#define BIND_KEY_F1 0x4000003A
#define BIND_KEY_F12 0x40000045
#define BIND_KEY_F13 0x40000068
#define BIND_KEY_F24 0x40000073
#define BIND_KEY_APPLICATION 0x40000065
#define BIND_KEY_MENU 0x40000076

typedef enum { BINDINGS_KEYBOARD, BINDINGS_GAMEPAD } BindingsDevice;

typedef struct {
    int code;                            // Keyboard: the keycode; gamepad: the label's index
    char command[BINDINGS_COMMAND_MAX];
    char key[64];                        // The line's key ("Hotkey3", "ButtonA"); "" for a new binding,
                                         // and for a loaded [Hotkeys] line with an empty name ("=#..."),
                                         // which inih reads as a hotkey: `original` tells the two apart
    char original[256];                  // The line as settings read it; "" for a new binding
    bool removed;
} Binding;

typedef struct Bindings Bindings;

Bindings *bindings_create(bool windows, bool gamepad_on);
void bindings_set_gamepad_on(Bindings *bindings, bool gamepad_on);
void bindings_free(Bindings *bindings);
bool bindings_load(Bindings *bindings, BindingsDevice device, const IniDocItem *items, int count);
int bindings_count(const Bindings *bindings, BindingsDevice device);
const Binding *bindings_at(const Bindings *bindings, BindingsDevice device, int index);
const char *bindings_label(int index);
int bindings_label_index(const char *label);
const char *bindings_refuse_key(const Bindings *bindings, BindingsDevice device, int code, const char *command);
const char *bindings_refuse_change(const Bindings *bindings, BindingsDevice device, int index, int code,
                                   const char *command, bool remove);
bool bindings_takes_navigation(BindingsDevice device, int code, const char *command);
int bindings_set(Bindings *bindings, BindingsDevice device, int index, int code, const char *command);
void bindings_remove(Bindings *bindings, BindingsDevice device, int index);
bool bindings_changed(const Bindings *bindings);
void bindings_discard(Bindings *bindings);
int bindings_edits(const Bindings *bindings, BindingsDevice device, ConfigListEdit *edits,
                   char (*values)[BINDINGS_VALUE_MAX], int max);

typedef enum { CAPTURE_IDLE, CAPTURE_LISTENING, CAPTURE_CAPTURED, CAPTURE_TIMED_OUT } CaptureState;
typedef struct { CaptureState state; unsigned int started; int starting_code; bool starting_held; int code; } Capture;
void capture_begin(Capture *capture, unsigned int now, int starting_code);
bool capture_press(Capture *capture, unsigned int now, int code, bool repeat);    // true: captured
void capture_release(Capture *capture, int code);
bool capture_expired(Capture *capture, unsigned int now);                         // true: timed out now

// The 10 s confirmation. probation_press() has no clock, so it would still confirm a press that comes
// after the 10 s: each frame the caller polls probation_expired() FIRST, and only then feeds that
// frame's presses to probation_press().
typedef struct { bool active; unsigned int started; int code; } Probation;
void probation_begin(Probation *probation, unsigned int now, int code);
bool probation_press(Probation *probation, int code);                             // true: confirmed
bool probation_expired(Probation *probation, unsigned int now);                   // true: revert now

#endif
