// The library grid's model (spec "Icons", choice 3): every library icon, grouped by the manifest's
// group with a header for each, filtered by what is typed, and moved through with the four arrows. Pure:
// no SDL, no globals; memory comes from alloc.h. settings_pickers.c draws it.
#ifndef ICONGRID_H
#define ICONGRID_H

#include <stdbool.h>
#include "library.h"

#define ICONGRID_QUERY_MAX 64   // The longest filter kept, with its terminator: the keyboard's limit is one less

typedef enum {
    ICONGRID_UP,
    ICONGRID_DOWN,
    ICONGRID_LEFT,
    ICONGRID_RIGHT,
    ICONGRID_OK,
    ICONGRID_BACK
} IconGridCommand;

typedef enum {
    ICONGRID_NONE,
    ICONGRID_MOVED,      // The cursor is on another icon: the preview shows it
    ICONGRID_CHOSEN,     // icongrid_chosen() gives the icon
    ICONGRID_CANCELED,
    ICONGRID_FILTER      // Up from the first row: the keyboard opens to filter the icons
} IconGridResult;

typedef struct IconGrid IconGrid;

IconGrid *icongrid_create(const LibraryInfo *icons, int count);   // Borrowed icons; NULL when out of memory
void icongrid_free(IconGrid *grid);
bool icongrid_filter(IconGrid *grid, const char *query);   // False when out of memory, with the grid as it was
const char *icongrid_query(const IconGrid *grid);
int icongrid_count(const IconGrid *grid);                  // The icons shown
int icongrid_groups(const IconGrid *grid);                 // The groups shown, each with an icon at least
const char *icongrid_group(const IconGrid *grid, int group);       // As the manifest names it; "" for none
int icongrid_group_first(const IconGrid *grid, int group);         // Its first icon among those shown
int icongrid_group_size(const IconGrid *grid, int group);
int icongrid_group_of(const IconGrid *grid, int shown);            // The group an icon shown is in; -1 past them
const LibraryInfo *icongrid_icon(const IconGrid *grid, int shown); // NULL past the icons shown
int icongrid_cursor(const IconGrid *grid);                 // -1 when no icon is shown
bool icongrid_select(IconGrid *grid, const char *name);    // The cursor to that icon, when it is shown
IconGridResult icongrid_command(IconGrid *grid, IconGridCommand command, int columns);
const LibraryInfo *icongrid_chosen(const IconGrid *grid);
const char *icongrid_why(const IconGrid *grid);            // Why the last OK did nothing, or NULL

#endif
