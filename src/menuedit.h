// The menus as data, for editing them from settings (spec "Saving and applying"): each section of
// config.ini that is not settings is a menu, and each of its entry lines a tile. The model is built from
// the file, not from the launcher's live lists, since a live Entry keeps neither its key nor its line
// (plan ruling 3). Each operation checks what it is given and refuses with a reason the caption can show;
// menuedit_apply() makes a fresh read of the file match the model, section by section (plan ruling 4).
// Pure: no SDL, no globals; memory comes from alloc.h.
#ifndef MENUEDIT_H
#define MENUEDIT_H

#include <stdbool.h>
#include "inidoc.h"
#include "config_save.h"

typedef struct {
    char *key;        // "Entry3"; NULL for a tile not yet written (numbered at save)
    char *title;
    char *icon;       // As the file has it: a library name or a path
    char *command;
    char *original;   // The line as read ("Entry3=Kodi;kodi;kodi"); NULL for a new or moved-in tile
} MenuTile;

typedef struct {
    char *name;
    char *original_name;   // The section as read; NULL for a new menu
    MenuTile *tiles;
    int count;
} MenuEditMenu;

typedef struct MenuEdit MenuEdit;

MenuEdit *menuedit_load(const IniDoc *doc, const char *default_menu);   // Every non-reserved section, in file order
MenuEdit *menuedit_copy(const MenuEdit *edit);                          // The snapshot Discard returns to
void menuedit_free(MenuEdit *edit);
int menuedit_count(const MenuEdit *edit);
const MenuEditMenu *menuedit_menu(const MenuEdit *edit, int m);
int menuedit_find(const MenuEdit *edit, const char *name);              // -1 when none
const char *menuedit_default(const MenuEdit *edit);
bool menuedit_changed(const MenuEdit *edit);

// Each returns NULL on success or the reason it refused, in a few words for the caption.
const char *menuedit_add(MenuEdit *edit, int m, int after, const char *title, const char *icon, const char *command);  // after: -1 = at the end
const char *menuedit_set(MenuEdit *edit, int m, int t, const char *title, const char *icon, const char *command);
const char *menuedit_remove(MenuEdit *edit, int m, int t);              // A submenu tile: see menuedit_remove_menu
const char *menuedit_move(MenuEdit *edit, int m, int from, int to);
const char *menuedit_move_to_menu(MenuEdit *edit, int m, int t, int to_menu);   // Appended at the end of to_menu
const char *menuedit_add_submenu(MenuEdit *edit, int m, int after, const char *name, const char *icon);
const char *menuedit_rename_menu(MenuEdit *edit, int m, const char *new_name);  // Rewrites each exact ":submenu <old>" and the default
const char *menuedit_remove_menu(MenuEdit *edit, int m, int t, bool with_tiles);   // t: the submenu tile in m
int menuedit_openers(const MenuEdit *edit, const char *name);           // Tiles whose command is exactly ":submenu <name>"
const char *menuedit_submenu_of(const MenuTile *tile);                  // The name after ":submenu ", or NULL

const char *menuedit_check_name(const MenuEdit *edit, const char *name, int except);   // Reserved, ']', too long, exists
const char *menuedit_check_tile(const char *title, const char *icon, const char *command);   // Empty part, ';' in title, line too long, inidoc_check
// Addition (fix pass 2): what menuedit_set() would say, before it is asked; a tile read from the file is weighed in
// its own line, its comment and spacing included (inidoc_check_in()), and a new one with its menu's new-tile key
const char *menuedit_check_set(const MenuEdit *edit, int m, int t, const char *title, const char *icon, const char *command);
#define MENUEDIT_TITLE_MAX 64

// Make `doc` (a fresh read) match `edit`: section renames, removals and additions first, then each menu's entry
// lines in the model's order, new tiles numbered "Entry<N>", then DefaultMenu when renamed. Notes go to `result`.
bool menuedit_apply(const MenuEdit *edit, IniDoc *doc, ConfigSaveResult *result);

#endif
