#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "settings.h"
#include "bindings.h"
#include "inidoc.h"
#include "alloc.h"
#include "fileio.h"
#include "config_save.h"
#include "menuedit.h"

#define ARROW " \xE2\x80\xBA "   // U+203A with a space either side, between pages in the page path
#define TIMES "\xC3\x97"         // U+00D7, the multiplication sign
#define ELLIPSIS "\xE2\x80\xA6"  // U+2026, the ellipsis

// A function to parse a value, failing the check when the parser refuses it
static SettingValue parsed(SettingId id, const char *text)
{
    SettingValue value;
    memset(&value, 0, sizeof(value));
    CHECK(setting_parse(setting_def(id), text, &value));
    return value;
}

// A function to write a value back out as config.ini would hold it
static const char *formatted(SettingId id, const SettingValue *value)
{
    static char text[SETTING_TEXT_MAX];
    setting_format(setting_def(id), value, text, sizeof(text));
    return text;
}

// A function to describe a value as the screen shows it
static const char *described(SettingId id, const SettingValue *value, const SettingValue *inherited)
{
    static char text[256];
    setting_describe(setting_def(id), value, inherited, text, sizeof(text));
    return text;
}

// A function to step a value several times one way
static SettingValue stepped(SettingId id, SettingValue value, const SettingValue *entry, int direction, int times)
{
    for (int i = 0; i < times; i++)
        value = setting_step(setting_def(id), &value, entry, direction);
    return value;
}

// A function to test that every type reads what the parser reads and writes it back unchanged
static void test_round_trips(void)
{
    static const struct { SettingId id; const char *text; } cases[] = {
        { SET_ID_LAYOUT_ROWS, "3" },
        { SET_ID_LAYOUT_COLUMNS, "12" },
        { SET_ID_LAYOUT_ROWS, "999999" },
        { SET_ID_LAYOUT_ICON_SIZE, "256" },
        { SET_ID_MENU_ICON_SIZE, "1024" },
        { SET_ID_BACKGROUND_MODE, "Slideshow" },
        { SET_ID_BACKGROUND_COLOR, "#1A2B3C" },
        { SET_ID_BACKGROUND_IMAGE, "C:\\My Pictures\\sunset.jpg" },
        { SET_ID_SLIDESHOW_DIRECTORY, "/home/me/Pictures" },
        { SET_ID_SLIDESHOW_DURATION, "30" },
        { SET_ID_SLIDESHOW_DURATION, "3600" },
        { SET_ID_SLIDESHOW_FADE, "0" },
        { SET_ID_SLIDESHOW_FADE, "0.5" },
        { SET_ID_SLIDESHOW_FADE, "1" },
        { SET_ID_SLIDESHOW_FADE, "1.5" },
        { SET_ID_SLIDESHOW_FADE, "2.5" },
        { SET_ID_SLIDESHOW_FADE, "3" },
        { SET_ID_SLIDESHOW_FADE, "1.25" },
        { SET_ID_SLIDESHOW_FADE, "0.123" },
        { SET_ID_TITLE_SIZE, "14%" },
        { SET_ID_TITLE_SIZE, "36" }
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        SettingValue value = parsed(cases[i].id, cases[i].text);
        CHECK_STR(formatted(cases[i].id, &value), cases[i].text);
    }

    // What the parser reads from files written other ways
    SettingValue value = parsed(SET_ID_BACKGROUND_COLOR, "#1a2b3c");
    CHECK_STR(formatted(SET_ID_BACKGROUND_COLOR, &value), "#1A2B3C");
    value = parsed(SET_ID_BACKGROUND_IMAGE, "\"C:\\My Pictures\\a.png\"");     // Quotes dropped, as clean_path does
    CHECK_STR(value.text, "C:\\My Pictures\\a.png");
    value = parsed(SET_ID_SLIDESHOW_DURATION, "30s");                          // atoi, as before
    CHECK_INT(value.number, 30);
    value = parsed(SET_ID_SLIDESHOW_FADE, "0.7");
    CHECK_INT(value.number, 700);
    value = parsed(SET_ID_SLIDESHOW_FADE, "1.2346");
    CHECK_INT(value.number, 1235);                                             // Rounded to the ms
    value = parsed(SET_ID_SLIDESHOW_FADE, "2.9995");
    CHECK_INT(value.number, 3000);                                             // Rounds up to the maximum

    // Following the default writes nothing: the key is removed
    SettingValue inherit;
    memset(&inherit, 0, sizeof(inherit));
    inherit.inherit = true;
    CHECK_STR(formatted(SET_ID_MENU_ROWS, &inherit), "");
}

// A function to test the values the parser refuses
static void test_rejects(void)
{
    SettingValue value;
    CHECK(!setting_parse(setting_def(SET_ID_LAYOUT_ROWS), "0", &value));
    CHECK(!setting_parse(setting_def(SET_ID_LAYOUT_ROWS), "3x", &value));
    CHECK(!setting_parse(setting_def(SET_ID_LAYOUT_ICON_SIZE), "31", &value));
    CHECK(!setting_parse(setting_def(SET_ID_LAYOUT_ICON_SIZE), "200px", &value));
    CHECK(!setting_parse(setting_def(SET_ID_BACKGROUND_MODE), "color", &value));
    CHECK(!setting_parse(setting_def(SET_ID_BACKGROUND_COLOR), "#12345G", &value));
    CHECK(!setting_parse(setting_def(SET_ID_BACKGROUND_COLOR), "123456", &value));
    CHECK(!setting_parse(setting_def(SET_ID_BACKGROUND_COLOR), "#1234567", &value));
    CHECK(!setting_parse(setting_def(SET_ID_BACKGROUND_IMAGE), "", &value));
    CHECK(!setting_parse(setting_def(SET_ID_SLIDESHOW_DURATION), "4", &value));
    CHECK(!setting_parse(setting_def(SET_ID_SLIDESHOW_DURATION), "3601", &value));
    CHECK(!setting_parse(setting_def(SET_ID_SLIDESHOW_FADE), "-1", &value));
    CHECK(!setting_parse(setting_def(SET_ID_SLIDESHOW_FADE), "3.5", &value));
    CHECK(!setting_parse(setting_def(SET_ID_SLIDESHOW_FADE), "3.0005", &value));   // Would round past the maximum
    CHECK(!setting_parse(setting_def(SET_ID_SLIDESHOW_FADE), "-0.0004", &value));  // Negative, though it rounds to 0
    CHECK(!setting_parse(setting_def(SET_ID_SLIDESHOW_FADE), "nan", &value));
    CHECK(!setting_parse(setting_def(SET_ID_SLIDESHOW_FADE), "inf", &value));
    CHECK(!setting_parse(setting_def(SET_ID_SLIDESHOW_FADE), "-inf", &value));
    CHECK(!setting_parse(setting_def(SET_ID_TITLE_SIZE), "0%", &value));
    CHECK(!setting_parse(setting_def(SET_ID_TITLE_SIZE), "abc", &value));
}

// A function to test how Left and Right step each type
static void test_steps(void)
{
    // Rows stop at their ends, and a value from the file past the last step stays reachable
    SettingValue one = parsed(SET_ID_LAYOUT_ROWS, "1");
    CHECK_INT(stepped(SET_ID_LAYOUT_ROWS, one, &one, -1, 1).number, 1);
    CHECK_INT(stepped(SET_ID_LAYOUT_ROWS, one, &one, 1, 1).number, 2);
    CHECK_INT(stepped(SET_ID_LAYOUT_ROWS, one, &one, 1, 20).number, 10);
    SettingValue big = parsed(SET_ID_LAYOUT_ROWS, "25");
    SettingValue ten = stepped(SET_ID_LAYOUT_ROWS, big, &big, -1, 1);
    CHECK_INT(ten.number, 10);
    CHECK_INT(stepped(SET_ID_LAYOUT_ROWS, ten, &big, 1, 1).number, 25);

    // A menu's lowest step follows All menus
    SettingValue three = parsed(SET_ID_MENU_ROWS, "3");
    SettingValue value = stepped(SET_ID_MENU_ROWS, three, &three, -1, 3);
    CHECK(value.inherit);
    value = stepped(SET_ID_MENU_ROWS, value, &three, 1, 1);
    CHECK(!value.inherit);
    CHECK_INT(value.number, 1);

    // IconSize: Fill, then the steps, with a value from the file in its sorted place
    SettingValue fill;
    memset(&fill, 0, sizeof(fill));
    fill.inherit = true;
    CHECK_INT(stepped(SET_ID_LAYOUT_ICON_SIZE, fill, &fill, 1, 1).number, 64);
    SettingValue odd = parsed(SET_ID_LAYOUT_ICON_SIZE, "200");
    SettingValue below = parsed(SET_ID_LAYOUT_ICON_SIZE, "192");
    CHECK_INT(stepped(SET_ID_LAYOUT_ICON_SIZE, below, &odd, 1, 1).number, 200);
    CHECK_INT(stepped(SET_ID_LAYOUT_ICON_SIZE, odd, &odd, 1, 1).number, 256);

    // Colors: a custom color from the file first, then the presets in order
    SettingValue custom = parsed(SET_ID_BACKGROUND_COLOR, "#123456");
    value = stepped(SET_ID_BACKGROUND_COLOR, custom, &custom, 1, 1);
    CHECK_STR(formatted(SET_ID_BACKGROUND_COLOR, &value), "#000000");
    value = stepped(SET_ID_BACKGROUND_COLOR, value, &custom, 1, 1);
    CHECK_STR(formatted(SET_ID_BACKGROUND_COLOR, &value), "#1E1E1E");
    value = stepped(SET_ID_BACKGROUND_COLOR, value, &custom, -1, 2);
    CHECK_STR(formatted(SET_ID_BACKGROUND_COLOR, &value), "#123456");
    SettingValue black = parsed(SET_ID_BACKGROUND_COLOR, "#000000");
    value = stepped(SET_ID_BACKGROUND_COLOR, black, &black, -1, 1);
    CHECK_STR(formatted(SET_ID_BACKGROUND_COLOR, &value), "#000000");
    value = stepped(SET_ID_BACKGROUND_COLOR, black, &black, 1, 20);
    CHECK_STR(formatted(SET_ID_BACKGROUND_COLOR, &value), "#4A1520");

    // A swatch that is not a preset (the titles' default white) steps as a custom color does: first,
    // then the presets in order, and back to it
    SettingValue white = parsed(SET_ID_TITLE_COLOR, "#FFFFFF");
    value = stepped(SET_ID_TITLE_COLOR, white, &white, -1, 1);
    CHECK_STR(formatted(SET_ID_TITLE_COLOR, &value), "#FFFFFF");
    value = stepped(SET_ID_TITLE_COLOR, white, &white, 1, 1);
    CHECK_STR(formatted(SET_ID_TITLE_COLOR, &value), "#000000");
    value = stepped(SET_ID_TITLE_COLOR, value, &white, 1, 1);
    CHECK_STR(formatted(SET_ID_TITLE_COLOR, &value), "#1E1E1E");
    value = stepped(SET_ID_TITLE_COLOR, value, &white, 1, 20);
    CHECK_STR(formatted(SET_ID_TITLE_COLOR, &value), "#4A1520");
    value = stepped(SET_ID_TITLE_COLOR, value, &white, -1, 20);
    CHECK_STR(formatted(SET_ID_TITLE_COLOR, &value), "#FFFFFF");

    // Title size: a fixed size from the file, then Small, Medium and Large
    SettingValue fixed = parsed(SET_ID_TITLE_SIZE, "36");
    value = stepped(SET_ID_TITLE_SIZE, fixed, &fixed, 1, 1);
    CHECK_STR(formatted(SET_ID_TITLE_SIZE, &value), "11%");
    value = stepped(SET_ID_TITLE_SIZE, value, &fixed, 1, 5);
    CHECK_STR(formatted(SET_ID_TITLE_SIZE, &value), "17%");
    value = stepped(SET_ID_TITLE_SIZE, value, &fixed, -1, 5);
    CHECK_STR(formatted(SET_ID_TITLE_SIZE, &value), "36");

    // Durations and fades follow their step lists
    SettingValue thirty = parsed(SET_ID_SLIDESHOW_DURATION, "30");
    CHECK_INT(stepped(SET_ID_SLIDESHOW_DURATION, thirty, &thirty, 1, 1).number, 60);
    SettingValue fade = parsed(SET_ID_SLIDESHOW_FADE, "1.5");
    CHECK_INT(stepped(SET_ID_SLIDESHOW_FADE, fade, &fade, 1, 1).number, 2000);

    // The mode stops at both ends; a path never steps
    SettingValue mode = parsed(SET_ID_BACKGROUND_MODE, "Color");
    CHECK_INT(stepped(SET_ID_BACKGROUND_MODE, mode, &mode, -1, 1).number, 0);
    CHECK_INT(stepped(SET_ID_BACKGROUND_MODE, mode, &mode, 1, 9).number, 3);
    SettingValue path = parsed(SET_ID_BACKGROUND_IMAGE, "/a.png");
    SettingValue same = stepped(SET_ID_BACKGROUND_IMAGE, path, &path, 1, 1);
    CHECK_STR(same.text, "/a.png");
}

// A function to test how values are described on screen
static void test_descriptions(void)
{
    SettingValue inherit;
    memset(&inherit, 0, sizeof(inherit));
    inherit.inherit = true;
    SettingValue four = parsed(SET_ID_LAYOUT_ROWS, "4");
    CHECK_STR(described(SET_ID_MENU_ROWS, &inherit, &four), "All menus (4)");
    SettingValue value = parsed(SET_ID_MENU_ROWS, "3");
    CHECK_STR(described(SET_ID_MENU_ROWS, &value, &four), "3");
    CHECK_STR(described(SET_ID_LAYOUT_ICON_SIZE, &inherit, NULL), "Fill");
    CHECK_STR(described(SET_ID_MENU_ICON_SIZE, &inherit, &inherit), "All menus (Fill)");
    SettingValue cap = parsed(SET_ID_LAYOUT_ICON_SIZE, "256");
    CHECK_STR(described(SET_ID_MENU_ICON_SIZE, &inherit, &cap), "All menus (256 px)");
    CHECK_STR(described(SET_ID_LAYOUT_ICON_SIZE, &cap, NULL), "256 px");
    value = parsed(SET_ID_BACKGROUND_MODE, "Color");
    CHECK_STR(described(SET_ID_BACKGROUND_MODE, &value, NULL), "Color");
    value = parsed(SET_ID_BACKGROUND_COLOR, "#1E1E1E");
    CHECK_STR(described(SET_ID_BACKGROUND_COLOR, &value, NULL), "Charcoal");
    value = parsed(SET_ID_BACKGROUND_COLOR, "#123456");
    CHECK_STR(described(SET_ID_BACKGROUND_COLOR, &value, NULL), "Custom #123456");
    // A color row names any of the color picker's swatches, as the picker does, not only the presets
    value = parsed(SET_ID_TITLE_COLOR, "#FFFFFF");
    CHECK_STR(described(SET_ID_TITLE_COLOR, &value, NULL), "White");
    value = parsed(SET_ID_CLOCK_COLOR, "#D04890");
    CHECK_STR(described(SET_ID_CLOCK_COLOR, &value, NULL), "Pink");
    value = parsed(SET_ID_TITLE_COLOR, "#4A1520");
    CHECK_STR(described(SET_ID_TITLE_COLOR, &value, NULL), "Burgundy");
    value = parsed(SET_ID_TITLE_COLOR, "#FFD700");
    CHECK_STR(described(SET_ID_TITLE_COLOR, &value, NULL), "Custom #FFD700");
    value = parsed(SET_ID_BACKGROUND_IMAGE, "C:\\Pics\\sunset.jpg");
    CHECK_STR(described(SET_ID_BACKGROUND_IMAGE, &value, NULL), "sunset.jpg");
    value = parsed(SET_ID_SLIDESHOW_DIRECTORY, "/home/me/Pictures/");
    CHECK_STR(described(SET_ID_SLIDESHOW_DIRECTORY, &value, NULL), "Pictures");
    memset(&value, 0, sizeof(value));
    CHECK_STR(described(SET_ID_BACKGROUND_IMAGE, &value, NULL), "Choose" ELLIPSIS);
    value = parsed(SET_ID_SLIDESHOW_DURATION, "30");
    CHECK_STR(described(SET_ID_SLIDESHOW_DURATION, &value, NULL), "30 s");
    value = parsed(SET_ID_SLIDESHOW_DURATION, "120");
    CHECK_STR(described(SET_ID_SLIDESHOW_DURATION, &value, NULL), "2 min");
    value = parsed(SET_ID_SLIDESHOW_DURATION, "90");
    CHECK_STR(described(SET_ID_SLIDESHOW_DURATION, &value, NULL), "90 s");
    value = parsed(SET_ID_SLIDESHOW_FADE, "1.5");
    CHECK_STR(described(SET_ID_SLIDESHOW_FADE, &value, NULL), "1.5 s");
    value = parsed(SET_ID_TITLE_SIZE, "11%");
    CHECK_STR(described(SET_ID_TITLE_SIZE, &value, NULL), "Small");
    value = parsed(SET_ID_TITLE_SIZE, "14%");
    CHECK_STR(described(SET_ID_TITLE_SIZE, &value, NULL), "Medium");
    value = parsed(SET_ID_TITLE_SIZE, "17%");
    CHECK_STR(described(SET_ID_TITLE_SIZE, &value, NULL), "Large");
    value = parsed(SET_ID_TITLE_SIZE, "12%");
    CHECK_STR(described(SET_ID_TITLE_SIZE, &value, NULL), "12%");
    value = parsed(SET_ID_TITLE_SIZE, "36");
    CHECK_STR(described(SET_ID_TITLE_SIZE, &value, NULL), "Fixed 36");
}

// A function to set a global setting's entry value from the file's text
static void entry(SettingsState *state, SettingId id, const char *text)
{
    SettingValue value = parsed(id, text);
    settings_set_entry(state, id, -1, &value);
}

// A function to get the value an event's slot holds now. An event with no slot (a wrong kind, or a
// NULL slot) fails a check and gives an empty value, so the checks after it report, never crash.
static const SettingValue *event_value(const SettingsEvent *event)
{
    static SettingValue none;
    if (event->slot != NULL)
        return &event->slot->value;
    check_count++;
    check_failures++;
    fprintf(stderr, "event_value: the event (kind %d) has no slot\n", (int) event->kind);
    return &none;
}

// A function to open a model over two menus, with the values a typical config gives
static SettingsState *open_model(void)
{
    static const char *const names[] = { "Main", "Games" };
    SettingsState *state = settings_create(names, 2);
    SettingValue value;
    value = parsed(SET_ID_BACKGROUND_MODE, "Color");
    settings_set_entry(state, SET_ID_BACKGROUND_MODE, -1, &value);
    value = parsed(SET_ID_BACKGROUND_COLOR, "#000000");
    settings_set_entry(state, SET_ID_BACKGROUND_COLOR, -1, &value);
    memset(&value, 0, sizeof(value));
    settings_set_entry(state, SET_ID_BACKGROUND_IMAGE, -1, &value);
    settings_set_entry(state, SET_ID_SLIDESHOW_DIRECTORY, -1, &value);
    value = parsed(SET_ID_SLIDESHOW_DURATION, "30");
    settings_set_entry(state, SET_ID_SLIDESHOW_DURATION, -1, &value);
    value = parsed(SET_ID_SLIDESHOW_FADE, "1.5");
    settings_set_entry(state, SET_ID_SLIDESHOW_FADE, -1, &value);
    value = parsed(SET_ID_LAYOUT_ROWS, "1");
    settings_set_entry(state, SET_ID_LAYOUT_ROWS, -1, &value);
    value = parsed(SET_ID_LAYOUT_COLUMNS, "4");
    settings_set_entry(state, SET_ID_LAYOUT_COLUMNS, -1, &value);
    memset(&value, 0, sizeof(value));
    value.inherit = true;
    settings_set_entry(state, SET_ID_LAYOUT_ICON_SIZE, -1, &value);
    value = parsed(SET_ID_TITLE_SIZE, "14%");
    settings_set_entry(state, SET_ID_TITLE_SIZE, -1, &value);
    value = parsed(SET_ID_MENU_ROWS, "3");
    settings_set_entry(state, SET_ID_MENU_ROWS, 1, &value);
    value = parsed(SET_ID_MENU_COLUMNS, "6");
    settings_set_entry(state, SET_ID_MENU_COLUMNS, 1, &value);
    entry(state, SET_ID_DEFAULT_MENU, "Main");
    entry(state, SET_ID_VSYNC, "true");
    entry(state, SET_ID_TITLES_ENABLED, "true");
    entry(state, SET_ID_TITLE_SHADOWS, "false");
    entry(state, SET_ID_HIGHLIGHT_ENABLED, "true");
    entry(state, SET_ID_HIGHLIGHT_OUTLINE_SIZE, "0");
    entry(state, SET_ID_SCROLL_ENABLED, "true");
    entry(state, SET_ID_OVERLAY, "false");
    entry(state, SET_ID_CLOCK_ENABLED, "false");
    entry(state, SET_ID_CLOCK_SHOW_DATE, "false");
    entry(state, SET_ID_SCREENSAVER_ENABLED, "false");
    entry(state, SET_ID_SCREENSAVER_IDLE_TIME, "300");
    entry(state, SET_ID_GAMEPAD_ENABLED, "true");
    entry(state, SET_ID_GAMEPAD_DEVICE, "-1");
    return state;
}

