#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <launcher_config.h>
#include "menuedit.h"
#include "fileio.h"
#include "alloc.h"

#define SUBMENU ":submenu "     // A submenu tile's command up to the menu's name: launcher.h's SCMD_SUBMENU and its space
#define SELECT ":select"        // launcher.h's SCMD_SELECT, which the parser refuses in an entry
#define KEY_ROOM "Entry99999"   // The key a tile with none is weighed with: its number is chosen only at the save
#define ENTRY_STEM "Entry"      // A new tile's key, numbered one above the section's highest
#define RENAMING "StreamFlex renaming %d"   // A section's name between its old name and its new one
#define OWN_SECTION "S"         // The section a tile's line as read is weighed in, alone in a document
#define OWN_HEADER "[" OWN_SECTION "]\n"

// The sections the parser reads as settings, never as menus: util.c's settings_section(), and Hotkeys
static const char *const RESERVED[] = {
    "General", "Layout", "Background", "Titles", "Highlight", "Scroll Indicators", "Clock", "Screensaver",
    "Hotkeys", "Gamepad"
};

static const char *const OUT_OF_MEMORY = "Out of memory";
static const char *const NO_SUCH_MENU = "No such menu";
static const char *const NO_SUCH_TILE = "No such tile";
static const char *const LINE_TOO_LONG = "It's too long for one line of config.ini (199 bytes)";
static const char *const LINE_TOO_LONG_COMMENT = "It's too long for its line of config.ini with the comment there (199 bytes)";
static const char *const LINE_TOO_LONG_SPACING = "It's too long for its line of config.ini with the spacing there (199 bytes)";
static const char *const NO_KEY = "Its line in config.ini has no key, so it can't be removed or moved";
static const char *const KEYLESS_MENU = "A line in this menu has no key in config.ini, so its tiles can't be moved";
static const char *const KEYLESS_OPENER = "A tile that opens it has no key in config.ini, so it can't be removed";

// A tile read from the file that was moved to another menu: where it is now, and the line it was read from,
// which stays where it is when the save skips the changes to the menu it was moved to
typedef struct {
    int menu;          // Where the tile is now
    int tile;
    int read;          // The menu it was read in, in `read`
    char *original;    // Its line as read, which the tile gave up when it moved
} MenuMove;

struct MenuEdit {
    MenuEditMenu *menus;   // The menus now, in the file's order, new ones at the end
    int count;
    char *default_menu;    // The default menu's name now (a rename carries it); NULL when none was given
    MenuEditMenu *read;    // The menus as read, which menuedit_changed() and menuedit_apply() weigh the model against
    int read_count;
    MenuMove *moves;       // The tiles moved from the menu they were read in, kept in step with every edit
    int move_count;
};

// A function to tell whitespace as inih does (isspace in the C locale)
static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

// A function to tell whether a text starts or ends with whitespace, which config.ini would drop
static bool edge_space(const char *text)
{
    size_t length = strlen(text);
    return length > 0 && (is_space(text[0]) || is_space(text[length - 1]));
}

// A function to tell whether a text has a ';' after whitespace, which config.ini would read as a comment
static bool comment_inside(const char *text)
{
    size_t length = strlen(text);
    for (size_t i = 1; i < length; i++) {
        if (text[i] == ';' && is_space(text[i - 1]))
            return true;
    }
    return false;
}

// A function to compare two names as the name rules do: an ASCII letter in either case is the same
static bool same_name(const char *a, const char *b)
{
    while (*a != '\0' && fileio_lower(*a) == fileio_lower(*b)) {
        a++;
        b++;
    }
    return fileio_lower(*a) == fileio_lower(*b);
}

// A function to tell whether a name is a settings section's: exactly, as the parser matches it, or in
// any case, as the name rules refuse it
static bool reserved(const char *name, bool any_case)
{
    for (size_t i = 0; i < sizeof(RESERVED) / sizeof(RESERVED[0]); i++) {
        if (any_case ? same_name(name, RESERVED[i]) : strcmp(name, RESERVED[i]) == 0)
            return true;
    }
    return false;
}

// A function to copy part of a text into new memory; NULL, with *ok false, when out of memory
static char *copy_span(const char *start, size_t length, bool *ok)
{
    char *copy = alloc_malloc(length + 1);
    if (copy == NULL) {
        *ok = false;
        return NULL;
    }
    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
}

// A function to copy a text, NULL for NULL; *ok goes false when out of memory
static char *copy_text(const char *text, bool *ok)
{
    return text != NULL ? copy_span(text, strlen(text), ok) : NULL;
}

// A function to free what a tile holds
static void free_tile(MenuTile *tile)
{
    alloc_free(tile->key);
    alloc_free(tile->title);
    alloc_free(tile->icon);
    alloc_free(tile->command);
    alloc_free(tile->original);
}

// A function to free what a menu holds
static void free_menu(MenuEditMenu *menu)
{
    for (int t = 0; t < menu->count; t++)
        free_tile(&menu->tiles[t]);
    alloc_free(menu->tiles);
    alloc_free(menu->name);
    alloc_free(menu->original_name);
}

// A function to free a list of menus
static void free_menus(MenuEditMenu *menus, int count)
{
    for (int m = 0; m < count; m++)
        free_menu(&menus[m]);
    alloc_free(menus);
}

// A function to copy a tile into `out`; false when out of memory, with nothing left allocated
static bool copy_tile(const MenuTile *tile, MenuTile *out)
{
    bool ok = true;
    out->key = copy_text(tile->key, &ok);
    out->title = copy_text(tile->title, &ok);
    out->icon = copy_text(tile->icon, &ok);
    out->command = copy_text(tile->command, &ok);
    out->original = copy_text(tile->original, &ok);
    if (!ok) {
        free_tile(out);
        memset(out, 0, sizeof(*out));
    }
    return ok;
}

// A function to copy a list of menus; NULL when out of memory, with nothing left allocated
static MenuEditMenu *copy_menus(const MenuEditMenu *menus, int count)
{
    MenuEditMenu *copy = alloc_calloc((size_t) count, sizeof(MenuEditMenu));
    if (copy == NULL)
        return NULL;
    for (int m = 0; m < count; m++) {
        bool ok = true;
        copy[m].name = copy_text(menus[m].name, &ok);
        copy[m].original_name = copy_text(menus[m].original_name, &ok);
        copy[m].tiles = alloc_calloc((size_t) menus[m].count, sizeof(MenuTile));
        ok = ok && copy[m].tiles != NULL;
        for (int t = 0; ok && t < menus[m].count; t++) {
            ok = copy_tile(&menus[m].tiles[t], &copy[m].tiles[t]);
            if (ok)
                copy[m].count = t + 1;
        }
        if (!ok) {
            free_menus(copy, m + 1);
            return NULL;
        }
    }
    return copy;
}

// A function to free a list of moves
static void free_moves(MenuMove *moves, int count)
{
    for (int k = 0; k < count; k++)
        alloc_free(moves[k].original);
    alloc_free(moves);
}

// A function to copy a list of moves; NULL when out of memory, with nothing left allocated
static MenuMove *copy_moves(const MenuMove *moves, int count)
{
    MenuMove *copy = alloc_calloc((size_t) count, sizeof(MenuMove));
    for (int k = 0; copy != NULL && k < count; k++) {
        bool ok = true;
        copy[k] = moves[k];
        copy[k].original = copy_text(moves[k].original, &ok);
        if (!ok) {
            free_moves(copy, k);
            return NULL;
        }
    }
    return copy;
}

