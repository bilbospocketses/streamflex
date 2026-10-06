#include <string.h>
#include "listpick.h"
#include "alloc.h"

struct ListPick {
    ListPickRow *rows;
    int count;
    int capacity;
    int cursor;
    const char *chosen;   // The value OK chose, in its row
    const char *why;      // Why the last OK did nothing; NULL otherwise
};

// A function to make an empty picker; NULL when out of memory
ListPick *listpick_create(void)
{
    return alloc_calloc(1, sizeof(ListPick));
}

// A function to free a row's text
static void free_row(ListPickRow *row)
{
    alloc_free(row->label);
    alloc_free(row->value);
}

// A function to free the picker
void listpick_free(ListPick *pick)
{
    if (pick == NULL)
        return;
    for (int i = 0; i < pick->count; i++)
        free_row(&pick->rows[i]);
    alloc_free(pick->rows);
    alloc_free(pick);
}

// A function to make room for one more row
static bool grow(ListPick *pick)
{
    if (pick->count < pick->capacity)
        return true;
    int capacity = pick->capacity ? pick->capacity * 2 : 16;
    ListPickRow *rows = alloc_realloc(pick->rows, (size_t) capacity * sizeof(ListPickRow));
    if (rows == NULL)
        return false;
    pick->rows = rows;
    pick->capacity = capacity;
    return true;
}

// A function to make a row at `at`, moving the rows after it down; false when out of memory, with
// nothing changed
static bool insert_row(ListPick *pick, int at, const char *label, const char *value, bool enabled,
                       const char *why, bool custom)
{
    ListPickRow row;
    row.label = alloc_strdup(label);
    row.value = alloc_strdup(value);
    row.enabled = enabled;
    row.why = enabled ? NULL : why;
    row.custom = custom;
    if (row.label == NULL || row.value == NULL || !grow(pick)) {
        free_row(&row);
        return false;
    }
    memmove(&pick->rows[at + 1], &pick->rows[at], (size_t) (pick->count - at) * sizeof(ListPickRow));
    pick->rows[at] = row;
    pick->count++;
    return true;
}

// A function to add a row at the end; false when out of memory, with nothing added
bool listpick_add(ListPick *pick, const char *label, const char *value, bool enabled, const char *why)
{
    return insert_row(pick, pick->count, label, value, enabled, why, false);
}

// A function to find the row that gives a value, not counting the Custom row; -1 when none does
static int find(const ListPick *pick, const char *value)
{
    for (int i = 0; i < pick->count; i++) {
        if (!pick->rows[i].custom && strcmp(pick->rows[i].value, value) == 0)
            return i;
    }
    return -1;
}

// A function to tell whether a row gives a value, for a caller that keeps its rows unique
bool listpick_has(const ListPick *pick, const char *value)
{
    return find(pick, value) >= 0;
}

// A function to put the cursor on the row that gives a value. A value no row gives is pinned
// first, labelled `custom_row_label` (the caller's "Custom: ..."), so choosing it keeps it; a Custom
// row pinned before goes. False when out of memory, with the cursor on the first row.
bool listpick_select(ListPick *pick, const char *value, const char *custom_row_label)
{
    pick->why = NULL;
    if (pick->count > 0 && pick->rows[0].custom) {
        if (strcmp(pick->rows[0].value, value) == 0) {
            pick->cursor = 0;
            return true;
        }
        if (pick->chosen == pick->rows[0].value)
            pick->chosen = NULL;   // Its text goes with the row
        free_row(&pick->rows[0]);
        memmove(&pick->rows[0], &pick->rows[1], (size_t) (pick->count - 1) * sizeof(ListPickRow));
        pick->count--;
    }
    int at = find(pick, value);
    pick->cursor = at >= 0 ? at : 0;
    if (at >= 0)
        return true;
    return insert_row(pick, 0, custom_row_label, value, true, NULL, true);
}

// A function to put the cursor on the row that gives a value, pinning and unpinning nothing (a
// Custom row stays); false, with the cursor where it was, when no row but the Custom row gives it
bool listpick_move_to(ListPick *pick, const char *value)
{
    int at = find(pick, value);
    if (at < 0)
        return false;
    pick->cursor = at;
    return true;
}

// A function to act on one key: move, page, choose or cancel
ListPickResult listpick_command(ListPick *pick, ListPickCommand command, int page_rows)
{
    int before = pick->cursor;
    int last = pick->count - 1;
    pick->why = NULL;
    if (page_rows < 1)
        page_rows = 1;
    switch (command) {
        case LISTPICK_UP:
            if (pick->cursor > 0)
                pick->cursor--;
            break;
        case LISTPICK_DOWN:
            if (pick->cursor < last)
                pick->cursor++;
            break;
        case LISTPICK_PAGE_UP:
            pick->cursor = pick->cursor - page_rows < 0 ? 0 : pick->cursor - page_rows;
            break;
        case LISTPICK_PAGE_DOWN:
            pick->cursor = pick->cursor + page_rows > last ? (last > 0 ? last : 0) : pick->cursor + page_rows;
            break;
        case LISTPICK_OK: {
            if (pick->cursor < 0 || pick->cursor > last)
                return LISTPICK_NONE;
            const ListPickRow *row = &pick->rows[pick->cursor];
            if (!row->enabled) {
                pick->why = row->why;
                return LISTPICK_NONE;
            }
            pick->chosen = row->value;
            return LISTPICK_CHOSEN;
        }
        case LISTPICK_BACK:
            return LISTPICK_CANCELLED;
    }
    return pick->cursor != before ? LISTPICK_MOVED : LISTPICK_NONE;
}

// A function to count the rows
int listpick_count(const ListPick *pick)
{
    return pick->count;
}

// A function to get a row; NULL past the end
const ListPickRow *listpick_row(const ListPick *pick, int index)
{
    return index >= 0 && index < pick->count ? &pick->rows[index] : NULL;
}

// A function to get the cursor
int listpick_cursor(const ListPick *pick)
{
    return pick->cursor;
}

// A function to get the value OK chose, valid until the picker changes or is freed; "" before any
const char *listpick_chosen(const ListPick *pick)
{
    return pick->chosen != NULL ? pick->chosen : "";
}

// A function to say why the last OK chose nothing; NULL when it did, or when it was not OK
const char *listpick_why(const ListPick *pick)
{
    return pick->why;
}