// A function to test the top level, the Menus page, one menu's page and Discard
static void test_top_and_menus(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    char path[256];
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 11);
    static const char *const labels[] = { "General", "Background", "Menus", "Titles", "Highlight",
                                          "Scroll indicators", "Clock", "Screensaver", "Controls" };
    for (int i = 0; i < 9; i++) {
        CHECK_STR(rows[i].label, labels[i]);
        CHECK_INT(rows[i].kind, SETTINGS_ROW_LINK);
    }
    CHECK_STR(rows[0].value, "Main");            // General: the default menu
    CHECK_STR(rows[1].value, "Color");
    CHECK_STR(rows[2].value, "2 menus");
    CHECK_STR(rows[3].value, "Medium");
    CHECK_STR(rows[4].value, "On");
    CHECK_STR(rows[5].value, "On");
    CHECK_STR(rows[6].value, "Off");
    CHECK_STR(rows[7].value, "Off");
    CHECK_STR(rows[8].value, "Gamepad on");
    CHECK_INT(rows[9].kind, SETTINGS_ROW_DIVIDER);
    CHECK_STR(rows[10].label, "Discard changes");
    CHECK(!rows[10].enabled);
    CHECK(rows[10].why == NULL);                  // Grayed with no reason: the cursor skips it
    CHECK_INT(settings_cursor(state), 0);
    for (int i = 0; i < 8; i++)
        CHECK_INT(settings_command(state, SETTINGS_DOWN).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_command(state, SETTINGS_DOWN).kind, SETTINGS_EVENT_NONE);   // Discard is grayed
    CHECK_INT(settings_cursor(state), 8);
    for (int i = 0; i < 6; i++)
        settings_command(state, SETTINGS_UP);
    CHECK_INT(settings_cursor(state), 2);

    // Menus: All menus, a divider, then each menu with its grid as columns x rows
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MENUS);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Menus");
    count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 4);
    CHECK_STR(rows[0].label, "All menus");
    CHECK_STR(rows[0].value, "4 " TIMES " 1");
    CHECK_STR(rows[2].label, "Main");
    CHECK_STR(rows[2].value, "All menus");
    CHECK_STR(rows[3].label, "Games");
    CHECK_STR(rows[3].value, "6 " TIMES " 3");
    CHECK_INT(settings_preview_menu(state), -1);
    settings_command(state, SETTINGS_DOWN);                  // Over the divider, onto Main
    CHECK_INT(settings_cursor(state), 2);
    CHECK_INT(settings_preview_menu(state), 0);
    settings_command(state, SETTINGS_DOWN);
    CHECK_INT(settings_preview_menu(state), 1);

    // Games' page: its own rows and columns, and IconSize following All menus
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MENU);
    CHECK_INT(settings_preview_menu(state), 1);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Menus" ARROW "Games");
    count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 4);                                     // Three rows and the note
    CHECK_STR(rows[0].value, "3");
    CHECK_STR(rows[1].value, "6");
    CHECK_STR(rows[2].value, "All menus (Fill)");

    // Rows 3 -> 2 -> 1 -> All menus (1)
    SettingsEvent event = settings_command(state, SETTINGS_LEFT);
    CHECK_INT(event.kind, SETTINGS_EVENT_CHANGED);
    CHECK(event.slot == settings_slot(state, SET_ID_MENU_ROWS, 1));
    CHECK_INT(event.before.number, 3);
    settings_command(state, SETTINGS_LEFT);
    settings_command(state, SETTINGS_LEFT);
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[0].value, "All menus (1)");
    CHECK(settings_changed(settings_slot(state, SET_ID_MENU_ROWS, 1)));
    CHECK_INT(settings_command(state, SETTINGS_LEFT).kind, SETTINGS_EVENT_NONE);

    // Back at the top, Discard is offered; it puts every value back and grays out again
    settings_command(state, SETTINGS_BACK);
    settings_command(state, SETTINGS_BACK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TOP);
    CHECK_INT(settings_cursor(state), 2);                    // Where it was
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK(rows[10].enabled);
    for (int i = 0; i < 7; i++)
        settings_command(state, SETTINGS_DOWN);
    CHECK_INT(settings_cursor(state), 10);
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_DISCARD);
    CHECK_INT(settings_slot(state, SET_ID_MENU_ROWS, 1)->value.number, 3);
    CHECK(!settings_any_changed(state));
    CHECK_INT(settings_cursor(state), 8);                    // Off the grayed Discard, onto Controls

    CHECK_INT(settings_command(state, SETTINGS_BACK).kind, SETTINGS_EVENT_CLOSE);
    CHECK_INT(settings_command(state, SETTINGS_HOME).kind, SETTINGS_EVENT_CLOSE_HOME);
    CHECK_INT(settings_command(state, SETTINGS_CLOSE).kind, SETTINGS_EVENT_CLOSE);
    settings_free(state);
}

// A function to test the Background page: its rows per mode, and the incomplete-mode rule
static void test_background_page(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    settings_command(state, SETTINGS_DOWN);
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BACKGROUND);
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 5);
    CHECK_STR(rows[0].value, "Color");
    CHECK_STR(rows[1].label, "Color");
    CHECK_STR(rows[1].value, "Black");
    CHECK_INT(rows[1].kind, SETTINGS_ROW_PICK);                // OK opens the color picker...
    CHECK_STR(rows[2].label, "Overlay");
    CHECK_STR(rows[3].label, "Overlay color");
    CHECK(!rows[3].enabled);                                    // ...and the overlay's rows wait for it
    CHECK_STR(rows[3].why, "Turn Overlay on to change this");
    CHECK(!rows[4].enabled);

    // Image with none chosen: Back puts the mode back, and says why
    settings_command(state, SETTINGS_RIGHT);
    count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 5);
    CHECK_INT(rows[1].kind, SETTINGS_ROW_BROWSE);
    CHECK_STR(rows[1].value, "Choose" ELLIPSIS);
    SettingsEvent event = settings_command(state, SETTINGS_BACK);
    CHECK_INT(event.kind, SETTINGS_EVENT_CHANGED);
    CHECK(event.slot == settings_slot(state, SET_ID_BACKGROUND_MODE, -1));
    CHECK_INT(event_value(&event)->number, 0);
    CHECK(strstr(settings_notice(state), "No image was chosen") != NULL);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TOP);
    CHECK(!settings_any_changed(state));

    // Image chosen in the browser: Back keeps it
    settings_command(state, SETTINGS_OK);
    settings_command(state, SETTINGS_RIGHT);
    settings_command(state, SETTINGS_DOWN);
    event = settings_command(state, SETTINGS_OK);
    CHECK_INT(event.kind, SETTINGS_EVENT_BROWSE);
    CHECK(event.slot == settings_slot(state, SET_ID_BACKGROUND_IMAGE, -1));
    // Never NULL, as event.slot may be
    SettingSlot *image = settings_slot(state, SET_ID_BACKGROUND_IMAGE, -1);
    CHECK_INT(settings_choose(state, image, "/pics/a.png").kind, SETTINGS_EVENT_CHANGED);
    CHECK_INT(settings_choose(state, image, "/pics/a.png").kind, SETTINGS_EVENT_NONE);
    CHECK_INT(settings_command(state, SETTINGS_BACK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_slot(state, SET_ID_BACKGROUND_MODE, -1)->value.number, 1);

    // Slideshow: a folder, how long each image shows, and the fade
    settings_command(state, SETTINGS_OK);
    settings_command(state, SETTINGS_RIGHT);
    count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 7);
    CHECK_STR(rows[1].label, "Folder");
    CHECK_STR(rows[2].value, "30 s");
    CHECK_STR(rows[3].value, "1.5 s");

    // Transparent: a note in place of the mode's rows, then on Linux, whose Transparent mode is a
    // chroma key, the see-through color, then the overlay's three rows. Windows' Transparent mode is
    // per-pixel alpha, with no key to choose: the note stays, and no see-through color follows it.
    settings_command(state, SETTINGS_RIGHT);
    count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(rows[1].kind, SETTINGS_ROW_NOTE);
    CHECK(rows[1].note != NULL && strstr(rows[1].note, "compositor") != NULL);
#ifdef _WIN32
    CHECK_INT(count, 5);
    CHECK(rows[2].slot == settings_slot(state, SET_ID_OVERLAY, -1));
    for (int i = 0; i < count; i++)
        CHECK(rows[i].slot != settings_slot(state, SET_ID_CHROMA_KEY_COLOR, -1));
#else
    CHECK_INT(count, 6);
    CHECK(rows[2].slot == settings_slot(state, SET_ID_CHROMA_KEY_COLOR, -1));
    CHECK_INT(rows[2].kind, SETTINGS_ROW_PICK);
#endif

    // Closing from the page with Slideshow and no folder puts back the mode the page opened with
    settings_command(state, SETTINGS_LEFT);
    event = settings_command(state, SETTINGS_CLOSE);
    CHECK_INT(event.kind, SETTINGS_EVENT_CLOSE);
    CHECK(event.slot == settings_slot(state, SET_ID_BACKGROUND_MODE, -1));
    CHECK_INT(event_value(&event)->number, 1);
    settings_free(state);
}

// A function to test the page a failed save shows
static void test_save_failed_page(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    settings_show_save_failed(state, "Couldn't save to /x/config.ini: permission denied");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_SAVE_FAILED);
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 3);
    CHECK_INT(rows[0].kind, SETTINGS_ROW_NOTE);
    CHECK(strstr(rows[0].note, "permission denied") != NULL);
    CHECK_STR(rows[1].label, "Try again");
    CHECK_STR(rows[2].label, "Leave without saving");
    CHECK_INT(settings_cursor(state), 1);
    CHECK_INT(settings_command(state, SETTINGS_HOME).kind, SETTINGS_EVENT_NONE);
    CHECK_INT(settings_command(state, SETTINGS_CLOSE).kind, SETTINGS_EVENT_NONE);
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_RETRY);

    // A retry that fails again stays on the same page, with the new reason
    settings_show_save_failed(state, "Couldn't save to /x/config.ini: the disk is full");
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK(strstr(rows[0].note, "disk is full") != NULL);
    settings_command(state, SETTINGS_DOWN);
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_LEAVE);
    CHECK_INT(settings_command(state, SETTINGS_BACK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TOP);
    settings_free(state);
}

// A function to test the All menus page, the Titles page, and UP on the first row
static void test_all_menus_and_titles_pages(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    char path[256];
    CHECK_INT(settings_command(state, SETTINGS_UP).kind, SETTINGS_EVENT_NONE);   // Nothing above the first row
    CHECK_INT(settings_cursor(state), 0);

    // All menus: [Layout]'s five settings, with no note about following All menus
    settings_command(state, SETTINGS_DOWN);
    settings_command(state, SETTINGS_DOWN);
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MENUS);
    CHECK_INT(settings_cursor(state), 0);
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MENU);
    CHECK_INT(settings_preview_menu(state), -1);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Menus" ARROW "All menus");
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 5);
    CHECK(rows[0].slot == settings_slot(state, SET_ID_LAYOUT_ROWS, -1));
    CHECK_STR(rows[0].value, "1");
    CHECK(rows[1].slot == settings_slot(state, SET_ID_LAYOUT_COLUMNS, -1));
    CHECK_STR(rows[1].value, "4");
    CHECK(rows[2].slot == settings_slot(state, SET_ID_LAYOUT_ICON_SIZE, -1));
    CHECK_STR(rows[2].value, "Fill");
    CHECK(rows[3].slot == settings_slot(state, SET_ID_ICON_SPACING, -1));
    CHECK(rows[4].slot == settings_slot(state, SET_ID_VCENTER, -1));
    for (int i = 0; i < count; i++)
        CHECK(rows[i].kind != SETTINGS_ROW_NOTE);
    SettingsEvent event = settings_command(state, SETTINGS_RIGHT);
    CHECK_INT(event.kind, SETTINGS_EVENT_CHANGED);
    CHECK(event.slot == settings_slot(state, SET_ID_LAYOUT_ROWS, -1));
    CHECK_INT(event_value(&event)->number, 2);

    // Titles: nine rows, its size first
    settings_command(state, SETTINGS_BACK);
    settings_command(state, SETTINGS_BACK);
    settings_command(state, SETTINGS_DOWN);
    CHECK_INT(settings_cursor(state), 3);
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TITLES);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Titles");
    count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 9);
    CHECK_STR(rows[0].label, "Size");
    CHECK_STR(rows[0].value, "Medium");
    event = settings_command(state, SETTINGS_RIGHT);
    CHECK_INT(event.kind, SETTINGS_EVENT_CHANGED);
    CHECK(event.slot == settings_slot(state, SET_ID_TITLE_SIZE, -1));
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[0].value, "Large");
    CHECK_INT(settings_command(state, SETTINGS_UP).kind, SETTINGS_EVENT_NONE);
    settings_free(state);
}

// A function to test walking every slot by its place: the global settings, then each menu's
static void test_slots_by_place(void)
{
    SettingsState *state = open_model();
    CHECK_INT(settings_slot_count(state), SET_ID_GLOBAL_COUNT + 2 * SET_ID_PER_MENU_COUNT);
    SettingSlot *slot = settings_slot_at(state, 0);
    CHECK(slot != NULL && slot->def->id == SET_ID_BACKGROUND_MODE && slot->menu == -1);
    slot = settings_slot_at(state, SET_ID_GLOBAL_COUNT);
    CHECK(slot != NULL && slot->def->id == SET_ID_MENU_ROWS && slot->menu == 0);
    CHECK(slot == settings_slot(state, SET_ID_MENU_ROWS, 0));
    slot = settings_slot_at(state, settings_slot_count(state) - 1);
    CHECK(slot != NULL && slot->def->id == SET_ID_MENU_ICON_SIZE && slot->menu == 1);
    CHECK(settings_slot_at(state, settings_slot_count(state)) == NULL);
    CHECK(settings_slot_at(state, -1) == NULL);
    CHECK(settings_slot(state, SET_ID_MENU_ROWS, 2) == NULL);
    settings_free(state);
}

// A function to test that one menu is "1 menu"
static void test_one_menu(void)
{
    static const char *const names[] = { "Main" };
    SettingsState *state = settings_create(names, 1);
    SettingsRow rows[SETTINGS_MAX_ROWS];
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[2].value, "1 menu");
    settings_free(state);
}

// A function to test that the Background page opened in an incomplete mode (Mode=Image with no
// Image= line) has nothing to go back to: Back and Close leave the mode, with no event and no notice
static void test_background_entered_incomplete(void)
{
    SettingsState *state = open_model();
    SettingValue image = parsed(SET_ID_BACKGROUND_MODE, "Image");
    settings_set_entry(state, SET_ID_BACKGROUND_MODE, -1, &image);
    settings_command(state, SETTINGS_DOWN);
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BACKGROUND);
    SettingsEvent event = settings_command(state, SETTINGS_BACK);
    CHECK_INT(event.kind, SETTINGS_EVENT_MOVED);
    CHECK(event.slot == NULL);
    CHECK_STR(settings_notice(state), "");
    CHECK_INT(settings_slot(state, SET_ID_BACKGROUND_MODE, -1)->value.number, 1);

    settings_command(state, SETTINGS_OK);
    event = settings_command(state, SETTINGS_CLOSE);
    CHECK_INT(event.kind, SETTINGS_EVENT_CLOSE);
    CHECK(event.slot == NULL);
    CHECK_STR(settings_notice(state), "");
    CHECK(!settings_any_changed(state));
    settings_free(state);
}

// A function to test a config with more menus than a page has rows: the Menus page lists what
// fits and says how many more there are in its last row, rather than cutting them off unseen
static void test_more_menus_than_rows(void)
{
    enum { MENUS = 70 };
    static char names[MENUS][16];
    const char *pointers[MENUS];
    for (int i = 0; i < MENUS; i++) {
        snprintf(names[i], sizeof(names[i]), "Menu %d", i + 1);
        pointers[i] = names[i];
    }
    SettingsState *state = settings_create(pointers, MENUS);
    SettingsRow rows[SETTINGS_MAX_ROWS];
    settings_command(state, SETTINGS_DOWN);
    settings_command(state, SETTINGS_DOWN);
    settings_command(state, SETTINGS_OK);
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[count - 2].label, "Menu 61");
    CHECK_INT(rows[count - 1].kind, SETTINGS_ROW_NOTE);
    CHECK(rows[count - 1].note != NULL && strstr(rows[count - 1].note, "9 more menus") != NULL);
    for (int i = 0; i < 100; i++)
        settings_command(state, SETTINGS_DOWN);
    CHECK_INT(settings_cursor(state), count - 2);             // The cursor stops on the last menu listed
    CHECK_INT(settings_preview_menu(state), 60);
    settings_free(state);

    // Exactly as many as fit need no note
    state = settings_create(pointers, SETTINGS_MAX_ROWS - 2);
    settings_command(state, SETTINGS_DOWN);
    settings_command(state, SETTINGS_DOWN);
    settings_command(state, SETTINGS_OK);
    count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, SETTINGS_MAX_ROWS);
    CHECK_INT(rows[count - 1].kind, SETTINGS_ROW_LINK);
    CHECK_STR(rows[count - 1].label, "Menu 62");
    settings_free(state);
}

// A function to test the new types' round trips: what the parser reads is written back unchanged
static void test_new_round_trips(void)
{
    static const struct { SettingId id; const char *text; } cases[] = {
        { SET_ID_WRAP_ENTRIES, "true" },
        { SET_ID_VSYNC, "false" },
        { SET_ID_FPS_LIMIT, "10" },                  // The documented minimum, which the old parser refused
        { SET_ID_FPS_LIMIT, "144" },
        { SET_ID_APPLICATION_TIMEOUT, "15" },
        { SET_ID_ON_LAUNCH, "Quit" },
        { SET_ID_STARTUP_CMD, ":submenu Games" },
        { SET_ID_QUIT_CMD, "\"C:\\Program Files\\Kodi\\kodi.exe\" --standalone" },
        { SET_ID_DEFAULT_MENU, "Main" },
        { SET_ID_CHROMA_KEY_COLOR, "#010101" },
        { SET_ID_OVERLAY_OPACITY, "50%" },
        { SET_ID_OVERLAY_OPACITY, "12.5%" },
        { SET_ID_OVERLAY_OPACITY, "33.33%" },
        { SET_ID_ICON_SPACING, "5%" },
        { SET_ID_ICON_SPACING, "40" },               // px, as the old parser read it
        { SET_ID_ICON_SPACING, "2000000000" },       // f15-limits: huge, and still read
        { SET_ID_ICON_SPACING, "2147483647" },       // The largest int, still read
        { SET_ID_VCENTER, "50%" },
        { SET_ID_TITLE_FONT, "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf" },
        { SET_ID_TITLE_FONT_FACE, "2" },
        { SET_ID_TITLE_OVERSIZE, "Shrink" },
        { SET_ID_TITLE_OVERSIZE, "None" },
        { SET_ID_TITLE_PADDING, "8%" },
        { SET_ID_TITLE_PADDING, "20" },
        { SET_ID_HIGHLIGHT_OUTLINE_SIZE, "3" },
        { SET_ID_HIGHLIGHT_CORNER_RADIUS, "25" },
        { SET_ID_HIGHLIGHT_HPADDING, "300" },        // Past the last step; the clamp is Effective's
        { SET_ID_CLOCK_FONT_SIZE, "50" },
        { SET_ID_CLOCK_MARGIN, "5%" },
        { SET_ID_CLOCK_TIME_FORMAT, "12hr" },
        { SET_ID_CLOCK_DATE_FORMAT, "Little" },
        { SET_ID_SCREENSAVER_IDLE_TIME, "300" },
        { SET_ID_SCREENSAVER_INTENSITY, "70%" },
        { SET_ID_GAMEPAD_DEVICE, "-1" },
        { SET_ID_GAMEPAD_DEVICE, "2" },
        { SET_ID_GAMEPAD_MAPPINGS, "/home/me/gamecontrollerdb.txt" }
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        SettingValue value = parsed(cases[i].id, cases[i].text);
        CHECK_STR(formatted(cases[i].id, &value), cases[i].text);
    }

    // Spellings the parser reads, and how they are written back
    SettingValue value = parsed(SET_ID_WRAP_ENTRIES, "True");
    CHECK_INT(value.number, 1);
    CHECK_STR(formatted(SET_ID_WRAP_ENTRIES, &value), "true");
    value = parsed(SET_ID_TITLE_OVERSIZE, "Truncated");                     // The old parser's only spelling
    CHECK_STR(formatted(SET_ID_TITLE_OVERSIZE, &value), "Truncate");
    value = parsed(SET_ID_OVERLAY_OPACITY, "12.50%");
    CHECK_INT(value.number, 1250);
    CHECK(value.percent);
    CHECK_STR(formatted(SET_ID_OVERLAY_OPACITY, &value), "12.5%");
    value = parsed(SET_ID_TITLE_FONT, "\"C:\\Fonts\\My Font.ttf\"");         // Quotes dropped, as clean_path does
    CHECK_STR(value.text, "C:\\Fonts\\My Font.ttf");
    value = parsed(SET_ID_APPLICATION_TIMEOUT, "15s");                     // atoi, as before
    CHECK_INT(value.number, 15);

    // An absent FPSLimit, FontFace or command writes nothing: the key is removed
    SettingValue inherit;
    memset(&inherit, 0, sizeof(inherit));
    inherit.inherit = true;
    CHECK_STR(formatted(SET_ID_FPS_LIMIT, &inherit), "");
    CHECK_STR(formatted(SET_ID_TITLE_FONT_FACE, &inherit), "");
    CHECK_STR(formatted(SET_ID_STARTUP_CMD, &inherit), "");
}

// A function to test what the new types refuse, including the spec's two parser bugs
static void test_new_rejects(void)
{
    SettingValue value;
    CHECK(!setting_parse(setting_def(SET_ID_FPS_LIMIT), "9", &value));
    CHECK(!setting_parse(setting_def(SET_ID_CLOCK_FONT_SIZE), "-5", &value));   // Wrapped to 4294967291 before
    CHECK(!setting_parse(setting_def(SET_ID_CLOCK_FONT_SIZE), "0", &value));
    CHECK(!setting_parse(setting_def(SET_ID_WRAP_ENTRIES), "yes", &value));
    CHECK(!setting_parse(setting_def(SET_ID_WRAP_ENTRIES), "TRUE", &value));
    CHECK(!setting_parse(setting_def(SET_ID_ON_LAUNCH), "blank", &value));
    CHECK(!setting_parse(setting_def(SET_ID_OVERLAY_OPACITY), "101%", &value));
    CHECK(!setting_parse(setting_def(SET_ID_OVERLAY_OPACITY), "abc%", &value));
    CHECK(!setting_parse(setting_def(SET_ID_OVERLAY_OPACITY), "50", &value));   // No px for an opacity
    CHECK(!setting_parse(setting_def(SET_ID_OVERLAY_OPACITY), "5.555%", &value));
    CHECK(!setting_parse(setting_def(SET_ID_OVERLAY_OPACITY), "5.%", &value));
    CHECK(!setting_parse(setting_def(SET_ID_ICON_SPACING), "40px", &value));
    CHECK(!setting_parse(setting_def(SET_ID_ICON_SPACING), "-4", &value));
    CHECK(!setting_parse(setting_def(SET_ID_ICON_SPACING), "2147483648", &value));  // Past an int
    CHECK(!setting_parse(setting_def(SET_ID_TITLE_PADDING), "8.5%", &value));  // Padding is whole
    CHECK(!setting_parse(setting_def(SET_ID_TITLE_PADDING), "51%", &value));
    CHECK(!setting_parse(setting_def(SET_ID_HIGHLIGHT_CORNER_RADIUS), "101", &value));
    CHECK(!setting_parse(setting_def(SET_ID_HIGHLIGHT_OUTLINE_SIZE), "-1", &value));
    CHECK(!setting_parse(setting_def(SET_ID_APPLICATION_TIMEOUT), "2", &value));
    CHECK(!setting_parse(setting_def(SET_ID_APPLICATION_TIMEOUT), "31", &value));
    CHECK(!setting_parse(setting_def(SET_ID_SCREENSAVER_IDLE_TIME), "901", &value));
    CHECK(!setting_parse(setting_def(SET_ID_GAMEPAD_DEVICE), "-2", &value));
    CHECK(!setting_parse(setting_def(SET_ID_GAMEPAD_DEVICE), "16", &value));
    CHECK(!setting_parse(setting_def(SET_ID_STARTUP_CMD), "", &value));
    CHECK(!setting_parse(setting_def(SET_ID_DEFAULT_MENU), "", &value));
    CHECK(!setting_parse(setting_def(SET_ID_OVERLAY_OPACITY), "100000%", &value));   // Past the five-digit cap

    // A command fills the text buffer less its terminator, and no more
    char command[SETTING_TEXT_MAX + 1];
    memset(command, 'x', (size_t) (SETTING_TEXT_MAX - 1));
    command[SETTING_TEXT_MAX - 1] = '\0';
    CHECK(setting_parse(setting_def(SET_ID_STARTUP_CMD), command, &value));
    command[SETTING_TEXT_MAX - 1] = 'x';
    command[SETTING_TEXT_MAX] = '\0';
    CHECK(!setting_parse(setting_def(SET_ID_STARTUP_CMD), command, &value));
}