// A function to tell whether a key line is one of a menu's own settings: Rows, Columns or IconSize with
// a value holding no ';', as the parser reads them (util.c's config_handler); one holding a ';' is an entry
static bool is_setting_line(const IniDocItem *item)
{
    bool layout_key = strcmp(item->key, SETTING_ROWS) == 0 || strcmp(item->key, SETTING_COLUMNS) == 0 ||
                      strcmp(item->key, SETTING_ICON_SIZE) == 0;
    return layout_key && strchr(item->value, ';') == NULL;
}

// A function to read an entry's value into a tile as the parser does (util.c's config_handler): the
// title and the icon each end at a ';', with any ';' before them skipped (strtok_r's way), and the
// command is all the rest. False when the parser makes no entry of it (fewer than three parts, or
// :select), with nothing copied; *ok goes false when a copy runs out of memory.
static bool parse_entry(const char *value, MenuTile *tile, bool *ok)
{
    const char *title = value + strspn(value, ";");
    size_t title_length = strcspn(title, ";");
    const char *icon = title + title_length;
    icon += strspn(icon, ";");
    size_t icon_length = strcspn(icon, ";");
    const char *command = icon + icon_length;
    if (*command == ';')
        command++;
    if (title_length == 0 || icon_length == 0 || *command == '\0' || strcmp(command, SELECT) == 0)
        return false;
    tile->title = copy_span(title, title_length, ok);
    tile->icon = copy_span(icon, icon_length, ok);
    tile->command = copy_text(command, ok);
    return true;
}

// A function to read one menu from the file: its entries, leaving out its own settings and the lines
// the parser ignores. False when out of memory, with nothing left allocated.
static bool load_menu(const IniDoc *doc, const char *name, MenuEditMenu *menu)
{
    memset(menu, 0, sizeof(*menu));
    int count = inidoc_list(doc, name, NULL, NULL, 0);
    IniDocItem *items = alloc_calloc((size_t) count, sizeof(IniDocItem));
    bool ok = items != NULL;
    menu->name = copy_text(name, &ok);
    menu->original_name = copy_text(name, &ok);
    menu->tiles = alloc_calloc((size_t) count, sizeof(MenuTile));
    ok = ok && menu->tiles != NULL;
    if (ok)
        inidoc_list(doc, name, NULL, items, count);
    for (int i = 0; ok && i < count; i++) {
        MenuTile *tile = &menu->tiles[menu->count];
        if (is_setting_line(&items[i]) || !parse_entry(items[i].value, tile, &ok))
            continue;
        tile->key = copy_text(items[i].key, &ok);
        tile->original = copy_text(items[i].text, &ok);
        menu->count++;
    }
    alloc_free(items);
    if (!ok) {
        free_menu(menu);
        memset(menu, 0, sizeof(*menu));
    }
    return ok;
}

// A function to read every menu from the file, in file order: each section the parser does not read as
// settings, an empty one included (plan ruling 2), but for one named "" (keys under an empty [] header),
// which no tile could open. `default_menu` is the default menu's name, or NULL. NULL when out of memory.
MenuEdit *menuedit_load(const IniDoc *doc, const char *default_menu)
{
    MenuEdit *edit = alloc_calloc(1, sizeof(MenuEdit));
    if (edit == NULL)
        return NULL;
    bool ok = true;
    edit->default_menu = copy_text(default_menu, &ok);
    int sections = inidoc_sections(doc, NULL, 0);
    const char **names = alloc_calloc((size_t) sections, sizeof(char *));
    edit->menus = alloc_calloc((size_t) sections, sizeof(MenuEditMenu));
    ok = ok && names != NULL && edit->menus != NULL;
    if (ok)
        inidoc_sections(doc, names, sections);
    for (int s = 0; ok && s < sections; s++) {
        if (names[s][0] == '\0' || reserved(names[s], false))
            continue;
        ok = load_menu(doc, names[s], &edit->menus[edit->count]);
        if (ok)
            edit->count++;
    }
    alloc_free((void *) names);
    if (ok) {
        edit->read = copy_menus(edit->menus, edit->count);
        ok = edit->read != NULL;
        edit->read_count = ok ? edit->count : 0;
    }
    if (!ok) {
        menuedit_free(edit);
        return NULL;
    }
    return edit;
}

// A function to copy a model, with what it weighs its changes against: the snapshot Discard returns
// to. NULL when out of memory.
MenuEdit *menuedit_copy(const MenuEdit *edit)
{
    MenuEdit *copy = alloc_calloc(1, sizeof(MenuEdit));
    if (copy == NULL)
        return NULL;
    bool ok = true;
    copy->default_menu = copy_text(edit->default_menu, &ok);
    copy->menus = copy_menus(edit->menus, edit->count);
    copy->count = copy->menus != NULL ? edit->count : 0;
    copy->read = copy_menus(edit->read, edit->read_count);
    copy->read_count = copy->read != NULL ? edit->read_count : 0;
    copy->moves = copy_moves(edit->moves, edit->move_count);
    copy->move_count = copy->moves != NULL ? edit->move_count : 0;
    if (!ok || copy->menus == NULL || copy->read == NULL || copy->moves == NULL) {
        menuedit_free(copy);
        return NULL;
    }
    return copy;
}

// A function to free a model
void menuedit_free(MenuEdit *edit)
{
    if (edit == NULL)
        return;
    free_menus(edit->menus, edit->count);
    free_menus(edit->read, edit->read_count);
    free_moves(edit->moves, edit->move_count);
    alloc_free(edit->default_menu);
    alloc_free(edit);
}

// A function to count the menus
int menuedit_count(const MenuEdit *edit)
{
    return edit->count;
}

// A function to give a menu; NULL past the ends
const MenuEditMenu *menuedit_menu(const MenuEdit *edit, int m)
{
    return m >= 0 && m < edit->count ? &edit->menus[m] : NULL;
}

// A function to find a menu by its name, exactly, as the parser matches names; -1 when none has it
int menuedit_find(const MenuEdit *edit, const char *name)
{
    for (int m = 0; m < edit->count; m++) {
        if (strcmp(edit->menus[m].name, name) == 0)
            return m;
    }
    return -1;
}

// A function to name the default menu, after any rename; NULL when none was given
const char *menuedit_default(const MenuEdit *edit)
{
    return edit->default_menu;
}

// A function to find a menu as read by its name; NULL for none (a new menu has no name as read)
static const MenuEditMenu *read_menu(const MenuEdit *edit, const char *name)
{
    for (int r = 0; name != NULL && r < edit->read_count; r++) {
        if (strcmp(edit->read[r].name, name) == 0)
            return &edit->read[r];
    }
    return NULL;
}

// A function to tell whether a tile differs from the one read from its line: a new or moved-in one does
static bool tile_changed(const MenuEditMenu *read, const MenuTile *tile)
{
    for (int r = 0; read != NULL && tile->original != NULL && r < read->count; r++) {
        const MenuTile *was = &read->tiles[r];
        if (strcmp(was->original, tile->original) == 0)
            return strcmp(was->title, tile->title) != 0 || strcmp(was->icon, tile->icon) != 0 ||
                   strcmp(was->command, tile->command) != 0;
    }
    return true;
}

// A function to find the menu read under a name; -1 when the model removed it
static int find_original(const MenuEdit *edit, const char *name)
{
    for (int m = 0; m < edit->count; m++) {
        if (edit->menus[m].original_name != NULL && strcmp(edit->menus[m].original_name, name) == 0)
            return m;
    }
    return -1;
}

// A function to tell whether a menu read from the file has another name now
static bool renamed(const MenuEditMenu *menu)
{
    return menu->original_name != NULL && strcmp(menu->name, menu->original_name) != 0;
}

