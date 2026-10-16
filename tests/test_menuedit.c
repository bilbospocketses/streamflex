#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "menuedit.h"

static const char *const CONFIG_TEXT =
    "; my launcher\n"
    "[General]\n"
    "DefaultMenu=Main\n"
    "\n"
    "[Main]\n"
    "Rows=2\n"
    "Entry1=Kodi;kodi;kodi\n"
    "Entry2=Games;games;:submenu Games ; my games\n"
    "Entry3=Steam;steam;steam\n"
    "\n"
    "[Games]\n"
    "Entry1=Retro;retro;retroarch\n"
    "Entry4=Back;back;:back\n"
    "\n"
    "[System]\n"
    "Columns=4\n"
    "Entry1=Shutdown;power;:shutdown\n"
    "Entry2=All games;games;:submenu Games\n"
    "\n"
    "[Hotkeys]\n"
    "Hotkey1=#4000003A;:quit\n";

static const char *const LINE_TOO_LONG = "It's too long for one line of config.ini (199 bytes)";
static const char *const LINE_TOO_LONG_THERE = "It's too long for its line of config.ini with the comment there (199 bytes)";
static const char *const NO_KEY = "Its line in config.ini has no key, so it can't be removed or moved";

// A function to parse a text, failing the check when it does not parse
static IniDoc *parse(const char *text)
{
    IniDoc *doc = inidoc_parse(text, strlen(text));
    CHECK(doc != NULL);
    return doc;
}

// A function to load the menus model from a text; the model keeps copies, so the document goes
static MenuEdit *load(const char *text, const char *default_menu)
{
    IniDoc *doc = parse(text);
    MenuEdit *edit = menuedit_load(doc, default_menu);
    CHECK(edit != NULL);
    inidoc_free(doc);
    return edit;
}

// A function to apply the model to a fresh read of `text`, as a save does, and give the file as it
// would be written (a static buffer)
static const char *applied(const MenuEdit *edit, const char *text, bool *ok, ConfigSaveResult *result)
{
    static char out[4096];
    memset(result, 0, sizeof(*result));
    IniDoc *doc = parse(text);
    *ok = menuedit_apply(edit, doc, result);
    char *written = inidoc_serialize(doc, NULL);
    snprintf(out, sizeof(out), "%s", written != NULL ? written : "");
    free(written);
    inidoc_free(doc);
    return out;
}

// A function to test loading: every section that is not settings is a menu, in file order, and every
// line the parser makes an entry of is a tile, read as the parser reads it
static void test_load(void)
{
    MenuEdit *edit = load(CONFIG_TEXT, "Main");
    CHECK_INT(menuedit_count(edit), 3);
    CHECK_STR(menuedit_menu(edit, 0)->name, "Main");
    CHECK_STR(menuedit_menu(edit, 0)->original_name, "Main");
    CHECK_STR(menuedit_menu(edit, 1)->name, "Games");
    CHECK_STR(menuedit_menu(edit, 2)->name, "System");
    CHECK(menuedit_menu(edit, 3) == NULL);
    CHECK(menuedit_menu(edit, -1) == NULL);
    const MenuEditMenu *main_menu = menuedit_menu(edit, 0);
    CHECK_INT(main_menu->count, 3);                            // Rows=2 is the menu's setting
    CHECK_STR(main_menu->tiles[1].key, "Entry2");
    CHECK_STR(main_menu->tiles[1].title, "Games");
    CHECK_STR(main_menu->tiles[1].icon, "games");
    CHECK_STR(main_menu->tiles[1].command, ":submenu Games");   // As inih reads it: the comment is not in it
    CHECK_STR(main_menu->tiles[1].original, "Entry2=Games;games;:submenu Games ; my games");
    CHECK_INT(menuedit_menu(edit, 2)->count, 2);               // Columns=4 likewise
    CHECK_STR(menuedit_default(edit), "Main");
    CHECK_INT(menuedit_find(edit, "Games"), 1);
    CHECK_INT(menuedit_find(edit, "games"), -1);               // Names match exactly, as the parser's do
    CHECK_INT(menuedit_find(edit, "Hotkeys"), -1);
    CHECK(!menuedit_changed(edit));
    menuedit_free(edit);

    // The parser's own rules: a layout key holding a ';' is an entry; empty parts are skipped; two parts,
    // or exactly :select, is no tile; a section with no lines is an empty menu; a reserved name in another
    // case is a menu
    edit = load("[Main]\nRows=2;rows;rows\nEntry1=;;Kodi;;kodi;kodi;x\nEntry2=Two;parts\nEntry3=Pick;apps;:select\n"
                "Entry4=Sel;apps;:select now\n[Empty]\n[general]\nEntry1=A;a;a\n[Layout]\nRows=3\n", NULL);
    CHECK_INT(menuedit_count(edit), 3);
    main_menu = menuedit_menu(edit, 0);
    CHECK_INT(main_menu->count, 3);
    CHECK_STR(main_menu->tiles[0].key, "Rows");
    CHECK_STR(main_menu->tiles[0].title, "2");
    CHECK_STR(main_menu->tiles[1].title, "Kodi");
    CHECK_STR(main_menu->tiles[1].icon, "kodi");
    CHECK_STR(main_menu->tiles[1].command, "kodi;x");
    CHECK_STR(main_menu->tiles[2].command, ":select now");
    CHECK_STR(menuedit_menu(edit, 1)->name, "Empty");
    CHECK_INT(menuedit_menu(edit, 1)->count, 0);
    CHECK_STR(menuedit_menu(edit, 2)->name, "general");
    CHECK(menuedit_default(edit) == NULL);
    menuedit_free(edit);

    // Keys before the first header, and keys under an empty [] header (both a section named "" to inih),
    // are no menu
    edit = load("Entry1=Stray;apps;:quit\n[Main]\nEntry1=A;apps;:quit\n[]\nEntry2=B;apps;:quit\n", NULL);
    CHECK_INT(menuedit_count(edit), 1);
    CHECK_STR(menuedit_menu(edit, 0)->name, "Main");
    CHECK_INT(menuedit_menu(edit, 0)->count, 1);
    CHECK_INT(menuedit_find(edit, ""), -1);
    menuedit_free(edit);
}