// A function to test that every built-in fallback reads as its own setting's value, so an unset
// CMake variable (an empty string) fails here rather than at start
static void test_fallbacks(void)
{
    for (int id = 0; id < SET_ID_GLOBAL_COUNT; id++) {
        const SettingDef *def = setting_def((SettingId) id);
        SettingValue value;
        if (def->fallback != NULL)
            CHECK(setting_parse(def, def->fallback, &value));
    }
}

// A function to test the new types' steps: limits, the Off step, and a file's value in its place
static void test_new_steps(void)
{
    SettingValue off = parsed(SET_ID_WRAP_ENTRIES, "false");
    CHECK_INT(stepped(SET_ID_WRAP_ENTRIES, off, &off, 1, 1).number, 1);
    CHECK_INT(stepped(SET_ID_WRAP_ENTRIES, off, &off, 1, 5).number, 1);

    // FPS limit: Off, 30, 60, 75, 120, 144, 165, 240; a file's 10 sits between Off and 30
    SettingValue ten = parsed(SET_ID_FPS_LIMIT, "10");
    CHECK(stepped(SET_ID_FPS_LIMIT, ten, &ten, -1, 1).inherit);
    CHECK_INT(stepped(SET_ID_FPS_LIMIT, ten, &ten, 1, 1).number, 30);
    CHECK_INT(stepped(SET_ID_FPS_LIMIT, ten, &ten, 1, 20).number, 240);
    SettingValue back = stepped(SET_ID_FPS_LIMIT, stepped(SET_ID_FPS_LIMIT, ten, &ten, 1, 3), &ten, -1, 3);
    CHECK_INT(back.number, 10);

    // Opacity: 0-100% in fives; a file's 12.5% sits between 10% and 15%
    SettingValue odd = parsed(SET_ID_OVERLAY_OPACITY, "12.5%");
    CHECK_INT(stepped(SET_ID_OVERLAY_OPACITY, odd, &odd, -1, 1).number, 1000);
    CHECK_INT(stepped(SET_ID_OVERLAY_OPACITY, odd, &odd, 1, 1).number, 1500);
    CHECK_INT(stepped(SET_ID_OVERLAY_OPACITY, odd, &odd, 1, 30).number, 10000);
    CHECK_INT(stepped(SET_ID_OVERLAY_OPACITY, odd, &odd, -1, 30).number, 0);

    // Icon spacing: a file's px value comes before every percentage
    SettingValue px = parsed(SET_ID_ICON_SPACING, "40");
    SettingValue first_pct = stepped(SET_ID_ICON_SPACING, px, &px, 1, 1);
    CHECK(first_pct.percent);
    CHECK_INT(first_pct.number, 0);
    CHECK(!stepped(SET_ID_ICON_SPACING, first_pct, &px, -1, 1).percent);
    CHECK_INT(stepped(SET_ID_ICON_SPACING, px, &px, 1, 20).number, 1000);   // 10% at most

    // A huge px value still sorts before every percentage
    SettingValue huge = parsed(SET_ID_ICON_SPACING, "2000000000");
    SettingValue after_huge = stepped(SET_ID_ICON_SPACING, huge, &huge, 1, 1);
    CHECK(after_huge.percent);
    CHECK_INT(after_huge.number, 0);

    // Vertical center: 25-75% in fives
    SettingValue center = parsed(SET_ID_VCENTER, "50%");
    CHECK_INT(stepped(SET_ID_VCENTER, center, &center, 1, 20).number, 7500);
    CHECK_INT(stepped(SET_ID_VCENTER, center, &center, -1, 20).number, 2500);

    // Padding: 0-20% in twos; a file's px sits first
    SettingValue padding = parsed(SET_ID_TITLE_PADDING, "8%");
    CHECK_INT(stepped(SET_ID_TITLE_PADDING, padding, &padding, 1, 1).number, 1000);
    CHECK_INT(stepped(SET_ID_TITLE_PADDING, padding, &padding, 1, 20).number, 2000);

    // A padding past the last step (300 px) stays reachable at the end
    SettingValue wide = parsed(SET_ID_HIGHLIGHT_HPADDING, "300");
    CHECK_INT(stepped(SET_ID_HIGHLIGHT_HPADDING, wide, &wide, -1, 1).number, 100);
    CHECK_INT(stepped(SET_ID_HIGHLIGHT_HPADDING, wide, &wide, 1, 1).number, 300);

    // Choices step through their own names only: Too long is Truncate or Shrink, and a file's None stays
    SettingValue none = parsed(SET_ID_TITLE_OVERSIZE, "None");
    CHECK_INT(stepped(SET_ID_TITLE_OVERSIZE, none, &none, -1, 1).number, 1);
    CHECK_INT(stepped(SET_ID_TITLE_OVERSIZE, none, &none, -1, 5).number, 0);

    // The spec's lists: app timeout, idle time, dim level, clock size
    SettingValue timeout = parsed(SET_ID_APPLICATION_TIMEOUT, "3");
    CHECK_INT(stepped(SET_ID_APPLICATION_TIMEOUT, timeout, &timeout, 1, 4).number, 20);
    SettingValue idle = parsed(SET_ID_SCREENSAVER_IDLE_TIME, "3");
    CHECK_INT(stepped(SET_ID_SCREENSAVER_IDLE_TIME, idle, &idle, 1, 5).number, 60);
    CHECK_INT(stepped(SET_ID_SCREENSAVER_IDLE_TIME, idle, &idle, 1, 20).number, 900);
    SettingValue dim = parsed(SET_ID_SCREENSAVER_INTENSITY, "70%");
    CHECK_INT(stepped(SET_ID_SCREENSAVER_INTENSITY, dim, &dim, -1, 20).number, 1000);
    SettingValue size = parsed(SET_ID_CLOCK_FONT_SIZE, "50");
    CHECK_INT(stepped(SET_ID_CLOCK_FONT_SIZE, size, &size, 1, 50).number, 120);
    CHECK_INT(stepped(SET_ID_CLOCK_FONT_SIZE, size, &size, -1, 50).number, 20);

    // Paths, fonts, commands, the default menu and the device are chosen elsewhere, never stepped here
    SettingValue cmd = parsed(SET_ID_STARTUP_CMD, ":quit");
    SettingValue same_cmd = stepped(SET_ID_STARTUP_CMD, cmd, &cmd, 1, 1);   // A local: MSVC's C4223 refuses .text on a returned struct
    CHECK_STR(same_cmd.text, ":quit");
    SettingValue device = parsed(SET_ID_GAMEPAD_DEVICE, "1");
    CHECK_INT(stepped(SET_ID_GAMEPAD_DEVICE, device, &device, 1, 1).number, 1);
}

// A function to test how the new types read on screen
static void test_new_descriptions(void)
{
    SettingValue value = parsed(SET_ID_WRAP_ENTRIES, "true");
    CHECK_STR(described(SET_ID_WRAP_ENTRIES, &value, NULL), "On");
    value = parsed(SET_ID_FPS_LIMIT, "60");
    CHECK_STR(described(SET_ID_FPS_LIMIT, &value, NULL), "60 fps");
    value.inherit = true;
    CHECK_STR(described(SET_ID_FPS_LIMIT, &value, NULL), "Off");
    value = parsed(SET_ID_OVERLAY_OPACITY, "12.5%");
    CHECK_STR(described(SET_ID_OVERLAY_OPACITY, &value, NULL), "12.5%");
    value = parsed(SET_ID_ICON_SPACING, "40");
    CHECK_STR(described(SET_ID_ICON_SPACING, &value, NULL), "40 px");
    value = parsed(SET_ID_HIGHLIGHT_HPADDING, "30");
    CHECK_STR(described(SET_ID_HIGHLIGHT_HPADDING, &value, NULL), "30 px");
    value = parsed(SET_ID_HIGHLIGHT_CORNER_RADIUS, "25");
    CHECK_STR(described(SET_ID_HIGHLIGHT_CORNER_RADIUS, &value, NULL), "25");
    value = parsed(SET_ID_APPLICATION_TIMEOUT, "15");
    CHECK_STR(described(SET_ID_APPLICATION_TIMEOUT, &value, NULL), "15 s");
    value = parsed(SET_ID_SCREENSAVER_IDLE_TIME, "300");
    CHECK_STR(described(SET_ID_SCREENSAVER_IDLE_TIME, &value, NULL), "5 min");
    value = parsed(SET_ID_ON_LAUNCH, "Blank");
    CHECK_STR(described(SET_ID_ON_LAUNCH, &value, NULL), "Blank screen");
    value = parsed(SET_ID_CLOCK_TIME_FORMAT, "12hr");
    CHECK_STR(described(SET_ID_CLOCK_TIME_FORMAT, &value, NULL), "2:05 PM");
    value = parsed(SET_ID_CLOCK_DATE_FORMAT, "Big");
    CHECK_STR(described(SET_ID_CLOCK_DATE_FORMAT, &value, NULL), "Sep 28");
    value = parsed(SET_ID_TITLE_FONT, "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    CHECK_STR(described(SET_ID_TITLE_FONT, &value, NULL), "DejaVuSans.ttf");
    value = parsed(SET_ID_STARTUP_CMD, ":submenu Games");
    CHECK_STR(described(SET_ID_STARTUP_CMD, &value, NULL), "Open submenu: Games");
    value = parsed(SET_ID_STARTUP_CMD, ":quit");
    CHECK_STR(described(SET_ID_STARTUP_CMD, &value, NULL), "Quit StreamFlex");
    value = parsed(SET_ID_STARTUP_CMD, "kodi --standalone");
    CHECK_STR(described(SET_ID_STARTUP_CMD, &value, NULL), "kodi --standalone");
    value.inherit = true;
    CHECK_STR(described(SET_ID_STARTUP_CMD, &value, NULL), "None");
    value = parsed(SET_ID_GAMEPAD_DEVICE, "-1");
    CHECK_STR(described(SET_ID_GAMEPAD_DEVICE, &value, NULL), "Any");
    value = parsed(SET_ID_GAMEPAD_DEVICE, "1");
    CHECK_STR(described(SET_ID_GAMEPAD_DEVICE, &value, NULL), "Pad 1");

    char label[64];
    setting_command_label(":select", label, sizeof(label));
    CHECK_STR(label, "OK");
    setting_command_label(":exit", label, sizeof(label));
    CHECK_STR(label, "Close the app on show");
}

// A function to test finding a setting by section and key, as the parser does
static void test_find(void)
{
    CHECK(setting_find("Titles", "Color") == setting_def(SET_ID_TITLE_COLOR));
    CHECK(setting_find("Background", "Color") == setting_def(SET_ID_BACKGROUND_COLOR));
    CHECK(setting_find("Highlight", "Enabled") == setting_def(SET_ID_HIGHLIGHT_ENABLED));
    CHECK(setting_find("Scroll Indicators", "Enabled") == setting_def(SET_ID_SCROLL_ENABLED));
    CHECK(setting_find("Layout", "MaxButtons") == setting_def(SET_ID_LAYOUT_COLUMNS));   // Its alias
    CHECK(setting_find("Titles", "FontFace") == setting_def(SET_ID_TITLE_FONT_FACE));
    CHECK(setting_find("Clock", "FontFace") == setting_def(SET_ID_CLOCK_FONT_FACE));
    CHECK(setting_find("General", "Nope") == NULL);
    CHECK(setting_find("Main", "Rows") == NULL);                   // Per-menu keys belong to menu sections
    CHECK(setting_find("Hotkeys", "Hotkey1") == NULL);

    // Every global setting is found by its own section and key, and has a label and a section
    for (int id = 0; id < SET_ID_GLOBAL_COUNT; id++) {
        const SettingDef *def = setting_def((SettingId) id);
        CHECK_INT((int) def->id, id);
        CHECK(def->section != NULL && def->label != NULL && def->key != NULL);
        CHECK(setting_find(def->section, def->key) == def);
    }
}

// A function to fail a check for a row the page on show does not have
static void no_such_row(const char *helper, const char *label)
{
    check_count++;
    check_failures++;
    fprintf(stderr, "%s: no row labeled \"%s\" on this page\n", helper, label);
}

// A function to find a row on the page on show by its label. A label the page does not have fails
// a check and gives an empty row (no label, no value, grayed, no reason), never NULL.
static const SettingsRow *row_labeled(SettingsState *state, SettingsRow *rows, const char *label)
{
    static SettingsRow missing;
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    for (int i = 0; i < count; i++) {
        if (strcmp(rows[i].label, label) == 0)
            return &rows[i];
    }
    no_such_row("row_labeled", label);
    return &missing;
}

// A function to move the cursor onto a row by its label: up to the first row it may rest on, then
// down until it is there. A label it never reaches fails a check.
static void cursor_to(SettingsState *state, const char *label)
{
    SettingsRow rows[SETTINGS_MAX_ROWS];
    for (int i = 0; i < SETTINGS_MAX_ROWS; i++)
        settings_command(state, SETTINGS_UP);
    for (int i = 0; i < SETTINGS_MAX_ROWS; i++) {
        settings_rows(state, rows, SETTINGS_MAX_ROWS);
        if (strcmp(rows[settings_cursor(state)].label, label) == 0)
            return;
        settings_command(state, SETTINGS_DOWN);
    }
    no_such_row("cursor_to", label);
}

// A function to open a top-level page by its label
static void open_page(SettingsState *state, const char *label)
{
    cursor_to(state, label);
    settings_command(state, SETTINGS_OK);
}

// A function to test the General page: its rows, a row grayed with its reason, and the default menu
static void test_general_page(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    char path[256];
    open_page(state, "General");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_GENERAL);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "General");
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 12);
    static const char *const labels[] = { "Default menu", "Wrap around", "Reset on Back", "Mouse select",
        "Block the OS screensaver", "VSync", "FPS limit", "After launching an app", "App timeout",
        "Startup command", "Quit command", "Keyboard layout" };
    for (int i = 0; i < 12; i++)
        CHECK_STR(rows[i].label, labels[i]);
    CHECK_INT(rows[0].kind, SETTINGS_ROW_PICK);
    CHECK_INT(rows[9].kind, SETTINGS_ROW_PICK);
    CHECK_STR(rows[9].value, "None");

    // FPS limit is grayed while VSync is on, says why, and can take the cursor, which changes nothing
    const SettingsRow *fps = row_labeled(state, rows, "FPS limit");
    CHECK(fps != NULL && !fps->enabled);
    CHECK_STR(fps->why, "Used only while VSync is off");
    cursor_to(state, "FPS limit");
    CHECK_STR(rows[settings_cursor(state)].label, "FPS limit");
    CHECK_INT(settings_command(state, SETTINGS_RIGHT).kind, SETTINGS_EVENT_NONE);
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_NONE);
    settings_command(state, SETTINGS_UP);                        // VSync off: FPS limit opens up
    CHECK_INT(settings_command(state, SETTINGS_LEFT).kind, SETTINGS_EVENT_CHANGED);
    fps = row_labeled(state, rows, "FPS limit");
    CHECK(fps->enabled && fps->why == NULL);

    // Default menu steps through the menus in file order, and OK opens its list
    cursor_to(state, "Default menu");
    CHECK_INT(settings_cursor(state), 0);
    SettingsEvent event = settings_command(state, SETTINGS_RIGHT);
    CHECK_INT(event.kind, SETTINGS_EVENT_CHANGED);
    CHECK_STR(event_value(&event)->text, "Games");
    CHECK_STR(event.before.text, "Main");
    CHECK_INT(settings_command(state, SETTINGS_RIGHT).kind, SETTINGS_EVENT_NONE);   // Games is the last
    event = settings_command(state, SETTINGS_LEFT);                                  // And back to Main
    CHECK_INT(event.kind, SETTINGS_EVENT_CHANGED);
    CHECK_STR(event_value(&event)->text, "Main");
    event = settings_command(state, SETTINGS_OK);
    CHECK_INT(event.kind, SETTINGS_EVENT_PICK);
    CHECK(event.slot == settings_slot(state, SET_ID_DEFAULT_MENU, -1));

    // A command picked in the command picker
    SettingValue quit = parsed(SET_ID_STARTUP_CMD, ":quit");
    SettingSlot *startup = settings_slot(state, SET_ID_STARTUP_CMD, -1);
    CHECK_INT(settings_choose_value(state, startup, &quit).kind, SETTINGS_EVENT_CHANGED);
    CHECK_INT(settings_choose_value(state, startup, &quit).kind, SETTINGS_EVENT_NONE);
    CHECK_STR(row_labeled(state, rows, "Startup command")->value, "Quit StreamFlex");
    settings_free(state);
}

// A function to test the pages whose rows follow a switch: Titles, Highlight, Clock
static void test_grayed_rows(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];

    // Titles: Shadow color waits for Shadows; titles off grays everything but Show titles
    open_page(state, "Titles");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TITLES);
    static const char *const labels[] = { "Size", "Show titles", "Font", "Color", "Opacity", "Shadows",
                                          "Shadow color", "Too long", "Padding" };
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 9);
    for (int i = 0; i < count && i < (int) (sizeof(labels) / sizeof(labels[0])); i++)
        CHECK_STR(rows[i].label, labels[i]);
    CHECK_STR(row_labeled(state, rows, "Shadow color")->why, "Turn Shadows on to change this");
    CHECK_INT(row_labeled(state, rows, "Font")->kind, SETTINGS_ROW_PICK);
    cursor_to(state, "Show titles");
    settings_command(state, SETTINGS_LEFT);
    count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    for (int i = 0; i < count; i++) {
        if (strcmp(rows[i].label, "Show titles") == 0)
            CHECK(rows[i].enabled);
        else
            CHECK(!rows[i].enabled && rows[i].why != NULL && strcmp(rows[i].why, "Titles are off") == 0);
    }
    settings_command(state, SETTINGS_BACK);
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[3].value, "Off");                     // The top page's summary follows

    // Highlight: the outline's color and opacity wait for a size; corners and an outline exclude each other
    open_page(state, "Highlight");
    CHECK_STR(row_labeled(state, rows, "Outline color")->why, "The outline's size is 0");
    CHECK(row_labeled(state, rows, "Corner radius")->enabled);
    cursor_to(state, "Outline size");
    settings_command(state, SETTINGS_RIGHT);
    CHECK(row_labeled(state, rows, "Outline color")->enabled);
    CHECK_STR(row_labeled(state, rows, "Corner radius")->why, "Rounded corners cannot be drawn with an outline");
    settings_command(state, SETTINGS_BACK);

    // Clock: off grays all but Show; with it on, the date's rows wait for Show date
    open_page(state, "Clock");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_CLOCK);
    CHECK_STR(row_labeled(state, rows, "Size")->why, "The clock is off");
    cursor_to(state, "Show");
    settings_command(state, SETTINGS_RIGHT);
    CHECK(row_labeled(state, rows, "Size")->enabled);
    CHECK_STR(row_labeled(state, rows, "Date")->why, "Turn Show date on to change this");
    CHECK_STR(row_labeled(state, rows, "Weekday")->why, "Turn Show date on to change this");
    settings_free(state);
}

// A function to test the Screensaver, Scroll indicators and Controls pages, and the gamepad's Device
static void test_other_pages(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    char path[256];
    open_page(state, "Screensaver");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_SCREENSAVER);
    CHECK_STR(row_labeled(state, rows, "Idle time")->why, "The screensaver is off");
    CHECK_STR(row_labeled(state, rows, "Idle time")->value, "5 min");
    cursor_to(state, "On");                                  // On: the top page says after how long
    settings_command(state, SETTINGS_RIGHT);
    settings_command(state, SETTINGS_BACK);
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[7].label, "Screensaver");
    CHECK_STR(rows[7].value, "After 5 min");

    open_page(state, "Scroll indicators");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_SCROLL);
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 5);
    settings_command(state, SETTINGS_BACK);

    // Controls > Gamepad: On, Device, Mappings file (next start) and its note
    open_page(state, "Controls");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_CONTROLS);
    open_page(state, "Gamepad");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_GAMEPAD);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Controls" ARROW "Gamepad");
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 4);
    CHECK_STR(rows[1].label, "Device");
    CHECK_STR(rows[1].value, "Any");
    CHECK_INT(rows[2].kind, SETTINGS_ROW_BROWSE);
    CHECK_INT(rows[3].kind, SETTINGS_ROW_NOTE);
    CHECK(strstr(rows[3].note, "next start") != NULL);

    // Device steps through the pads present, by name; one the file names that is gone stays reachable
    static const char *const pads[] = { "Xbox Controller", "8BitDo Pro 2" };
    settings_set_pads(state, pads, 2);
    CHECK_INT(settings_pad_count(state), 2);
    CHECK_STR(settings_pad_name(state, 0), "Xbox Controller");
    CHECK(settings_pad_name(state, 2) == NULL);
    CHECK(settings_pad_name(state, -1) == NULL);
    cursor_to(state, "Device");
    SettingsEvent event = settings_command(state, SETTINGS_RIGHT);
    CHECK_INT(event.kind, SETTINGS_EVENT_CHANGED);
    CHECK_INT(event_value(&event)->number, 0);
    CHECK_STR(row_labeled(state, rows, "Device")->value, "Xbox Controller");
    settings_command(state, SETTINGS_RIGHT);
    CHECK_STR(row_labeled(state, rows, "Device")->value, "8BitDo Pro 2");
    CHECK_INT(settings_command(state, SETTINGS_RIGHT).kind, SETTINGS_EVENT_NONE);
    SettingValue gone = parsed(SET_ID_GAMEPAD_DEVICE, "5");
    settings_set_entry(state, SET_ID_GAMEPAD_DEVICE, -1, &gone);
    CHECK_STR(row_labeled(state, rows, "Device")->value, "Pad 5 (not connected)");
    settings_command(state, SETTINGS_LEFT);
    CHECK_INT(settings_slot(state, SET_ID_GAMEPAD_DEVICE, -1)->value.number, 1);
    event = settings_command(state, SETTINGS_RIGHT);                 // And back to it
    CHECK_INT(event.kind, SETTINGS_EVENT_CHANGED);
    CHECK_INT(event_value(&event)->number, 5);
    settings_command(state, SETTINGS_LEFT);
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_PICK);

    // A failed list (SDL_GetJoysticks() is NULL on error) names no pads
    settings_set_pads(state, NULL, -1);
    CHECK_INT(settings_pad_count(state), 0);

    // A pad with no name (SDL_GetJoystickNameForID() gives NULL) is "Pad N", in the row and in the list
    static const char *const nameless[] = { NULL, "8BitDo Pro 2" };
    settings_set_pads(state, nameless, 2);
    settings_command(state, SETTINGS_LEFT);
    CHECK_INT(settings_slot(state, SET_ID_GAMEPAD_DEVICE, -1)->value.number, 0);
    CHECK_STR(row_labeled(state, rows, "Device")->value, "Pad 0");
    CHECK_STR(settings_pad_name(state, 0), "Pad 0");
    CHECK_STR(settings_pad_name(state, 1), "8BitDo Pro 2");

    // The gamepad off grays the rest, and the pages above say so
    cursor_to(state, "On");
    settings_command(state, SETTINGS_LEFT);
    CHECK_STR(row_labeled(state, rows, "Device")->why, "The gamepad is off");
    settings_command(state, SETTINGS_BACK);
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[0].value, "Off");
    settings_command(state, SETTINGS_BACK);
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[8].label, "Controls");
    CHECK_STR(rows[8].value, "Gamepad off");
    settings_free(state);
}