// A function to tell whether a menu differs from how it was read: new, renamed, or a tile added,
// removed, moved or changed
static bool menu_changed(const MenuEdit *edit, const MenuEditMenu *menu)
{
    const MenuEditMenu *read = read_menu(edit, menu->original_name);
    if (read == NULL || strcmp(menu->name, read->name) != 0 || menu->count != read->count)
        return true;
    for (int t = 0; t < menu->count; t++) {
        const MenuTile *tile = &menu->tiles[t];
        if (tile->original == NULL || strcmp(tile->original, read->tiles[t].original) != 0 || tile_changed(read, tile))
            return true;
    }
    return false;
}

// A function to tell whether the model differs from how it was read: a menu changed, added or removed
bool menuedit_changed(const MenuEdit *edit)
{
    for (int m = 0; m < edit->count; m++) {
        if (menu_changed(edit, &edit->menus[m]))
            return true;
    }
    for (int r = 0; r < edit->read_count; r++) {
        if (find_original(edit, edit->read[r].name) < 0)
            return true;
    }
    return false;
}

// A function to say why a tile's new value would not fit the line it was read from, as inidoc_list_set()
// writes it there, or NULL: the line's spacing and any comment after its value count toward its 199
// bytes (inidoc_check_in(), on a document of that line alone). Out of memory when that document cannot
// be made. Every other reason inidoc_check_in() has, tile_problem() gives first.
static const char *own_line_problem(const char *key, const char *original, const char *value)
{
    size_t header = strlen(OWN_HEADER);
    size_t length = strlen(original);
    char *text = alloc_malloc(header + length + 1);
    if (text == NULL)
        return OUT_OF_MEMORY;
    memcpy(text, OWN_HEADER, header);
    memcpy(text + header, original, length + 1);
    IniDoc *doc = inidoc_parse(text, header + length);
    alloc_free(text);
    if (doc == NULL)
        return OUT_OF_MEMORY;
    const char *why = inidoc_check_in(doc, OWN_SECTION, key, value);
    inidoc_free(doc);
    if (why == NULL)
        return NULL;
    return strstr(why, "comment") != NULL ? LINE_TOO_LONG_COMMENT : LINE_TOO_LONG_SPACING;
}

// A function to say why a tile could not be written as key=title;icon;command so that the parser reads
// it back as it is, or NULL. `key` is the tile's own, or the key a new one is weighed with (room_key());
// `original` is the tile's line as read, NULL for one numbered at the save. These are inidoc_check()'s
// rules for this shape of line, so inidoc never refuses what passes them; a tile with a line is then
// weighed in it, as inidoc_check_in() weighs it.
static const char *tile_problem(const char *key, const char *original, const char *title, const char *icon,
                                const char *command)
{
    if (title[0] == '\0')
        return "Type a title";
    if (icon[0] == '\0')
        return "Choose an icon";
    if (command[0] == '\0')
        return "Choose a command";
    if (strpbrk(title, "\r\n") != NULL || strpbrk(icon, "\r\n") != NULL || strpbrk(command, "\r\n") != NULL)
        return "It can't contain a line break";
    if (strchr(title, ';') != NULL)
        return "A title can't contain ;";
    if (edge_space(title))
        return "A title can't start or end with a space";
    if (strchr(icon, ';') != NULL)
        return "An icon's name or path can't contain ;";
    if (edge_space(icon))
        return "An icon's name or path can't start or end with a space";
    if (edge_space(command))
        return "A command can't start or end with a space";
    if (comment_inside(command))
        return "A command can't have ; after a space";
    if (strcmp(command, SELECT) == 0)
        return "A tile can't run :select";
    size_t length = strlen(key != NULL ? key : KEY_ROOM) + 1 + strlen(title) + 1 + strlen(icon) + 1 + strlen(command);
    if (length > INIDOC_MAX_LINE)
        return LINE_TOO_LONG;
    if (original == NULL || key == NULL)
        return NULL;
    char value[INIDOC_MAX_LINE + 1];
    snprintf(value, sizeof(value), "%s;%s;%s", title, icon, command);   // It fits: the length is weighed above
    return own_line_problem(key, original, value);
}

// A function to give, into `out`, the key a new tile in a menu is weighed with: KEY_ROOM, or "Entry" and one
// digit more than the menu's longest EntryN number when that is longer (a hand-numbered Entry100000), since
// the save numbers a new tile one above the section's highest (config_save_next_key())
static void room_key(const MenuEditMenu *menu, char *out, size_t size)
{
    size_t stem = strlen(ENTRY_STEM);
    size_t length = strlen(KEY_ROOM);
    for (int t = 0; t < menu->count; t++) {
        const char *key = menu->tiles[t].key;
        if (key == NULL || strncmp(key, ENTRY_STEM, stem) != 0 || key[stem] == '\0' ||
            strspn(key + stem, "0123456789") != strlen(key + stem))
            continue;
        const char *digits = key + stem + strspn(key + stem, "0");
        if (stem + strlen(digits) + 1 > length)
            length = stem + strlen(digits) + 1;
    }
    if (length > size - 1)
        length = size - 1;
    memcpy(out, ENTRY_STEM, stem);
    memset(out + stem, '9', length - stem);
    out[length] = '\0';
}

// A function to give the key a tile in a menu is weighed with: its own, or for a new tile (numbered only at the
// save) the menu's room_key(), put into `room`
static const char *weighed_key(const MenuEditMenu *menu, const MenuTile *tile, char *room, size_t size)
{
    if (tile->key != NULL)
        return tile->key;
    room_key(menu, room, size);
    return room;
}

// A function to say why a new tile could not be written, or NULL. It knows no menu, so it weighs the line
// with KEY_ROOM; menuedit_add() weighs it with its menu's room_key().
const char *menuedit_check_tile(const char *title, const char *icon, const char *command)
{
    return tile_problem(NULL, NULL, title, icon, command);
}

// A function to say why menuedit_set() would refuse a tile's new title, icon and command, or NULL: the rules,
// with the tile's own key (a new tile's: its menu's room_key()), and its own line as read, its comment and
// spacing counted. Out of memory, too, when the line cannot be weighed.
const char *menuedit_check_set(const MenuEdit *edit, int m, int t, const char *title, const char *icon, const char *command)
{
    if (m < 0 || m >= edit->count || t < 0 || t >= edit->menus[m].count)
        return NO_SUCH_TILE;
    const MenuTile *tile = &edit->menus[m].tiles[t];
    char room[INIDOC_MAX_LINE + 1];
    return tile_problem(weighed_key(&edit->menus[m], tile, room, sizeof(room)), tile->original, title, icon, command);
}

// A function to say why a menu could not have a name, or NULL. `except` is the menu being renamed, which
// may keep its name in another case; -1 for a new one. A reserved or existing name is refused in any
// case: inih would read Games and games as two menus, which read the same on screen.
const char *menuedit_check_name(const MenuEdit *edit, const char *name, int except)
{
    if (name[0] == '\0')
        return "Type a name";
    if (strpbrk(name, "\r\n") != NULL)
        return "It can't contain a line break";
    if (edge_space(name))
        return "A name can't start or end with a space";
    if (strchr(name, ']') != NULL)
        return "A name can't contain ]";
    if (strchr(name, ';') != NULL)
        return "A name can't contain ;";
    if (strlen(name) > (size_t) INIDOC_MAX_SECTION)
        return "A name can be 49 bytes at most";
    if (reserved(name, true))
        return "That name is kept for settings";
    for (int m = 0; m < edit->count; m++) {
        if (m != except && same_name(edit->menus[m].name, name))
            return "There's already a menu with that name";
    }
    return NULL;
}