// A function to test what opens a submenu: a command of exactly ":submenu " and a name, the name being
// all after the one space, as execute_command() reads it
static void test_openers(void)
{
    MenuEdit *edit = load(CONFIG_TEXT, "Main");
    CHECK_INT(menuedit_openers(edit, "Games"), 2);
    CHECK_INT(menuedit_openers(edit, "Main"), 0);
    CHECK_STR(menuedit_submenu_of(&menuedit_menu(edit, 0)->tiles[1]), "Games");
    CHECK(menuedit_submenu_of(&menuedit_menu(edit, 0)->tiles[0]) == NULL);
    char command[32] = ":submenu";
    MenuTile tile = { NULL, NULL, NULL, command, NULL };
    CHECK(menuedit_submenu_of(&tile) == NULL);                 // No name
    snprintf(command, sizeof(command), ":submenu ");
    CHECK(menuedit_submenu_of(&tile) == NULL);
    snprintf(command, sizeof(command), ":submenu  Games");
    CHECK_STR(menuedit_submenu_of(&tile), " Games");
    snprintf(command, sizeof(command), ":submenuGames");
    CHECK(menuedit_submenu_of(&tile) == NULL);
    menuedit_free(edit);
}

// A function to test the name rules (Review Focus 2)
static void test_check_name(void)
{
    MenuEdit *edit = load(CONFIG_TEXT, "Main");
    CHECK(menuedit_check_name(edit, "Kids", -1) == NULL);
    CHECK_STR(menuedit_check_name(edit, "", -1), "Type a name");
    CHECK_STR(menuedit_check_name(edit, "Kids ", -1), "A name can't start or end with a space");
    CHECK_STR(menuedit_check_name(edit, " Kids", -1), "A name can't start or end with a space");
    CHECK_STR(menuedit_check_name(edit, "Kids]", -1), "A name can't contain ]");
    CHECK_STR(menuedit_check_name(edit, "Kids;TV", -1), "A name can't contain ;");
    CHECK_STR(menuedit_check_name(edit, "Kids\nTV", -1), "It can't contain a line break");
    char name[64];
    memset(name, 'K', INIDOC_MAX_SECTION);
    name[INIDOC_MAX_SECTION] = '\0';
    CHECK(menuedit_check_name(edit, name, -1) == NULL);         // 49 bytes: inih keeps them all
    name[INIDOC_MAX_SECTION] = 'K';
    name[INIDOC_MAX_SECTION + 1] = '\0';
    CHECK_STR(menuedit_check_name(edit, name, -1), "A name can be 49 bytes at most");
    CHECK_STR(menuedit_check_name(edit, "Hotkeys", -1), "That name is kept for settings");
    CHECK_STR(menuedit_check_name(edit, "Scroll Indicators", -1), "That name is kept for settings");
    CHECK_STR(menuedit_check_name(edit, "general", -1), "That name is kept for settings");   // In any case
    CHECK_STR(menuedit_check_name(edit, "System", -1), "There's already a menu with that name");
    CHECK_STR(menuedit_check_name(edit, "games", -1), "There's already a menu with that name");   // In any case
    CHECK(menuedit_check_name(edit, "games", 1) == NULL);       // Games itself, in another case
    CHECK(menuedit_check_name(edit, "Gamepads", -1) == NULL);   // Begins like Gamepad, but is not it
    menuedit_free(edit);
}

// A function to test the tile rules (Review Focus 2), and that whatever passes them inidoc writes as it is
static void test_check_tile(void)
{
    CHECK(menuedit_check_tile("Kodi", "kodi", "kodi --fullscreen") == NULL);
    CHECK(menuedit_check_tile("Kodi", "C:\\Icons\\kodi.png", "cmd /c a;b") == NULL);   // ';' after no space
    CHECK_STR(menuedit_check_tile("", "kodi", "kodi"), "Type a title");
    CHECK_STR(menuedit_check_tile("Kodi", "", "kodi"), "Choose an icon");
    CHECK_STR(menuedit_check_tile("Kodi", "kodi", ""), "Choose a command");
    CHECK_STR(menuedit_check_tile("Ko;di", "kodi", "kodi"), "A title can't contain ;");
    CHECK_STR(menuedit_check_tile("Kodi ", "kodi", "kodi"), "A title can't start or end with a space");   // "Kodi ;" starts a comment
    CHECK_STR(menuedit_check_tile(" Kodi", "kodi", "kodi"), "A title can't start or end with a space");
    CHECK_STR(menuedit_check_tile("Kodi", "a;b.png", "kodi"), "An icon's name or path can't contain ;");
    CHECK_STR(menuedit_check_tile("Kodi", "kodi ", "kodi"), "An icon's name or path can't start or end with a space");
    CHECK_STR(menuedit_check_tile("Kodi", "kodi", "kodi "), "A command can't start or end with a space");
    CHECK_STR(menuedit_check_tile("Kodi", "kodi", "kodi ;x"), "A command can't have ; after a space");
    CHECK_STR(menuedit_check_tile("Kodi", "kodi", ":select"), "A tile can't run :select");
    CHECK_STR(menuedit_check_tile("Ko\ndi", "kodi", "kodi"), "It can't contain a line break");
    CHECK_STR(menuedit_check_tile("Kodi", "kodi", "kodi\r"), "It can't contain a line break");

    // The line: "Entry99999=" (a tile numbered only at the save) and the value, 199 bytes at most
    char command[256];
    memset(command, 'c', 184);
    command[184] = '\0';                                         // 11 + "T;i;" + 184 = 199
    CHECK(menuedit_check_tile("T", "i", command) == NULL);
    command[184] = 'c';
    command[185] = '\0';
    CHECK_STR(menuedit_check_tile("T", "i", command), LINE_TOO_LONG);

    // The rules are inidoc_check()'s for this shape of line: what passes them, inidoc writes as it is
    static const char *const parts[] = { "a", "a b", "a;b", "a ;b", " a", "a ", "\ta", "a\t", ";a", "a;", "x\x01y" };
    const size_t count = sizeof(parts) / sizeof(parts[0]);
    char value[64];
    for (size_t t = 0; t < count; t++) {
        for (size_t i = 0; i < count; i++) {
            for (size_t c = 0; c < count; c++) {
                if (menuedit_check_tile(parts[t], parts[i], parts[c]) != NULL)
                    continue;
                snprintf(value, sizeof(value), "%s;%s;%s", parts[t], parts[i], parts[c]);
                CHECK(inidoc_check("Entry99999", value) == NULL);
            }
        }
    }
}