// A function to test what the model tells the screen about a row: whether Left and Right step it (a
// setting, or a picker for a color, the default menu or the device) and whether the cursor may rest
// on it (not a divider or a note, and a grayed row only when it says why)
static void test_row_steps_and_selectable(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    open_page(state, "General");
    const SettingsRow *row = row_labeled(state, rows, "Default menu");
    CHECK(row->kind == SETTINGS_ROW_PICK && row->steps && settings_row_selectable(row));   // A picker that steps
    row = row_labeled(state, rows, "Startup command");
    CHECK(row->kind == SETTINGS_ROW_PICK && !row->steps && settings_row_selectable(row));  // One that does not
    row = row_labeled(state, rows, "Wrap around");
    CHECK(row->kind == SETTINGS_ROW_SETTING && row->steps);                               // A setting
    row = row_labeled(state, rows, "FPS limit");                                          // Grayed, with a reason
    CHECK(!row->enabled && row->why != NULL && row->steps && settings_row_selectable(row));
    settings_command(state, SETTINGS_BACK);

    open_page(state, "Background");
    row = row_labeled(state, rows, "Color");
    CHECK(row->kind == SETTINGS_ROW_PICK && row->steps);                                  // A color steps
    settings_command(state, SETTINGS_BACK);

    open_page(state, "Titles");
    row = row_labeled(state, rows, "Font");
    CHECK(row->kind == SETTINGS_ROW_PICK && !row->steps);                                 // A font does not
    settings_command(state, SETTINGS_BACK);

    open_page(state, "Controls");
    row = row_labeled(state, rows, "Gamepad");
    CHECK(row->kind == SETTINGS_ROW_LINK && !row->steps && settings_row_selectable(row));  // A link
    open_page(state, "Gamepad");
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 4);
    CHECK(rows[1].kind == SETTINGS_ROW_PICK && rows[1].steps);                            // The device steps
    CHECK(rows[2].kind == SETTINGS_ROW_BROWSE && !rows[2].steps);
    CHECK(rows[3].kind == SETTINGS_ROW_NOTE && !settings_row_selectable(&rows[3]));       // A note
    settings_command(state, SETTINGS_BACK);
    settings_command(state, SETTINGS_BACK);

    // The top page: a divider, and Discard grayed with nothing to discard and no reason to give
    count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 11);
    CHECK(rows[9].kind == SETTINGS_ROW_DIVIDER && !settings_row_selectable(&rows[9]));
    CHECK(rows[10].kind == SETTINGS_ROW_ACTION && !rows[10].enabled && rows[10].why == NULL);
    CHECK(!settings_row_selectable(&rows[10]) && !rows[10].steps);
    settings_free(state);
}

// A function to name keys in tests: "#<HEX>" for a key, the label for a pad's control
static void test_namer(int device, int code, char *out, size_t size)
{
    if (device == BINDINGS_KEYBOARD)
        snprintf(out, size, "#%X", (unsigned int) code);
    else
        snprintf(out, size, "%s", bindings_label(code));
}

// A function to give a model bindings loaded from hotkey lines
static Bindings *with_bindings(SettingsState *state, const char *hotkeys)
{
    char text[512];
    snprintf(text, sizeof(text), "[Hotkeys]\n%s\n", hotkeys);
    IniDoc *doc = inidoc_parse(text, strlen(text));
    IniDocItem items[8];
    Bindings *b = bindings_create(false, true);
    CHECK(bindings_load(b, BINDINGS_KEYBOARD, items, inidoc_list(doc, "Hotkeys", NULL, items, 8)));
    inidoc_free(doc);
    settings_set_bindings(state, b, test_namer);
    return b;
}

// A function to test adding a hotkey: Add binding, capture, Keep, then the command, which commits it
static void test_add_binding(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    Bindings *b = with_bindings(state, "Hotkey1=#4000003A;:quit");
    open_page(state, "Controls");
    open_page(state, "Keyboard");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_KEYBOARD);
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[0].label, "Add binding");
    CHECK_INT(rows[1].kind, SETTINGS_ROW_BINDING);
    CHECK_STR(rows[1].label, "#4000003A");
    CHECK_STR(rows[1].value, "Quit StreamFlex");
    CHECK_INT(rows[count - 1].kind, SETTINGS_ROW_NOTE);

    // Add binding: the page of a new binding, its key first
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BINDING);
    SettingsEvent event = settings_command(state, SETTINGS_OK);
    CHECK_INT(event.kind, SETTINGS_EVENT_CAPTURE);
    CHECK_INT(event.device, BINDINGS_KEYBOARD);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_CAPTURE);

    // A refused key ends the capture with its reason; a good one asks to keep it
    settings_capture_ended(state, NULL);
    settings_command(state, SETTINGS_OK);
    event = settings_captured(state, BIND_KEY_LEFT);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BINDING);
    CHECK(strstr(settings_notice(state), "keep their own meaning") != NULL);
    settings_command(state, SETTINGS_OK);
    settings_captured(state, 0x4000003E);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_CONFIRM);
    CHECK_STR(row_labeled(state, rows, "Keep")->label, "Keep");
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MOVED);   // Keep
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BINDING);
    CHECK_STR(row_labeled(state, rows, "Key")->value, "#4000003E");

    // The command commits the new binding
    cursor_to(state, "Command");
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_PICK_COMMAND);
    event = settings_bind_command(state, ":home");
    CHECK_INT(event.kind, SETTINGS_EVENT_BINDINGS);
    CHECK(!event.confirm);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_KEYBOARD);
    CHECK_INT(bindings_count(b, BINDINGS_KEYBOARD), 2);
    CHECK_STR(bindings_at(b, BINDINGS_KEYBOARD, 1)->command, ":home");
    CHECK(settings_any_changed(state));
    settings_free(state);
    bindings_free(b);
}

// A function to test a change that takes Up's navigation away: allowed while another key goes up,
// flagged for the 10 s, and reverted when it is not confirmed
static void test_navigation_confirm(void)
{
    SettingsState *state = open_model();
    Bindings *b = with_bindings(state, "Hotkey1=#4000003A;:up");
    open_page(state, "Controls");
    open_page(state, "Keyboard");
    settings_command(state, SETTINGS_OK);                    // Add binding
    settings_command(state, SETTINGS_OK);                    // Key
    settings_captured(state, BIND_KEY_UP);
    settings_command(state, SETTINGS_OK);                    // Keep
    SettingsEvent event = settings_bind_command(state, ":quit");
    CHECK_INT(event.kind, SETTINGS_EVENT_BINDINGS);
    CHECK(event.confirm);
    CHECK_INT(event.code, BIND_KEY_UP);
    CHECK_INT(bindings_count(b, BINDINGS_KEYBOARD), 2);
    event = settings_revert_binding(state);
    CHECK_INT(event.kind, SETTINGS_EVENT_BINDINGS);
    CHECK(bindings_at(b, BINDINGS_KEYBOARD, 1)->removed);
    CHECK(!settings_any_changed(state));
    settings_free(state);
    bindings_free(b);
}

// A function to test Remove: grayed with the floor's reason for the last way to a command, and
// allowed otherwise
static void test_remove_binding(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    Bindings *b = with_bindings(state, "Hotkey1=#4000003A;:quit");
    open_page(state, "Controls");
    open_page(state, "Keyboard");
    settings_command(state, SETTINGS_DOWN);                  // The binding
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BINDING);
    const SettingsRow *remove = row_labeled(state, rows, "Remove");
    CHECK(remove != NULL && remove->enabled);
    cursor_to(state, "Remove");
    SettingsEvent event = settings_command(state, SETTINGS_OK);
    CHECK_INT(event.kind, SETTINGS_EVENT_BINDINGS);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_KEYBOARD);
    CHECK(bindings_at(b, BINDINGS_KEYBOARD, 0)->removed);
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 2);   // Add binding and the note

    // Discard brings it back
    settings_command(state, SETTINGS_BACK);
    settings_command(state, SETTINGS_BACK);
    cursor_to(state, "Discard changes");
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_DISCARD);
    CHECK(!bindings_at(b, BINDINGS_KEYBOARD, 0)->removed);
    settings_free(state);
    bindings_free(b);
}

// A function to give a model bindings loaded from lines of both sections, on Windows or not
static Bindings *with_lines(SettingsState *state, const char *hotkeys, const char *controls, bool windows)
{
    char text[2048];
    snprintf(text, sizeof(text), "[Hotkeys]\n%s\n[Gamepad]\n%s\n", hotkeys, controls);
    IniDoc *doc = inidoc_parse(text, strlen(text));
    IniDocItem items[8];
    Bindings *b = bindings_create(windows, true);
    CHECK(bindings_load(b, BINDINGS_KEYBOARD, items, inidoc_list(doc, "Hotkeys", NULL, items, 8)));
    CHECK(bindings_load(b, BINDINGS_GAMEPAD, items, inidoc_list(doc, "Gamepad", NULL, items, 8)));
    inidoc_free(doc);
    settings_set_bindings(state, b, test_namer);
    return b;
}

// A function to make a command `length` bytes long, for the length of a line
static const char *long_command(int length)
{
    static char text[512];
    memset(text, 'x', (size_t) length);
    text[length] = '\0';
    return text;
}

// A function to take a new binding through its key, captured and kept, from its device's list
static void new_binding(SettingsState *state, int code)
{
    cursor_to(state, "Add binding");
    settings_command(state, SETTINGS_OK);                    // Add binding
    cursor_to(state, "Key");
    settings_command(state, SETTINGS_OK);                    // Key: the capture
    settings_captured(state, code);
    cursor_to(state, "Keep");
    settings_command(state, SETTINGS_OK);
}

// A function to test the binding pages' rows and paths: the Keyboard row's count, the gamepad's
// bindings on its page, a new binding's page with Cancel, the capture and confirm pages, Try again,
// and Cancel on the confirm page
static void test_binding_pages(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    char path[256];
    Bindings *b = with_lines(state, "Hotkey1=#4000003A;:quit\nHotkey2=#4000003B;:home", "ButtonY=:quit", false);
    open_page(state, "Controls");
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 2);
    CHECK_INT(row_labeled(state, rows, "Keyboard")->kind, SETTINGS_ROW_LINK);
    CHECK_STR(row_labeled(state, rows, "Keyboard")->value, "2 hotkeys");

    // The gamepad's page: its own rows, then Add binding, its bindings and the built-in note
    open_page(state, "Gamepad");
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_INT(count, 7);
    CHECK(rows[3].kind == SETTINGS_ROW_NOTE && strstr(rows[3].note, "next start") != NULL);
    CHECK_STR(rows[4].label, "Add binding");
    CHECK_INT(rows[5].kind, SETTINGS_ROW_BINDING);
    CHECK_STR(rows[5].label, "ButtonY");
    CHECK_STR(rows[5].value, "Quit StreamFlex");
    CHECK_INT(rows[5].binding, 0);
    CHECK(rows[6].kind == SETTINGS_ROW_NOTE && strstr(rows[6].note, "built-in buttons") != NULL);

    // A new binding's page: no key, no command (not those of the binding last open), and Cancel; its
    // key is captured from the pad
    open_page(state, "ButtonY");
    CHECK_STR(row_labeled(state, rows, "Command")->value, "Quit StreamFlex");
    settings_command(state, SETTINGS_BACK);
    cursor_to(state, "Add binding");
    settings_command(state, SETTINGS_OK);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Controls" ARROW "Gamepad" ARROW "Binding");
    CHECK_STR(row_labeled(state, rows, "Key")->value, "Choose" ELLIPSIS);
    CHECK_STR(row_labeled(state, rows, "Command")->value, "None");
    CHECK_INT(row_labeled(state, rows, "Command")->action, SETTINGS_ACTION_BIND_COMMAND);
    SettingsEvent event = settings_command(state, SETTINGS_OK);
    CHECK_INT(event.kind, SETTINGS_EVENT_CAPTURE);
    CHECK_INT(event.device, BINDINGS_GAMEPAD);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Controls" ARROW "Gamepad" ARROW "Binding" ARROW "Press a key");
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 1);
    CHECK_INT(rows[0].kind, SETTINGS_ROW_NOTE);
    settings_captured(state, 11);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Controls" ARROW "Gamepad" ARROW "Binding" ARROW "Keep it?");
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[0].note, "Captured: ButtonB");

    // Try again captures again in its place; Cancel on the confirm page keeps no key
    cursor_to(state, "Try again");
    event = settings_command(state, SETTINGS_OK);
    CHECK_INT(event.kind, SETTINGS_EVENT_CAPTURE);
    CHECK_INT(event.device, BINDINGS_GAMEPAD);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Controls" ARROW "Gamepad" ARROW "Binding" ARROW "Press a key");
    settings_captured(state, 12);
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[0].note, "Captured: ButtonX");
    cursor_to(state, "Cancel");
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BINDING);
    CHECK_STR(row_labeled(state, rows, "Key")->value, "Choose" ELLIPSIS);

    // Cancel on a new binding's page goes back to the list, adding nothing
    cursor_to(state, "Cancel");
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_GAMEPAD);
    CHECK_INT(bindings_count(b, BINDINGS_GAMEPAD), 1);

    // The Keyboard row leaves removed hotkeys out of its count
    settings_command(state, SETTINGS_BACK);
    bindings_remove(b, BINDINGS_KEYBOARD, 1);
    CHECK_STR(row_labeled(state, rows, "Keyboard")->value, "1 hotkey");
    bindings_remove(b, BINDINGS_KEYBOARD, 0);
    CHECK_STR(row_labeled(state, rows, "Keyboard")->value, "0 hotkeys");
    open_page(state, "Keyboard");
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Controls" ARROW "Keyboard");
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 2);
    CHECK(strstr(rows[1].note, "keep their own meaning") != NULL);

    // Changed bindings alone make Discard available
    settings_command(state, SETTINGS_BACK);
    settings_command(state, SETTINGS_BACK);
    CHECK(row_labeled(state, rows, "Discard changes")->enabled);
    settings_free(state);
    bindings_free(b);
}

// A function to test editing a binding in place, a command with no key yet, a key whose command
// comes later, and the 10 s for an existing binding put back as it was; and that a capture's end
// outside the capture page changes nothing
static void test_binding_edits(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    Bindings *b = with_bindings(state, "Hotkey1=#4000003A;:quit\nHotkey2=#4000003B;:up");
    open_page(state, "Controls");
    open_page(state, "Keyboard");
    CHECK_INT(settings_captured(state, 0x4000003E).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_KEYBOARD);
    settings_capture_ended(state, "Nothing was pressed in 5 s");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_KEYBOARD);
    CHECK_STR(settings_notice(state), "");

    // An existing binding's page shows its key and command; a new command changes it in place
    open_page(state, "#4000003A");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BINDING);
    CHECK_STR(row_labeled(state, rows, "Key")->value, "#4000003A");
    CHECK_STR(row_labeled(state, rows, "Command")->value, "Quit StreamFlex");
    CHECK_STR(settings_binding_command(state), ":quit");
    SettingsEvent event = settings_bind_command(state, ":home");
    CHECK_INT(event.kind, SETTINGS_EVENT_BINDINGS);
    CHECK(!event.confirm);
    CHECK_INT(bindings_count(b, BINDINGS_KEYBOARD), 2);
    CHECK_STR(bindings_at(b, BINDINGS_KEYBOARD, 0)->command, ":home");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_KEYBOARD);

    // None commits nothing: the page waits for a command
    open_page(state, "#4000003A");
    CHECK_INT(settings_bind_command(state, "").kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BINDING);
    CHECK_STR(row_labeled(state, rows, "Command")->value, "None");
    CHECK_STR(bindings_at(b, BINDINGS_KEYBOARD, 0)->command, ":home");
    settings_command(state, SETTINGS_BACK);

    // A command chosen before the key waits for it; Keep then commits the two
    cursor_to(state, "Add binding");
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_bind_command(state, ":sleep").kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BINDING);
    CHECK_STR(row_labeled(state, rows, "Command")->value, "Sleep");
    CHECK_INT(bindings_count(b, BINDINGS_KEYBOARD), 2);
    cursor_to(state, "Key");
    settings_command(state, SETTINGS_OK);
    settings_captured(state, 0x4000003E);
    cursor_to(state, "Keep");
    event = settings_command(state, SETTINGS_OK);
    CHECK_INT(event.kind, SETTINGS_EVENT_BINDINGS);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_KEYBOARD);
    CHECK_INT(bindings_count(b, BINDINGS_KEYBOARD), 3);
    CHECK_INT(bindings_at(b, BINDINGS_KEYBOARD, 2)->code, 0x4000003E);
    CHECK_STR(bindings_at(b, BINDINGS_KEYBOARD, 2)->command, ":sleep");

    // The new binding's own page offers Remove: a new line is not a line with no name
    open_page(state, "#4000003E");
    CHECK(row_labeled(state, rows, "Remove")->enabled);
    settings_command(state, SETTINGS_BACK);

    // A capture that ends with no reason leaves no notice
    cursor_to(state, "Add binding");
    settings_command(state, SETTINGS_OK);
    settings_command(state, SETTINGS_OK);                    // Key
    settings_capture_ended(state, NULL);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BINDING);
    CHECK_STR(settings_notice(state), "");
    settings_command(state, SETTINGS_BACK);

    // The :up hotkey moved to the Menu key takes its navigation away: unconfirmed, it goes back to
    // F2, as it was, not removed
    open_page(state, "#4000003B");
    cursor_to(state, "Key");
    settings_command(state, SETTINGS_OK);
    settings_captured(state, BIND_KEY_APPLICATION);
    cursor_to(state, "Keep");
    event = settings_command(state, SETTINGS_OK);
    CHECK_INT(event.kind, SETTINGS_EVENT_BINDINGS);
    CHECK(event.confirm);
    CHECK_INT(event.code, BIND_KEY_APPLICATION);
    CHECK_INT(event.device, BINDINGS_KEYBOARD);
    CHECK_INT(bindings_at(b, BINDINGS_KEYBOARD, 1)->code, BIND_KEY_APPLICATION);
    CHECK_INT(settings_revert_binding(state).kind, SETTINGS_EVENT_BINDINGS);
    CHECK_INT(bindings_at(b, BINDINGS_KEYBOARD, 1)->code, 0x4000003B);
    CHECK_STR(bindings_at(b, BINDINGS_KEYBOARD, 1)->command, ":up");
    CHECK(!bindings_at(b, BINDINGS_KEYBOARD, 1)->removed);
    CHECK_INT(settings_revert_binding(state).kind, SETTINGS_EVENT_NONE);   // Once
    settings_free(state);
    bindings_free(b);
}

// A function to test the 10 s holding the pages: while a change waits to be confirmed, no binding's
// page opens, and the screen says why; kept, they open again. Discard ends the 10 s too.
static void test_binding_waits(void)
{
    SettingsState *state = open_model();
    Bindings *b = with_bindings(state, "Hotkey1=#4000003A;:up");
    open_page(state, "Controls");
    open_page(state, "Keyboard");
    new_binding(state, BIND_KEY_UP);
    CHECK(settings_bind_command(state, ":quit").confirm);
    cursor_to(state, "Add binding");
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_NONE);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_KEYBOARD);
    CHECK(strstr(settings_notice(state), "Press #40000052 again to keep the last change first") != NULL);
    cursor_to(state, "#4000003A");
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_NONE);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_KEYBOARD);

    // Kept: nothing goes back, and the pages open again
    settings_keep_binding(state);
    CHECK_INT(settings_revert_binding(state).kind, SETTINGS_EVENT_NONE);
    CHECK(!bindings_at(b, BINDINGS_KEYBOARD, 1)->removed);
    cursor_to(state, "Add binding");
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BINDING);
    settings_command(state, SETTINGS_BACK);

    // Another change waiting, then Discard: everything goes back, and nothing waits any more
    new_binding(state, BIND_KEY_MENU);
    CHECK(settings_bind_command(state, ":home").confirm);
    settings_command(state, SETTINGS_BACK);
    settings_command(state, SETTINGS_BACK);
    cursor_to(state, "Discard changes");
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_DISCARD);
    CHECK_INT(bindings_count(b, BINDINGS_KEYBOARD), 1);
    CHECK_INT(settings_revert_binding(state).kind, SETTINGS_EVENT_NONE);
    settings_free(state);
    bindings_free(b);
}