// A function to give the menu a tile opens: all after ":submenu ", as execute_command() takes it; NULL
// for a tile that opens none
const char *menuedit_submenu_of(const MenuTile *tile)
{
    size_t length = strlen(SUBMENU);
    if (strncmp(tile->command, SUBMENU, length) != 0 || tile->command[length] == '\0')
        return NULL;
    return tile->command + length;
}

// A function to tell whether a tile opens a menu: its command is exactly ":submenu <name>"
static bool opens(const MenuTile *tile, const char *name)
{
    const char *submenu = menuedit_submenu_of(tile);
    return submenu != NULL && strcmp(submenu, name) == 0;
}

// A function to count the tiles, in every menu, that open a menu
int menuedit_openers(const MenuEdit *edit, const char *name)
{
    int openers = 0;
    for (int m = 0; m < edit->count; m++) {
        for (int t = 0; t < edit->menus[m].count; t++) {
            if (opens(&edit->menus[m].tiles[t], name))
                openers++;
        }
    }
    return openers;
}

// A function to tell whether a menu has a tile
static bool has_tile(const MenuEdit *edit, int m, int t)
{
    return m >= 0 && m < edit->count && t >= 0 && t < edit->menus[m].count;
}

// A function to tell whether a tile's line has no key ("=Title;icon;command", a tile to the parser), which
// inidoc refuses to remove or move: the indented lines after it would join the key before it
static bool keyless(const MenuTile *tile)
{
    return tile->key != NULL && tile->key[0] == '\0';
}

// A function to tell whether a menu holds a tile whose line has no key, which putting its tiles in another
// order could move
static bool has_keyless(const MenuEditMenu *menu)
{
    for (int t = 0; t < menu->count; t++) {
        if (keyless(&menu->tiles[t]))
            return true;
    }
    return false;
}

// A function to put a tile into a menu at an index, taking over what it holds; false when out of memory,
// with the menu as it was
static bool insert_tile(MenuEditMenu *menu, int at, const MenuTile *tile)
{
    MenuTile *tiles = alloc_realloc(menu->tiles, (size_t) (menu->count + 1) * sizeof(MenuTile));
    if (tiles == NULL)
        return false;
    memmove(&tiles[at + 1], &tiles[at], (size_t) (menu->count - at) * sizeof(MenuTile));
    tiles[at] = *tile;
    menu->tiles = tiles;
    menu->count++;
    return true;
}

// A function to take a tile out of a menu, freeing nothing: the caller has what it held
static void take_tile(MenuEditMenu *menu, int t)
{
    memmove(&menu->tiles[t], &menu->tiles[t + 1], (size_t) (menu->count - t - 1) * sizeof(MenuTile));
    menu->count--;
}

// A function to find the move that brought a tile where it is; -1 for one that was not moved from the menu it
// was read in
static int find_move(const MenuEdit *edit, int m, int t)
{
    for (int k = 0; k < edit->move_count; k++) {
        if (edit->moves[k].menu == m && edit->moves[k].tile == t)
            return k;
    }
    return -1;
}

// A function to forget a move: its tile went from the model
static void forget_move(MenuEdit *edit, int k)
{
    alloc_free(edit->moves[k].original);
    memmove(&edit->moves[k], &edit->moves[k + 1], (size_t) (edit->move_count - k - 1) * sizeof(MenuMove));
    edit->move_count--;
}

// A function to keep the moves in step with the tiles of menu m from index `from` on, which moved `by` places
static void shift_moves(MenuEdit *edit, int m, int from, int by)
{
    for (int k = 0; k < edit->move_count; k++) {
        if (edit->moves[k].menu == m && edit->moves[k].tile >= from)
            edit->moves[k].tile += by;
    }
}

// A function to put a tile into menu m at an index, as insert_tile() does, with the moves kept in step
static bool put_tile(MenuEdit *edit, int m, int at, const MenuTile *tile)
{
    if (!insert_tile(&edit->menus[m], at, tile))
        return false;
    shift_moves(edit, m, at, 1);
    return true;
}

// A function to remove tile t of menu m and free it, with the moves kept in step: its own is forgotten
static void drop_tile(MenuEdit *edit, int m, int t)
{
    int k = find_move(edit, m, t);
    if (k >= 0)
        forget_move(edit, k);
    free_tile(&edit->menus[m].tiles[t]);
    take_tile(&edit->menus[m], t);
    shift_moves(edit, m, t + 1, -1);
}

// A function to add a tile after the tile `after`, or at the end when it is -1; it has no key or line
// until the save numbers and writes it
const char *menuedit_add(MenuEdit *edit, int m, int after, const char *title, const char *icon, const char *command)
{
    if (m < 0 || m >= edit->count)
        return NO_SUCH_MENU;
    if (after < -1 || after >= edit->menus[m].count)
        return NO_SUCH_TILE;
    char room[INIDOC_MAX_LINE + 1];
    room_key(&edit->menus[m], room, sizeof(room));
    const char *why = tile_problem(room, NULL, title, icon, command);
    if (why != NULL)
        return why;
    MenuTile tile;
    memset(&tile, 0, sizeof(tile));
    bool ok = true;
    tile.title = copy_text(title, &ok);
    tile.icon = copy_text(icon, &ok);
    tile.command = copy_text(command, &ok);
    if (!ok || !put_tile(edit, m, after < 0 ? edit->menus[m].count : after + 1, &tile)) {
        free_tile(&tile);
        return OUT_OF_MEMORY;
    }
    return NULL;
}

// A function to change a tile's title, icon and command. It keeps its key and its line as read, which
// the save finds the line by.
const char *menuedit_set(MenuEdit *edit, int m, int t, const char *title, const char *icon, const char *command)
{
    if (!has_tile(edit, m, t))
        return NO_SUCH_TILE;
    MenuTile *tile = &edit->menus[m].tiles[t];
    char room[INIDOC_MAX_LINE + 1];
    const char *why = tile_problem(weighed_key(&edit->menus[m], tile, room, sizeof(room)), tile->original, title, icon, command);
    if (why != NULL)
        return why;
    bool ok = true;
    char *new_title = copy_text(title, &ok);
    char *new_icon = copy_text(icon, &ok);
    char *new_command = copy_text(command, &ok);
    if (!ok) {
        alloc_free(new_title);
        alloc_free(new_icon);
        alloc_free(new_command);
        return OUT_OF_MEMORY;
    }
    alloc_free(tile->title);
    alloc_free(tile->icon);
    alloc_free(tile->command);
    tile->title = new_title;
    tile->icon = new_icon;
    tile->command = new_command;
    return NULL;
}

// A function to remove a tile. One that opens a menu the model has goes through menuedit_remove_menu(),
// which the confirm page asks; one whose menu is gone is a tile like any other. One whose line has no key
// cannot go.
const char *menuedit_remove(MenuEdit *edit, int m, int t)
{
    if (!has_tile(edit, m, t))
        return NO_SUCH_TILE;
    if (keyless(&edit->menus[m].tiles[t]))
        return NO_KEY;
    const char *submenu = menuedit_submenu_of(&edit->menus[m].tiles[t]);
    if (submenu != NULL && menuedit_find(edit, submenu) >= 0)
        return "It opens a submenu";
    drop_tile(edit, m, t);
    return NULL;
}

