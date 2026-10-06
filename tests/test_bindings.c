#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "check.h"
#include "inidoc.h"
#include "bindings.h"

// A function to load a device's lines from text, as settings do from the fresh file
static Bindings *loaded(bool windows, const char *hotkeys, const char *gamepad)
{
    Bindings *b = bindings_create(windows, true);
    IniDocItem items[16];
    int count = 0;
    char text[2048];
    snprintf(text, sizeof(text), "[Hotkeys]\n%s\n[Gamepad]\nEnabled=true\n%s\n", hotkeys, gamepad);
    IniDoc *doc = inidoc_parse(text, strlen(text));
    static const char *const skip[] = { "Enabled", "DeviceIndex", "ControllerMappingsFile", NULL };
    count = inidoc_list(doc, "Hotkeys", NULL, items, 16);
    CHECK(bindings_load(b, BINDINGS_KEYBOARD, items, count));
    count = inidoc_list(doc, "Gamepad", skip, items, 16);
    CHECK(bindings_load(b, BINDINGS_GAMEPAD, items, count));
    inidoc_free(doc);
    return b;
}

// A function to test loading lines: codes, commands, keys and the lines as read; lines the launcher
// would not read are left out
static void test_load(void)
{
    Bindings *b = loaded(false, "Hotkey1=#4000003A;:quit\nHotkey2=nonsense\nHotkey3=#4000003B;kodi --standalone",
                         "ButtonA=:select\nButtonNope=:quit\nButtonA=:up");
    CHECK_INT(bindings_count(b, BINDINGS_KEYBOARD), 2);
    const Binding *first = bindings_at(b, BINDINGS_KEYBOARD, 0);
    CHECK_INT(first->code, 0x4000003A);
    CHECK_STR(first->command, ":quit");
    CHECK_STR(first->key, "Hotkey1");
    CHECK_STR(first->original, "Hotkey1=#4000003A;:quit");
    CHECK_STR(bindings_at(b, BINDINGS_KEYBOARD, 1)->command, "kodi --standalone");
    CHECK_INT(bindings_count(b, BINDINGS_GAMEPAD), 2);                // An unknown label is left out
    CHECK_INT(bindings_at(b, BINDINGS_GAMEPAD, 0)->code, bindings_label_index("ButtonA"));
    CHECK_STR(bindings_label(bindings_at(b, BINDINGS_GAMEPAD, 1)->code), "ButtonA");
    CHECK_STR(bindings_label(0), "LStickX-");
    CHECK_STR(bindings_label(24), "ButtonDPadRight");
    CHECK_INT(bindings_label_index("RTrigger"), 9);
    CHECK_INT(bindings_label_index("Nope"), -1);
    CHECK(!bindings_changed(b));
    bindings_free(b);
}

// A function to test which lines loading keeps, as the launcher reads them: a hotkey with no '#', no
// ';' or no command is left out; semicolons before it are skipped, as config_handler()'s strtok_r()
// skips them; a command may itself start with ';'; a key with an empty name is kept, since inih reads
// it; a control with no command is left out; and a line too long for inih to read whole is left out
static void test_load_lines(void)
{
    Bindings *b = loaded(false, "Hotkey1=#4000003A;\nHotkey2=4000003A;:quit\nHotkey3=#4000003A\n"
                         "Hotkey4=;;#4000003B;:home\n=#4000003C;:back\nHotkey5=#4000003D;;x",
                         "ButtonA=\nButtonB=:back");
    CHECK_INT(bindings_count(b, BINDINGS_KEYBOARD), 3);
    const Binding *semicolons = bindings_at(b, BINDINGS_KEYBOARD, 0);
    CHECK_INT(semicolons->code, 0x4000003B);
    CHECK_STR(semicolons->command, ":home");
    CHECK_STR(semicolons->key, "Hotkey4");
    const Binding *nameless = bindings_at(b, BINDINGS_KEYBOARD, 1);
    CHECK_INT(nameless->code, 0x4000003C);
    CHECK_STR(nameless->command, ":back");
    CHECK_STR(nameless->key, "");
    CHECK_STR(nameless->original, "=#4000003C;:back");
    CHECK_STR(bindings_at(b, BINDINGS_KEYBOARD, 2)->command, ";x");
    CHECK_INT(bindings_count(b, BINDINGS_GAMEPAD), 1);
    CHECK_STR(bindings_at(b, BINDINGS_GAMEPAD, 0)->command, ":back");
    bindings_free(b);

    // inih reads 199 bytes of a line at most (INIDOC_MAX_LINE): a line of 199 is kept, one of 200 is not
    char lines[512];
    snprintf(lines, sizeof(lines), "Hotkey1=#4000003A;%0181d\nHotkey2=#4000003B;%0182d", 0, 0);
    b = loaded(false, lines, "");
    CHECK_INT(bindings_count(b, BINDINGS_KEYBOARD), 1);
    CHECK_STR(bindings_at(b, BINDINGS_KEYBOARD, 0)->key, "Hotkey1");
    CHECK_INT((int) strlen(bindings_at(b, BINDINGS_KEYBOARD, 0)->original), 199);
    bindings_free(b);
}