// A function to test the changes the pages refuse, each with its reason and nothing changed: the
// floor's, a Windows exit hotkey off F1 to F24, a command config.ini cannot hold on one line, and a
// Remove grayed by the floor or by a line with no name; and a binding the list has no room for
static void test_binding_refusals(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    Bindings *b = with_bindings(state, "Hotkey1=#40000052;:quit\nHotkey2=#4000003A;:up");
    open_page(state, "Controls");
    open_page(state, "Keyboard");
    open_page(state, "#4000003A");                            // F1, the last way up
    CHECK_INT(settings_bind_command(state, ":home").kind, SETTINGS_EVENT_MOVED);
    CHECK_STR(settings_notice(state), "That would leave no key for Up");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BINDING);
    CHECK_STR(bindings_at(b, BINDINGS_KEYBOARD, 1)->command, ":up");
    CHECK_STR(settings_binding_command(state), ":up");       // The page keeps its command, not the refused one
    CHECK_STR(row_labeled(state, rows, "Command")->value, "Up");
    const SettingsRow *remove = row_labeled(state, rows, "Remove");
    CHECK(!remove->enabled);
    CHECK_STR(remove->why, "That would leave no key for Up");
    cursor_to(state, "Remove");
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_NONE);
    CHECK(!bindings_at(b, BINDINGS_KEYBOARD, 1)->removed);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BINDING);
    settings_free(state);
    bindings_free(b);

    // A [Hotkeys] line with no name runs, and is listed; removing it would change how the lines after
    // it read, so Remove is grayed, but its command can change
    state = open_model();
    b = with_bindings(state, "=#4000003C;:home");
    open_page(state, "Controls");
    open_page(state, "Keyboard");
    open_page(state, "#4000003C");
    remove = row_labeled(state, rows, "Remove");
    CHECK(!remove->enabled);
    CHECK_STR(remove->why, "This line has no name in config.ini, so removing it would change how the lines after it read");
    cursor_to(state, "Remove");
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_NONE);
    CHECK(!bindings_at(b, BINDINGS_KEYBOARD, 0)->removed);
    CHECK_INT(settings_bind_command(state, ":quit").kind, SETTINGS_EVENT_BINDINGS);
    CHECK_STR(bindings_at(b, BINDINGS_KEYBOARD, 0)->command, ":quit");
    settings_free(state);
    bindings_free(b);

    // The gamepad's floor grays the Remove of OK's only button
    state = open_model();
    b = with_lines(state, "", "ButtonA=:select", false);
    open_page(state, "Controls");
    open_page(state, "Gamepad");
    open_page(state, "ButtonA");
    CHECK_STR(row_labeled(state, rows, "Remove")->why, "That would leave no button for OK");
    settings_free(state);
    bindings_free(b);

    // On Windows, the exit hotkey on F12 is refused when the command comes
    state = open_model();
    b = with_lines(state, "Hotkey1=#4000003A;:quit", "", true);
    open_page(state, "Controls");
    open_page(state, "Keyboard");
    new_binding(state, BIND_KEY_F12);
    CHECK_INT(settings_bind_command(state, ":exit").kind, SETTINGS_EVENT_MOVED);
    CHECK_STR(settings_notice(state), "The exit hotkey must be F1 to F24, but not F12");
    CHECK_INT(bindings_count(b, BINDINGS_KEYBOARD), 1);
    settings_free(state);
    bindings_free(b);

    // On Windows, F12 kept for an exit hotkey is refused at Keep: its page keeps the key it had
    state = open_model();
    b = with_lines(state, "Hotkey1=#4000003A;:exit\nHotkey2=#4000003B;:quit", "", true);
    open_page(state, "Controls");
    open_page(state, "Keyboard");
    open_page(state, "#4000003A");
    cursor_to(state, "Key");
    settings_command(state, SETTINGS_OK);                    // Key: the capture
    settings_captured(state, BIND_KEY_F12);
    cursor_to(state, "Keep");
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MOVED);
    CHECK_STR(settings_notice(state), "The exit hotkey must be F1 to F24, but not F12");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BINDING);
    CHECK_STR(row_labeled(state, rows, "Key")->value, "#4000003A");
    CHECK_INT(bindings_at(b, BINDINGS_KEYBOARD, 0)->code, 0x4000003A);
    settings_free(state);
    bindings_free(b);

    // A line config.ini cannot hold: a new hotkey is numbered one above the highest HotkeyN (Hotkey99
    // here: "Hotkey500x", "Hotkez150" and a number too long to read are not numbers), and one more for
    // each new hotkey before it (Hotkey100); a loaded line keeps its own key; a control is its label
    state = open_model();
    b = with_lines(state, "Hotkey98=#4000003A;:quit\nHotkey500x=#4000003B;:home\nHotkey99999999999999999999=#4000003C;:sleep\n"
                   "HotkeyLongName=#4000003D;:quit\nHotkez150=#40000041;:quit", "", false);
    open_page(state, "Controls");
    open_page(state, "Keyboard");
    new_binding(state, 0x4000003E);                          // "Hotkey99=#4000003E;" is 19 bytes
    CHECK_INT(settings_bind_command(state, long_command(181)).kind, SETTINGS_EVENT_MOVED);
    CHECK_STR(settings_notice(state), "config.ini cannot hold this binding: it is too long for one line of config.ini (199 bytes at most)");
    CHECK_INT(bindings_count(b, BINDINGS_KEYBOARD), 5);
    CHECK_INT(settings_bind_command(state, long_command(180)).kind, SETTINGS_EVENT_BINDINGS);
    CHECK_STR(settings_notice(state), "");                   // The refusal's reason goes with it
    new_binding(state, 0x4000003F);                          // "Hotkey100=#4000003F;" is 20
    CHECK_INT(settings_bind_command(state, long_command(180)).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_bind_command(state, long_command(179)).kind, SETTINGS_EVENT_BINDINGS);
    open_page(state, "#4000003D");                            // "HotkeyLongName=#4000003D;" is 25
    CHECK_INT(settings_bind_command(state, long_command(175)).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_bind_command(state, long_command(174)).kind, SETTINGS_EVENT_BINDINGS);
    open_page(state, "#4000003A");
    CHECK_INT(settings_bind_command(state, "kodi ;x").kind, SETTINGS_EVENT_MOVED);
    CHECK_STR(settings_notice(state), "config.ini cannot hold this binding: it has a semicolon after a space, which config.ini would read as a comment");
    settings_command(state, SETTINGS_BACK);                  // The binding's page, then Keyboard
    settings_command(state, SETTINGS_BACK);
    open_page(state, "Gamepad");
    new_binding(state, 13);                                // "ButtonY=" is 8
    CHECK_INT(settings_bind_command(state, long_command(192)).kind, SETTINGS_EVENT_MOVED);
    SettingsEvent event = settings_bind_command(state, long_command(191));
    CHECK_INT(event.kind, SETTINGS_EVENT_BINDINGS);
    CHECK_INT(event.device, BINDINGS_GAMEPAD);              // The change is the pad's
    CHECK_STR(bindings_at(b, BINDINGS_GAMEPAD, 0)->key, "");
    settings_free(state);
    bindings_free(b);
}

// A function standing in for realloc that always fails
static void *failing_realloc(void *memory, size_t size)
{
    (void) memory;
    (void) size;
    return NULL;
}

// A function standing in for free
static void plain_free(void *memory)
{
    free(memory);
}

// A function to test a binding the list has no room for (out of memory): it says so, and nothing
// changes; and keys named with no namer, as "#<HEX>"
static void test_binding_no_room(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    Bindings *b = with_bindings(state, "Hotkey1=#4000003A;:quit\nHotkey2=#4000003B;:quit\nHotkey3=#4000003C;:quit\n"
        "Hotkey4=#4000003D;:quit\nHotkey5=#4000003E;:quit\nHotkey6=#4000003F;:quit\nHotkey7=#40000040;:quit\n"
        "Hotkey8=#40000041;:quit");
    settings_set_bindings(state, b, NULL);
    open_page(state, "Controls");
    open_page(state, "Keyboard");
    CHECK_STR(row_labeled(state, rows, "#4000003A")->value, "Quit StreamFlex");
    new_binding(state, 0x40000042);
    static const AllocHooks failing = { failing_realloc, plain_free };
    alloc_set_hooks(&failing);
    SettingsEvent event = settings_bind_command(state, ":home");
    alloc_set_hooks(NULL);
    CHECK_INT(event.kind, SETTINGS_EVENT_MOVED);
    CHECK_STR(settings_notice(state), "out of memory");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BINDING);
    CHECK_INT(bindings_count(b, BINDINGS_KEYBOARD), 8);
    settings_free(state);
    bindings_free(b);
}

// A function to test that a setting that applies at next start says so when it changes
static void test_next_start(void)
{
    SettingsState *state = open_model();
    SettingSlot *slot = settings_slot(state, SET_ID_GAMEPAD_MAPPINGS, -1);
    CHECK_INT(settings_choose(state, slot, "/pads/new.txt").kind, SETTINGS_EVENT_CHANGED);
    CHECK_STR(settings_notice(state), "This applies at next start");
    CHECK_INT(settings_choose(state, settings_slot(state, SET_ID_BACKGROUND_IMAGE, -1), "/a.png").kind, SETTINGS_EVENT_CHANGED);
    CHECK_STR(settings_notice(state), "");
    settings_free(state);
}

// A function to test that the next-start notice comes only with a change: the same file chosen again,
// or a path too long to keep, changes nothing and says nothing
static void test_next_start_only_on_a_change(void)
{
    SettingsState *state = open_model();
    SettingSlot *slot = settings_slot(state, SET_ID_GAMEPAD_MAPPINGS, -1);
    CHECK_INT(settings_choose(state, slot, "/pads/new.txt").kind, SETTINGS_EVENT_CHANGED);
    CHECK_INT(settings_choose(state, slot, "/pads/new.txt").kind, SETTINGS_EVENT_NONE);
    CHECK_STR(settings_notice(state), "");
    char long_path[SETTING_TEXT_MAX + 8];
    memset(long_path, 'p', sizeof(long_path) - 1);
    long_path[sizeof(long_path) - 1] = '\0';
    CHECK_INT(settings_choose(state, slot, long_path).kind, SETTINGS_EVENT_NONE);
    CHECK_STR(settings_notice(state), "");
    CHECK_STR(slot->value.text, "/pads/new.txt");
    settings_free(state);
}

// A function to test the restart prompt's words: a setting's name, and the names of none, one, two
// and three settings joined
static void test_restart_words(void)
{
    static const char *const names[] = { "the mappings file", "the VSync", "the rows" };
    char out[256];
    settings_restart_name("Mappings file", out, sizeof(out));
    CHECK_STR(out, "the mappings file");
    settings_restart_name("VSync", out, sizeof(out));   // A capital pair stays as it is
    CHECK_STR(out, "the VSync");
    settings_restart_name("rows", out, sizeof(out));
    CHECK_STR(out, "the rows");
    settings_join_names(names, 0, out, sizeof(out));
    CHECK_STR(out, "");
    settings_join_names(names, 1, out, sizeof(out));
    CHECK_STR(out, "the mappings file");
    settings_join_names(names, 2, out, sizeof(out));
    CHECK_STR(out, "the mappings file and the VSync");
    settings_join_names(names, 3, out, sizeof(out));
    CHECK_STR(out, "the mappings file, the VSync and the rows");
}

// A function to test which changes the restart prompt names: a change to a setting that applies at
// next start counts, any other does not, nor one changed back before the save. A setting given the
// flag later is named with no other code: two here, VSync and one per-menu setting changed for both
// menus, which is named once.
static void test_restart_tracking(void)
{
    SettingsState *state = open_model();
    char names[256];
    CHECK_INT(settings_next_start(state, names, sizeof(names)), 0);
    CHECK_STR(names, "");
    settings_choose(state, settings_slot(state, SET_ID_BACKGROUND_IMAGE, -1), "/a.png");
    CHECK_INT(settings_next_start(state, names, sizeof(names)), 0);
    SettingSlot *mappings = settings_slot(state, SET_ID_GAMEPAD_MAPPINGS, -1);
    CHECK_INT(settings_choose(state, mappings, "/pads/new.txt").kind, SETTINGS_EVENT_CHANGED);
    CHECK_INT(settings_next_start(state, names, sizeof(names)), 1);
    CHECK_STR(names, "the mappings file");
    CHECK_INT(settings_choose(state, mappings, mappings->entry.text).kind, SETTINGS_EVENT_CHANGED);
    CHECK(!settings_changed(mappings));
    CHECK_INT(settings_next_start(state, names, sizeof(names)), 0);
    CHECK_STR(names, "");

    SettingDef vsync = *setting_def(SET_ID_VSYNC);
    SettingDef rows = *setting_def(SET_ID_MENU_ROWS);
    vsync.flags |= SET_FLAG_NEXT_START;
    rows.flags |= SET_FLAG_NEXT_START;
    SettingSlot *vsync_slot = settings_slot(state, SET_ID_VSYNC, -1);
    vsync_slot->def = &vsync;
    vsync_slot->value.number = !vsync_slot->entry.number;
    settings_choose(state, mappings, "/pads/new.txt");
    CHECK_INT(settings_next_start(state, names, sizeof(names)), 2);
    CHECK_STR(names, "the VSync and the mappings file");
    for (int menu = 0; menu < 2; menu++) {
        SettingSlot *slot = settings_slot(state, SET_ID_MENU_ROWS, menu);
        slot->def = &rows;
        slot->value = parsed(SET_ID_MENU_ROWS, menu == 0 ? "5" : "6");
        CHECK(settings_changed(slot));
    }
    CHECK_INT(settings_next_start(state, names, sizeof(names)), 3);
    CHECK_STR(names, "the VSync, the mappings file and the rows");
    settings_free(state);
}

// A function to test the restart page: it replaces the pages open, asks its question with Yes under
// the cursor, Up and Down move between Yes and No, OK chooses, Back is No, and the keys that close
// settings, and Left and Right, do nothing there
static void test_restart_page(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    char path[256];
    settings_command(state, SETTINGS_DOWN);
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_BACKGROUND);
    settings_show_restart(state, "the mappings file");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_RESTART);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Restart?");
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 3);
    CHECK_INT(rows[0].kind, SETTINGS_ROW_NOTE);
    CHECK_STR(rows[0].note, "Restart StreamFlex now to apply the mappings file?");
    CHECK_STR(rows[1].label, "Yes");
    CHECK_STR(rows[2].label, "No");
    CHECK_INT(settings_cursor(state), 1);
    CHECK_INT(settings_command(state, SETTINGS_HOME).kind, SETTINGS_EVENT_NONE);
    CHECK_INT(settings_command(state, SETTINGS_CLOSE).kind, SETTINGS_EVENT_NONE);
    CHECK_INT(settings_command(state, SETTINGS_LEFT).kind, SETTINGS_EVENT_NONE);
    CHECK_INT(settings_command(state, SETTINGS_RIGHT).kind, SETTINGS_EVENT_NONE);
    CHECK_INT(settings_command(state, SETTINGS_UP).kind, SETTINGS_EVENT_NONE);
    CHECK_INT(settings_cursor(state), 1);
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_RESTART);
    CHECK_INT(settings_command(state, SETTINGS_DOWN).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_cursor(state), 2);
    CHECK_INT(settings_command(state, SETTINGS_DOWN).kind, SETTINGS_EVENT_NONE);
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_CLOSE_SAVED);
    CHECK_INT(settings_command(state, SETTINGS_UP).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_cursor(state), 1);
    CHECK_INT(settings_command(state, SETTINGS_BACK).kind, SETTINGS_EVENT_CLOSE_SAVED);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_RESTART);   // The screen closes settings, not the model
    settings_free(state);
}

// A function to test making the model again over menus that came, went, were renamed or moved (plan
// ruling 5): a pending change to a menu's settings follows the menu by its old name, a new menu's
// settings start unchanged, everything else is carried across, the Default menu row follows a rename
// and goes back to its entry when the menu it named is gone, and a page about a menu follows it, or
// closes when it is gone
static void test_remodel(void)
{
    SettingsState *old = open_model();                       // Main, and Games with Rows 3 and Columns 6
    static const char *const text = "[Main]\nEntry1=Kodi;kodi;kodi\n\n[Games]\nRows=3\nEntry1=Retro;emulators;:back\n";
    IniDoc *doc = inidoc_parse(text, strlen(text));
    MenuEdit *menus = menuedit_load(doc, "Main");
    inidoc_free(doc);
    settings_set_menus(old, menus);
    static const char *const pads[] = { "Xbox Controller" };
    settings_set_pads(old, pads, 1);
    settings_slot(old, SET_ID_MENU_ROWS, 1)->value = parsed(SET_ID_MENU_ROWS, "2");          // Games: 3 -> 2
    settings_slot(old, SET_ID_MENU_COLUMNS, 0)->value = parsed(SET_ID_MENU_COLUMNS, "5");    // Main: All menus -> 5
    settings_slot(old, SET_ID_LAYOUT_ROWS, -1)->value = parsed(SET_ID_LAYOUT_ROWS, "2");     // [Layout]: 1 -> 2
    SettingSlot *default_menu = settings_slot(old, SET_ID_DEFAULT_MENU, -1);
    snprintf(default_menu->value.text, sizeof(default_menu->value.text), "Games");           // Main -> Games, unsaved

    // Games' page: Menus, then down past All menus and Main
    settings_command(old, SETTINGS_DOWN);
    settings_command(old, SETTINGS_DOWN);
    settings_command(old, SETTINGS_OK);
    settings_command(old, SETTINGS_DOWN);
    settings_command(old, SETTINGS_DOWN);
    settings_command(old, SETTINGS_OK);
    CHECK_INT(settings_preview_menu(old), 1);

    // Kids new at the front, Games renamed Play, Main after it
    static const char *const names[] = { "Kids", "Play", "Main" };
    static const char *const old_names[] = { NULL, "Games", "Main" };
    SettingsState *state = settings_remodel(old, names, old_names, 3);
    CHECK(state != NULL);
    CHECK_INT(settings_slot_count(state), SET_ID_GLOBAL_COUNT + 3 * SET_ID_PER_MENU_COUNT);
    SettingSlot *rows = settings_slot(state, SET_ID_MENU_ROWS, 1);
    CHECK_INT(rows->menu, 1);
    CHECK_INT(rows->value.number, 2);
    CHECK_INT(rows->entry.number, 3);
    CHECK(settings_changed(rows));
    CHECK_INT(settings_slot(state, SET_ID_MENU_COLUMNS, 1)->value.number, 6);
    SettingSlot *columns = settings_slot(state, SET_ID_MENU_COLUMNS, 2);
    CHECK_INT(columns->menu, 2);
    CHECK_INT(columns->value.number, 5);
    CHECK(columns->entry.inherit);
    for (int id = SET_ID_MENU_ROWS; id < SET_ID_COUNT; id++) {
        SettingSlot *slot = settings_slot(state, (SettingId) id, 0);   // Kids: new, following [Layout]
        CHECK(slot->value.inherit && slot->entry.inherit && !settings_changed(slot));
    }
    CHECK_INT(settings_slot(state, SET_ID_LAYOUT_ROWS, -1)->value.number, 2);
    CHECK(settings_changed(settings_slot(state, SET_ID_LAYOUT_ROWS, -1)));
    CHECK_STR(settings_slot(state, SET_ID_DEFAULT_MENU, -1)->value.text, "Play");   // The unsaved Games follows its rename
    CHECK_STR(settings_slot(state, SET_ID_DEFAULT_MENU, -1)->entry.text, "Main");
    CHECK(settings_menus(state) == menus);
    CHECK_STR(settings_pad_name(state, 0), "Xbox Controller");
    CHECK(settings_any_changed(state));

    // The page about Games is Play's now, at Play's place
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MENU);
    CHECK_INT(settings_preview_menu(state), 1);
    char path[256];
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Menus" ARROW "Play");
    SettingsRow shown[SETTINGS_MAX_ROWS];
    settings_rows(state, shown, SETTINGS_MAX_ROWS);
    CHECK_STR(shown[0].value, "2");

    // The old model is as it was: the screen frees it once the new one is made
    CHECK_INT(settings_slot(old, SET_ID_MENU_ROWS, 1)->value.number, 2);
    CHECK_STR(settings_slot(old, SET_ID_DEFAULT_MENU, -1)->value.text, "Games");
    settings_free(old);

    // Play removed while its page is open: the page closes, and the Default menu row, which named it,
    // goes back to its entry; Main's own change stays, at Main's new place
    static const char *const fewer[] = { "Kids", "Main" };
    SettingsState *after = settings_remodel(state, fewer, fewer, 2);
    CHECK(after != NULL);
    CHECK_INT(settings_page(after), SETTINGS_PAGE_MENUS);
    CHECK_STR(settings_slot(after, SET_ID_DEFAULT_MENU, -1)->value.text, "Main");
    CHECK(!settings_changed(settings_slot(after, SET_ID_DEFAULT_MENU, -1)));
    CHECK_INT(settings_slot(after, SET_ID_MENU_COLUMNS, 1)->value.number, 5);
    settings_free(state);

    // The default menu renamed: its value and its entry both follow, so there is nothing to save for it
    static const char *const home[] = { "Kids", "Home" };
    SettingsState *renamed = settings_remodel(after, home, fewer, 2);
    CHECK(renamed != NULL);
    CHECK_STR(settings_slot(renamed, SET_ID_DEFAULT_MENU, -1)->value.text, "Home");
    CHECK_STR(settings_slot(renamed, SET_ID_DEFAULT_MENU, -1)->entry.text, "Home");
    CHECK(!settings_changed(settings_slot(renamed, SET_ID_DEFAULT_MENU, -1)));
    settings_free(after);
    settings_free(renamed);
    menuedit_free(menus);
}

// A function to test that the Menus list's cursor stays on its menu when the menus change: Back from a
// page about a menu that moved lands on that menu's row, so the preview shows the same menu; a cursor
// on the list itself follows its menu by name, through a rename; and one whose menu is gone keeps its
// row, now the nearest menu's
static void test_remodel_menus_cursor(void)
{
    // Games' page: Menus, then down past All menus and Main to Games, row 3
    SettingsState *old = open_model();
    settings_command(old, SETTINGS_DOWN);
    settings_command(old, SETTINGS_DOWN);
    settings_command(old, SETTINGS_OK);
    settings_command(old, SETTINGS_DOWN);
    settings_command(old, SETTINGS_DOWN);
    settings_command(old, SETTINGS_OK);

    // Kids new between Main and Games: Back lands on Games, a row lower, not on Kids
    static const char *const kids[] = { "Main", "Kids", "Games" };
    static const char *const kids_was[] = { "Main", NULL, "Games" };
    SettingsState *state = settings_remodel(old, kids, kids_was, 3);
    CHECK(state != NULL);
    settings_command(state, SETTINGS_BACK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MENUS);
    CHECK_INT(settings_cursor(state), 4);
    CHECK_INT(settings_preview_menu(state), 2);
    settings_free(old);

    // On the list itself, Games renamed Play and moved to the front: the cursor follows it to row 2
    static const char *const play[] = { "Play", "Main", "Kids" };
    static const char *const play_was[] = { "Games", "Main", "Kids" };
    SettingsState *renamed = settings_remodel(state, play, play_was, 3);
    CHECK(renamed != NULL);
    CHECK_INT(settings_cursor(renamed), 2);
    CHECK_INT(settings_preview_menu(renamed), 0);
    settings_free(state);

    // Main highlighted (row 3) and removed: the cursor keeps its row, Kids' now, the menu after it
    settings_command(renamed, SETTINGS_DOWN);
    CHECK_INT(settings_cursor(renamed), 3);
    static const char *const fewer[] = { "Play", "Kids" };
    SettingsState *after = settings_remodel(renamed, fewer, fewer, 2);
    CHECK(after != NULL);
    CHECK_INT(settings_cursor(after), 3);
    CHECK_INT(settings_preview_menu(after), 1);
    settings_free(renamed);
    settings_free(after);
}