// A function to move a tile within its menu, so it ends up at index `to`. Not in a menu with a line that has
// no key, which the save's new order could move.
const char *menuedit_move(MenuEdit *edit, int m, int from, int to)
{
    if (!has_tile(edit, m, from) || !has_tile(edit, m, to))
        return NO_SUCH_TILE;
    if (has_keyless(&edit->menus[m]))
        return KEYLESS_MENU;
    MenuEditMenu *menu = &edit->menus[m];
    MenuTile tile = menu->tiles[from];
    int k = find_move(edit, m, from);
    take_tile(menu, from);
    memmove(&menu->tiles[to + 1], &menu->tiles[to], (size_t) (menu->count - to) * sizeof(MenuTile));
    menu->tiles[to] = tile;
    menu->count++;
    shift_moves(edit, m, from + 1, -1);
    shift_moves(edit, m, to, 1);
    if (k >= 0)
        edit->moves[k].tile = to;
    return NULL;
}

// A function to move a tile to the end of another menu. It is a new line there (its key and its line as
// read go), so its line is weighed with a new tile's key. A tile read from the file keeps its line as read
// in a move, which follows it from menu to menu, for the save to keep in the menu it was read in when the
// changes to the menu it is in are skipped.
const char *menuedit_move_to_menu(MenuEdit *edit, int m, int t, int to_menu)
{
    if (!has_tile(edit, m, t))
        return NO_SUCH_TILE;
    if (to_menu < 0 || to_menu >= edit->count)
        return NO_SUCH_MENU;
    if (to_menu == m)
        return "It's in that menu already";
    MenuTile *tile = &edit->menus[m].tiles[t];
    if (keyless(tile))
        return NO_KEY;
    if (opens(tile, edit->menus[to_menu].name))
        return "A menu can't hold a tile that opens it";
    MenuEditMenu *target = &edit->menus[to_menu];
    char room[INIDOC_MAX_LINE + 1];
    room_key(target, room, sizeof(room));
    const char *why = tile_problem(room, NULL, tile->title, tile->icon, tile->command);
    if (why != NULL)
        return why;
    MenuTile *tiles = alloc_realloc(target->tiles, (size_t) (target->count + 1) * sizeof(MenuTile));
    if (tiles == NULL)
        return OUT_OF_MEMORY;
    target->tiles = tiles;   // Room for one more; the count says whether it is used
    int k = find_move(edit, m, t);
    const MenuEditMenu *read = read_menu(edit, edit->menus[m].original_name);
    if (k < 0 && tile->original != NULL && read != NULL) {
        MenuMove *moves = alloc_realloc(edit->moves, (size_t) (edit->move_count + 1) * sizeof(MenuMove));
        if (moves == NULL)
            return OUT_OF_MEMORY;
        edit->moves = moves;
        k = edit->move_count++;
        edit->moves[k].read = (int) (read - edit->read);
        edit->moves[k].original = tile->original;   // The move takes the line as read over from the tile
        tile->original = NULL;
    }
    MenuTile moving = *tile;
    alloc_free(moving.key);
    alloc_free(moving.original);
    moving.key = NULL;
    moving.original = NULL;
    take_tile(&edit->menus[m], t);
    shift_moves(edit, m, t + 1, -1);
    target->tiles[target->count++] = moving;
    if (k >= 0) {
        edit->moves[k].menu = to_menu;
        edit->moves[k].tile = target->count - 1;
    }
    return NULL;
}

// A function to make a submenu: an empty menu at the end, and a tile that opens it, titled with its name,
// after the tile `after` in menu m (or at its end for -1). It gets no Back tile: Back and B go back.
const char *menuedit_add_submenu(MenuEdit *edit, int m, int after, const char *name, const char *icon)
{
    if (m < 0 || m >= edit->count)
        return NO_SUCH_MENU;
    if (after < -1 || after >= edit->menus[m].count)
        return NO_SUCH_TILE;
    const char *why = menuedit_check_name(edit, name, -1);
    if (why != NULL)
        return why;
    char command[INIDOC_MAX_LINE + 1];
    snprintf(command, sizeof(command), "%s%s", SUBMENU, name);   // A name is 49 bytes at most: it fits
    char room[INIDOC_MAX_LINE + 1];
    room_key(&edit->menus[m], room, sizeof(room));
    why = tile_problem(room, NULL, name, icon, command);
    if (why != NULL)
        return why;
    MenuEditMenu menu;
    MenuTile tile;
    memset(&menu, 0, sizeof(menu));
    memset(&tile, 0, sizeof(tile));
    bool ok = true;
    menu.name = copy_text(name, &ok);
    menu.tiles = alloc_calloc(0, sizeof(MenuTile));
    tile.title = copy_text(name, &ok);
    tile.icon = copy_text(icon, &ok);
    tile.command = copy_text(command, &ok);
    MenuEditMenu *menus = ok && menu.tiles != NULL
                        ? alloc_realloc(edit->menus, (size_t) (edit->count + 1) * sizeof(MenuEditMenu)) : NULL;
    if (menus != NULL)
        edit->menus = menus;   // Room for one more; the count says whether it is used
    if (menus == NULL || !put_tile(edit, m, after < 0 ? edit->menus[m].count : after + 1, &tile)) {
        free_menu(&menu);
        free_tile(&tile);
        return OUT_OF_MEMORY;
    }
    edit->menus[edit->count++] = menu;
    return NULL;
}

// A function to rename a menu: every tile whose command is exactly ":submenu <old>" opens it by its new
// name, and takes the new name as its title when its title was the old name (a title of its own stays);
// the default menu follows. A rename any of those lines has no room for (its comment and spacing counted)
// is refused with that reason.
// Every copy is made before anything changes, so running out of memory changes nothing.
const char *menuedit_rename_menu(MenuEdit *edit, int m, const char *new_name)
{
    if (m < 0 || m >= edit->count)
        return NO_SUCH_MENU;
    const char *why = menuedit_check_name(edit, new_name, m);
    if (why != NULL)
        return why;
    const char *old_name = edit->menus[m].name;
    if (strcmp(old_name, new_name) == 0)
        return NULL;
    char command[INIDOC_MAX_LINE + 1];
    snprintf(command, sizeof(command), "%s%s", SUBMENU, new_name);   // A name is 49 bytes at most: it fits
    int openers = 0;
    for (int k = 0; k < edit->count; k++) {
        for (int t = 0; t < edit->menus[k].count; t++) {
            const MenuTile *tile = &edit->menus[k].tiles[t];
            if (!opens(tile, old_name))
                continue;
            char room[INIDOC_MAX_LINE + 1];
            why = tile_problem(weighed_key(&edit->menus[k], tile, room, sizeof(room)), tile->original,
                               strcmp(tile->title, old_name) == 0 ? new_name : tile->title, tile->icon, command);
            if (why != NULL)
                return why;
            openers++;
        }
    }

    // Each opener's new command, then its new title or NULL, in the order the openers are met
    bool ok = true;
    char *name = copy_text(new_name, &ok);
    char *default_menu = edit->default_menu != NULL && strcmp(edit->default_menu, old_name) == 0
                       ? copy_text(new_name, &ok) : NULL;
    char **texts = alloc_calloc((size_t) openers * 2, sizeof(char *));
    ok = ok && texts != NULL;
    int n = 0;
    for (int k = 0; ok && k < edit->count; k++) {
        for (int t = 0; ok && t < edit->menus[k].count; t++) {
            const MenuTile *tile = &edit->menus[k].tiles[t];
            if (!opens(tile, old_name))
                continue;
            texts[n] = copy_text(command, &ok);
            if (strcmp(tile->title, old_name) == 0)
                texts[n + 1] = copy_text(new_name, &ok);
            n += 2;
        }
    }
    if (!ok) {
        for (int i = 0; texts != NULL && i < n; i++)
            alloc_free(texts[i]);
        alloc_free(texts);
        alloc_free(name);
        alloc_free(default_menu);
        return OUT_OF_MEMORY;
    }
    n = 0;
    for (int k = 0; k < edit->count; k++) {
        for (int t = 0; t < edit->menus[k].count; t++) {
            MenuTile *tile = &edit->menus[k].tiles[t];
            if (!opens(tile, old_name))
                continue;
            alloc_free(tile->command);
            tile->command = texts[n];
            if (texts[n + 1] != NULL) {
                alloc_free(tile->title);
                tile->title = texts[n + 1];
            }
            n += 2;
        }
    }
    alloc_free(texts);
    alloc_free(edit->menus[m].name);   // old_name: its last use is above
    edit->menus[m].name = name;
    if (default_menu != NULL) {
        alloc_free(edit->default_menu);
        edit->default_menu = default_menu;
    }
    return NULL;
}