// A function to test adding, changing and removing tiles
static void test_tiles(void)
{
    MenuEdit *edit = load(CONFIG_TEXT, "Main");

    // Added after the highlighted tile, or at the end, with no key or line until the save
    CHECK(menuedit_add(edit, 0, 0, "Plex", "plex", "plex") == NULL);
    CHECK(menuedit_add(edit, 0, -1, "Edge", "edge", "msedge") == NULL);
    const MenuEditMenu *main_menu = menuedit_menu(edit, 0);
    CHECK_INT(main_menu->count, 5);
    CHECK_STR(main_menu->tiles[1].title, "Plex");
    CHECK(main_menu->tiles[1].key == NULL && main_menu->tiles[1].original == NULL);
    CHECK_STR(main_menu->tiles[2].title, "Games");
    CHECK_STR(main_menu->tiles[4].title, "Edge");
    CHECK(menuedit_changed(edit));
    CHECK_STR(menuedit_add(edit, 0, 0, "Pl;ex", "plex", "plex"), "A title can't contain ;");
    CHECK_STR(menuedit_add(edit, 0, 5, "Plex", "plex", "plex"), "No such tile");
    CHECK_STR(menuedit_add(edit, 0, -2, "Plex", "plex", "plex"), "No such tile");
    CHECK_STR(menuedit_add(edit, 3, -1, "Plex", "plex", "plex"), "No such menu");
    CHECK_INT(main_menu->count, 5);

    // Changed in place: the key and the line as read stay, for the save to find the line by
    CHECK(menuedit_set(edit, 0, 0, "Kodi 21", "kodi", "kodi --fullscreen") == NULL);
    CHECK_STR(main_menu->tiles[0].title, "Kodi 21");
    CHECK_STR(main_menu->tiles[0].key, "Entry1");
    CHECK_STR(main_menu->tiles[0].original, "Entry1=Kodi;kodi;kodi");
    CHECK_STR(menuedit_set(edit, 0, 0, "Kodi", "kodi", ""), "Choose a command");
    CHECK_STR(main_menu->tiles[0].command, "kodi --fullscreen");   // A refusal changes nothing
    CHECK_STR(menuedit_set(edit, 0, 9, "Kodi", "kodi", "kodi"), "No such tile");

    // A tile that opens a submenu is removed through menuedit_remove_menu(), with its confirm page; one
    // whose submenu is not there is a tile like any other
    CHECK_STR(menuedit_remove(edit, 0, 2), "It opens a submenu");
    CHECK(menuedit_remove(edit, 0, 3) == NULL);                  // Steam
    CHECK_INT(main_menu->count, 4);
    CHECK_STR(main_menu->tiles[3].title, "Edge");
    CHECK(menuedit_add(edit, 0, -1, "Lost", "apps", ":submenu Nowhere") == NULL);
    CHECK(menuedit_remove(edit, 0, 4) == NULL);
    CHECK_INT(main_menu->count, 4);
    CHECK_STR(menuedit_remove(edit, 0, 4), "No such tile");
    menuedit_free(edit);

    // The tile's own key counts toward its line: a long hand-written key leaves less room than a new
    // tile's would
    char key[128];
    char text[256];
    char command[128];
    memset(key, 'K', 100);
    key[100] = '\0';
    snprintf(text, sizeof(text), "[Main]\n%s=A;a;a\n", key);
    memset(command, 'c', 100);
    command[100] = '\0';
    edit = load(text, NULL);
    CHECK(menuedit_check_tile("A", "a", command) == NULL);       // 11 + 4 + 100 = 115 with a new tile's key
    CHECK_STR(menuedit_set(edit, 0, 0, "A", "a", command), LINE_TOO_LONG);   // 100 + 1 + 4 + 100 = 205
    menuedit_free(edit);

    // A tile read from the file is weighed in its own line, as the save writes it there: the comment after
    // its value counts. "Entry1=" + title + ";a;a" + " ; note" is 18 bytes and the title.
    char title[256];
    memset(title, 'T', 182);
    title[182] = '\0';
    edit = load("[Main]\nEntry1=A;a;a ; note\n", NULL);
    CHECK(menuedit_check_tile(title, "a", "a") == NULL);         // 11 + 182 + 4 = 197 with a new tile's key
    CHECK_STR(menuedit_check_set(edit, 0, 0, title, "a", "a"), LINE_TOO_LONG_THERE);   // 18 + 182 = 200
    CHECK_STR(menuedit_set(edit, 0, 0, title, "a", "a"), LINE_TOO_LONG_THERE);
    CHECK_STR(menuedit_menu(edit, 0)->tiles[0].title, "A");
    title[181] = '\0';
    CHECK(menuedit_check_set(edit, 0, 0, title, "a", "a") == NULL);     // 199
    CHECK(menuedit_set(edit, 0, 0, title, "a", "a") == NULL);
    CHECK_STR(menuedit_check_set(edit, 0, 1, "A", "a", "a"), "No such tile");
    menuedit_free(edit);

    // The spacing around its '=' counts too: "Entry1 =  " is 10 bytes before the value, 10 + 186 + 4 = 200
    edit = load("[Main]\nEntry1 =  A;a;a\n", NULL);
    memset(title, 'T', 186);
    title[186] = '\0';
    CHECK_STR(menuedit_check_set(edit, 0, 0, title, "a", "a"),
              "It's too long for its line of config.ini with the spacing there (199 bytes)");
    menuedit_free(edit);

    // A menu a hand edit numbered past Entry99999: the save numbers its next tile Entry100001, so a new tile
    // there is weighed with a key a digit longer than its longest number, Entry9999999 (12 bytes)
    char long_command[256];
    memset(long_command, 'c', 183);
    long_command[183] = '\0';
    CHECK(menuedit_check_tile("T", "i", long_command) == NULL);  // 11 + 4 + 183 = 198
    edit = load("[Main]\nEntry100000=A;a;a\n[Games]\n", NULL);
    CHECK_STR(menuedit_add(edit, 0, -1, "T", "i", long_command), LINE_TOO_LONG);   // 13 + 4 + 183 = 200
    CHECK(menuedit_add(edit, 1, -1, "T", "i", long_command) == NULL);   // Games has no numbers: Entry99999
    long_command[182] = '\0';
    CHECK(menuedit_add(edit, 0, -1, "T", "i", long_command) == NULL);   // 199

    // That new tile is weighed with the same key when it is changed, and so is a new tile a rename rewrites:
    // the save numbers each Entry100001 or above
    CHECK(menuedit_check_set(edit, 0, 1, "T", "i", long_command) == NULL);   // 13 + 4 + 182 = 199
    long_command[182] = 'c';
    CHECK_STR(menuedit_check_set(edit, 0, 1, "T", "i", long_command), LINE_TOO_LONG);   // 200
    CHECK_STR(menuedit_set(edit, 0, 1, "T", "i", long_command), LINE_TOO_LONG);
    memset(title, 'T', 168);
    title[168] = '\0';
    CHECK(menuedit_add(edit, 0, -1, title, "i", ":submenu Games") == NULL);   // 13 + 168 + 3 + 14 = 198
    CHECK_STR(menuedit_rename_menu(edit, 1, "Gamesxx"), LINE_TOO_LONG);       // 13 + 168 + 3 + 16 = 200
    CHECK(menuedit_rename_menu(edit, 1, "Gamesx") == NULL);                   // 199
    menuedit_free(edit);
}