#define REMODEL_DIR "settings-remodel-fixture"
#define REMODEL_CONFIG REMODEL_DIR "/config.ini"

// A function to write the menus model into a save's fresh read, as the save's document callback
static bool apply_menus(IniDoc *doc, void *context, ConfigSaveResult *result)
{
    return menuedit_apply((const MenuEdit *) context, doc, result);
}

// A function to save a model's changed settings over the menus model, as the screen's save makes its
// edits: each slot's section is its menu's name, `sections[slot->menu]` (the screen reads the live
// menu's, which menus_apply() names as the menus model does), and a menu's own setting goes under its
// header. True when the save succeeded; `result` says what it could not make as asked.
static bool save_over_menus(SettingsState *state, const char *const *sections, MenuEdit *edit, ConfigSaveResult *result)
{
    ConfigEdit edits[4];
    static char values[4][SETTING_TEXT_MAX];
    int n = 0;
    for (int i = 0; i < settings_slot_count(state); i++) {
        SettingSlot *slot = settings_slot_at(state, i);
        if (!settings_changed(slot))
            continue;
        CHECK(n < 4);
        if (n >= 4)
            break;
        setting_format(slot->def, &slot->value, values[n], SETTING_TEXT_MAX);
        edits[n].section = slot->def->section != NULL ? slot->def->section : sections[slot->menu];
        edits[n].key = slot->def->key;
        edits[n].alias = slot->def->alias;
        edits[n].value = slot->value.inherit ? NULL : values[n];
        edits[n].placement = slot->def->section != NULL ? INIDOC_AFTER_LAST_KEY : INIDOC_UNDER_HEADER;
        n++;
    }
    memset(result, 0, sizeof(*result));
    return config_save_with(REMODEL_CONFIG, NULL, NULL, edits, n, NULL, 0, apply_menus, edit, result);
}

// A function to start the remodel's save from a known file, with no backup lying about
static void reset_remodel_config(const char *text)
{
    fileio_remove(REMODEL_CONFIG);
    fileio_remove(REMODEL_CONFIG ".bak");
    CHECK(fileio_make_dirs(REMODEL_DIR));
    CHECK(fileio_write_all(REMODEL_CONFIG, text, strlen(text)));
}

// A function to test that an unsaved change to a menu's own setting follows the menu through a rename
// made in settings, to the save: the remodel carries it to the renamed menu, the save's edit names the
// menu's new section, and the menus model's save (menuedit_apply(), the document callback, run before
// the slot edits) renames the section first, so the change lands under the new header with no note.
// Named by the old section instead, the edit would be skipped as a menu gone (the reason it must not be).
static void test_remodel_slot_edit_follows_rename(void)
{
    static const char *const text =
        "[General]\nDefaultMenu=Main\n\n[Main]\nEntry1=Games;games;:submenu Games\n\n[Games]\nRows=3\nEntry1=Retro;retro;retroarch\n";
    static const char *const saved =
        "[General]\nDefaultMenu=Main\n\n[Main]\nEntry1=Play;games;:submenu Play\n\n[Play]\nRows=2\nEntry1=Retro;retro;retroarch\n";
    static const char *const names[] = { "Main", "Games" };
    for (int pass = 0; pass < 2; pass++) {
        reset_remodel_config(text);
        IniDoc *doc = inidoc_parse(text, strlen(text));
        MenuEdit *edit = doc != NULL ? menuedit_load(doc, "Main") : NULL;
        inidoc_free(doc);
        SettingsState *old = settings_create(names, 2);
        CHECK(edit != NULL && old != NULL);
        if (edit == NULL || old == NULL) {
            menuedit_free(edit);
            settings_free(old);
            return;
        }
        SettingValue rows = parsed(SET_ID_MENU_ROWS, "3");
        settings_set_entry(old, SET_ID_MENU_ROWS, 1, &rows);
        settings_slot(old, SET_ID_MENU_ROWS, 1)->value = parsed(SET_ID_MENU_ROWS, "2");   // Games: 3 -> 2, unsaved

        // Games renamed Play in the menus model, and the settings model made again over it, as
        // settings_menus_changed() makes it: each menu's name now, and the name it had
        CHECK(menuedit_rename_menu(edit, 1, "Play") == NULL);
        const char *now[2];
        for (int m = 0; m < 2; m++)
            now[m] = menuedit_menu(edit, m)->name;
        CHECK_STR(now[1], "Play");
        SettingsState *state = settings_remodel(old, now, names, 2);
        settings_free(old);
        CHECK(state != NULL);
        if (state == NULL) {
            menuedit_free(edit);
            return;
        }
        CHECK_INT(settings_slot(state, SET_ID_MENU_ROWS, 1)->value.number, 2);

        ConfigSaveResult result;
        char *written = NULL;
        if (pass == 0) {
            CHECK(save_over_menus(state, now, edit, &result));
            CHECK_INT(result.note_count, 0);
            CHECK_STR(result.notes, "");
            written = fileio_read_all(REMODEL_CONFIG, NULL);
            CHECK(written != NULL);
            CHECK_STR(written != NULL ? written : "", saved);
        }
        else {
            // The old section's name: the change is skipped, so Play keeps its Rows=3, and the note says
            // the menu is gone
            CHECK(save_over_menus(state, names, edit, &result));
            CHECK_INT(result.note_count, 1);
            CHECK_INT(result.note_kinds[0], CONFIG_NOTE_SECTION_GONE);
            written = fileio_read_all(REMODEL_CONFIG, NULL);
            CHECK(written != NULL && strstr(written, "\n[Play]\nRows=3\nEntry1=Retro;retro;retroarch\n") != NULL);
        }
        alloc_free(written);
        settings_free(state);
        menuedit_free(edit);
    }
    fileio_remove(REMODEL_CONFIG);
    fileio_remove(REMODEL_CONFIG ".bak");
}

// A function to test the save after a menu is removed or renamed with unsaved changes pending, Games'
// Rows (3 -> 2) and the Default menu row: a removed menu's own settings go with it (settings_remodel()
// makes no slots for it), so the save writes nothing for it and notes nothing, and a Default menu row
// naming it goes back to its entry; a Default menu row naming a renamed menu, or the file's DefaultMenu
// naming one (which the menus model's save rewrites), names the new name. Every save names a menu that
// exists, with no note.
static void test_remodel_save_after_remove_and_rename(void)
{
    static const char *const text =
        "[General]\nDefaultMenu=Main\n\n[Main]\nEntry1=Games;games;:submenu Games\n\n[Games]\nRows=3\nEntry1=Retro;retro;retroarch\n";
    static const struct {
        const char *what;            // "remove Games", "rename Games", "rename Main"
        const char *pending_default; // The Default menu row's unsaved value; NULL: unchanged
        const char *saved;
    } cases[] = {
        { "remove Games", "Games", "[General]\nDefaultMenu=Main\n\n[Main]\n" },
        { "rename Games", "Games",
          "[General]\nDefaultMenu=Play\n\n[Main]\nEntry1=Play;games;:submenu Play\n\n[Play]\nRows=2\nEntry1=Retro;retro;retroarch\n" },
        { "rename Main", NULL,
          "[General]\nDefaultMenu=Home\n\n[Home]\nEntry1=Games;games;:submenu Games\n\n[Games]\nRows=2\nEntry1=Retro;retro;retroarch\n" },
    };
    static const char *const names[] = { "Main", "Games" };
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        reset_remodel_config(text);
        IniDoc *doc = inidoc_parse(text, strlen(text));
        MenuEdit *edit = doc != NULL ? menuedit_load(doc, "Main") : NULL;
        inidoc_free(doc);
        SettingsState *old = settings_create(names, 2);
        CHECK(edit != NULL && old != NULL);
        if (edit == NULL || old == NULL) {
            menuedit_free(edit);
            settings_free(old);
            return;
        }
        SettingValue rows = parsed(SET_ID_MENU_ROWS, "3");
        settings_set_entry(old, SET_ID_MENU_ROWS, 1, &rows);
        settings_slot(old, SET_ID_MENU_ROWS, 1)->value = parsed(SET_ID_MENU_ROWS, "2");   // Games: 3 -> 2, unsaved
        entry(old, SET_ID_DEFAULT_MENU, "Main");
        if (cases[c].pending_default != NULL) {
            SettingSlot *slot = settings_slot(old, SET_ID_DEFAULT_MENU, -1);
            snprintf(slot->value.text, sizeof(slot->value.text), "%s", cases[c].pending_default);
        }

        // The change to the menus model, then the settings model made again over it, as
        // settings_menus_changed() makes it: each menu's name now, and the name it had
        const char *why = strcmp(cases[c].what, "remove Games") == 0 ? menuedit_remove_menu(edit, 0, 0, true)
                        : strcmp(cases[c].what, "rename Games") == 0 ? menuedit_rename_menu(edit, 1, "Play")
                        : menuedit_rename_menu(edit, 0, "Home");
        CHECK(why == NULL);
        int count = menuedit_count(edit);
        const char *now[2] = { NULL, NULL };
        const char *was[2] = { NULL, NULL };
        for (int m = 0; m < count && m < 2; m++) {
            const MenuEditMenu *menu = menuedit_menu(edit, m);
            now[m] = menu->name;
            was[m] = menu->original_name;
        }
        SettingsState *state = settings_remodel(old, now, was, count);
        settings_free(old);
        CHECK(state != NULL);
        if (state == NULL) {
            menuedit_free(edit);
            return;
        }
        CHECK_INT(settings_slot_count(state), SET_ID_GLOBAL_COUNT + count * SET_ID_PER_MENU_COUNT);
        if (strcmp(cases[c].what, "remove Games") == 0) {
            CHECK(!settings_any_changed(state));   // Games' Rows went with Games; the Default menu row went back
            CHECK_STR(settings_slot(state, SET_ID_DEFAULT_MENU, -1)->value.text, "Main");
        }

        ConfigSaveResult result;
        CHECK(save_over_menus(state, now, edit, &result));
        CHECK_INT(result.note_count, 0);
        CHECK_STR(result.notes, "");
        char *written = fileio_read_all(REMODEL_CONFIG, NULL);
        CHECK_STR(written != NULL ? written : "", cases[c].saved);
        alloc_free(written);
        settings_free(state);
        menuedit_free(edit);
    }
    fileio_remove(REMODEL_CONFIG);
    fileio_remove(REMODEL_CONFIG ".bak");
}

// A function to test the keyboard's layout setting (3c): QWERTY or ABC, as the file and the screen name
// them, a choice the General page steps
static void test_keyboard_layout(void)
{
    SettingValue value = parsed(SET_ID_KEYBOARD_LAYOUT, "ABC");
    CHECK_INT(value.number, 1);
    CHECK_STR(formatted(SET_ID_KEYBOARD_LAYOUT, &value), "ABC");
    value = parsed(SET_ID_KEYBOARD_LAYOUT, "QWERTY");
    CHECK_INT(value.number, 0);
    CHECK_STR(described(SET_ID_KEYBOARD_LAYOUT, &value, NULL), "QWERTY");
    SettingValue stepped_to = setting_step(setting_def(SET_ID_KEYBOARD_LAYOUT), &value, &value, 1);
    CHECK_STR(formatted(SET_ID_KEYBOARD_LAYOUT, &stepped_to), "ABC");
    CHECK(!setting_parse(setting_def(SET_ID_KEYBOARD_LAYOUT), "abc", &value));   // Names match exactly, as the parser's
    CHECK(setting_find("General", "KeyboardLayout") == setting_def(SET_ID_KEYBOARD_LAYOUT));

    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    entry(state, SET_ID_KEYBOARD_LAYOUT, "QWERTY");
    open_page(state, "General");
    const SettingsRow *row = row_labeled(state, rows, "Keyboard layout");
    CHECK_INT(row->kind, SETTINGS_ROW_SETTING);
    CHECK_STR(row->value, "QWERTY");
    cursor_to(state, "Keyboard layout");
    CHECK_INT(settings_command(state, SETTINGS_RIGHT).kind, SETTINGS_EVENT_CHANGED);
    CHECK_STR(row_labeled(state, rows, "Keyboard layout")->value, "ABC");
    settings_free(state);
}

// The menus the Tiles pages' tests start from: Main, with a submenu tile and a plain one, and Games
static const char *const TILES_TEXT =
    "[General]\nDefaultMenu=Main\n\n[Main]\nEntry1=Games;games;:submenu Games\nEntry2=Kodi;kodi;kodi --standalone\n\n"
    "[Games]\nRows=3\nEntry1=Retro;emulators;:back\nEntry2=Pong;games;:back\n";

// A function to give a model the menus model of a config's text, with its default menu; the caller frees
// it after the model
static MenuEdit *with_menus(SettingsState *state, const char *text, const char *default_menu)
{
    IniDoc *doc = inidoc_parse(text, strlen(text));
    MenuEdit *edit = menuedit_load(doc, default_menu);
    inidoc_free(doc);
    CHECK(edit != NULL);
    settings_set_menus(state, edit);
    return edit;
}

// A function to open a menu's Tiles page from the top page: Menus, the menu, Tiles
static void open_tiles(SettingsState *state, const char *menu)
{
    open_page(state, "Menus");
    cursor_to(state, menu);
    settings_command(state, SETTINGS_OK);
    cursor_to(state, "Tiles");
    settings_command(state, SETTINGS_OK);
}

// A function to get a tile of the menus model; NULL when there is none
static const MenuTile *tile_of(const MenuEdit *edit, const char *menu, int t)
{
    const MenuEditMenu *m = menuedit_menu(edit, menuedit_find(edit, menu));
    return m != NULL && t >= 0 && t < m->count ? &m->tiles[t] : NULL;
}

// A function to test the Tiles pages (3c): the Menu page's Tiles row; the Tiles page's rows, a submenu tile a
// link; the tile the preview highlights, the one under the cursor; the Tiles page's Add, at the end, through
// the Special page; a tile's page, its title and its removal
static void test_tiles_pages(void)
{
    SettingsState *state = open_model();
    MenuEdit *edit = with_menus(state, TILES_TEXT, "Main");
    SettingsRow rows[SETTINGS_MAX_ROWS];
    char path[256];
    char label[64];
    open_page(state, "Menus");
    cursor_to(state, "Main");
    settings_command(state, SETTINGS_OK);
    const SettingsRow *tiles = row_labeled(state, rows, "Tiles");
    CHECK_INT(tiles->kind, SETTINGS_ROW_LINK);
    CHECK_STR(tiles->value, "2 tiles");
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 5);   // Rows, Columns, Largest button, Tiles, the note
    CHECK_INT(rows[4].kind, SETTINGS_ROW_NOTE);
    cursor_to(state, "Tiles");
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TILES);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Menus" ARROW "Main" ARROW "Tiles");
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 3);
    CHECK_STR(rows[0].label, "Add");
    CHECK_INT(rows[0].kind, SETTINGS_ROW_LINK);
    CHECK_STR(rows[1].label, "Games");
    CHECK_INT(rows[1].kind, SETTINGS_ROW_LINK);
    CHECK_STR(rows[1].value, "2 tiles");
    CHECK_STR(rows[2].label, "Kodi");
    CHECK_INT(rows[2].kind, SETTINGS_ROW_ACTION);
    setting_command_label("kodi --standalone", label, sizeof(label));
    CHECK_STR(rows[2].value, label);

    // The preview shows Main, the tile under the cursor highlighted; Add is about none
    CHECK_INT(settings_preview_menu(state), 0);
    CHECK_INT(settings_preview_tile(state), -1);
    settings_command(state, SETTINGS_DOWN);
    CHECK_INT(settings_preview_tile(state), 0);
    settings_command(state, SETTINGS_UP);
    CHECK_INT(settings_preview_tile(state), -1);

    // Add, Special, Sleep: the Tiles page's Add puts it at the end, wherever its cursor has been, with the
    // special's title, icon and command
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_ADD);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Menus" ARROW "Main" ARROW "Tiles" ARROW "Add");
    cursor_to(state, "Special");
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_SPECIAL);
    static const char *const specials[] = { "Home", "Back", "Settings", "Sleep", "Restart", "Shut down", "Quit" };
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 7);
    for (int i = 0; i < 7; i++)
        CHECK_STR(rows[i].label, specials[i]);
    cursor_to(state, "Sleep");
    SettingsEvent event = settings_command(state, SETTINGS_OK);
    CHECK_INT(event.kind, SETTINGS_EVENT_MENUS);
    CHECK_INT(event.menu, 0);
    CHECK_INT(event.tile, 2);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TILES);
    const MenuTile *sleep = tile_of(edit, "Main", 2);
    CHECK(sleep != NULL && sleep->key == NULL);
    CHECK_STR(sleep->title, "Sleep");
    CHECK_STR(sleep->icon, "sleep");
    CHECK_STR(sleep->command, ":sleep");
    CHECK_STR(tile_of(edit, "Main", 1)->title, "Kodi");
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[settings_cursor(state)].label, "Sleep");
    CHECK_INT(settings_preview_tile(state), 2);
    CHECK(settings_menus_edited(state));
    CHECK(!settings_any_changed(state));                           // The slots and bindings alone

    // The tile's page: its title through the keyboard's event, and Remove, back on the tile now in its place
    // (the last, Kodi)
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TILE);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Menus" ARROW "Main" ARROW "Tiles" ARROW "Sleep");
    CHECK_STR(row_labeled(state, rows, "Title")->value, "Sleep");
    CHECK_INT(row_labeled(state, rows, "Add after this tile")->kind, SETTINGS_ROW_LINK);
    cursor_to(state, "Title");
    event = settings_command(state, SETTINGS_OK);
    CHECK_INT(event.kind, SETTINGS_EVENT_TYPE_TITLE);
    CHECK_INT(event.menu, 0);
    CHECK_INT(event.tile, 2);
    CHECK_STR(settings_title_problem(state, 0, 2, "a;b"), "A title can't contain ;");
    CHECK(settings_title_problem(state, 0, 2, "Nap") == NULL);
    event = settings_set_title(state, 0, 2, "Nap");
    CHECK_INT(event.kind, SETTINGS_EVENT_MENUS);
    CHECK(!event.rebind);
    CHECK_STR(tile_of(edit, "Main", 2)->title, "Nap");
    event = settings_set_title(state, 0, 2, "");
    CHECK_INT(event.kind, SETTINGS_EVENT_MOVED);
    CHECK_STR(settings_notice(state), "Type a title");
    cursor_to(state, "Remove");
    event = settings_command(state, SETTINGS_OK);
    CHECK_INT(event.kind, SETTINGS_EVENT_MENUS);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TILES);
    CHECK(tile_of(edit, "Main", 2) == NULL);
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[settings_cursor(state)].label, "Kodi");
    CHECK(!settings_menus_edited(state));                          // Back as it was read

    // The top page's Discard is enabled by a menus change alone
    settings_command(state, SETTINGS_OK);
    cursor_to(state, "Remove");
    settings_command(state, SETTINGS_OK);
    for (int i = 0; i < 3; i++)
        settings_command(state, SETTINGS_BACK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TOP);
    CHECK(row_labeled(state, rows, "Discard changes")->enabled);
    settings_free(state);
    menuedit_free(edit);
}

// A function to test an empty menu's Tiles page, and a new submenu through the keyboard's events: its name
// checked, made where the Add page puts a tile (in an empty menu, the first), with the folder icon
static void test_empty_menu_and_new_submenu(void)
{
    SettingsState *state = open_model();
    MenuEdit *edit = with_menus(state, "[Main]\nEntry1=Kodi;kodi;kodi\n\n[Games]\n", "Main");
    SettingsRow rows[SETTINGS_MAX_ROWS];
    open_tiles(state, "Games");
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 1);
    CHECK_STR(rows[0].label, "Empty: Add");
    CHECK_INT(rows[0].kind, SETTINGS_ROW_LINK);
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_ADD);
    cursor_to(state, "Submenu");
    SettingsEvent event = settings_command(state, SETTINGS_OK);
    CHECK_INT(event.kind, SETTINGS_EVENT_TYPE_NAME);
    CHECK_STR(settings_name_problem(state, "Main"), "There's already a menu with that name");
    CHECK_STR(settings_name_problem(state, "main"), "There's already a menu with that name");
    CHECK_STR(settings_name_problem(state, "General"), "That name is kept for settings");
    CHECK_STR(settings_name_problem(state, "a]b"), "A name can't contain ]");
    CHECK(settings_name_problem(state, "Kids") == NULL);
    event = settings_add_submenu(state, "Kids");
    CHECK_INT(event.kind, SETTINGS_EVENT_MENUS);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TILES);
    CHECK_INT(menuedit_count(edit), 3);
    const MenuTile *kids = tile_of(edit, "Games", 0);
    CHECK(kids != NULL);
    CHECK_STR(kids->title, "Kids");
    CHECK_STR(kids->icon, "folder");
    CHECK_STR(kids->command, ":submenu Kids");
    event = settings_add_submenu(state, "Kids");                    // Refused: the Add page is gone, and so is the name
    CHECK_INT(event.kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(menuedit_count(edit), 3);
    settings_free(state);
    menuedit_free(edit);
}