// A function to remove a submenu tile `t` of menu m, as the confirm page chooses: the tile alone, only
// while another tile opens its menu too; or the menu with its tiles, and every tile anywhere that opens
// it, none of which would open anything. The menus it opens stay. Never the default menu, and never a tile
// whose line has no key (the tile asked, or, with its tiles, any other that opens the menu). It allocates
// nothing.
const char *menuedit_remove_menu(MenuEdit *edit, int m, int t, bool with_tiles)
{
    if (!has_tile(edit, m, t))
        return NO_SUCH_TILE;
    if (keyless(&edit->menus[m].tiles[t]))
        return NO_KEY;
    const char *submenu = menuedit_submenu_of(&edit->menus[m].tiles[t]);
    int s = submenu != NULL ? menuedit_find(edit, submenu) : -1;
    if (s < 0)
        return "It doesn't open a submenu";
    if (!with_tiles) {
        if (menuedit_openers(edit, submenu) < 2)
            return "No other tile opens that menu";
        drop_tile(edit, m, t);
        return NULL;
    }
    if (edit->default_menu != NULL && strcmp(edit->default_menu, submenu) == 0)
        return "The default menu can't be removed";
    for (int k = 0; k < edit->count; k++) {
        for (int i = 0; k != s && i < edit->menus[k].count; i++) {
            if (keyless(&edit->menus[k].tiles[i]) && opens(&edit->menus[k].tiles[i], submenu))
                return KEYLESS_OPENER;
        }
    }

    // `submenu` points into the tile about to go: the menu's own name is used from here on
    for (int k = 0; k < edit->count; k++) {
        MenuEditMenu *menu = &edit->menus[k];
        for (int i = menu->count - 1; i >= 0; i--) {
            if (k != s && opens(&menu->tiles[i], edit->menus[s].name))
                drop_tile(edit, k, i);
        }
    }
    free_menu(&edit->menus[s]);
    memmove(&edit->menus[s], &edit->menus[s + 1], (size_t) (edit->count - s - 1) * sizeof(MenuEditMenu));
    edit->count--;
    for (int k = edit->move_count - 1; k >= 0; k--) {   // The moves of its tiles go with it; those after it move up
        if (edit->moves[k].menu == s)
            forget_move(edit, k);
        else if (edit->moves[k].menu > s)
            edit->moves[k].menu--;
    }
    return NULL;
}

// A function to fail a save, with its reason
static bool fail(ConfigSaveResult *result, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(result->why, sizeof(result->why), format, arguments);
    va_end(arguments);
    return false;
}

// A function to note what the save could not make as asked, in 3b's words for a line
static void note(ConfigSaveResult *result, ConfigNoteKind kind, const char *format, const char *text)
{
    char line[CONFIG_SAVE_NOTES_SIZE];
    snprintf(line, sizeof(line), format, text);
    config_save_note(result, kind, line);
}

// A function to write a tile's value, title;icon;command. Every tile written passed tile_problem(), so it fits.
static void format_value(const MenuTile *tile, char *out, size_t size)
{
    snprintf(out, size, "%s;%s;%s", tile->title, tile->icon, tile->command);
}

// A function to list a menu section's entry lines in order: every key line but the menu's own settings.
// Into new memory (caller frees); NULL when out of memory.
static IniDocItem *entry_items(const IniDoc *doc, const char *section, int *count)
{
    int all = inidoc_list(doc, section, NULL, NULL, 0);
    IniDocItem *items = alloc_calloc((size_t) all, sizeof(IniDocItem));
    *count = 0;
    if (items != NULL) {
        inidoc_list(doc, section, NULL, items, all);
        for (int i = 0; i < all; i++) {
            if (!is_setting_line(&items[i]))
                items[(*count)++] = items[i];
        }
    }
    return items;
}

// A function to find the line of a menu section's entry at a rank; false (with the reason) when out of memory
static bool entry_line(const IniDoc *doc, const char *section, int rank, int *line, ConfigSaveResult *result)
{
    int count = 0;
    IniDocItem *items = entry_items(doc, section, &count);
    if (items == NULL)
        return fail(result, "out of memory");
    *line = items[rank].line;
    alloc_free(items);
    return true;
}

// A function to find a tile's rank among the section's entries; -1 when it has no line
static int rank_of(const int *owner, int count, int t)
{
    for (int r = 0; r < count; r++) {
        if (owner[r] == t)
            return r;
    }
    return -1;
}

// A function to mark each tile as read that a tile now holds, by its text: a line read twice and held
// once is held once
static void mark_held(const MenuEditMenu *menu, const MenuEditMenu *read, bool *held)
{
    for (int t = 0; read != NULL && t < menu->count; t++) {
        if (menu->tiles[t].original == NULL)
            continue;
        for (int r = 0; r < read->count; r++) {
            if (!held[r] && strcmp(read->tiles[r].original, menu->tiles[t].original) == 0) {
                held[r] = true;
                break;
            }
        }
    }
}

// A function to mark, in `kept` (the tiles held, to begin with), the line of each tile moved from this menu
// as read to a menu whose changes are skipped: the move is skipped, so its line stays, with a note
static void keep_moved(const MenuEdit *edit, const MenuEditMenu *read, const bool *skipped, bool *kept,
                       ConfigSaveResult *result)
{
    for (int k = 0; read != NULL && k < edit->move_count; k++) {
        const MenuMove *move = &edit->moves[k];
        if (&edit->read[move->read] != read || !skipped[move->menu])
            continue;
        int r = 0;
        while (r < read->count && (kept[r] || strcmp(read->tiles[r].original, move->original) != 0))
            r++;
        if (r < read->count)
            kept[r] = true;
        char line[CONFIG_SAVE_NOTES_SIZE];
        snprintf(line, sizeof(line), "'%s' is not moved to [%s], since the changes to that menu are skipped", move->original,
                 edit->menus[move->menu].name);
        config_save_note(result, CONFIG_NOTE_SECTION_GONE, line);
    }
}

// A function to remove the lines of the tiles as read that are not kept (held by a tile, or kept by a skipped
// move), found by their text as read. One a hand edit removed or changed meanwhile is not there: its removal
// is skipped, with a note, as 3b's are.
static bool remove_tiles(IniDoc *doc, const MenuEditMenu *menu, const MenuEditMenu *read, const bool *kept,
                         ConfigSaveResult *result)
{
    for (int r = 0; read != NULL && r < read->count; r++) {
        if (kept[r])
            continue;
        const char *original = read->tiles[r].original;
        int line = inidoc_find_line(doc, menu->name, original);
        if (line < 0)
            note(result, CONFIG_NOTE_REMOVAL_SKIPPED, "'%s' is not there any more, so its removal is skipped", original);
        else if (!inidoc_list_remove(doc, line))
            return fail(result, "the line '%s' in [%s] cannot be removed: %s", original, menu->name, inidoc_why(doc));
    }
    return true;
}