// A function to test moving tiles: within a menu, and to another menu's end, as a new line there
static void test_moves(void)
{
    MenuEdit *edit = load(CONFIG_TEXT, "Main");
    CHECK(menuedit_move(edit, 0, 2, 0) == NULL);                 // Steam to the front
    const MenuEditMenu *main_menu = menuedit_menu(edit, 0);
    CHECK_STR(main_menu->tiles[0].title, "Steam");
    CHECK_STR(main_menu->tiles[1].title, "Kodi");
    CHECK_STR(main_menu->tiles[2].title, "Games");
    CHECK(menuedit_changed(edit));
    CHECK(menuedit_move(edit, 0, 0, 2) == NULL);                 // And back to the end
    CHECK_STR(main_menu->tiles[2].title, "Steam");
    CHECK(!menuedit_changed(edit));                              // The order as read again: nothing to save
    CHECK_STR(menuedit_move(edit, 0, 0, 3), "No such tile");

    CHECK(menuedit_move_to_menu(edit, 0, 0, 2) == NULL);         // Kodi to System
    CHECK_INT(main_menu->count, 2);
    const MenuEditMenu *system = menuedit_menu(edit, 2);
    CHECK_INT(system->count, 3);
    CHECK_STR(system->tiles[2].title, "Kodi");
    CHECK(system->tiles[2].key == NULL && system->tiles[2].original == NULL);
    CHECK_STR(menuedit_move_to_menu(edit, 2, 2, 2), "It's in that menu already");
    CHECK_STR(menuedit_move_to_menu(edit, 0, 0, 1), "A menu can't hold a tile that opens it");   // Games' tile into Games
    CHECK_STR(menuedit_move_to_menu(edit, 0, 0, 5), "No such menu");
    CHECK_STR(menuedit_move_to_menu(edit, 0, 7, 1), "No such tile");
    menuedit_free(edit);

    // A tile moved gets a new key, so its line is weighed with one: "K=" + 196 fits, "Entry99999=" + 196 does not
    char command[256];
    char text[512];
    memset(command, 'c', 192);
    command[192] = '\0';
    snprintf(text, sizeof(text), "[Main]\nK=A;a;%s\n[Games]\n", command);
    edit = load(text, NULL);
    CHECK_STR(menuedit_move_to_menu(edit, 0, 0, 1), LINE_TOO_LONG);
    CHECK_INT(menuedit_menu(edit, 0)->count, 1);
    menuedit_free(edit);

    // A line with no key ("=B;b;b") is a tile to the parser, but inidoc refuses to remove or move it, since
    // indented lines after it would join the key before it: removing or moving it is refused, and so is
    // putting its menu's tiles in another order, which could move it, and removing a submenu it opens.
    // Its menu's other tiles still go.
    edit = load("[Main]\nEntry1=A;a;a\n=B;b;b\nEntry2=G;g;:submenu Games\n[Games]\nEntry1=C;c;c\n"
                "[Kids]\n=K;k;:submenu Games\n", NULL);
    CHECK_INT(menuedit_menu(edit, 0)->count, 3);
    CHECK_STR(menuedit_menu(edit, 0)->tiles[1].key, "");
    CHECK_STR(menuedit_remove(edit, 0, 1), NO_KEY);
    CHECK_STR(menuedit_move_to_menu(edit, 0, 1, 1), NO_KEY);
    CHECK_STR(menuedit_move(edit, 0, 0, 2), "A line in this menu has no key in config.ini, so its tiles can't be moved");
    CHECK_STR(menuedit_remove_menu(edit, 2, 0, false), NO_KEY);
    CHECK_STR(menuedit_remove_menu(edit, 0, 2, true), "A tile that opens it has no key in config.ini, so it can't be removed");
    CHECK_INT(menuedit_count(edit), 3);
    CHECK(menuedit_remove_menu(edit, 0, 2, false) == NULL);     // G's line has a key, and K still opens Games
    CHECK(menuedit_remove(edit, 0, 0) == NULL);                 // So has A's
    CHECK_INT(menuedit_menu(edit, 0)->count, 1);
    CHECK(menuedit_set(edit, 0, 0, "B2", "b", "b") == NULL);     // Changed in its own line, as any tile is
    menuedit_free(edit);
}

