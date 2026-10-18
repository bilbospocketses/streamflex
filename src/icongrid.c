#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "icongrid.h"
#include "alloc.h"

static const char *const NO_MATCH = "No icon matches what was typed";

struct IconGrid {
    const LibraryInfo *icons;   // Borrowed: the library's, in the manifest's order
    int count;
    int *group_of;              // Each icon's group, an index into `names`
    const char **names;         // The groups, in the order the manifest first names each ("" for none)
    int group_count;
    int *shown;                 // The icons shown, group by group, each group in the manifest's order
    int shown_count;
    int *firsts;                // Each group shown: its first icon in `shown`...
    int *sizes;                 // ...how many icons it shows...
    int *groups;                // ...and which of `names` it is
    int shown_groups;
    int cursor;                 // An index into `shown`; -1 when nothing is shown
    char query[ICONGRID_QUERY_MAX];
    const LibraryInfo *chosen;  // The icon OK chose
    const char *why;            // Why the last OK did nothing; NULL otherwise
};

// A function to fold an ASCII capital to lower case, as the app list's filter folds; any other byte
// stays as it is
static char fold(char c)
{
    return c >= 'A' && c <= 'Z' ? (char) (c - 'A' + 'a') : c;
}

// A function to tell whether a text holds the query, ASCII letters compared in either case; an empty
// query is in every text
static bool holds(const char *text, const char *query)
{
    size_t length = strlen(query);
    if (length == 0)
        return true;
    for (const char *at = text != NULL ? text : ""; *at != '\0'; at++) {
        size_t k = 0;
        while (k < length && at[k] != '\0' && fold(at[k]) == fold(query[k]))
            k++;
        if (k == length)
            return true;
    }
    return false;
}

// A function to tell whether an icon is shown for a query: its title, its name or one of its aliases
// holds it
static bool shown_for(const LibraryInfo *icon, const char *query)
{
    if (holds(icon->title, query) || holds(icon->name, query))
        return true;
    for (int a = 0; a < icon->match_count; a++) {
        if (holds(icon->match[a], query))
            return true;
    }
    return false;
}

// A function to find a group among those named so far; -1 for a new one
static int group_named(const IconGrid *grid, const char *name)
{
    for (int g = 0; g < grid->group_count; g++) {
        if (strcmp(grid->names[g], name) == 0)
            return g;
    }
    return -1;
}

// A function to make the grid over the library's icons (borrowed: they must outlive it), every icon
// shown, the cursor on the first; NULL when out of memory
IconGrid *icongrid_create(const LibraryInfo *icons, int count)
{
    IconGrid *grid = alloc_calloc(1, sizeof(IconGrid));
    if (grid == NULL)
        return NULL;
    grid->icons = icons;
    grid->count = icons != NULL && count > 0 ? count : 0;
    grid->cursor = -1;
    size_t room = (size_t) (grid->count > 0 ? grid->count : 1);
    grid->group_of = alloc_calloc(room, sizeof(int));
    grid->names = alloc_calloc(room, sizeof(char*));
    if (grid->group_of == NULL || grid->names == NULL) {
        icongrid_free(grid);
        return NULL;
    }
    for (int i = 0; i < grid->count; i++) {
        const char *name = icons[i].group != NULL ? icons[i].group : "";
        int g = group_named(grid, name);
        if (g < 0) {
            g = grid->group_count++;
            grid->names[g] = name;
        }
        grid->group_of[i] = g;
    }
    if (!icongrid_filter(grid, "")) {
        icongrid_free(grid);
        return NULL;
    }
    return grid;
}

// A function to free the grid
void icongrid_free(IconGrid *grid)
{
    if (grid == NULL)
        return;
    alloc_free(grid->group_of);
    alloc_free(grid->names);
    alloc_free(grid->shown);
    alloc_free(grid->firsts);
    alloc_free(grid->sizes);
    alloc_free(grid->groups);
    alloc_free(grid);
}

// A function to show only the icons whose title, name or an alias holds the query (ASCII letters in
// either case; "" or NULL shows them all), group by group, a group with none left out. The cursor
// stays on its icon while that is still shown, else goes to the first. False when out of memory, with
// the grid as it was.
bool icongrid_filter(IconGrid *grid, const char *query)
{
    char wanted[ICONGRID_QUERY_MAX];   // A copy: the query may be the grid's own (icongrid_query())
    snprintf(wanted, sizeof(wanted), "%s", query != NULL ? query : "");
    size_t icons = (size_t) (grid->count > 0 ? grid->count : 1);
    size_t groups = (size_t) (grid->group_count > 0 ? grid->group_count : 1);
    int *shown = alloc_calloc(icons, sizeof(int));
    int *firsts = alloc_calloc(groups, sizeof(int));
    int *sizes = alloc_calloc(groups, sizeof(int));
    int *which = alloc_calloc(groups, sizeof(int));
    if (shown == NULL || firsts == NULL || sizes == NULL || which == NULL) {
        alloc_free(shown);
        alloc_free(firsts);
        alloc_free(sizes);
        alloc_free(which);
        return false;
    }
    int count = 0;
    int made = 0;
    for (int g = 0; g < grid->group_count; g++) {
        int first = count;
        for (int i = 0; i < grid->count; i++) {
            if (grid->group_of[i] == g && shown_for(&grid->icons[i], wanted))
                shown[count++] = i;
        }
        if (count > first) {
            firsts[made] = first;
            sizes[made] = count - first;
            which[made] = g;
            made++;
        }
    }
    int was = grid->cursor >= 0 ? grid->shown[grid->cursor] : -1;
    int cursor = count > 0 ? 0 : -1;
    for (int s = 0; s < count; s++) {
        if (shown[s] == was)
            cursor = s;
    }
    alloc_free(grid->shown);
    alloc_free(grid->firsts);
    alloc_free(grid->sizes);
    alloc_free(grid->groups);
    grid->shown = shown;
    grid->firsts = firsts;
    grid->sizes = sizes;
    grid->groups = which;
    grid->shown_count = count;
    grid->shown_groups = made;
    grid->cursor = cursor;
    snprintf(grid->query, sizeof(grid->query), "%s", wanted);
    grid->why = NULL;
    return true;
}