// A function to test the lookups out of range, and an index out of range changing nothing
static void test_lookups(void)
{
    CHECK_STR(bindings_label(-1), "");
    CHECK_STR(bindings_label(BINDINGS_LABELS), "");
    CHECK_INT(bindings_label_index("LStickX-"), 0);
    CHECK_INT(bindings_label_index("ButtonDPadRight"), 24);
    Bindings *b = loaded(false, "Hotkey1=#4000003A;:quit", "");
    CHECK(bindings_at(b, BINDINGS_KEYBOARD, -1) == NULL);
    CHECK(bindings_at(b, BINDINGS_KEYBOARD, 1) == NULL);
    CHECK(bindings_at(b, BINDINGS_GAMEPAD, 0) == NULL);
    CHECK_INT(bindings_set(b, BINDINGS_KEYBOARD, 1, 0x4000003B, ":up"), -1);
    bindings_remove(b, BINDINGS_KEYBOARD, 1);
    bindings_remove(b, BINDINGS_KEYBOARD, -1);
    CHECK_INT(bindings_count(b, BINDINGS_KEYBOARD), 1);
    CHECK(!bindings_changed(b));
    bindings_free(b);

    // An empty list has no binding 0: set and remove leave it alone, and the floor has nothing to
    // weigh there (it is not taken as a new binding)
    b = loaded(false, "", "");
    bindings_remove(b, BINDINGS_KEYBOARD, 0);
    CHECK_INT(bindings_set(b, BINDINGS_KEYBOARD, 0, 0x4000003B, ":up"), -1);
    CHECK_INT(bindings_count(b, BINDINGS_KEYBOARD), 0);
    CHECK(bindings_refuse_change(b, BINDINGS_KEYBOARD, 0, BIND_KEY_UP, ":quit", false) == NULL);
    bindings_free(b);

    // Ten lines: the list grows past its first eight
    char lines[512] = "";
    for (int i = 0; i < 10; i++) {
        size_t used = strlen(lines);
        snprintf(lines + used, sizeof(lines) - used, "Hotkey%d=#%X;:home\n", i + 1, (unsigned int) (0x4000003A + i));
    }
    b = loaded(false, lines, "");
    CHECK_INT(bindings_count(b, BINDINGS_KEYBOARD), 10);
    CHECK_INT(bindings_at(b, BINDINGS_KEYBOARD, 9)->code, 0x40000043);
    CHECK_STR(bindings_at(b, BINDINGS_KEYBOARD, 9)->key, "Hotkey10");
    bindings_free(b);
}

// A function to test the keys the floor refuses outright
static void test_refused_keys(void)
{
    Bindings *b = loaded(false, "", "");
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, BIND_KEY_UNKNOWN, ":quit") != NULL);
    CHECK_STR(bindings_refuse_key(b, BINDINGS_KEYBOARD, BIND_KEY_LEFT, ":quit"),
              "The arrows, OK and Back keep their own meaning, so a hotkey on them would never run");
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, BIND_KEY_RETURN, ":quit") != NULL);
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, BIND_KEY_BACKSPACE, ":quit") != NULL);
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, BIND_KEY_UP, ":quit") == NULL);          // Allowed, with a confirmation
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, 0x4000003E, ":exit") == NULL);          // Linux: :exit logs, harmless
    CHECK(bindings_refuse_key(b, BINDINGS_GAMEPAD, -1, ":quit") != NULL);
    bindings_free(b);

    // On Windows the exit hotkey must be a function key, F1 to F24, but not F12
    b = loaded(true, "", "");
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, 0x4000003E, ":exit") == NULL);
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, BIND_KEY_F12, ":exit") != NULL);
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, 0x61, ":exit") != NULL);
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, BIND_KEY_F24, ":exit") == NULL);
    bindings_free(b);
}

// A function to test the rest of the keys and buttons refused: Right, a button past the last label,
// the exit hotkey's rule on Windows alone and for :exit alone, and the function keys' edges
static void test_refused_keys_more(void)
{
    Bindings *b = loaded(false, "", "");
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, BIND_KEY_RIGHT, ":quit") != NULL);
    CHECK_STR(bindings_refuse_key(b, BINDINGS_GAMEPAD, BINDINGS_LABELS, ":quit"),
              "This button has no name StreamFlex can store");
    CHECK(bindings_refuse_key(b, BINDINGS_GAMEPAD, BINDINGS_LABELS - 1, ":quit") == NULL);
    CHECK(bindings_refuse_key(b, BINDINGS_GAMEPAD, 0, ":quit") == NULL);
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, 0x61, ":exit") == NULL);                 // Not Windows: any key
    bindings_free(b);

    b = loaded(true, "", "");
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, 0x61, ":quit") == NULL);                 // Only :exit has the rule
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, BIND_KEY_F1, ":exit") == NULL);
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, BIND_KEY_F1 - 1, ":exit") != NULL);
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, BIND_KEY_F12 - 1, ":exit") == NULL);     // F11
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, BIND_KEY_F12 + 1, ":exit") != NULL);
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, BIND_KEY_F13, ":exit") == NULL);
    CHECK(bindings_refuse_key(b, BINDINGS_KEYBOARD, BIND_KEY_F13 - 1, ":exit") != NULL);
    CHECK_STR(bindings_refuse_key(b, BINDINGS_KEYBOARD, BIND_KEY_F24 + 1, ":exit"),
              "The exit hotkey must be F1 to F24, but not F12");
    CHECK_STR(bindings_refuse_key(b, BINDINGS_KEYBOARD, BIND_KEY_UNKNOWN, ":exit"),
              "This key has no code StreamFlex can store (a CEC remote's OK and Back arrive this way on Linux)");
    bindings_free(b);
}