// A function to test submenus (Review Focus 3): made, renamed (every exact opener, and the default),
// and removed (its tile alone only while another opens it; never the default menu)
static void test_submenus(void)
{
    MenuEdit *edit = load(CONFIG_TEXT, "Main");
    CHECK(menuedit_add_submenu(edit, 0, 0, "Kids", "kids") == NULL);
    CHECK_INT(menuedit_count(edit), 4);
    const MenuEditMenu *kids = menuedit_menu(edit, 3);
    CHECK_STR(kids->name, "Kids");
    CHECK(kids->original_name == NULL);
    CHECK_INT(kids->count, 0);
    const MenuTile *opener = &menuedit_menu(edit, 0)->tiles[1];
    CHECK_STR(opener->title, "Kids");
    CHECK_STR(opener->icon, "kids");
    CHECK_STR(opener->command, ":submenu Kids");
    CHECK(opener->key == NULL && opener->original == NULL);
    CHECK_STR(menuedit_add_submenu(edit, 0, -1, "kids", "kids"), "There's already a menu with that name");
    CHECK_STR(menuedit_add_submenu(edit, 0, -1, "Toys", ""), "Choose an icon");
    CHECK_STR(menuedit_add_submenu(edit, 0, 9, "Toys", "toys"), "No such tile");
    CHECK_INT(menuedit_count(edit), 4);
    CHECK(menuedit_add(edit, 3, -1, "Paint", "paint", "mspaint") == NULL);

    // Renaming rewrites every tile whose command is exactly ":submenu Games", and the title of each that
    // was the name; a title of its own stays, and a command in another case is another menu's
    CHECK(menuedit_add(edit, 2, -1, "Other", "games", ":submenu games") == NULL);
    CHECK(menuedit_rename_menu(edit, 1, "Play") == NULL);
    CHECK_STR(menuedit_menu(edit, 1)->name, "Play");
    CHECK_STR(menuedit_menu(edit, 1)->original_name, "Games");
    opener = &menuedit_menu(edit, 0)->tiles[2];
    CHECK_STR(opener->title, "Play");
    CHECK_STR(opener->command, ":submenu Play");
    CHECK_STR(opener->original, "Entry2=Games;games;:submenu Games ; my games");   // The save finds its line by it
    const MenuEditMenu *system = menuedit_menu(edit, 2);
    CHECK_STR(system->tiles[1].title, "All games");
    CHECK_STR(system->tiles[1].command, ":submenu Play");
    CHECK_STR(system->tiles[2].command, ":submenu games");
    CHECK_INT(menuedit_openers(edit, "Games"), 0);
    CHECK_INT(menuedit_openers(edit, "Play"), 2);
    CHECK_STR(menuedit_rename_menu(edit, 1, "System"), "There's already a menu with that name");
    CHECK_STR(menuedit_rename_menu(edit, 1, "Clock"), "That name is kept for settings");
    CHECK_STR(menuedit_rename_menu(edit, 7, "Toys"), "No such menu");
    CHECK(menuedit_rename_menu(edit, 1, "play") == NULL);        // Itself, in another case
    CHECK(menuedit_rename_menu(edit, 1, "Play") == NULL);
    CHECK_STR(menuedit_menu(edit, 0)->tiles[2].title, "Play");
    CHECK(menuedit_rename_menu(edit, 0, "Home") == NULL);        // The default menu: its name goes with it
    CHECK_STR(menuedit_default(edit), "Home");

    // Home: Kodi, Kids, Play, Steam. System: Shutdown, All games, Other.
    CHECK_STR(menuedit_remove_menu(edit, 0, 0, true), "It doesn't open a submenu");
    CHECK_STR(menuedit_remove_menu(edit, 0, 9, true), "No such tile");
    CHECK(menuedit_remove_menu(edit, 2, 1, false) == NULL);      // System's tile; Home's still opens Play
    CHECK_INT(menuedit_menu(edit, 2)->count, 2);
    CHECK_STR(menuedit_remove_menu(edit, 0, 2, false), "No other tile opens that menu");
    CHECK(menuedit_add(edit, 2, -1, "Again", "games", ":submenu Play") == NULL);
    CHECK(menuedit_remove_menu(edit, 0, 2, true) == NULL);       // Play, its tiles, and every tile that opens it
    CHECK_INT(menuedit_count(edit), 3);
    CHECK_INT(menuedit_find(edit, "Play"), -1);
    CHECK_INT(menuedit_menu(edit, 0)->count, 3);                 // Kodi, Kids and Steam
    CHECK_INT(menuedit_menu(edit, 1)->count, 2);                 // Shutdown and Other: Again went with Play
    CHECK_STR(menuedit_menu(edit, 2)->name, "Kids");
    CHECK(menuedit_add(edit, 2, -1, "Home", "home", ":submenu Home") == NULL);
    CHECK_STR(menuedit_remove_menu(edit, 2, 1, true), "The default menu can't be removed");
    CHECK_INT(menuedit_count(edit), 3);
    menuedit_free(edit);

    // A menu renamed away from its name, then a new menu given that name, which is free: the new one has
    // no original_name, and the renamed one keeps "Games" as its, so Task 4's menu_for() never takes the
    // renamed menu's live Menu for the new one (the new one is matched by name among menus made in settings)
    edit = load(CONFIG_TEXT, "Main");
    CHECK(menuedit_rename_menu(edit, 1, "Play") == NULL);
    CHECK(menuedit_add_submenu(edit, 0, -1, "Games", "games") == NULL);
    int fresh = menuedit_find(edit, "Games");
    CHECK(fresh >= 0 && fresh != 1);
    CHECK(fresh >= 0 && menuedit_menu(edit, fresh)->original_name == NULL);
    CHECK_STR(menuedit_menu(edit, 1)->name, "Play");
    CHECK_STR(menuedit_menu(edit, 1)->original_name, "Games");
    menuedit_free(edit);
}

// A function to test the copy Discard returns to: the screen keeps one made at open, and Discard frees
// the model and copies it again
static void test_copy(void)
{
    MenuEdit *edit = load(CONFIG_TEXT, "Main");
    MenuEdit *snapshot = menuedit_copy(edit);
    CHECK(snapshot != NULL);
    CHECK(menuedit_rename_menu(edit, 1, "Play") == NULL);
    CHECK(menuedit_remove(edit, 0, 0) == NULL);
    CHECK(menuedit_changed(edit));
    menuedit_free(edit);
    edit = menuedit_copy(snapshot);
    CHECK(!menuedit_changed(edit));
    CHECK_STR(menuedit_menu(edit, 1)->name, "Games");
    CHECK_INT(menuedit_menu(edit, 0)->count, 3);
    CHECK_STR(menuedit_default(edit), "Main");

    // A copy of a changed model keeps what it weighs the changes against
    CHECK(menuedit_remove(edit, 0, 0) == NULL);
    MenuEdit *copy = menuedit_copy(edit);
    CHECK(menuedit_changed(copy));
    menuedit_free(copy);
    menuedit_free(edit);
    menuedit_free(snapshot);
}