// A function to find each tile's line by its text as read, the first not yet found. A tile whose line is
// not there was removed or changed by hand meanwhile: one the model changed is written as a new line,
// with a note; one it did not change is left gone, as the hand edit left it.
static void match_tiles(const MenuEditMenu *menu, const MenuEditMenu *read, const IniDocItem *items, int count,
                        int *owner, bool *gone, ConfigSaveResult *result)
{
    for (int r = 0; r < count; r++)
        owner[r] = -1;
    for (int t = 0; t < menu->count; t++) {
        const MenuTile *tile = &menu->tiles[t];
        if (tile->original == NULL)
            continue;
        int r = 0;
        while (r < count && (owner[r] >= 0 || strcmp(items[r].text, tile->original) != 0))
            r++;
        if (r < count)
            owner[r] = t;
        else if (tile_changed(read, tile))
            note(result, CONFIG_NOTE_CHANGED_MEANWHILE, "'%s' changed meanwhile, so its change is written as a new line",
                 tile->original);
        else
            gone[t] = true;
    }
}

// A function to write each changed tile into its own line, keeping its key, its spacing and its comment
static bool set_tiles(IniDoc *doc, const MenuEditMenu *menu, const MenuEditMenu *read, const IniDocItem *items,
                      int count, const int *owner, ConfigSaveResult *result)
{
    for (int r = 0; r < count; r++) {
        if (owner[r] < 0)
            continue;
        const MenuTile *tile = &menu->tiles[owner[r]];
        if (!tile_changed(read, tile))
            continue;
        char value[INIDOC_MAX_LINE + 1];
        format_value(tile, value, sizeof(value));
        if (!inidoc_list_set(doc, items[r].line, tile->key, value))
            return fail(result, "the %s value in [%s] cannot be written: %s", tile->key, menu->name, inidoc_why(doc));
    }
    return true;
}

// A function to tell whether the model moved a tile it read: its tiles as read, in its order, against the
// lines it holds, in the order read. When it moved none, the order the file has now is kept, a hand edit's
// included.
static bool reordered(const MenuEditMenu *menu, const MenuEditMenu *read, const bool *held)
{
    int r = 0;
    for (int t = 0; t < menu->count; t++) {
        if (menu->tiles[t].original == NULL)
            continue;
        while (r < read->count && !held[r])
            r++;
        if (r == read->count || strcmp(read->tiles[r].original, menu->tiles[t].original) != 0)
            return true;
        r++;
    }
    return false;
}

// A function to put the tiles that have lines in the model's order, moving only them: the lines a hand
// edit added meanwhile and the menu's settings stay where they are. The j-th tile with a line goes to the
// j-th entry a tile holds, moved up to just before the line there.
static bool order_tiles(IniDoc *doc, const MenuEditMenu *menu, int *owner, int count, ConfigSaveResult *result)
{
    int slot = 0;
    for (int t = 0; t < menu->count; t++) {
        int at = rank_of(owner, count, t);
        if (at < 0)
            continue;
        while (owner[slot] < 0)
            slot++;
        if (at != slot) {
            int line = 0;
            int before = 0;
            if (!entry_line(doc, menu->name, at, &line, result) || !entry_line(doc, menu->name, slot, &before, result))
                return false;
            if (!inidoc_list_move(doc, line, before))
                return fail(result, "the line '%s' in [%s] cannot be moved: %s", menu->tiles[t].original, menu->name,
                            inidoc_why(doc));
            memmove(&owner[slot + 1], &owner[slot], (size_t) (at - slot) * sizeof(int));
            owner[slot] = t;
        }
        slot++;
    }
    return true;
}

// A function to write the tiles that have no line, in the model's order: each goes before the next tile
// in the model's order that has a line, else after the section's last key, numbered one above the
// highest EntryN there, so the numbers rise in the model's order
static bool insert_tiles(IniDoc *doc, const MenuEditMenu *menu, int *owner, int count, const bool *gone,
                         ConfigSaveResult *result)
{
    for (int t = 0; t < menu->count; t++) {
        if (gone[t] || rank_of(owner, count, t) >= 0)
            continue;
        int next = -1;
        for (int k = t + 1; k < menu->count && next < 0; k++)
            next = rank_of(owner, count, k);
        int before = -1;
        char key[INIDOC_MAX_LINE + 1];
        char value[INIDOC_MAX_LINE + 1];
        if ((next >= 0 && !entry_line(doc, menu->name, next, &before, result)) ||
            !config_save_next_key(doc, menu->name, ENTRY_STEM, key, sizeof(key), result))
            return false;
        format_value(&menu->tiles[t], value, sizeof(value));
        if (!inidoc_list_insert(doc, menu->name, before, key, value))
            return fail(result, "the %s value in [%s] cannot be written: %s", key, menu->name, inidoc_why(doc));
        int at = next >= 0 ? next : count;
        memmove(&owner[at + 1], &owner[at], (size_t) (count - at) * sizeof(int));
        owner[at] = t;
        count++;
    }
    return true;
}

// A function to make a changed menu's section match the model (plan ruling 4): the lines of removed tiles
// go, changed tiles are written into their own lines, the tiles are put in the model's order when it moved
// any, and new tiles are written where the model has them. Lines a hand edit added meanwhile, the lines of
// tiles moved to a menu whose changes are skipped, and the menu's Rows, Columns and IconSize, stay as they are.
static bool reconcile(const MenuEdit *edit, IniDoc *doc, const MenuEditMenu *menu, const bool *skipped,
                      ConfigSaveResult *result)
{
    const MenuEditMenu *read = read_menu(edit, menu->original_name);
    int read_count = read != NULL ? read->count : 0;
    bool *held = alloc_calloc((size_t) read_count * 2, sizeof(bool));   // Held by a tile, then kept: held or a skipped move's
    bool *gone = alloc_calloc((size_t) menu->count, sizeof(bool));
    if (held == NULL || gone == NULL) {
        alloc_free(held);
        alloc_free(gone);
        return fail(result, "out of memory");
    }
    mark_held(menu, read, held);
    bool *kept = held + read_count;
    memcpy(kept, held, (size_t) read_count * sizeof(bool));
    keep_moved(edit, read, skipped, kept, result);
    bool ok = remove_tiles(doc, menu, read, kept, result);
    int count = 0;
    IniDocItem *items = ok ? entry_items(doc, menu->name, &count) : NULL;
    int *owner = items != NULL ? alloc_calloc((size_t) (count + menu->count), sizeof(int)) : NULL;
    if (ok && owner == NULL)
        ok = fail(result, "out of memory");
    if (ok) {
        match_tiles(menu, read, items, count, owner, gone, result);
        ok = set_tiles(doc, menu, read, items, count, owner, result);
    }
    alloc_free(items);
    if (ok && reordered(menu, read, held))
        ok = order_tiles(doc, menu, owner, count, result);
    ok = ok && insert_tiles(doc, menu, owner, count, gone, result);
    alloc_free(owner);
    alloc_free(held);
    alloc_free(gone);
    return ok;
}

// A function to remove the sections of the menus the model removed. One a hand edit removed or renamed
// meanwhile is not there to remove: the removal is skipped, with a note.
static void remove_menus(const MenuEdit *edit, IniDoc *doc, ConfigSaveResult *result)
{
    for (int r = 0; r < edit->read_count; r++) {
        const char *name = edit->read[r].name;
        if (find_original(edit, name) >= 0)
            continue;
        if (!inidoc_has_section(doc, name))
            note(result, CONFIG_NOTE_SECTION_GONE, "[%s] is not there any more, so its removal is skipped", name);
        else
            inidoc_remove_section(doc, name);
    }
}