// A function to test the floor: the last way to a navigation command or :settings stays
static void test_floor(void)
{
    // Keyboard: Up's own key moves up, so a hotkey for :up can go; taking Up over leaves none
    Bindings *b = loaded(false, "Hotkey1=#4000003A;:up", "");
    CHECK(bindings_refuse_change(b, BINDINGS_KEYBOARD, 0, 0, NULL, true) == NULL);
    CHECK(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_UP, ":quit", false) == NULL);   // F1 still goes up
    bindings_free(b);
    b = loaded(false, "", "");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_UP, ":quit", false),
              "That would leave no key for Up");
    // Both Menu keys taken leaves no key for Settings; one of them is fine
    CHECK(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_APPLICATION, ":quit", false) == NULL);
    bindings_set(b, BINDINGS_KEYBOARD, -1, BIND_KEY_APPLICATION, ":quit");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_MENU, ":home", false),
              "That would leave no key for Settings");
    bindings_free(b);

    // Gamepad: the built-in Up, Down and Settings controls come back when nothing else has them
    b = loaded(false, "", "ButtonDPadUp=:up\nButtonA=:select\nButtonB=:back");
    CHECK(bindings_refuse_change(b, BINDINGS_GAMEPAD, 0, 0, NULL, true) == NULL);              // The D-pad's own default
    CHECK_STR(bindings_refuse_change(b, BINDINGS_GAMEPAD, 1, 0, NULL, true),
              "That would leave no button for OK");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_GAMEPAD, 2, bindings_label_index("ButtonB"), ":home", false),
              "That would leave no button for Back");
    CHECK(bindings_refuse_change(b, BINDINGS_GAMEPAD, 1, bindings_label_index("ButtonX"), ":select", false) == NULL);
    bindings_free(b);

    // A command that had no binding to begin with is not the floor's to keep
    b = loaded(false, "", "ButtonA=:quit");
    CHECK(bindings_refuse_change(b, BINDINGS_GAMEPAD, 0, 0, NULL, true) == NULL);
    bindings_free(b);

    // The gamepad off: its list has no floor
    b = bindings_create(false, false);
    IniDoc *doc = inidoc_parse("[Gamepad]\nButtonA=:select\n", 26);
    IniDocItem items[2];
    CHECK(bindings_load(b, BINDINGS_GAMEPAD, items, inidoc_list(doc, "Gamepad", NULL, items, 2)));
    CHECK(bindings_refuse_change(b, BINDINGS_GAMEPAD, 0, 0, NULL, true) == NULL);
    CHECK(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_UP, ":quit", false) != NULL);   // The keyboard's stays
    inidoc_free(doc);
    bindings_free(b);
}