// A function to test Add after this tile (user ruling 2026-10-16, Fable review C3): a tile's page offers it,
// and the new tile goes right after that tile, here the third of four, back on the Tiles page on it; the
// Tiles page's own Add still puts one at the end, wherever its cursor has been
static void test_add_after_tile(void)
{
    static const char *const text = "[Main]\nEntry1=A;apps;:home\nEntry2=B;apps;:home\nEntry3=C;apps;:home\n"
                                    "Entry4=D;apps;:home\n";
    SettingsState *state = open_model();
    MenuEdit *edit = with_menus(state, text, "Main");
    SettingsRow rows[SETTINGS_MAX_ROWS];
    char path[256];
    open_tiles(state, "Main");
    cursor_to(state, "C");
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TILE);
    cursor_to(state, "Add after this tile");
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_ADD);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Menus" ARROW "Main" ARROW "Tiles" ARROW "C" ARROW "Add");
    CHECK_INT(settings_preview_tile(state), 2);                     // The preview highlights the tile it goes after
    cursor_to(state, "Special");
    settings_command(state, SETTINGS_OK);
    cursor_to(state, "Home");
    SettingsEvent event = settings_command(state, SETTINGS_OK);
    CHECK_INT(event.kind, SETTINGS_EVENT_MENUS);
    CHECK_INT(event.tile, 3);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TILES);           // Under the tile's page, on the new tile
    CHECK_STR(tile_of(edit, "Main", 2)->title, "C");
    CHECK_STR(tile_of(edit, "Main", 3)->title, "Home");
    CHECK_STR(tile_of(edit, "Main", 4)->title, "D");
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[settings_cursor(state)].label, "Home");

    // The Tiles page's Add, its cursor having passed B on the way up: at the end all the same
    cursor_to(state, "B");
    cursor_to(state, "Add");
    settings_command(state, SETTINGS_OK);
    cursor_to(state, "Special");
    settings_command(state, SETTINGS_OK);
    cursor_to(state, "Back");
    event = settings_command(state, SETTINGS_OK);
    CHECK_INT(event.tile, 5);
    CHECK_STR(tile_of(edit, "Main", 5)->title, "Back");
    CHECK_STR(tile_of(edit, "Main", 4)->title, "D");
    settings_free(state);
    menuedit_free(edit);
}

// The menus of the tests that weigh a tile in its own line: Main's one tile has a comment after it, 152 bytes
// in all, its comment 130 (Fable review M2)
static char commented_text[256];

// A function to make commented_text, and give a 60-byte value that fits a line of config.ini alone but not
// that tile's line with its comment
static const char *commented_menus(char long_value[61])
{
    snprintf(commented_text, sizeof(commented_text), "[Main]\nEntry1=A;apps;:home ; ");
    size_t used = strlen(commented_text);
    memset(commented_text + used, 'c', 130);
    snprintf(commented_text + used + 130, sizeof(commented_text) - used - 130, "\n");
    memset(long_value, 'a', 60);
    long_value[60] = '\0';
    return commented_text;
}

// A function to test that a tile's title is weighed in the tile's own line as config.ini has it (Task 3's
// menuedit_check_set(), Fable review M2): the comment after the line leaves less room, so Done is grayed for
// a title menuedit_check_tile() alone would pass, and the save never refuses it
static void test_title_weighed_in_its_line(void)
{
    char long_value[61];
    SettingsState *state = open_model();
    MenuEdit *edit = with_menus(state, commented_menus(long_value), "Main");
    CHECK(menuedit_check_tile(long_value, "apps", ":home") == NULL);   // Entry99999= + 60 + ;apps;:home: 82
    CHECK_STR(settings_title_problem(state, 0, 0, long_value),         // In its line: 7 + 60 + 11 + 3 + 130 = 211
              "It's too long for its line of config.ini with the comment there (199 bytes)");
    CHECK(settings_title_problem(state, 0, 0, "Nap") == NULL);
    settings_free(state);
    menuedit_free(edit);
}

// A function to test a submenu tile's Remove: the confirm page's two rows, the second only while another
// tile opens the menu; Remove only this tile; Remove the menu with its tiles, and every tile that opens it;
// and the default menu, which cannot be removed
static void test_remove_submenu(void)
{
    static const char *const text = "[Main]\nEntry1=Games;games;:submenu Games\nEntry2=Play;games;:submenu Games\n"
                                     "Entry3=Kodi;kodi;kodi\n\n[Games]\nEntry1=Retro;emulators;:back\nEntry2=Pong;games;:back\n";
    SettingsState *state = open_model();
    MenuEdit *edit = with_menus(state, text, "Main");
    SettingsRow rows[SETTINGS_MAX_ROWS];
    char path[256];
    open_tiles(state, "Main");
    cursor_to(state, "Games");
    settings_command(state, SETTINGS_OK);
    cursor_to(state, "Remove");
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_REMOVE_MENU);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Menus" ARROW "Main" ARROW "Tiles" ARROW "Games" ARROW "Remove?");
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 2);
    CHECK_STR(rows[0].label, "Remove Games and its 2 tiles");
    CHECK(rows[0].enabled);
    CHECK_STR(rows[1].label, "Remove only this tile");
    cursor_to(state, "Remove only this tile");
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MENUS);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TILES);
    CHECK_STR(tile_of(edit, "Main", 0)->title, "Play");
    CHECK_INT(menuedit_find(edit, "Games"), 1);

    // Play is the only opener now: one row, and the menu goes with its tiles
    cursor_to(state, "Play");
    settings_command(state, SETTINGS_OK);
    cursor_to(state, "Remove");
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 1);
    CHECK_STR(rows[0].label, "Remove Games and its 2 tiles");
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MENUS);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TILES);
    CHECK_INT(menuedit_find(edit, "Games"), -1);
    CHECK_STR(tile_of(edit, "Main", 0)->title, "Kodi");
    settings_free(state);
    menuedit_free(edit);

    // The default menu's tile: the row is grayed with its reason, and OK on it changes nothing
    state = open_model();
    edit = with_menus(state, text, "Games");
    open_tiles(state, "Main");
    cursor_to(state, "Games");
    settings_command(state, SETTINGS_OK);
    cursor_to(state, "Remove");
    settings_command(state, SETTINGS_OK);
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK(!rows[0].enabled);
    CHECK_STR(rows[0].why, "The default menu can't be removed");
    CHECK_INT(settings_cursor(state), 0);
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_NONE);
    CHECK_INT(menuedit_find(edit, "Games"), 1);
    settings_free(state);
    menuedit_free(edit);
}

// A function to test a submenu tile's title, which renames its menu (spec "Renaming a submenu tile renames
// the submenu"): the name rules, the tile's own title following, and every binding and Startup or Quit
// command that holds exactly ":submenu <old>" rewritten (Known integration work 1), the rest left
static void test_rename_submenu(void)
{
    static const char *const text = "[Main]\nEntry1=Fun;games;:submenu Games\nEntry2=Kodi;kodi;kodi\n\n"
                                     "[Games]\nEntry1=Retro;emulators;:back\n";
    SettingsState *state = open_model();
    MenuEdit *edit = with_menus(state, text, "Main");
    Bindings *b = with_lines(state, "Hotkey1=#4000003A;:submenu Games\nHotkey2=#4000003B;:submenu Gamesx",
                             "ButtonX=:submenu Games", false);
    entry(state, SET_ID_STARTUP_CMD, ":submenu Games");
    entry(state, SET_ID_QUIT_CMD, ":submenu Gamesx");
    CHECK_STR(settings_title_problem(state, 0, 0, "Main"), "There's already a menu with that name");
    CHECK_STR(settings_title_problem(state, 0, 0, "a]b"), "A name can't contain ]");
    CHECK(settings_title_problem(state, 0, 0, "Play") == NULL);

    SettingsEvent refused = settings_set_title(state, 0, 0, "Main");
    CHECK_INT(refused.kind, SETTINGS_EVENT_MOVED);
    CHECK_STR(settings_notice(state), "There's already a menu with that name");
    CHECK_INT(menuedit_find(edit, "Games"), 1);

    SettingsEvent event = settings_set_title(state, 0, 0, "Play");
    CHECK_INT(event.kind, SETTINGS_EVENT_MENUS);
    CHECK(event.rebind);
    CHECK_INT(menuedit_find(edit, "Play"), 1);
    CHECK_STR(tile_of(edit, "Main", 0)->title, "Play");             // Its own title ("Fun") follows too
    CHECK_STR(tile_of(edit, "Main", 0)->command, ":submenu Play");
    CHECK_STR(bindings_at(b, BINDINGS_KEYBOARD, 0)->command, ":submenu Play");
    CHECK_STR(bindings_at(b, BINDINGS_KEYBOARD, 1)->command, ":submenu Gamesx");
    CHECK_STR(bindings_at(b, BINDINGS_GAMEPAD, 0)->command, ":submenu Play");
    CHECK_STR(settings_slot(state, SET_ID_STARTUP_CMD, -1)->value.text, ":submenu Play");
    CHECK_STR(settings_slot(state, SET_ID_QUIT_CMD, -1)->value.text, ":submenu Gamesx");
    CHECK(settings_changed(settings_slot(state, SET_ID_STARTUP_CMD, -1)));
    CHECK(bindings_changed(b));
    settings_free(state);
    bindings_free(b);
    menuedit_free(edit);

    // A tile inside the menu it opens (written by hand: Move to menu forbids making one): the rename leaves
    // the settings' name for the menu the old one, so the tile is found again by the menus model's index
    // (Fable review I3), and takes the new name as its title
    state = open_model();
    edit = with_menus(state, "[Main]\nEntry1=Kodi;kodi;kodi\n\n[Games]\nEntry1=Loop;games;:submenu Games\n", "Main");
    event = settings_set_title(state, 1, 0, "Play");
    CHECK_INT(event.kind, SETTINGS_EVENT_MENUS);
    CHECK_STR(tile_of(edit, "Play", 0)->command, ":submenu Play");
    CHECK_STR(tile_of(edit, "Play", 0)->title, "Play");
    settings_free(state);
    menuedit_free(edit);
}

// A function to test that a page about a tile the model no longer has shows no rows, and that the Tiles
// row is absent while no menus model was read
static void test_tiles_without_menus(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    open_page(state, "Menus");
    cursor_to(state, "Main");
    settings_command(state, SETTINGS_OK);
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    for (int i = 0; i < count; i++)
        CHECK(strcmp(rows[i].label, "Tiles") != 0);
    CHECK_INT(settings_preview_tile(state), -1);
    CHECK(!settings_menus_edited(state));
    settings_free(state);
}

// A function to test the Add page's rows and a tile's Command (3c, Task 10b): App and Browse for a program
// first, each asking the screen for its picker; a tile added as the app list adds one; the Command row,
// grayed on a submenu tile; a command set, and ones a tile cannot run refused with their reasons
static void test_tile_commands(void)
{
    SettingsState *state = open_model();
    MenuEdit *edit = with_menus(state, TILES_TEXT, "Main");
    SettingsRow rows[SETTINGS_MAX_ROWS];
    char label[64];
    open_tiles(state, "Main");
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_ADD);
    static const char *const adds[] = { "App", "Browse for a program", "Submenu", "Special" };
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 4);
    for (int i = 0; i < 4; i++)
        CHECK_STR(rows[i].label, adds[i]);
    SettingsEvent event = settings_command(state, SETTINGS_OK);
    CHECK_INT(event.kind, SETTINGS_EVENT_PICK_APP);
    cursor_to(state, "Browse for a program");
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_BROWSE_PROGRAM);
    event = settings_add_tile(state, "Kodi Box", "kodi", "/usr/share/applications/kodi.desktop");
    CHECK_INT(event.kind, SETTINGS_EVENT_MENUS);
    CHECK_INT(event.tile, 2);
    CHECK_STR(tile_of(edit, "Main", 2)->command, "/usr/share/applications/kodi.desktop");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TILES);

    cursor_to(state, "Games");
    settings_command(state, SETTINGS_OK);
    const SettingsRow *command = row_labeled(state, rows, "Command");
    CHECK(!command->enabled);
    CHECK_STR(command->why, "A submenu tile's command is its menu's name: change it with Title");
    cursor_to(state, "Command");
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_NONE);
    settings_command(state, SETTINGS_BACK);

    cursor_to(state, "Kodi");
    settings_command(state, SETTINGS_OK);
    setting_command_label("kodi --standalone", label, sizeof(label));
    CHECK_STR(row_labeled(state, rows, "Command")->value, label);
    cursor_to(state, "Command");
    event = settings_command(state, SETTINGS_OK);
    CHECK_INT(event.kind, SETTINGS_EVENT_PICK_TILE_COMMAND);
    CHECK_INT(event.menu, 0);
    CHECK_INT(event.tile, 1);
    CHECK_STR(settings_command_problem(state, 0, 1, ":select"), "A tile can't run :select");
    CHECK_STR(settings_command_problem(state, 0, 1, "a ;b"), "A command can't have ; after a space");
    CHECK(settings_command_problem(state, 0, 1, ":fork kodi") == NULL);
    event = settings_set_command(state, 0, 1, ":fork kodi");
    CHECK_INT(event.kind, SETTINGS_EVENT_MENUS);
    CHECK_STR(tile_of(edit, "Main", 1)->command, ":fork kodi");
    CHECK_STR(tile_of(edit, "Main", 1)->title, "Kodi");
    event = settings_set_command(state, 0, 1, "");
    CHECK_INT(event.kind, SETTINGS_EVENT_MOVED);
    CHECK_STR(settings_notice(state), "Choose a command");
    settings_free(state);
    menuedit_free(edit);

    // A command is weighed in the tile's own line, its comment counted (Task 10a's commented_menus(), Fable
    // review M2): 7 + 1 + 6 + 60 + 3 + 130 = 207 bytes there, though 78 alone
    char long_value[61];
    state = open_model();
    edit = with_menus(state, commented_menus(long_value), "Main");
    CHECK(menuedit_check_tile("A", "apps", long_value) == NULL);
    CHECK_STR(settings_command_problem(state, 0, 0, long_value),
              "It's too long for its line of config.ini with the comment there (199 bytes)");
    CHECK(settings_command_problem(state, 0, 0, ":quit") == NULL);
    settings_free(state);
    menuedit_free(edit);
}

// A function to test an app's or a file's name made a tile's title: ';' a comma, control characters spaces,
// the edges trimmed of both (a leading tab would be a leading space, which config.ini's line refuses), cut
// on a character's edge to the keyboard's limit; nothing left is "App"
static void test_tile_title(void)
{
    char title[MENUEDIT_TITLE_MAX + 1];
    settings_tile_title("  Foo; Bar  ", title, sizeof(title));
    CHECK_STR(title, "Foo, Bar");
    settings_tile_title("Line\tbreak\n", title, sizeof(title));
    CHECK_STR(title, "Line break");
    settings_tile_title("\t \x7F" "Tab first", title, sizeof(title));
    CHECK_STR(title, "Tab first");
    CHECK(menuedit_check_tile(title, "apps", ":home") == NULL);
    char name[200];
    memset(name, 'a', 70);
    name[70] = '\0';
    settings_tile_title(name, title, sizeof(title));
    CHECK_INT((int) strlen(title), MENUEDIT_TITLE_MAX);
    name[0] = 'a';
    for (int i = 0; i < 40; i++)
        memcpy(name + 1 + 2 * i, "\xC3\xA9", 2);                    // e acute, two bytes each
    name[81] = '\0';
    settings_tile_title(name, title, sizeof(title));
    CHECK_INT((int) strlen(title), 63);                              // 64 would cut the 32nd in half
    settings_tile_title("", title, sizeof(title));
    CHECK_STR(title, "App");
    settings_tile_title(" ; ", title, sizeof(title));
    CHECK_STR(title, ",");
}

// A function to test where a picked-up tile goes for each arrow (spec "Move"), by the launcher's own rule
// (layout_move()): in a grid Left and Right within the row, stopping at its ends, Up and Down a row keeping
// the column, onto a shorter last row's last tile, stopping at the first and last rows; a strip's Left and
// Right along it, its Up and Down nothing. Five tiles three wide: [0 1 2] [3 4].
static void test_move_target(void)
{
    CHECK_INT(settings_move_target(0, 5, 2, 3, SETTINGS_RIGHT), 1);
    CHECK_INT(settings_move_target(0, 5, 2, 3, SETTINGS_LEFT), 0);
    CHECK_INT(settings_move_target(2, 5, 2, 3, SETTINGS_RIGHT), 2);   // The row's end: it stays
    CHECK_INT(settings_move_target(3, 5, 2, 3, SETTINGS_LEFT), 3);    // The row's start: it stays
    CHECK_INT(settings_move_target(4, 5, 2, 3, SETTINGS_RIGHT), 4);
    CHECK_INT(settings_move_target(1, 5, 2, 3, SETTINGS_DOWN), 4);
    CHECK_INT(settings_move_target(2, 5, 2, 3, SETTINGS_DOWN), 4);    // A shorter last row: its last tile
    CHECK_INT(settings_move_target(3, 5, 2, 3, SETTINGS_DOWN), 3);    // The last row: it stays
    CHECK_INT(settings_move_target(4, 5, 2, 3, SETTINGS_UP), 1);
    CHECK_INT(settings_move_target(1, 5, 2, 3, SETTINGS_UP), 1);      // The first row: it stays
    CHECK_INT(settings_move_target(2, 5, 1, 3, SETTINGS_RIGHT), 3);   // A strip scrolls on: no row's end
    CHECK_INT(settings_move_target(4, 5, 1, 3, SETTINGS_RIGHT), 4);
    CHECK_INT(settings_move_target(2, 5, 1, 3, SETTINGS_DOWN), 2);    // A strip's Up and Down do nothing
    CHECK_INT(settings_move_target(2, 5, 1, 3, SETTINGS_UP), 2);
    CHECK_INT(settings_move_target(0, 0, 2, 3, SETTINGS_RIGHT), 0);   // No tiles
}

// A function to test Move (spec "Move"): OK picks the tile up; the arrows move it in the menus model, the
// pages and the preview following it; OK drops it; Back puts it back where it was picked up; Home drops it
// where it is and closes
static void test_move_tile(void)
{
    static const char *const text = "[Main]\nEntry1=A;apps;:home\nEntry2=B;apps;:home\nEntry3=C;apps;:home\n"
                                    "Entry4=D;apps;:home\nEntry5=E;apps;:home\n\n[Games]\nEntry1=Retro;emulators;:back\n";
    SettingsState *state = open_model();
    MenuEdit *edit = with_menus(state, text, "Main");
    SettingsRow rows[SETTINGS_MAX_ROWS];
    char path[256];
    settings_set_grid(state, 2, 3);
    open_tiles(state, "Main");
    cursor_to(state, "A");
    settings_command(state, SETTINGS_OK);
    CHECK_INT(row_labeled(state, rows, "Move")->kind, SETTINGS_ROW_ACTION);
    CHECK_INT(row_labeled(state, rows, "Move to menu")->kind, SETTINGS_ROW_LINK);
    cursor_to(state, "Move");
    CHECK_INT(settings_moving_tile(state), -1);
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_moving_tile(state), 0);
    SettingsEvent event = settings_command(state, SETTINGS_RIGHT);
    CHECK_INT(event.kind, SETTINGS_EVENT_MENUS);
    CHECK_INT(event.tile, 1);
    CHECK_STR(tile_of(edit, "Main", 1)->title, "A");
    event = settings_command(state, SETTINGS_DOWN);
    CHECK_INT(event.tile, 4);
    CHECK_STR(tile_of(edit, "Main", 4)->title, "A");
    CHECK_INT(settings_moving_tile(state), 4);
    CHECK_INT(settings_preview_tile(state), 4);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Menus" ARROW "Main" ARROW "Tiles" ARROW "A");   // The page follows its tile
    CHECK_INT(settings_command(state, SETTINGS_DOWN).kind, SETTINGS_EVENT_NONE);       // The last already
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_moving_tile(state), -1);
    CHECK_STR(tile_of(edit, "Main", 4)->title, "A");
    CHECK(settings_menus_edited(state));
    settings_command(state, SETTINGS_BACK);                       // The Tiles page, on A where it was dropped
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[settings_cursor(state)].label, "A");

    // Picked up again, moved, then Back: back where it was picked up, the mode over, the page as it was
    settings_command(state, SETTINGS_OK);
    cursor_to(state, "Move");
    settings_command(state, SETTINGS_OK);
    settings_command(state, SETTINGS_LEFT);
    settings_command(state, SETTINGS_UP);
    CHECK_STR(tile_of(edit, "Main", 0)->title, "A");
    event = settings_command(state, SETTINGS_BACK);
    CHECK_INT(event.kind, SETTINGS_EVENT_MENUS);
    CHECK_INT(settings_moving_tile(state), -1);
    CHECK_STR(tile_of(edit, "Main", 4)->title, "A");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TILE);

    // Home while it is up drops it where it is, then closes
    settings_command(state, SETTINGS_OK);
    settings_command(state, SETTINGS_LEFT);
    CHECK_INT(settings_command(state, SETTINGS_HOME).kind, SETTINGS_EVENT_CLOSE_HOME);
    CHECK_INT(settings_moving_tile(state), -1);
    CHECK_STR(tile_of(edit, "Main", 3)->title, "A");
    settings_free(state);
    menuedit_free(edit);
}

// A function to test Move to menu: every other menu, but for a submenu tile the menu it opens and every menu
// inside that one, grayed with the reasons; a tile moved to the end of the menu chosen
static void test_move_to_menu(void)
{
    static const char *const names[] = { "Main", "Games", "Arcade" };
    static const char *const text = "[Main]\nEntry1=Kodi;kodi;kodi\nEntry2=Games;games;:submenu Games\n\n"
                                    "[Games]\nEntry1=Arcade;games;:submenu Arcade\nEntry2=Retro;emulators;:back\n\n"
                                    "[Arcade]\nEntry1=Pong;games;:back\n";
    SettingsState *state = settings_create(names, 3);
    MenuEdit *edit = with_menus(state, text, "Main");
    SettingsRow rows[SETTINGS_MAX_ROWS];
    char path[256];
    open_tiles(state, "Main");
    cursor_to(state, "Games");
    settings_command(state, SETTINGS_OK);
    cursor_to(state, "Move to menu");
    settings_command(state, SETTINGS_OK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MOVE_TO);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Menus" ARROW "Main" ARROW "Tiles" ARROW "Games" ARROW "Move to");
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 2);
    CHECK_STR(rows[0].label, "Games");
    CHECK(!rows[0].enabled);
    CHECK_STR(rows[0].why, "This tile opens that menu");
    CHECK_STR(rows[1].label, "Arcade");
    CHECK(!rows[1].enabled);
    CHECK_STR(rows[1].why, "That menu is inside the one this tile opens");
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_NONE);
    settings_command(state, SETTINGS_BACK);
    settings_command(state, SETTINGS_BACK);

    // A plain tile: both offered; Arcade chosen, the tile goes to its end, back on Main's Tiles page
    cursor_to(state, "Kodi");
    settings_command(state, SETTINGS_OK);
    cursor_to(state, "Move to menu");
    settings_command(state, SETTINGS_OK);
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK(rows[0].enabled && rows[1].enabled);
    cursor_to(state, "Arcade");
    SettingsEvent event = settings_command(state, SETTINGS_OK);
    CHECK_INT(event.kind, SETTINGS_EVENT_MENUS);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TILES);
    CHECK_STR(tile_of(edit, "Arcade", 1)->title, "Kodi");
    CHECK_STR(tile_of(edit, "Main", 0)->title, "Games");
    CHECK(tile_of(edit, "Main", 1) == NULL);
    settings_free(state);
    menuedit_free(edit);
}