// A function to get the query the icons are filtered by; "" for none
const char *icongrid_query(const IconGrid *grid)
{
    return grid->query;
}

// A function to count the icons shown
int icongrid_count(const IconGrid *grid)
{
    return grid->shown_count;
}

// A function to count the groups shown: each holds at least one icon
int icongrid_groups(const IconGrid *grid)
{
    return grid->shown_groups;
}

// A function to name a group shown, as the manifest names it ("" for icons with no group); NULL past
// the groups shown
const char *icongrid_group(const IconGrid *grid, int group)
{
    return group >= 0 && group < grid->shown_groups ? grid->names[grid->groups[group]] : NULL;
}

// A function to give a group's first icon among those shown; -1 past the groups shown
int icongrid_group_first(const IconGrid *grid, int group)
{
    return group >= 0 && group < grid->shown_groups ? grid->firsts[group] : -1;
}

// A function to count a group's icons shown; 0 past the groups shown
int icongrid_group_size(const IconGrid *grid, int group)
{
    return group >= 0 && group < grid->shown_groups ? grid->sizes[group] : 0;
}

// A function to give the group shown an icon shown is in; -1 past the icons shown
int icongrid_group_of(const IconGrid *grid, int shown)
{
    for (int g = 0; shown >= 0 && shown < grid->shown_count && g < grid->shown_groups; g++) {
        if (shown < grid->firsts[g] + grid->sizes[g])
            return g;
    }
    return -1;
}

// A function to give an icon shown, in the grid's order; NULL past the icons shown
const LibraryInfo *icongrid_icon(const IconGrid *grid, int shown)
{
    return shown >= 0 && shown < grid->shown_count ? &grid->icons[grid->shown[shown]] : NULL;
}

// A function to give the icon under the cursor, as an index among those shown; -1 when none is shown
int icongrid_cursor(const IconGrid *grid)
{
    return grid->cursor;
}

// A function to put the cursor on the icon of a name, when it is shown; false (the cursor where it
// was) when it is not
bool icongrid_select(IconGrid *grid, const char *name)
{
    for (int s = 0; name != NULL && s < grid->shown_count; s++) {
        if (strcmp(grid->icons[grid->shown[s]].name, name) == 0) {
            grid->cursor = s;
            return true;
        }
    }
    return false;
}

// A function to act on one key, the grid `columns` icons wide (at least one). Left and Right step
// through the icons in the grid's order, across the groups' headers; Up and Down go to the same column
// of the row above or below, or of the next group's first row or the group above's last row, or to a
// shorter row's last icon. Up from the first row asks for the keyboard (ICONGRID_FILTER), as it does with
// no icon shown. OK chooses the icon under the cursor, and does nothing, saying why, when none is shown;
// Back cancels.
IconGridResult icongrid_command(IconGrid *grid, IconGridCommand command, int columns)
{
    grid->why = NULL;
    if (columns < 1)
        columns = 1;
    if (command == ICONGRID_BACK)
        return ICONGRID_CANCELED;
    if (grid->cursor < 0) {
        if (command == ICONGRID_UP)
            return ICONGRID_FILTER;
        if (command == ICONGRID_OK)
            grid->why = NO_MATCH;
        return ICONGRID_NONE;
    }
    if (command == ICONGRID_OK) {
        grid->chosen = &grid->icons[grid->shown[grid->cursor]];
        return ICONGRID_CHOSEN;
    }
    int g = icongrid_group_of(grid, grid->cursor);
    int place = grid->cursor - grid->firsts[g];
    int row = place / columns;
    int column = place % columns;
    int to = grid->cursor;
    switch (command) {
        case ICONGRID_LEFT:
            to = grid->cursor - 1;
            break;
        case ICONGRID_RIGHT:
            to = grid->cursor + 1;
            break;
        case ICONGRID_DOWN:
            if ((row + 1) * columns < grid->sizes[g]) {
                int next = (row + 1) * columns + column;
                to = grid->firsts[g] + (next < grid->sizes[g] ? next : grid->sizes[g] - 1);
            }
            else if (g + 1 < grid->shown_groups)
                to = grid->firsts[g + 1] + (column < grid->sizes[g + 1] ? column : grid->sizes[g + 1] - 1);
            break;
        case ICONGRID_UP:
            if (row > 0)
                to = grid->firsts[g] + (row - 1) * columns + column;
            else if (g > 0) {
                int last = (grid->sizes[g - 1] - 1) / columns * columns + column;
                to = grid->firsts[g - 1] + (last < grid->sizes[g - 1] ? last : grid->sizes[g - 1] - 1);
            }
            else
                return ICONGRID_FILTER;
            break;
        case ICONGRID_OK:
        case ICONGRID_BACK:
            break;
    }
    if (to < 0 || to >= grid->shown_count || to == grid->cursor)
        return ICONGRID_NONE;
    grid->cursor = to;
    return ICONGRID_MOVED;
}

// A function to give the icon OK chose; NULL before one was
const LibraryInfo *icongrid_chosen(const IconGrid *grid)
{
    return grid->chosen;
}

// A function to say why the last OK did nothing; NULL when it did something, or no OK came
const char *icongrid_why(const IconGrid *grid)
{
    return grid->why;
}