// A function to test writing the model into the file it was read from, with no hand edit meanwhile
static void test_apply(void)
{
    ConfigSaveResult result;
    bool ok = false;
    MenuEdit *edit = load(CONFIG_TEXT, "Main");
    CHECK_STR(applied(edit, CONFIG_TEXT, &ok, &result), CONFIG_TEXT);   // Nothing changed: nothing moves
    CHECK(ok);
    CHECK_INT(result.note_count, 0);

    // A tile changed in its own line (its comment kept), one removed, and one added after Kodi, numbered
    // after the highest EntryN once Steam's line is gone; Rows left alone
    CHECK(menuedit_set(edit, 0, 1, "Games", "joystick", ":submenu Games") == NULL);
    CHECK(menuedit_remove(edit, 0, 2) == NULL);
    CHECK(menuedit_add(edit, 0, 0, "Plex", "plex", "plex") == NULL);
    CHECK_STR(applied(edit, CONFIG_TEXT, &ok, &result),
        "; my launcher\n[General]\nDefaultMenu=Main\n\n"
        "[Main]\nRows=2\nEntry1=Kodi;kodi;kodi\nEntry3=Plex;plex;plex\nEntry2=Games;joystick;:submenu Games ; my games\n\n"
        "[Games]\nEntry1=Retro;retro;retroarch\nEntry4=Back;back;:back\n\n"
        "[System]\nColumns=4\nEntry1=Shutdown;power;:shutdown\nEntry2=All games;games;:submenu Games\n\n"
        "[Hotkeys]\nHotkey1=#4000003A;:quit\n");
    CHECK(ok);
    menuedit_free(edit);

    // A tile moved to the front goes under the menu's Rows, as its first entry; one moved to another menu
    // leaves its line and gets a new one at that menu's end
    edit = load(CONFIG_TEXT, "Main");
    CHECK(menuedit_move(edit, 0, 2, 0) == NULL);
    CHECK(menuedit_move_to_menu(edit, 1, 0, 2) == NULL);         // Retro to System
    CHECK_STR(applied(edit, CONFIG_TEXT, &ok, &result),
        "; my launcher\n[General]\nDefaultMenu=Main\n\n"
        "[Main]\nRows=2\nEntry3=Steam;steam;steam\nEntry1=Kodi;kodi;kodi\nEntry2=Games;games;:submenu Games ; my games\n\n"
        "[Games]\nEntry4=Back;back;:back\n\n"
        "[System]\nColumns=4\nEntry1=Shutdown;power;:shutdown\nEntry2=All games;games;:submenu Games\n"
        "Entry3=Retro;retro;retroarch\n\n"
        "[Hotkeys]\nHotkey1=#4000003A;:quit\n");
    CHECK(ok);
    menuedit_free(edit);

    // Renamed: the header, every tile that opens it (its comment kept), and DefaultMenu when it named it.
    // A new submenu: its section at the end of the file, its tiles numbered from 1 in the model's order
    edit = load(CONFIG_TEXT, "Main");
    CHECK(menuedit_rename_menu(edit, 1, "Play") == NULL);
    CHECK(menuedit_rename_menu(edit, 0, "Home") == NULL);
    CHECK(menuedit_add_submenu(edit, 0, -1, "Kids", "kids") == NULL);
    CHECK(menuedit_add(edit, 3, -1, "Paint", "paint", "mspaint") == NULL);
    CHECK(menuedit_add(edit, 3, -1, "Tux", "tux", "tuxpaint") == NULL);
    CHECK_STR(applied(edit, CONFIG_TEXT, &ok, &result),
        "; my launcher\n[General]\nDefaultMenu=Home\n\n"
        "[Home]\nRows=2\nEntry1=Kodi;kodi;kodi\nEntry2=Play;games;:submenu Play ; my games\nEntry3=Steam;steam;steam\n"
        "Entry4=Kids;kids;:submenu Kids\n\n"
        "[Play]\nEntry1=Retro;retro;retroarch\nEntry4=Back;back;:back\n\n"
        "[System]\nColumns=4\nEntry1=Shutdown;power;:shutdown\nEntry2=All games;games;:submenu Play\n\n"
        "[Hotkeys]\nHotkey1=#4000003A;:quit\n\n"
        "[Kids]\nEntry1=Paint;paint;mspaint\nEntry2=Tux;tux;tuxpaint\n");
    CHECK(ok);
    menuedit_free(edit);

    // A submenu removed with its tiles: its section goes, and so does every line that opened it
    edit = load(CONFIG_TEXT, "Main");
    CHECK(menuedit_remove_menu(edit, 0, 1, true) == NULL);
    CHECK_STR(applied(edit, CONFIG_TEXT, &ok, &result),
        "; my launcher\n[General]\nDefaultMenu=Main\n\n"
        "[Main]\nRows=2\nEntry1=Kodi;kodi;kodi\nEntry3=Steam;steam;steam\n\n"
        "[System]\nColumns=4\nEntry1=Shutdown;power;:shutdown\n\n"
        "[Hotkeys]\nHotkey1=#4000003A;:quit\n");
    CHECK(ok);
    menuedit_free(edit);
}