// A function to test the keyboard's floor as handle_keypress() dispatches: Down as Up; a Menu key
// bound to :settings; a change replacing the binding it changes; a removed hotkey taking no key and
// running nothing; a hotkey on a key the dispatcher takes first, or after the first on its key, never
// running; Windows' :exit hotkey being no hotkey at all; and a command with no key to begin with
static void test_floor_keyboard(void)
{
    Bindings *b = loaded(false, "", "");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_DOWN, ":quit", false),
              "That would leave no key for Down");
    bindings_set(b, BINDINGS_KEYBOARD, -1, BIND_KEY_APPLICATION, ":quit");
    CHECK(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_MENU, ":settings", false) == NULL);
    bindings_free(b);

    // F1 no longer goes up once it is changed to Up's own key for :quit
    b = loaded(false, "Hotkey1=#4000003A;:up", "");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_KEYBOARD, 0, BIND_KEY_UP, ":quit", false),
              "That would leave no key for Up");
    bindings_free(b);

    // With Up's :quit hotkey removed, Up's own key is back
    b = loaded(false, "Hotkey1=#40000052;:quit\nHotkey2=#4000003A;:up", "");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_KEYBOARD, 1, 0, NULL, true), "That would leave no key for Up");
    bindings_remove(b, BINDINGS_KEYBOARD, 0);
    CHECK(bindings_refuse_change(b, BINDINGS_KEYBOARD, 1, 0, NULL, true) == NULL);
    bindings_free(b);

    // A removed hotkey runs nothing, and nor does one on Left
    b = loaded(false, "Hotkey1=#4000003A;:up\nHotkey2=#40000050;:up", "");
    bindings_remove(b, BINDINGS_KEYBOARD, 0);
    CHECK_STR(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_UP, ":quit", false),
              "That would leave no key for Up");
    bindings_free(b);

    // Only the first hotkey on a key runs; one before it that is removed no longer stands in the way
    b = loaded(false, "Hotkey1=#4000003A;:quit\nHotkey2=#4000003A;:up", "");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_UP, ":quit", false),
              "That would leave no key for Up");
    bindings_remove(b, BINDINGS_KEYBOARD, 0);
    CHECK(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_UP, ":quit", false) == NULL);
    bindings_free(b);

    // Windows keeps an :exit hotkey out of the hotkey list (add_hotkey()), so it takes no key and hides
    // no hotkey after it on its key. But the first :exit on a key Windows can register (F1-F11, F13-F24:
    // keycode_convert.h) is registered system-wide (set_exit_hotkey(), register_exit_hotkey()): that key
    // never reaches SDL, so no hotkey on it runs, before or after the :exit. Elsewhere :exit is a hotkey
    // like any other.
    static const char *const exit_on_up = "Hotkey1=#40000052;:exit\nHotkey2=#4000003A;:up";
    static const char *const exit_first = "Hotkey1=#4000003A;:exit\nHotkey2=#4000003A;:up";
    static const char *const exit_unregistered = "Hotkey1=#61;:exit\nHotkey2=#61;:up";
    b = loaded(true, exit_on_up, "");
    CHECK(bindings_refuse_change(b, BINDINGS_KEYBOARD, 1, 0, NULL, true) == NULL);
    bindings_free(b);
    b = loaded(false, exit_on_up, "");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_KEYBOARD, 1, 0, NULL, true), "That would leave no key for Up");
    bindings_free(b);
    b = loaded(true, exit_first, "");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_UP, ":quit", false),
              "That would leave no key for Up");
    bindings_remove(b, BINDINGS_KEYBOARD, 0);                      // A removed :exit registers nothing
    CHECK(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_UP, ":quit", false) == NULL);
    bindings_free(b);
    b = loaded(false, exit_first, "");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_UP, ":quit", false),
              "That would leave no key for Up");
    bindings_free(b);
    b = loaded(true, exit_unregistered, "");                       // 'a' cannot be registered
    CHECK(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_UP, ":quit", false) == NULL);
    bindings_free(b);
    b = loaded(false, exit_unregistered, "");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_UP, ":quit", false),
              "That would leave no key for Up");
    bindings_free(b);

    // The first :exit that can be registered is the one that is: an unregistrable one before it does not
    // stand in its place
    b = loaded(true, "Hotkey1=#61;:exit\nHotkey2=#4000003A;:exit\nHotkey3=#4000003A;:up", "");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_UP, ":quit", false),
              "That would leave no key for Up");
    bindings_free(b);

    // Of two :exit hotkeys Windows can register, the first is registered, and the second's key still runs
    b = loaded(true, "Hotkey1=#4000003A;:exit\nHotkey2=#4000003B;:exit\nHotkey3=#4000003B;:up\n"
                     "Hotkey4=#40000052;:home", "");
    CHECK(bindings_refuse_change(b, BINDINGS_KEYBOARD, 3, 0, NULL, true) == NULL);
    CHECK_STR(bindings_refuse_change(b, BINDINGS_KEYBOARD, 2, 0, NULL, true), "That would leave no key for Up");
    bindings_free(b);

    // The registered key swallows a hotkey listed before the :exit too; off Windows nothing is registered
    static const char *const up_then_exit = "Hotkey1=#4000003A;:up\nHotkey2=#4000003A;:exit";
    b = loaded(true, up_then_exit, "");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_UP, ":quit", false),
              "That would leave no key for Up");
    bindings_free(b);
    b = loaded(false, up_then_exit, "");
    CHECK(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_UP, ":quit", false) == NULL);
    bindings_free(b);

    // F12 cannot be registered: an :exit on it registers nothing, and a hotkey on F12 still runs
    b = loaded(true, "Hotkey1=#40000045;:exit\nHotkey2=#40000045;:up", "");
    CHECK(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_UP, ":quit", false) == NULL);
    bindings_free(b);

    // A new :exit is weighed as registering its key
    b = loaded(true, "Hotkey1=#40000052;:home\nHotkey2=#4000003B;:up", "");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, 0x4000003B, ":exit", false),
              "That would leave no key for Up");
    bindings_free(b);

    // The keyboard's Left, Right, Return and Backspace are always there: a lone hotkey for their
    // commands can go
    static const char *const fixed[] = { ":left", ":right", ":select", ":back" };
    for (int i = 0; i < 4; i++) {
        char line[64];
        snprintf(line, sizeof(line), "Hotkey1=#4000003A;%s", fixed[i]);
        b = loaded(false, line, "");
        CHECK(bindings_refuse_change(b, BINDINGS_KEYBOARD, 0, 0, NULL, true) == NULL);
        bindings_free(b);
    }

    // A command is read by its first word, as execute_command() reads it: ":up now" goes up
    b = loaded(false, "Hotkey1=#4000003A;:up now", "");
    CHECK(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, BIND_KEY_UP, ":quit", false) == NULL);
    bindings_free(b);

    // Both Menu keys taken in the file, and no hotkey for :settings: nothing for the floor to keep
    b = loaded(false, "Hotkey1=#40000065;:quit\nHotkey2=#40000076;:home", "");
    CHECK(bindings_refuse_change(b, BINDINGS_KEYBOARD, -1, 0x4000003A, ":quit", false) == NULL);
    bindings_free(b);

    // A binding being removed hides nothing, even on the key SDL cannot name (#0, a CEC remote's OK on
    // Linux), where a hotkey after it still runs
    b = loaded(false, "Hotkey1=#4000003A;:home\nHotkey2=#40000052;:quit\nHotkey3=#0;:up", "");
    CHECK(bindings_refuse_change(b, BINDINGS_KEYBOARD, 0, 0, NULL, true) == NULL);
    CHECK_STR(bindings_refuse_change(b, BINDINGS_KEYBOARD, 2, 0, NULL, true), "That would leave no key for Up");
    bindings_free(b);
}