// A function to find the menus a hand edit renamed or removed meanwhile: their sections are not there, so
// all their edits are skipped, with a note when there were any. One the model renamed fails the save, as a
// clash does: the tiles that open it in other menus already open it by its new name, which no section would
// have, and they are saved with those menus.
static bool find_gone(const MenuEdit *edit, const IniDoc *doc, bool *skipped, ConfigSaveResult *result)
{
    for (int m = 0; m < edit->count; m++) {
        const MenuEditMenu *menu = &edit->menus[m];
        if (menu->original_name == NULL || inidoc_has_section(doc, menu->original_name))
            continue;
        if (renamed(menu))
            return fail(result, "[%s] was renamed or removed in config.ini meanwhile, so it cannot be renamed to %s",
                        menu->original_name, menu->name);
        skipped[m] = true;
        if (menu_changed(edit, menu))
            note(result, CONFIG_NOTE_SECTION_GONE, "[%s] is not there any more, so the changes to that menu are skipped",
                 menu->original_name);
    }
    return true;
}

// A function to skip the new menus opened only from menus whose changes are skipped, which no section would
// open, with a note: until no more are found, since a new menu skipped can be the only one opening another,
// earlier or later in the model's order. A new menu nothing opens is not skipped: it is added, as asked.
static void skip_unopened(const MenuEdit *edit, bool *skipped, ConfigSaveResult *result)
{
    bool found = true;
    while (found) {
        found = false;
        for (int m = 0; m < edit->count; m++) {
            if (skipped[m] || edit->menus[m].original_name != NULL)
                continue;
            int openers = 0;
            int written = 0;
            for (int k = 0; k < edit->count; k++) {
                for (int t = 0; t < edit->menus[k].count; t++) {
                    if (opens(&edit->menus[k].tiles[t], edit->menus[m].name)) {
                        openers++;
                        written += skipped[k] ? 0 : 1;
                    }
                }
            }
            if (openers > 0 && written == 0) {
                skipped[m] = true;
                found = true;
                note(result, CONFIG_NOTE_SECTION_GONE, "[%s] is opened only from menus whose changes are skipped, so it is not added",
                     edit->menus[m].name);
            }
        }
    }
}

// A function to refuse a name a hand edit gave a section meanwhile: a menu renamed or added under it would
// join that section, since inih reads two headers of one name as one. A name a renamed menu gives up is free.
static bool check_clashes(const MenuEdit *edit, const IniDoc *doc, const bool *skipped, ConfigSaveResult *result)
{
    for (int m = 0; m < edit->count; m++) {
        const MenuEditMenu *menu = &edit->menus[m];
        if (skipped[m] || (menu->original_name != NULL && !renamed(menu)) || !inidoc_has_section(doc, menu->name))
            continue;
        if (find_original(edit, menu->name) < 0)
            return fail(result, "a section named [%s] was added to config.ini meanwhile, so no menu can take that name",
                        menu->name);
    }
    return true;
}

// A function to rename the sections of renamed menus. One whose new name no section holds is renamed straight
// to it. One whose new name a section still holds (Games to Play while Play becomes Fun) goes to a name of its
// own first, then to its new name once every other has moved, so names that swap or follow on never meet;
// that name is in neither the file nor the model's names, so no menu's new name is ever in its way.
static bool rename_menus(const MenuEdit *edit, IniDoc *doc, const bool *skipped, ConfigSaveResult *result)
{
    int *numbers = alloc_calloc((size_t) edit->count, sizeof(int));
    if (numbers == NULL)
        return fail(result, "out of memory");
    char temporary[INIDOC_MAX_SECTION + 1];
    int next = 0;
    bool ok = true;
    for (int m = 0; ok && m < edit->count; m++) {
        const MenuEditMenu *menu = &edit->menus[m];
        if (skipped[m] || !renamed(menu))
            continue;
        if (!inidoc_has_section(doc, menu->name)) {
            if (!inidoc_rename_section(doc, menu->original_name, menu->name))
                ok = fail(result, "[%s] cannot be renamed to %s: %s", menu->original_name, menu->name, inidoc_why(doc));
            continue;
        }
        do
            snprintf(temporary, sizeof(temporary), RENAMING, ++next);
        while (inidoc_has_section(doc, temporary) || menuedit_find(edit, temporary) >= 0);
        numbers[m] = next;
        if (!inidoc_rename_section(doc, menu->original_name, temporary))
            ok = fail(result, "[%s] cannot be renamed: %s", menu->original_name, inidoc_why(doc));
    }
    for (int m = 0; ok && m < edit->count; m++) {
        if (numbers[m] == 0)
            continue;
        snprintf(temporary, sizeof(temporary), RENAMING, numbers[m]);
        if (!inidoc_rename_section(doc, temporary, edit->menus[m].name))
            ok = fail(result, "[%s] cannot be renamed to %s: %s", edit->menus[m].original_name, edit->menus[m].name,
                      inidoc_why(doc));
    }
    alloc_free(numbers);
    return ok;
}

// A function to add the sections of new menus at the end of the file, but for those skipped
static bool add_menus(const MenuEdit *edit, IniDoc *doc, const bool *skipped, ConfigSaveResult *result)
{
    for (int m = 0; m < edit->count; m++) {
        const MenuEditMenu *menu = &edit->menus[m];
        if (menu->original_name == NULL && !skipped[m] && !inidoc_add_section(doc, menu->name))
            return fail(result, "the menu %s cannot be added: %s", menu->name, inidoc_why(doc));
    }
    return true;
}

// A function to point DefaultMenu at a renamed menu's new name, when the file still names its old one
static bool rename_default(const MenuEdit *edit, IniDoc *doc, const bool *skipped, ConfigSaveResult *result)
{
    const char *current = inidoc_get(doc, "General", SETTING_DEFAULT_MENU);
    for (int m = 0; current != NULL && m < edit->count; m++) {
        const MenuEditMenu *menu = &edit->menus[m];
        if (skipped[m] || !renamed(menu) || strcmp(menu->original_name, current) != 0)
            continue;
        if (!inidoc_set(doc, "General", SETTING_DEFAULT_MENU, menu->name, INIDOC_AFTER_LAST_KEY))
            return fail(result, "the %s value in [General] cannot be written: %s", SETTING_DEFAULT_MENU, inidoc_why(doc));
        return true;
    }
    return true;
}

// A function to make a fresh read of the file match the model, as config_save_with()'s document callback:
// removed menus' sections go, menus a hand edit renamed or removed meanwhile are skipped (one the model
// renamed too fails the save), and so are new menus opened only from skipped ones, renamed menus' sections
// are renamed and new ones added, each changed menu's section is reconciled (a tile moved to a skipped menu
// stays where it was), and DefaultMenu follows a rename. False, with the reason, fails the save; what could not be made as asked is noted.
bool menuedit_apply(const MenuEdit *edit, IniDoc *doc, ConfigSaveResult *result)
{
    bool *skipped = alloc_calloc((size_t) edit->count, sizeof(bool));
    if (skipped == NULL)
        return fail(result, "out of memory");
    remove_menus(edit, doc, result);
    bool ok = find_gone(edit, doc, skipped, result);
    if (ok)
        skip_unopened(edit, skipped, result);
    ok = ok && check_clashes(edit, doc, skipped, result) && rename_menus(edit, doc, skipped, result) &&
         add_menus(edit, doc, skipped, result);
    for (int m = 0; ok && m < edit->count; m++) {
        if (!skipped[m] && menu_changed(edit, &edit->menus[m]))
            ok = reconcile(edit, doc, &edit->menus[m], skipped, result);
    }
    ok = ok && rename_default(edit, doc, skipped, result);
    alloc_free(skipped);
    return ok;
}