// A function to test a hand edit to config.ini between opening settings and saving (Review Focus 1):
// the save succeeds; an edit whose target is gone is skipped or written as a new line, with 3b's notes;
// a menu renamed or removed by hand keeps the hand edit; nothing else in the file moves
static void test_apply_after_hand_edits(void)
{
    // The model: Kodi retitled, Steam removed, Retro retitled (in Games), Shutdown retitled (in System)
    MenuEdit *edit = load(CONFIG_TEXT, "Main");
    CHECK(menuedit_set(edit, 0, 0, "Kodi 21", "kodi", "kodi") == NULL);
    CHECK(menuedit_remove(edit, 0, 2) == NULL);
    CHECK(menuedit_set(edit, 1, 0, "RetroArch", "retro", "retroarch") == NULL);
    CHECK(menuedit_set(edit, 2, 0, "Power off", "power", ":shutdown") == NULL);

    // Meanwhile, by hand: Kodi's line changed, Steam's removed, a Plex line added; Games renamed Gaming;
    // System's two tiles swapped
    static const char *const hand =
        "; my launcher\n[General]\nDefaultMenu=Main\n\n"
        "[Main]\nRows=2\nEntry1=Kodi;kodi;kodi --fullscreen\nEntry2=Games;games;:submenu Games ; my games\n"
        "Entry9=Plex;plex;plex\n\n"
        "[Gaming]\nEntry1=Retro;retro;retroarch\nEntry4=Back;back;:back\n\n"
        "[System]\nColumns=4\nEntry2=All games;games;:submenu Games\nEntry1=Shutdown;power;:shutdown\n\n"
        "[Hotkeys]\nHotkey1=#4000003A;:quit\n";
    ConfigSaveResult result;
    bool ok = false;
    CHECK_STR(applied(edit, hand, &ok, &result),
        "; my launcher\n[General]\nDefaultMenu=Main\n\n"
        "[Main]\nRows=2\nEntry1=Kodi;kodi;kodi --fullscreen\nEntry10=Kodi 21;kodi;kodi\n"
        "Entry2=Games;games;:submenu Games ; my games\nEntry9=Plex;plex;plex\n\n"
        "[Gaming]\nEntry1=Retro;retro;retroarch\nEntry4=Back;back;:back\n\n"
        "[System]\nColumns=4\nEntry2=All games;games;:submenu Games\nEntry1=Power off;power;:shutdown\n\n"
        "[Hotkeys]\nHotkey1=#4000003A;:quit\n");
    CHECK(ok);
    CHECK_INT(result.note_count, 3);
    CHECK_INT(result.note_kinds[0], CONFIG_NOTE_SECTION_GONE);
    CHECK_INT(result.note_kinds[1], CONFIG_NOTE_REMOVAL_SKIPPED);
    CHECK_INT(result.note_kinds[2], CONFIG_NOTE_CHANGED_MEANWHILE);
    CHECK_STR(result.notes,
        "[Games] is not there any more, so the changes to that menu are skipped\n"
        "'Entry3=Steam;steam;steam' is not there any more, so its removal is skipped\n"
        "'Entry1=Kodi;kodi;kodi' changed meanwhile, so its change is written as a new line");
    menuedit_free(edit);

    // DefaultMenu follows a renamed menu only while the file still names it; tiles the model did not
    // change, whose lines a hand edit removed, stay gone with no note
    edit = load(CONFIG_TEXT, "Main");
    CHECK(menuedit_rename_menu(edit, 0, "Home") == NULL);
    static const char *const other_default =
        "[General]\nDefaultMenu=System\n[Main]\nEntry1=Kodi;kodi;kodi\n[System]\nEntry1=Shutdown;power;:shutdown\n";
    CHECK_STR(applied(edit, other_default, &ok, &result),
        "[General]\nDefaultMenu=System\n[Home]\nEntry1=Kodi;kodi;kodi\n[System]\nEntry1=Shutdown;power;:shutdown\n");
    CHECK(ok);
    CHECK_INT(result.note_count, 0);
    menuedit_free(edit);

    // A menu the model removed that a hand edit removed already: its removal is skipped, with a note,
    // and the lines that opened it still go
    edit = load(CONFIG_TEXT, "Main");
    CHECK(menuedit_remove_menu(edit, 0, 1, true) == NULL);
    static const char *const no_games =
        "[General]\nDefaultMenu=Main\n[Main]\nRows=2\nEntry1=Kodi;kodi;kodi\nEntry2=Games;games;:submenu Games ; my games\n"
        "Entry3=Steam;steam;steam\n[System]\nColumns=4\nEntry1=Shutdown;power;:shutdown\nEntry2=All games;games;:submenu Games\n";
    CHECK_STR(applied(edit, no_games, &ok, &result),
        "[General]\nDefaultMenu=Main\n[Main]\nRows=2\nEntry1=Kodi;kodi;kodi\nEntry3=Steam;steam;steam\n"
        "[System]\nColumns=4\nEntry1=Shutdown;power;:shutdown\n");
    CHECK(ok);
    CHECK_INT(result.note_count, 1);
    CHECK_INT(result.note_kinds[0], CONFIG_NOTE_SECTION_GONE);
    CHECK_STR(result.notes, "[Games] is not there any more, so its removal is skipped");
    menuedit_free(edit);

    // A name a hand edit gave a section meanwhile cannot be taken, by a rename or by a new menu: the two
    // would be one section to inih. The save fails with the reason.
    static const char *const taken =
        "[Main]\nEntry1=Kodi;kodi;kodi\n[Games]\nEntry1=Retro;retro;retroarch\n[Play]\nEntry1=Chess;chess;chess\n";
    static const char *const taken_why = "a section named [Play] was added to config.ini meanwhile, so no menu can take that name";
    edit = load(CONFIG_TEXT, "Main");
    CHECK(menuedit_rename_menu(edit, 1, "Play") == NULL);
    applied(edit, taken, &ok, &result);
    CHECK(!ok);
    CHECK_STR(result.why, taken_why);
    menuedit_free(edit);
    edit = load(CONFIG_TEXT, "Main");
    CHECK(menuedit_add_submenu(edit, 0, -1, "Play", "games") == NULL);
    applied(edit, taken, &ok, &result);
    CHECK(!ok);
    CHECK_STR(result.why, taken_why);
    menuedit_free(edit);

    // A menu renamed in settings whose section a hand edit renamed (or removed) meanwhile: the tiles that
    // open it, in other menus, already open it by its new name, which no section would have, so the save
    // fails with the reason, as a clash does, whether or not the hand also fixed those tiles
    static const char *const renamed_by_hand =
        "[General]\nDefaultMenu=Main\n[Main]\nEntry1=Kodi;kodi;kodi\nEntry2=Games;games;:submenu Games ; my games\n"
        "[Fun]\nEntry1=Retro;retro;retroarch\n";
    static const char *const fixed_by_hand =
        "[General]\nDefaultMenu=Main\n[Main]\nEntry1=Kodi;kodi;kodi\nEntry2=Games;games;:submenu Fun\n"
        "[Fun]\nEntry1=Retro;retro;retroarch\n";
    static const char *const gone_why = "[Games] was renamed or removed in config.ini meanwhile, so it cannot be renamed to Play";
    edit = load(CONFIG_TEXT, "Main");
    CHECK(menuedit_rename_menu(edit, 1, "Play") == NULL);
    applied(edit, renamed_by_hand, &ok, &result);
    CHECK(!ok);
    CHECK_STR(result.why, gone_why);
    applied(edit, fixed_by_hand, &ok, &result);
    CHECK(!ok);
    CHECK_STR(result.why, gone_why);
    menuedit_free(edit);
}