// A function to test the gamepad's floor with the defaults util.c adds: Settings' ButtonStart while
// nothing has it (a removed control leaves it free); Up's two defaults, either one enough; Down's;
// a new control taking the last free default; and Left and Right, which have none
static void test_floor_gamepad(void)
{
    Bindings *b = loaded(false, "", "ButtonX=:settings");
    CHECK(bindings_refuse_change(b, BINDINGS_GAMEPAD, 0, 0, NULL, true) == NULL);
    bindings_free(b);
    b = loaded(false, "", "ButtonStart=:quit\nButtonX=:settings");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_GAMEPAD, 1, 0, NULL, true),
              "That would leave no button for Settings");
    bindings_remove(b, BINDINGS_GAMEPAD, 0);
    CHECK(bindings_refuse_change(b, BINDINGS_GAMEPAD, 1, 0, NULL, true) == NULL);
    bindings_free(b);

    b = loaded(false, "", "ButtonDPadUp=:quit\nButtonX=:up");
    CHECK(bindings_refuse_change(b, BINDINGS_GAMEPAD, 1, 0, NULL, true) == NULL);              // LStickY- is free
    bindings_free(b);
    b = loaded(false, "", "ButtonDPadUp=:quit\nLStickY-=:home\nButtonX=:up");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_GAMEPAD, 2, 0, NULL, true), "That would leave no button for Up");
    bindings_free(b);
    b = loaded(false, "", "ButtonDPadDown=:quit\nLStickY+=:home\nButtonX=:down");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_GAMEPAD, 2, 0, NULL, true), "That would leave no button for Down");
    bindings_free(b);

    b = loaded(false, "", "LStickY-=:home");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_GAMEPAD, -1, bindings_label_index("ButtonDPadUp"), ":quit", false),
              "That would leave no button for Up");
    bindings_free(b);

    // A control is read by its first word, as execute_command() reads it; a longer word is another command
    b = loaded(false, "", "ButtonA=:select foo");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_GAMEPAD, 0, 0, NULL, true), "That would leave no button for OK");
    bindings_free(b);
    b = loaded(false, "", "ButtonA=:selectx");
    CHECK(bindings_refuse_change(b, BINDINGS_GAMEPAD, 0, 0, NULL, true) == NULL);
    bindings_free(b);

    // A control removed before still counts for nothing
    b = loaded(false, "", "ButtonA=:select\nButtonX=:select");
    bindings_remove(b, BINDINGS_GAMEPAD, 0);
    CHECK_STR(bindings_refuse_change(b, BINDINGS_GAMEPAD, 1, 0, NULL, true), "That would leave no button for OK");
    bindings_free(b);

    b = loaded(false, "", "ButtonDPadLeft=:left\nButtonDPadRight=:right");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_GAMEPAD, 0, 0, NULL, true), "That would leave no button for Left");
    CHECK_STR(bindings_refuse_change(b, BINDINGS_GAMEPAD, 1, 0, NULL, true), "That would leave no button for Right");
    bindings_free(b);
}

