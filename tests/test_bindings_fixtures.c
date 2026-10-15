// 3b's bindings fixtures, written for SDL2-era files, load to exactly the bindings they held (spec
// section 9, test 2): each line's key, code and command, and every keyboard code reads as the same key
// from the code that key sends on SDL3 (keycodes.h), on both platforms
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "check.h"
#include "inidoc.h"
#include "bindings.h"
#include "fileio.h"
#include "alloc.h"
#include "keycodes.h"

typedef struct { const char *key; int code; const char *command; } Expected;   // code -1: the label's index

// A function to load one device's lines of a fixture as the settings screen loads them
// (settings_screen.c's load_section()), and check them against the bindings they must give
static void check_fixture(const char *name, BindingsDevice device, const Expected *expected, int count)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.ini", FIXTURES, name);
    size_t length = 0;
    char *text = fileio_read_all(path, &length);
    CHECK(text != NULL);
    if (text == NULL)
        return;
    IniDoc *doc = inidoc_parse(text, length);
    alloc_free(text);
    static const char *const skip[] = { "Enabled", "DeviceIndex", "ControllerMappingsFile", NULL };
    IniDocItem items[16];
    int lines = inidoc_list(doc, device == BINDINGS_KEYBOARD ? "Hotkeys" : "Gamepad",
                            device == BINDINGS_KEYBOARD ? NULL : skip, items, 16);
    Bindings *b = bindings_create(false, true);
    CHECK(bindings_load(b, device, items, lines));
    CHECK_INT(bindings_count(b, device), count);
    for (int i = 0; i < count && i < bindings_count(b, device); i++) {
        const Binding *binding = bindings_at(b, device, i);
        CHECK_STR(binding->key, expected[i].key);
        CHECK_INT(binding->code, expected[i].code >= 0 ? expected[i].code : bindings_label_index(expected[i].key));
        CHECK_STR(binding->command, expected[i].command);
        if (device == BINDINGS_KEYBOARD) {
            uint32_t code = (uint32_t) binding->code;
            CHECK(keycode_to_stored(keycode_from_stored(code), false) == code);
            CHECK(keycode_to_stored(keycode_from_stored(code), true) == code);
        }
    }
    bindings_free(b);
    inidoc_free(doc);
}

int main(void)
{
    static const Expected empty[] = { { "", 0x4000003C, ":home" } };
    static const Expected exits[] = { { "Hotkey1", 0x4000003E, ":exit" }, { "Hotkey2", 0x4000003F, ":exit" },
                                      { "Hotkey3", 0x40000040, ":home" } };
    static const Expected note[] = { { "Hotkey1", 0x4000003E, ":home" }, { "Hotkey2", 0x4000003F, ":home" } };
    static const Expected select[] = { { "Hotkey1", 0x40000040, ":select" } };
    static const Expected up[] = { { "Hotkey1", 0x4000003A, ":up" } };
    static const Expected word[] = { { "Hotkey1", 0x4000003E, ":down now" } };
    static const Expected media[] = { { "Hotkey1", 0x40000105, ":quit" }, { "Hotkey2", 0x40000102, ":right" },
                                      { "Hotkey3", 0x40000103, ":left" }, { "Hotkey4", 0x40000104, ":home" },
                                      { "Hotkey5", 0x4000010E, ":back" } };
    static const Expected button_a[] = { { "ButtonA", -1, ":select" } };
    static const Expected padok[] = { { "ButtonA", -1, ":select" }, { "ButtonY", -1, ":quit" } };
    check_fixture("f62-empty", BINDINGS_KEYBOARD, empty, 1);
    check_fixture("f62-exit", BINDINGS_KEYBOARD, exits, 3);
    check_fixture("f62-note", BINDINGS_KEYBOARD, note, 2);
    check_fixture("f62-select", BINDINGS_KEYBOARD, select, 1);
    check_fixture("f62-up", BINDINGS_KEYBOARD, up, 1);
    check_fixture("f62-uppad", BINDINGS_KEYBOARD, up, 1);
    check_fixture("f62-word", BINDINGS_KEYBOARD, word, 1);
    check_fixture("f62-media", BINDINGS_KEYBOARD, media, 5);
    check_fixture("f62-keys", BINDINGS_KEYBOARD, NULL, 0);
    check_fixture("f62-keys", BINDINGS_GAMEPAD, NULL, 0);
    check_fixture("f62-pad", BINDINGS_GAMEPAD, NULL, 0);
    check_fixture("f62-padoff", BINDINGS_GAMEPAD, NULL, 0);
    check_fixture("f62-gpoff", BINDINGS_GAMEPAD, button_a, 1);
    check_fixture("f62-padok", BINDINGS_GAMEPAD, padok, 2);
    check_fixture("f62-uppad", BINDINGS_GAMEPAD, button_a, 1);
    return check_report();
}