// A function to test renames that meet (two menus swapping names through a third, past a section that
// already has the first renaming name), and renames a line has no room for, which fail the save
static void test_apply_renames(void)
{
    static const char *const text =
        "[General]\nDefaultMenu=Games\n[Main]\nEntry1=Games;games;:submenu Games\nEntry2=Tools;tools;:submenu Tools\n"
        "[Games]\nEntry1=Retro;retro;retroarch\n[Tools]\nEntry1=Paint;paint;mspaint\n[StreamFlex renaming 1]\nEntry1=Odd;apps;odd\n";
    MenuEdit *edit = load(text, "Games");
    CHECK(menuedit_rename_menu(edit, 1, "Swap") == NULL);
    CHECK(menuedit_rename_menu(edit, 2, "Games") == NULL);
    CHECK(menuedit_rename_menu(edit, 1, "Tools") == NULL);
    CHECK_STR(menuedit_default(edit), "Tools");
    ConfigSaveResult result;
    bool ok = false;
    CHECK_STR(applied(edit, text, &ok, &result),
        "[General]\nDefaultMenu=Tools\n[Main]\nEntry1=Tools;games;:submenu Tools\nEntry2=Games;tools;:submenu Games\n"
        "[Tools]\nEntry1=Retro;retro;retroarch\n[Games]\nEntry1=Paint;paint;mspaint\n[StreamFlex renaming 1]\nEntry1=Odd;apps;odd\n");
    CHECK(ok);
    menuedit_free(edit);

    char name[INIDOC_MAX_SECTION + 1];
    memset(name, 'N', INIDOC_MAX_SECTION);
    name[INIDOC_MAX_SECTION] = '\0';
    char comment[200];
    char long_text[512];

    // The header's comment leaves no room for the new name: 1 + 49 + 1 + 160 = 211
    memset(comment, 'c', 158);
    comment[158] = '\0';
    snprintf(long_text, sizeof(long_text), "[Main]\nEntry1=G;games;:submenu G\n[G] ;%s\nEntry1=Retro;retro;retroarch\n", comment);
    edit = load(long_text, "Main");
    CHECK(menuedit_rename_menu(edit, 1, name) == NULL);
    applied(edit, long_text, &ok, &result);
    CHECK(!ok);
    CHECK(strncmp(result.why, "[G] cannot be renamed to NNNN", 29) == 0);
    menuedit_free(edit);

    // DefaultMenu's comment leaves no room for the new name: 12 + 49 + 153 = 214
    memset(comment, 'c', 150);
    comment[150] = '\0';
    snprintf(long_text, sizeof(long_text), "[General]\nDefaultMenu=Games ; %s\n[Main]\nEntry1=Games;games;:submenu Games\n"
             "[Games]\nEntry1=Retro;retro;retroarch\n", comment);
    edit = load(long_text, "Games");
    CHECK(menuedit_rename_menu(edit, 1, name) == NULL);
    applied(edit, long_text, &ok, &result);
    CHECK(!ok);
    CHECK_STR(result.why, "the DefaultMenu value in [General] cannot be written: it is too long for one line of config.ini "
                          "(199 bytes at most) with its comment");
    menuedit_free(edit);

    // A header whose comment leaves room for the new name but not for a renaming name (21 bytes): no section
    // holds the new name, so the section is renamed straight to it. "[G] ;" and 194 bytes is 199, as is "[H] ;".
    memset(comment, 'c', 194);
    comment[194] = '\0';
    snprintf(long_text, sizeof(long_text), "[Main]\nEntry1=G;games;:submenu G\n[G] ;%s\nEntry1=Retro;retro;retroarch\n", comment);
    char renamed_text[512];
    snprintf(renamed_text, sizeof(renamed_text), "[Main]\nEntry1=H;games;:submenu H\n[H] ;%s\nEntry1=Retro;retro;retroarch\n",
             comment);
    edit = load(long_text, "Main");
    CHECK(menuedit_rename_menu(edit, 1, "H") == NULL);
    CHECK_STR(applied(edit, long_text, &ok, &result), renamed_text);
    CHECK(ok);
    menuedit_free(edit);

    // A renaming name is never one a menu is renamed to: A takes "StreamFlex renaming 2", which its section
    // gives up as Zed, so A goes through "StreamFlex renaming 1"; then B, swapping names with C, needs a
    // renaming name of its own, and the next free one in the file, 2, is A's new name, so B takes 3
    static const char *const taken_names =
        "[A]\nEntry1=A;a;a\n[StreamFlex renaming 2]\nEntry1=Z;z;z\n[B]\nEntry1=B;b;b\n[C]\nEntry1=C;c;c\n";
    edit = load(taken_names, NULL);
    CHECK(menuedit_rename_menu(edit, 1, "Zed") == NULL);
    CHECK(menuedit_rename_menu(edit, 0, "StreamFlex renaming 2") == NULL);
    CHECK(menuedit_rename_menu(edit, 2, "Swap") == NULL);
    CHECK(menuedit_rename_menu(edit, 3, "B") == NULL);
    CHECK(menuedit_rename_menu(edit, 2, "C") == NULL);
    CHECK_STR(applied(edit, taken_names, &ok, &result),
        "[StreamFlex renaming 2]\nEntry1=A;a;a\n[Zed]\nEntry1=Z;z;z\n[C]\nEntry1=B;b;b\n[B]\nEntry1=C;c;c\n");
    CHECK(ok);
    menuedit_free(edit);
}

// A function to test the order the save writes: tiles the model moved are put in its order, moving only
// them, so a line a hand edit added between them stays; and a line read twice is two tiles
static void test_apply_order(void)
{
    MenuEdit *edit = load(CONFIG_TEXT, "Main");
    CHECK(menuedit_move(edit, 0, 2, 0) == NULL);                 // Steam to the front
    static const char *const hand =
        "[Main]\nRows=2\nEntry1=Kodi;kodi;kodi\nEntry9=Plex;plex;plex\nEntry2=Games;games;:submenu Games ; my games\n"
        "Entry3=Steam;steam;steam\n";
    ConfigSaveResult result;
    bool ok = false;
    CHECK_STR(applied(edit, hand, &ok, &result),
        "[Main]\nRows=2\nEntry3=Steam;steam;steam\nEntry1=Kodi;kodi;kodi\nEntry9=Plex;plex;plex\n"
        "Entry2=Games;games;:submenu Games ; my games\n");
    CHECK(ok);
    menuedit_free(edit);

    static const char *const twice = "[Main]\nEntry1=A;a;a\nEntry1=A;a;a\nEntry2=B;b;b\n";
    edit = load(twice, NULL);
    CHECK_INT(menuedit_menu(edit, 0)->count, 3);
    CHECK(menuedit_remove(edit, 0, 0) == NULL);
    CHECK(menuedit_set(edit, 0, 0, "A2", "a", "a") == NULL);     // The second A
    CHECK_STR(applied(edit, twice, &ok, &result), "[Main]\nEntry1=A2;a;a\nEntry2=B;b;b\n");
    CHECK(ok);
    menuedit_free(edit);
}

int main(void)
{
    test_load();
    test_openers();
    test_check_name();
    test_check_tile();
    test_tiles();
    test_moves();
    test_submenus();
    test_copy();
    test_apply();
    test_apply_after_hand_edits();
    test_apply_renames();
    test_apply_order();
    return check_report();
}