// A function to test which keyboard bindings take a key's navigation away, needing a confirmation
static void test_takes_navigation(void)
{
    CHECK(bindings_takes_navigation(BINDINGS_KEYBOARD, BIND_KEY_UP, ":quit"));
    CHECK(!bindings_takes_navigation(BINDINGS_KEYBOARD, BIND_KEY_UP, ":up"));
    CHECK(bindings_takes_navigation(BINDINGS_KEYBOARD, BIND_KEY_DOWN, ":select"));
    CHECK(bindings_takes_navigation(BINDINGS_KEYBOARD, BIND_KEY_MENU, ":home"));
    CHECK(!bindings_takes_navigation(BINDINGS_KEYBOARD, BIND_KEY_APPLICATION, ":settings"));
    CHECK(!bindings_takes_navigation(BINDINGS_KEYBOARD, 0x4000003E, ":quit"));
    CHECK(!bindings_takes_navigation(BINDINGS_GAMEPAD, bindings_label_index("ButtonDPadUp"), ":quit"));
    CHECK(!bindings_takes_navigation(BINDINGS_KEYBOARD, BIND_KEY_DOWN, ":down"));
    CHECK(bindings_takes_navigation(BINDINGS_KEYBOARD, BIND_KEY_APPLICATION, ":quit"));
    CHECK(!bindings_takes_navigation(BINDINGS_KEYBOARD, BIND_KEY_MENU, ":settings"));
    CHECK(!bindings_takes_navigation(BINDINGS_GAMEPAD, BIND_KEY_UP, ":quit"));          // Never on the gamepad
    CHECK(!bindings_takes_navigation(BINDINGS_KEYBOARD, BIND_KEY_UP, ":up 2"));         // Its first word is :up
    CHECK(bindings_takes_navigation(BINDINGS_KEYBOARD, BIND_KEY_UP, ":upper"));
}

// A function to test the edits a save gets: a change, a removal and an addition; a new binding that
// was removed again writes nothing; Discard puts every list back
static void test_edits(void)
{
    Bindings *b = loaded(false, "Hotkey1=#4000003A;:quit\nHotkey2=#4000003B;:home", "ButtonA=:select");
    bindings_set(b, BINDINGS_KEYBOARD, 0, 0x4000003A, ":settings");
    bindings_remove(b, BINDINGS_KEYBOARD, 1);
    int added = bindings_set(b, BINDINGS_KEYBOARD, -1, 0x4000003C, ":back");
    CHECK_INT(added, 2);
    int gone = bindings_set(b, BINDINGS_KEYBOARD, -1, 0x4000003D, ":up");
    bindings_remove(b, BINDINGS_KEYBOARD, gone);
    CHECK(bindings_changed(b));
    ConfigListEdit edits[8];
    char values[8][BINDINGS_VALUE_MAX];
    int count = bindings_edits(b, BINDINGS_KEYBOARD, edits, values, 8);
    CHECK_INT(count, 3);
    CHECK_INT(edits[0].op, CONFIG_LIST_SET);
    CHECK_STR(edits[0].section, "Hotkeys");
    CHECK_STR(edits[0].original, "Hotkey1=#4000003A;:quit");
    CHECK_STR(edits[0].key, "Hotkey");
    CHECK(edits[0].numbered);
    CHECK_STR(edits[0].value, "#4000003A;:settings");
    CHECK_INT(edits[1].op, CONFIG_LIST_REMOVE);
    CHECK_STR(edits[1].original, "Hotkey2=#4000003B;:home");
    CHECK_INT(edits[2].op, CONFIG_LIST_ADD);
    CHECK_STR(edits[2].value, "#4000003C;:back");
    CHECK_INT(bindings_edits(b, BINDINGS_GAMEPAD, edits, values, 8), 0);

    // A gamepad change keeps its label as the key, with no number
    bindings_set(b, BINDINGS_GAMEPAD, 0, bindings_label_index("ButtonX"), ":select");
    count = bindings_edits(b, BINDINGS_GAMEPAD, edits, values, 8);
    CHECK_INT(count, 1);
    CHECK_STR(edits[0].section, "Gamepad");
    CHECK_STR(edits[0].key, "ButtonX");
    CHECK(!edits[0].numbered);
    CHECK_STR(edits[0].value, ":select");

    bindings_discard(b);
    CHECK(!bindings_changed(b));
    CHECK_INT(bindings_count(b, BINDINGS_KEYBOARD), 2);
    CHECK_STR(bindings_at(b, BINDINGS_KEYBOARD, 0)->command, ":quit");
    CHECK(!bindings_at(b, BINDINGS_KEYBOARD, 1)->removed);
    CHECK_INT(bindings_edits(b, BINDINGS_KEYBOARD, edits, values, 8), 0);
    bindings_free(b);
}

