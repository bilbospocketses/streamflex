// The shared list picker's model: rows to choose from, each with a label, the value it gives, and
// whether it can be chosen (with the reason when it cannot); a cursor, paged with Left and Right;
// and a pinned "Custom: ..." row for a value from the file that matches no other row, so choosing
// it keeps it. It serves the command, font, default menu and device pickers. Pure: no SDL, no
// globals; memory comes from alloc.h. settings_pickers.c draws it.
#ifndef LISTPICK_H
#define LISTPICK_H

#include <stdbool.h>

#define LISTPICK_TEXT_MAX 1024  // The buffer size a caller uses for a row's text; rows keep any length

typedef struct {
    char *label;        // What the row shows
    char *value;        // What choosing it gives
    bool enabled;
    const char *why;    // Why it cannot be chosen; NULL when it can (a static string)
    bool custom;        // The pinned row for a value that matched no other
} ListPickRow;

typedef enum {
    LISTPICK_UP,
    LISTPICK_DOWN,
    LISTPICK_PAGE_UP,
    LISTPICK_PAGE_DOWN,
    LISTPICK_OK,
    LISTPICK_BACK
} ListPickCommand;

typedef enum {
    LISTPICK_NONE,       // Nothing happened (an end, or OK on a row that cannot be chosen)
    LISTPICK_MOVED,
    LISTPICK_CHOSEN,     // listpick_chosen() gives the value
    LISTPICK_CANCELED
} ListPickResult;

typedef struct ListPick ListPick;

ListPick *listpick_create(void);
void listpick_free(ListPick *pick);
// label and value (here and in listpick_select) are never NULL
bool listpick_add(ListPick *pick, const char *label, const char *value, bool enabled, const char *why);
bool listpick_has(const ListPick *pick, const char *value);
bool listpick_select(ListPick *pick, const char *value, const char *custom_row_label);
bool listpick_move_to(ListPick *pick, const char *value);
ListPickResult listpick_command(ListPick *pick, ListPickCommand command, int page_rows);
int listpick_count(const ListPick *pick);
const ListPickRow *listpick_row(const ListPick *pick, int index);
int listpick_cursor(const ListPick *pick);
const char *listpick_chosen(const ListPick *pick);
const char *listpick_why(const ListPick *pick);

#endif