// A function to open a model over Main and Games with the menus model Task 10a's pages read: Main's three
// tiles and Games' two. The caller frees the menus model after the model.
static SettingsState *tiles_model(MenuEdit **menus)
{
    static const char *const names[] = { "Main", "Games" };
    static const char *const text = "[General]\nDefaultMenu=Main\n\n[Main]\nEntry1=Kodi;kodi;kodi\n"
                                     "Entry2=Games;games;:submenu Games\nEntry3=Bye;quit;:quit\n\n"
                                     "[Games]\nEntry1=Retro;emulators;retro\nEntry2=Steam;steam;steam\n";
    SettingsState *state = settings_create(names, 2);
    IniDoc *doc = inidoc_parse(text, strlen(text));
    *menus = doc != NULL ? menuedit_load(doc, "Main") : NULL;
    inidoc_free(doc);
    CHECK(state != NULL && *menus != NULL);
    settings_set_menus(state, *menus);
    return state;
}

// A function to name the row under the cursor on the page on show
static const char *cursor_label(SettingsState *state)
{
    static SettingsRow rows[SETTINGS_MAX_ROWS];
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    int cursor = settings_cursor(state);
    return cursor >= 0 && cursor < count ? rows[cursor].label : "(no row)";
}

// A function to test opening the pages at a tile (the hold-OK shortcut's way in, and the harness's):
// Menus, the menu, its Tiles page and the tile's page, each opened from the row that leads on, so Back
// walks up them with each cursor where it was; tile -1 stops at the Tiles page; a tile or a menu that is
// not there stops at the last page that opened
static void test_show_tile(void)
{
    MenuEdit *menus = NULL;
    SettingsState *state = tiles_model(&menus);
    settings_show_tile(state, 1, 1);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TILE);
    settings_command(state, SETTINGS_BACK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TILES);
    CHECK_STR(cursor_label(state), "Steam");
    settings_command(state, SETTINGS_BACK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MENU);
    CHECK_INT(settings_preview_menu(state), 1);
    settings_command(state, SETTINGS_BACK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MENUS);
    CHECK_STR(cursor_label(state), "Games");
    settings_command(state, SETTINGS_BACK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TOP);
    CHECK_STR(cursor_label(state), "Menus");

    settings_show_tile(state, 0, -1);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TILES);
    CHECK_INT(settings_preview_menu(state), 0);
    settings_show_tile(state, 0, 7);              // Main has no tile 7: its Tiles page
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TILES);
    settings_show_tile(state, 5, 0);              // No menu 5: the Menus page
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MENUS);
    settings_free(state);
    menuedit_free(menus);
}

// A function to open a model over `count` menus named in `names`, with the menus model of `text`; the
// caller frees the menus model after the model
static SettingsState *text_model(const char *const *names, int count, const char *text, MenuEdit **menus)
{
    SettingsState *state = settings_create(names, count);
    IniDoc *doc = inidoc_parse(text, strlen(text));
    *menus = doc != NULL ? menuedit_load(doc, names[0]) : NULL;
    inidoc_free(doc);
    CHECK(state != NULL && *menus != NULL);
    settings_set_menus(state, *menus);
    return state;
}

// A function to test opening the pages at a tile the pages have no row for (Fable review I4): the 66th tile
// of a 70-tile menu, past the Tiles page's rows (its note counts it), and the one tile of the 66th of 70
// menus, past the Menus page's; each page opens all the same, and Back walks up the pages as ever
static void test_show_tile_past_rows(void)
{
    static const char *const main_name[] = { "Main" };
    char text[4096] = "[Main]\n";
    for (int t = 0; t < 70; t++) {
        size_t used = strlen(text);
        snprintf(text + used, sizeof(text) - used, "Entry%d=T%d;apps;:home\n", t + 1, t);
    }
    MenuEdit *menus = NULL;
    SettingsState *state = text_model(main_name, 1, text, &menus);
    char path[256];
    settings_show_tile(state, 0, 65);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TILE);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Menus" ARROW "Main" ARROW "Tiles" ARROW "T65");
    CHECK_INT(settings_preview_tile(state), 65);
    settings_command(state, SETTINGS_BACK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TILES);
    CHECK_STR(cursor_label(state), "Add");                   // No row leads to T65: the cursor where it opened
    settings_free(state);
    menuedit_free(menus);

    static char labels[70][8];
    const char *names[70];
    text[0] = '\0';
    for (int m = 0; m < 70; m++) {
        snprintf(labels[m], sizeof(labels[m]), "M%d", m);
        names[m] = labels[m];
        size_t used = strlen(text);
        snprintf(text + used, sizeof(text) - used, "[M%d]\nEntry1=Only%d;apps;:home\n", m, m);
    }
    state = text_model(names, 70, text, &menus);
    settings_show_tile(state, 65, 0);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TILE);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Menus" ARROW "M65" ARROW "Tiles" ARROW "Only65");
    settings_command(state, SETTINGS_BACK);
    settings_command(state, SETTINGS_BACK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MENU);
    CHECK_INT(settings_preview_menu(state), 65);
    settings_command(state, SETTINGS_BACK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MENUS);
    settings_free(state);
    menuedit_free(menus);
}

// A function to test the Icon page (spec "Icons"): opened from a tile's page, about that tile; until the
// screen has looked, Library and Browse alone; then the choices in the spec's order with the cursor on
// the first that applies (the library match, else its own icon, else the generic icon); OK on each row
// hands the screen that row, the page staying; a choice made goes back to the tile's page, and one
// refused stays with why
static void test_icon_page(void)
{
    MenuEdit *menus = NULL;
    SettingsState *state = tiles_model(&menus);
    settings_show_tile(state, 1, 1);              // Games' Steam
    cursor_to(state, "Icon");
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_ICON);
    int menu = -1;
    int tile = -1;
    CHECK(settings_icon_tile(state, &menu, &tile));
    CHECK_INT(menu, 1);
    CHECK_INT(tile, 1);
    CHECK_INT(settings_preview_menu(state), 1);
    char path[512];
    settings_path(state, path, sizeof(path));
    size_t length = strlen(path);
    CHECK(length > strlen(ARROW "Icon") && strcmp(path + length - strlen(ARROW "Icon"), ARROW "Icon") == 0);
    CHECK(!settings_icon_choices(state)->ready);
    SettingsRow rows[SETTINGS_MAX_ROWS];
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 2);
    CHECK_STR(rows[0].label, "Library");
    CHECK_STR(rows[1].label, "Browse for an image");
    CHECK_INT(settings_cursor(state), 0);

    // A match and its own icon: the match first, and the cursor put back on it
    SettingsIconChoices choices;
    memset(&choices, 0, sizeof(choices));
    snprintf(choices.suggested, sizeof(choices.suggested), "steam");
    snprintf(choices.suggested_title, sizeof(choices.suggested_title), "Steam");
    choices.matched = true;
    choices.own = true;
    settings_command(state, SETTINGS_DOWN);
    settings_set_icon_choices(state, &choices);
    CHECK(settings_icon_choices(state)->ready);
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 4);
    CHECK_STR(rows[0].label, "Steam");
    CHECK_STR(rows[0].value, "Library match");
    CHECK_STR(rows[1].label, "Its own icon");
    CHECK_STR(rows[2].label, "Library");
    CHECK_STR(rows[3].label, "Browse for an image");
    CHECK_INT(settings_cursor(state), 0);
    static const SettingsAction actions[] = { SETTINGS_ACTION_ICON_SUGGESTED, SETTINGS_ACTION_ICON_OWN,
                                             SETTINGS_ACTION_ICON_GRID, SETTINGS_ACTION_ICON_BROWSE };
    for (int i = 0; i < 4; i++) {
        SettingsEvent event = settings_command(state, SETTINGS_OK);
        CHECK_INT(event.kind, SETTINGS_EVENT_ICON);
        CHECK_INT(event.action, actions[i]);
        CHECK_INT(settings_page(state), SETTINGS_PAGE_ICON);   // The screen acts; the page waits for it
        settings_command(state, SETTINGS_DOWN);
    }

    // The screen sets the icon it chose through the model (a library name or an image's path), as a tile's
    // command is set: one the tile's line cannot hold is refused with the reason, the tile as it was
    SettingsEvent set = settings_set_icon(state, 1, 1, "kodi");
    CHECK_INT(set.kind, SETTINGS_EVENT_MENUS);
    CHECK_INT(set.menu, 1);
    CHECK_INT(set.tile, 1);
    CHECK_STR(tile_of(menus, "Games", 1)->icon, "kodi");
    CHECK_STR(tile_of(menus, "Games", 1)->title, "Steam");
    set = settings_set_icon(state, 1, 1, "");
    CHECK_INT(set.kind, SETTINGS_EVENT_MOVED);
    CHECK_STR(settings_notice(state), "Choose an icon");
    CHECK_STR(tile_of(menus, "Games", 1)->icon, "kodi");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_ICON);       // The page is the screen's to leave

    // Refused: the page stays, its reason in the caption until the next key; chosen: the tile's page
    settings_icon_done(state, "Couldn't save its icon");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_ICON);
    CHECK_STR(settings_notice(state), "Couldn't save its icon");
    settings_command(state, SETTINGS_UP);
    CHECK_STR(settings_notice(state), "");
    settings_icon_done(state, NULL);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TILE);
    CHECK(!settings_icon_tile(state, &menu, &tile));
    settings_icon_done(state, NULL);              // Not on the Icon page: nothing happens
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TILE);

    // Opened again, the screen looks again. Its own icon alone comes first; with neither, the generic icon
    cursor_to(state, "Icon");
    settings_command(state, SETTINGS_OK);
    CHECK(!settings_icon_choices(state)->ready);
    memset(&choices, 0, sizeof(choices));
    choices.own = true;
    settings_set_icon_choices(state, &choices);
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 3);
    CHECK_STR(rows[0].label, "Its own icon");
    snprintf(choices.suggested, sizeof(choices.suggested), "apps");
    snprintf(choices.suggested_title, sizeof(choices.suggested_title), "Apps");
    choices.own = false;
    settings_set_icon_choices(state, &choices);
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 3);
    CHECK_STR(rows[0].label, "Apps");
    CHECK_STR(rows[0].value, "Generic icon");
    settings_command(state, SETTINGS_BACK);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TILE);
    settings_free(state);
    menuedit_free(menus);
}

// A function to tell whether the page on show has a row by its label, failing nothing when it has not
static bool has_row(SettingsState *state, const char *label)
{
    SettingsRow rows[SETTINGS_MAX_ROWS];
    int count = settings_rows(state, rows, SETTINGS_MAX_ROWS);
    for (int i = 0; i < count; i++) {
        if (strcmp(rows[i].label, label) == 0)
            return true;
    }
    return false;
}

// A function to test Controls' Desktop media keys row: Linux only, and only once a desktop is set; it reads On
// while the desktop holds a key, Off for StreamFlex while it holds none and its shortcuts are saved, else Off
static void test_media_keys_row(void)
{
    SettingsState *state = open_model();
    open_page(state, "Controls");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_CONTROLS);
    CHECK(!has_row(state, "Desktop media keys"));                   // No desktop: no row
    settings_set_media_keys(state, MEDIAKEYS_PLASMA, true, false);
#ifdef _WIN32
    CHECK(!has_row(state, "Desktop media keys"));                   // Windows: never a row
    settings_set_media_keys(state, MEDIAKEYS_GNOME, false, true);
    CHECK(!has_row(state, "Desktop media keys"));
#else
    SettingsRow rows[SETTINGS_MAX_ROWS];
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 2);
    CHECK_STR(rows[0].label, "Gamepad");                            // Last: after Gamepad (and Keyboard, with bindings)
    CHECK_STR(rows[1].label, "Desktop media keys");
    CHECK_INT(rows[1].kind, SETTINGS_ROW_LINK);
    CHECK_INT(rows[1].target, SETTINGS_PAGE_DESKTOP_KEYS);
    CHECK_STR(rows[1].value, "On");
    settings_set_media_keys(state, MEDIAKEYS_PLASMA, true, true);   // Holding a key again outranks a save
    CHECK_STR(row_labeled(state, rows, "Desktop media keys")->value, "On");
    settings_set_media_keys(state, MEDIAKEYS_PLASMA, false, true);
    CHECK_STR(row_labeled(state, rows, "Desktop media keys")->value, "Off for StreamFlex");
    settings_set_media_keys(state, MEDIAKEYS_PLASMA, false, false);
    CHECK_STR(row_labeled(state, rows, "Desktop media keys")->value, "Off");
    settings_set_media_keys(state, MEDIAKEYS_GNOME, false, true);
    CHECK_STR(row_labeled(state, rows, "Desktop media keys")->value, "Off for StreamFlex");
    settings_set_media_keys(state, MEDIAKEYS_GNOME, true, false);
    CHECK_STR(row_labeled(state, rows, "Desktop media keys")->value, "On");

    // A desktop that is neither again takes the row away, the cursor off it
    cursor_to(state, "Desktop media keys");
    settings_set_media_keys(state, MEDIAKEYS_NONE, false, false);
    CHECK(!has_row(state, "Desktop media keys"));
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 1);
    CHECK_INT(settings_cursor(state), 0);
#endif
    settings_free(state);
}

#ifndef _WIN32
// A function to test the Desktop media keys page: Give them back to the desktop, grayed while it holds a key,
// and Turn them off, grayed while it holds none, each with its reason and its event; nothing they do is a
// changed setting, and the page stays for the screen to refresh. Then the failure page over it.
static void test_desktop_keys_page(void)
{
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    char path[256];
    settings_set_media_keys(state, MEDIAKEYS_PLASMA, true, false);
    open_page(state, "Controls");
    open_page(state, "Desktop media keys");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_DESKTOP_KEYS);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Controls" ARROW "Desktop media keys");
    CHECK(!settings_media_keys_prompt_open(state));
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 2);
    CHECK_STR(rows[0].label, "Give them back to KDE Plasma");
    CHECK(!rows[0].enabled);
    CHECK_STR(rows[0].why, "KDE Plasma already has them");
    CHECK_STR(rows[1].label, "Turn them off");
    CHECK(rows[1].enabled);
    CHECK(rows[1].why == NULL);
    CHECK_INT(settings_cursor(state), 0);                           // Grayed with a reason: the cursor rests there
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_NONE);
    settings_command(state, SETTINGS_DOWN);
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MEDIA_KEYS_OFF);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_DESKTOP_KEYS);
    CHECK(!settings_any_changed(state));

    // Turned off, as the screen gives it again: Give them back now, Turn them off grayed
    settings_set_media_keys(state, MEDIAKEYS_PLASMA, false, true);
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK(rows[0].enabled);
    CHECK(rows[0].why == NULL);
    CHECK(!rows[1].enabled);
    CHECK_STR(rows[1].why, "They are already off");
    CHECK_INT(settings_cursor(state), 1);
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_NONE);
    settings_command(state, SETTINGS_UP);
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MEDIA_KEYS_RESTORE);
    settings_set_media_keys(state, MEDIAKEYS_PLASMA, false, false);   // Nothing saved: Give them back sets defaults
    CHECK(row_labeled(state, rows, "Give them back to KDE Plasma")->enabled);
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MEDIA_KEYS_RESTORE);
    CHECK(!settings_any_changed(state));

    // The failure page, over the page that tried: its reason, OK, and back to that page with OK or Back
    settings_show_media_keys_failed(state, "kglobalaccel did not answer");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MEDIA_KEYS_FAILED);
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Controls" ARROW "Desktop media keys" ARROW "Couldn't change shortcuts");
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 2);
    CHECK_INT(rows[0].kind, SETTINGS_ROW_NOTE);
    CHECK_STR(rows[0].note, "Couldn't change Plasma's shortcuts: kglobalaccel did not answer. Nothing was changed.");
    CHECK_STR(rows[1].label, "OK");
    CHECK_INT(settings_cursor(state), 1);
    settings_show_media_keys_failed(state, "the session bus is gone");   // A second failure: the new reason, one page
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[0].note, "Couldn't change Plasma's shortcuts: the session bus is gone. Nothing was changed.");
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_DESKTOP_KEYS);
    settings_show_media_keys_failed(state, "gsettings did not answer");
    CHECK_INT(settings_command(state, SETTINGS_BACK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_DESKTOP_KEYS);
    CHECK_INT(settings_command(state, SETTINGS_BACK).kind, SETTINGS_EVENT_MOVED);   // Not the prompt: Back goes back
    CHECK_INT(settings_page(state), SETTINGS_PAGE_CONTROLS);

    // GNOME's page names GNOME
    settings_set_media_keys(state, MEDIAKEYS_GNOME, true, false);
    open_page(state, "Desktop media keys");
    CHECK_STR(row_labeled(state, rows, "Give them back to GNOME")->why, "GNOME already has them");
    settings_set_media_keys(state, MEDIAKEYS_GNOME, false, false);
    CHECK_STR(row_labeled(state, rows, "Turn them off")->why, "They are already off");
    settings_show_media_keys_failed(state, "gsettings did not answer");
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[0].note, "Couldn't change GNOME's shortcuts: gsettings did not answer. Nothing was changed.");
    settings_free(state);
}
#endif

// A function to test the Media keys prompt settings open on at start: it replaces the pages open, its note
// names the desktop, Turn them off and Don't ask again send their events, and Not now and Back close
// settings with nothing changed; with no desktop set it never opens
static void test_media_keys_prompt(void)
{
    static const char *const KDE_NOTE = "KDE Plasma uses the next-track and previous-track keys for its own media "
        "controls, so StreamFlex never receives them. Turn Plasma's shortcuts for them off? Other music players "
        "will stop responding to these keys until you turn them back on in Settings \xE2\x80\xBA Controls "
        "\xE2\x80\xBA Desktop media keys.";
    static const char *const GNOME_NOTE = "GNOME uses the next-track and previous-track keys for its own media "
        "controls, so StreamFlex never receives them. Turn GNOME's shortcuts for them off? Other music players "
        "will stop responding to these keys until you turn them back on in Settings \xE2\x80\xBA Controls "
        "\xE2\x80\xBA Desktop media keys.";
    SettingsState *state = open_model();
    SettingsRow rows[SETTINGS_MAX_ROWS];
    char path[256];
    settings_show_media_keys(state);                               // No desktop: no prompt
    CHECK_INT(settings_page(state), SETTINGS_PAGE_TOP);
    CHECK(!settings_media_keys_prompt_open(state));
    open_page(state, "General");
    settings_set_media_keys(state, MEDIAKEYS_PLASMA, true, false);
    settings_show_media_keys(state);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MEDIA_KEYS);
    CHECK(settings_media_keys_prompt_open(state));
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Media keys");
    CHECK_INT(settings_rows(state, rows, SETTINGS_MAX_ROWS), 4);
    CHECK_INT(rows[0].kind, SETTINGS_ROW_NOTE);
    CHECK_STR(rows[0].note, KDE_NOTE);
    CHECK_STR(rows[1].label, "Turn them off");
    CHECK_STR(rows[2].label, "Not now");
    CHECK_STR(rows[3].label, "Don't ask again");
    CHECK_INT(settings_cursor(state), 1);

    // Each row's event; the screen acts and closes settings, so the page stays in the model
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MEDIA_KEYS_OFF);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MEDIA_KEYS);
    settings_command(state, SETTINGS_DOWN);
    SettingsEvent event = settings_command(state, SETTINGS_OK);
    CHECK_INT(event.kind, SETTINGS_EVENT_CLOSE);                   // Not now: the usual close, nothing to apply
    CHECK(event.slot == NULL);
    settings_command(state, SETTINGS_DOWN);
    CHECK_INT(settings_command(state, SETTINGS_OK).kind, SETTINGS_EVENT_MEDIA_KEYS_NEVER);
    event = settings_command(state, SETTINGS_BACK);
    CHECK_INT(event.kind, SETTINGS_EVENT_CLOSE);                   // Back is Not now
    CHECK(event.slot == NULL);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MEDIA_KEYS);
    CHECK(!settings_any_changed(state));
    CHECK_INT(settings_command(state, SETTINGS_HOME).kind, SETTINGS_EVENT_CLOSE_HOME);

    // A failure over the prompt: the prompt is still open under it, and Back goes back to it, then closes
    settings_show_media_keys_failed(state, "kglobalaccel did not answer");
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MEDIA_KEYS_FAILED);
    CHECK(settings_media_keys_prompt_open(state));
    settings_path(state, path, sizeof(path));
    CHECK_STR(path, "Settings" ARROW "Media keys" ARROW "Couldn't change shortcuts");
    CHECK_INT(settings_command(state, SETTINGS_BACK).kind, SETTINGS_EVENT_MOVED);
    CHECK_INT(settings_page(state), SETTINGS_PAGE_MEDIA_KEYS);
    CHECK_INT(settings_command(state, SETTINGS_BACK).kind, SETTINGS_EVENT_CLOSE);
    settings_free(state);

    // GNOME's note names GNOME
    state = open_model();
    settings_set_media_keys(state, MEDIAKEYS_GNOME, true, false);
    settings_show_media_keys(state);
    settings_rows(state, rows, SETTINGS_MAX_ROWS);
    CHECK_STR(rows[0].note, GNOME_NOTE);
    settings_free(state);
}

int main(void)
{
    test_add_binding();
    test_navigation_confirm();
    test_remove_binding();
    test_binding_pages();
    test_binding_edits();
    test_binding_waits();
    test_binding_refusals();
    test_binding_no_room();
    test_round_trips();
    test_rejects();
    test_steps();
    test_descriptions();
    test_top_and_menus();
    test_background_page();
    test_save_failed_page();
    test_all_menus_and_titles_pages();
    test_slots_by_place();
    test_one_menu();
    test_background_entered_incomplete();
    test_more_menus_than_rows();
    test_general_page();
    test_grayed_rows();
    test_other_pages();
    test_new_round_trips();
    test_new_rejects();
    test_new_steps();
    test_new_descriptions();
    test_find();
    test_fallbacks();
    test_row_steps_and_selectable();
    test_next_start();
    test_next_start_only_on_a_change();
    test_restart_words();
    test_restart_tracking();
    test_restart_page();
    test_keyboard_layout();
    test_remodel();
    test_tiles_pages();
    test_empty_menu_and_new_submenu();
    test_add_after_tile();
    test_title_weighed_in_its_line();
    test_remove_submenu();
    test_rename_submenu();
    test_tiles_without_menus();
    test_tile_commands();
    test_tile_title();
    test_move_target();
    test_move_tile();
    test_move_to_menu();
    test_show_tile();
    test_show_tile_past_rows();
    test_icon_page();
    test_remodel_menus_cursor();
    test_remodel_slot_edit_follows_rename();
    test_remodel_save_after_remove_and_rename();
    test_media_keys_row();
#ifndef _WIN32
    test_desktop_keys_page();
#endif
    test_media_keys_prompt();
    return check_report();
}