// A function to test the rest of the edits: set back as loaded is no change; a key alone or a command
// alone is; `max` is kept to; a removed line set again is a change, not a removal; a change on the
// gamepad alone counts; the gamepad's additions and removals; Discard on both lists; loading again
static void test_edits_more(void)
{
    Bindings *b = loaded(false, "Hotkey1=#4000003A;:quit\nHotkey2=#4000003B;:home", "ButtonA=:select\nButtonB=:back");
    ConfigListEdit edits[8];
    char values[8][BINDINGS_VALUE_MAX];
    bindings_set(b, BINDINGS_KEYBOARD, 0, 0x4000003E, ":quit");
    bindings_set(b, BINDINGS_KEYBOARD, 0, 0x4000003A, ":quit");
    CHECK(!bindings_changed(b));
    CHECK_INT(bindings_edits(b, BINDINGS_KEYBOARD, edits, values, 8), 0);

    bindings_set(b, BINDINGS_KEYBOARD, 0, 0x4000003E, ":quit");
    bindings_set(b, BINDINGS_KEYBOARD, 1, 0x4000003B, ":back");
    CHECK_INT(bindings_edits(b, BINDINGS_KEYBOARD, edits, values, 8), 2);
    CHECK_STR(edits[0].value, "#4000003E;:quit");
    CHECK_STR(edits[1].value, "#4000003B;:back");
    CHECK_INT(bindings_edits(b, BINDINGS_KEYBOARD, edits, values, 1), 1);

    bindings_discard(b);
    bindings_remove(b, BINDINGS_KEYBOARD, 1);
    bindings_set(b, BINDINGS_KEYBOARD, 1, 0x61, ":home");
    CHECK_INT(bindings_edits(b, BINDINGS_KEYBOARD, edits, values, 8), 1);
    CHECK_INT(edits[0].op, CONFIG_LIST_SET);
    CHECK_STR(edits[0].value, "#61;:home");

    bindings_discard(b);
    bindings_remove(b, BINDINGS_GAMEPAD, 1);
    CHECK(bindings_changed(b));
    CHECK_INT(bindings_set(b, BINDINGS_GAMEPAD, -1, bindings_label_index("ButtonX"), ":home"), 2);
    CHECK_INT(bindings_edits(b, BINDINGS_GAMEPAD, edits, values, 8), 2);
    CHECK_INT(edits[0].op, CONFIG_LIST_REMOVE);
    CHECK_STR(edits[0].original, "ButtonB=:back");
    CHECK_INT(edits[1].op, CONFIG_LIST_ADD);
    CHECK(edits[1].original == NULL);
    CHECK_STR(edits[1].section, "Gamepad");
    CHECK_STR(edits[1].key, "ButtonX");
    CHECK(!edits[1].numbered);
    CHECK_STR(edits[1].value, ":home");

    bindings_discard(b);
    CHECK_INT(bindings_count(b, BINDINGS_GAMEPAD), 2);
    CHECK(!bindings_at(b, BINDINGS_GAMEPAD, 1)->removed);
    CHECK(!bindings_changed(b));
    bindings_free(b);

    // Loading again replaces the list, and Discard goes back to the new one
    b = loaded(false, "Hotkey1=#4000003A;:quit", "");
    static const char *const again = "[Hotkeys]\nHotkey7=#4000003B;:home\nHotkey8=#4000003C;:back\n";
    IniDoc *doc = inidoc_parse(again, strlen(again));
    IniDocItem items[4];
    CHECK(bindings_load(b, BINDINGS_KEYBOARD, items, inidoc_list(doc, "Hotkeys", NULL, items, 4)));
    inidoc_free(doc);
    bindings_set(b, BINDINGS_KEYBOARD, 0, 0x4000003D, ":up");
    bindings_discard(b);
    CHECK_INT(bindings_count(b, BINDINGS_KEYBOARD), 2);
    CHECK_STR(bindings_at(b, BINDINGS_KEYBOARD, 0)->original, "Hotkey7=#4000003B;:home");
    CHECK_STR(bindings_at(b, BINDINGS_KEYBOARD, 0)->command, ":home");

    // A new binding removed again is no change at all
    int gone = bindings_set(b, BINDINGS_KEYBOARD, -1, 0x4000003E, ":up");
    bindings_remove(b, BINDINGS_KEYBOARD, gone);
    CHECK(!bindings_changed(b));
    bindings_discard(b);

    // Loaded again with fewer lines, a new binding in a place a loaded one had keeps nothing of it
    doc = inidoc_parse("[Hotkeys]\nHotkey9=#4000003F;:quit\n", 34);
    CHECK(bindings_load(b, BINDINGS_KEYBOARD, items, inidoc_list(doc, "Hotkeys", NULL, items, 4)));
    inidoc_free(doc);
    CHECK_INT(bindings_set(b, BINDINGS_KEYBOARD, -1, 0x4000003E, ":up"), 1);
    CHECK_STR(bindings_at(b, BINDINGS_KEYBOARD, 1)->key, "");
    CHECK_STR(bindings_at(b, BINDINGS_KEYBOARD, 1)->original, "");
    bindings_free(b);
}

// A function to test the capture: the starting key's repeats and release are ignored (Review Focus
// 5), the same key pressed again after its release counts, and 5 s with nothing gives up
static void test_capture(void)
{
    Capture c;
    capture_begin(&c, 1000, BIND_KEY_RETURN);
    CHECK_INT(c.state, CAPTURE_LISTENING);
    CHECK(!capture_press(&c, 1100, BIND_KEY_RETURN, true));      // Its auto-repeats
    CHECK(!capture_press(&c, 1200, BIND_KEY_RETURN, true));
    capture_release(&c, BIND_KEY_RETURN);                          // Its release
    CHECK_INT(c.state, CAPTURE_LISTENING);
    CHECK(capture_press(&c, 1300, BIND_KEY_RETURN, false));       // A new press of it counts
    CHECK_INT(c.code, BIND_KEY_RETURN);
    CHECK_INT(c.state, CAPTURE_CAPTURED);

    capture_begin(&c, 1000, BIND_KEY_RETURN);
    CHECK(capture_press(&c, 1500, 0x4000003E, false));             // Another key while it is held counts
    CHECK_INT(c.code, 0x4000003E);

    capture_begin(&c, 1000, BIND_KEY_RETURN);
    CHECK(!capture_expired(&c, 5999));
    CHECK(capture_expired(&c, 6000));
    CHECK_INT(c.state, CAPTURE_TIMED_OUT);
    CHECK(!capture_press(&c, 6100, 0x4000003E, false));           // Nothing after it gave up
    CHECK(!capture_expired(&c, 7000));                              // Timing out is said once
}

// A function to test the rest of the capture: once captured, later presses change nothing and it never
// times out; beginning again forgets the last code; a press 5 s on is not captured even before the
// timeout is told; another key's release leaves the starting key held; a repeat of the starting key
// after its release is still ignored; and the clock wrapping round
static void test_capture_more(void)
{
    Capture c;
    capture_begin(&c, 1000, BIND_KEY_RETURN);
    CHECK(capture_press(&c, 1100, 0x4000003E, false));
    CHECK(!capture_press(&c, 1200, 0x4000003F, false));
    CHECK_INT(c.code, 0x4000003E);
    CHECK(!capture_expired(&c, 9000));
    CHECK_INT(c.state, CAPTURE_CAPTURED);

    capture_begin(&c, 2000, BIND_KEY_RETURN);
    CHECK_INT(c.code, BIND_KEY_UNKNOWN);
    CHECK(!capture_press(&c, 7000, 0x4000003E, false));
    CHECK_INT(c.state, CAPTURE_LISTENING);
    CHECK(capture_expired(&c, 7000));

    capture_begin(&c, 1000, BIND_KEY_RETURN);
    capture_release(&c, 0x4000003E);
    CHECK(!capture_press(&c, 1100, BIND_KEY_RETURN, false));      // Still held
    capture_release(&c, BIND_KEY_RETURN);
    CHECK(!capture_press(&c, 1200, BIND_KEY_RETURN, true));
    CHECK(capture_press(&c, 1300, BIND_KEY_RETURN, false));

    // SDL's milliseconds wrap after 49 days: 5 s from just before the wrap ends just after it
    capture_begin(&c, 0xFFFFF000u, BIND_KEY_RETURN);
    CHECK(capture_press(&c, 0xFFFFF001u, 0x4000003E, false));
    capture_begin(&c, 0xFFFFF000u, BIND_KEY_RETURN);
    CHECK(!capture_expired(&c, 0xFFFFF001u));
    CHECK(!capture_expired(&c, 0x387u));
    CHECK(capture_press(&c, 0x387u, 0x4000003E, false));
    capture_begin(&c, 0xFFFFF000u, BIND_KEY_RETURN);
    CHECK(!capture_press(&c, 0x388u, 0x4000003E, false));
    CHECK(capture_expired(&c, 0x388u));
}

// A function to test the 10 s confirmation: the new key confirms it; any other key does not; 10 s
// with nothing reverts it, once
static void test_probation(void)
{
    Probation p;
    probation_begin(&p, 1000, BIND_KEY_UP);
    CHECK(!probation_press(&p, BIND_KEY_DOWN));
    CHECK(!probation_expired(&p, 10999));
    CHECK(probation_press(&p, BIND_KEY_UP));
    CHECK(!p.active);
    CHECK(!probation_expired(&p, 20000));                           // Confirmed: nothing to revert

    probation_begin(&p, 1000, BIND_KEY_UP);
    CHECK(probation_expired(&p, 11000));
    CHECK(!probation_expired(&p, 12000));
}

// A function to test the rest of the confirmation: once reverted the key confirms nothing, and the
// clock wrapping round
static void test_probation_more(void)
{
    Probation p;
    probation_begin(&p, 1000, BIND_KEY_UP);
    CHECK(probation_expired(&p, 11000));
    CHECK(!probation_press(&p, BIND_KEY_UP));

    probation_begin(&p, 0xFFFFF000u, BIND_KEY_UP);
    CHECK(!probation_expired(&p, 0xFFFFF001u));
    CHECK(!probation_expired(&p, 0x170Fu));
    CHECK(probation_expired(&p, 0x1710u));
}

int main(void)
{
    test_load();
    test_load_lines();
    test_lookups();
    test_refused_keys();
    test_refused_keys_more();
    test_floor();
    test_floor_keyboard();
    test_floor_gamepad();
    test_takes_navigation();
    test_edits();
    test_edits_more();
    test_capture();
    test_capture_more();
    test_probation();
    test_probation_more();
    return check_report();
}
