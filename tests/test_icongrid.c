#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "icongrid.h"

// A library as the manifest lists one: groups first named in a mixed order, Kodi with an alias
static const char *const KODI_ALIASES[] = { "xbmc" };
static const LibraryInfo ICONS[] = {
    { "power", "Power", "system", NULL, 0 },
    { "kodi", "Kodi", "servers", KODI_ALIASES, 1 },
    { "restart", "Restart", "system", NULL, 0 },
    { "plex", "Plex", "servers", NULL, 0 },
    { "netflix", "Netflix", "streaming", NULL, 0 },
    { "sleep", "Sleep", "system", NULL, 0 },
    { "jellyfin", "Jellyfin", "servers", NULL, 0 },
    { "emby", "Emby", "servers", NULL, 0 },
    { "apps", "Apps", "general", NULL, 0 },
    { "settings", "Settings", "system", NULL, 0 },
    { "vlc", "VLC", "servers", NULL, 0 }
};
#define ICON_COUNT ((int) (sizeof(ICONS) / sizeof(ICONS[0])))

// A function to name the icon under the cursor; "(none)" when no icon is shown
static const char *at_cursor(const IconGrid *grid)
{
    const LibraryInfo *icon = icongrid_icon(grid, icongrid_cursor(grid));
    return icon != NULL ? icon->name : "(none)";
}

// A function to list the icons shown, group by group: "system: power restart | servers: kodi"
static const char *listed(const IconGrid *grid)
{
    static char out[512];
    out[0] = '\0';
    for (int g = 0; g < icongrid_groups(grid); g++) {
        size_t used = strlen(out);
        snprintf(out + used, sizeof(out) - used, "%s%s:", g == 0 ? "" : " | ", icongrid_group(grid, g));
        for (int s = icongrid_group_first(grid, g); s < icongrid_group_first(grid, g) + icongrid_group_size(grid, g); s++) {
            used = strlen(out);
            snprintf(out + used, sizeof(out) - used, " %s", icongrid_icon(grid, s)->name);
        }
    }
    return out;
}

// A function to test the groups: each named once, in the order the manifest first names it, its icons
// in the manifest's order; the cursor on the first icon; and the accessors' answers past the end
static void test_groups(void)
{
    IconGrid *grid = icongrid_create(ICONS, ICON_COUNT);
    CHECK(grid != NULL);
    CHECK_INT(icongrid_count(grid), 11);
    CHECK_INT(icongrid_groups(grid), 4);
    CHECK_STR(listed(grid), "system: power restart sleep settings | servers: kodi plex jellyfin emby vlc | "
                            "streaming: netflix | general: apps");
    CHECK_INT(icongrid_group_first(grid, 1), 4);
    CHECK_INT(icongrid_group_size(grid, 1), 5);
    CHECK_INT(icongrid_group_of(grid, 8), 1);
    CHECK_INT(icongrid_group_of(grid, 9), 2);
    CHECK_INT(icongrid_cursor(grid), 0);
    CHECK_STR(icongrid_query(grid), "");
    CHECK(icongrid_group(grid, 4) == NULL);
    CHECK_INT(icongrid_group_first(grid, -1), -1);
    CHECK_INT(icongrid_group_size(grid, 4), 0);
    CHECK_INT(icongrid_group_of(grid, 11), -1);
    CHECK(icongrid_icon(grid, 11) == NULL);
    CHECK(icongrid_icon(grid, -1) == NULL);
    CHECK(icongrid_chosen(grid) == NULL);
    icongrid_free(grid);
}

// A function to test the arrows three icons wide: Left and Right through the icons across the headers,
// Down and Up to the same column, a shorter row's last icon, the next group's first row and the last
// group's last row; Up from the first row asks for the keyboard; nothing moves past either end
static void test_moves(void)
{
    IconGrid *grid = icongrid_create(ICONS, ICON_COUNT);
    // system: [power restart sleep] [settings]; servers: [kodi plex jellyfin] [emby vlc];
    // streaming: [netflix]; general: [apps]
    CHECK_INT(icongrid_command(grid, ICONGRID_LEFT, 3), ICONGRID_NONE);
    CHECK_INT(icongrid_command(grid, ICONGRID_RIGHT, 3), ICONGRID_MOVED);
    CHECK_STR(at_cursor(grid), "restart");
    CHECK_INT(icongrid_command(grid, ICONGRID_DOWN, 3), ICONGRID_MOVED);
    CHECK_STR(at_cursor(grid), "settings");                 // Its row is shorter: its last icon
    CHECK_INT(icongrid_command(grid, ICONGRID_DOWN, 3), ICONGRID_MOVED);
    CHECK_STR(at_cursor(grid), "kodi");                     // The next group's first row
    CHECK_INT(icongrid_command(grid, ICONGRID_DOWN, 3), ICONGRID_MOVED);
    CHECK_STR(at_cursor(grid), "emby");
    CHECK_INT(icongrid_command(grid, ICONGRID_RIGHT, 3), ICONGRID_MOVED);
    CHECK_STR(at_cursor(grid), "vlc");
    CHECK_INT(icongrid_command(grid, ICONGRID_DOWN, 3), ICONGRID_MOVED);
    CHECK_STR(at_cursor(grid), "netflix");                  // Column 1 of a group of one: its only icon
    CHECK_INT(icongrid_command(grid, ICONGRID_DOWN, 3), ICONGRID_MOVED);
    CHECK_STR(at_cursor(grid), "apps");
    CHECK_INT(icongrid_command(grid, ICONGRID_DOWN, 3), ICONGRID_NONE);
    CHECK_INT(icongrid_command(grid, ICONGRID_RIGHT, 3), ICONGRID_NONE);
    CHECK_INT(icongrid_command(grid, ICONGRID_UP, 3), ICONGRID_MOVED);
    CHECK_STR(at_cursor(grid), "netflix");
    CHECK_INT(icongrid_command(grid, ICONGRID_UP, 3), ICONGRID_MOVED);
    CHECK_STR(at_cursor(grid), "emby");                     // The group above's last row
    CHECK_INT(icongrid_command(grid, ICONGRID_UP, 3), ICONGRID_MOVED);
    CHECK_STR(at_cursor(grid), "kodi");
    CHECK_INT(icongrid_command(grid, ICONGRID_LEFT, 3), ICONGRID_MOVED);
    CHECK_STR(at_cursor(grid), "settings");                 // Left crosses the header
    CHECK_INT(icongrid_command(grid, ICONGRID_UP, 3), ICONGRID_MOVED);
    CHECK_STR(at_cursor(grid), "power");
    CHECK_INT(icongrid_command(grid, ICONGRID_UP, 3), ICONGRID_FILTER);
    CHECK_STR(at_cursor(grid), "power");

    // Four wide, system is one row: Down from restart goes to the servers' column 1, Up from plex back
    CHECK(icongrid_select(grid, "restart"));
    CHECK_INT(icongrid_command(grid, ICONGRID_DOWN, 4), ICONGRID_MOVED);
    CHECK_STR(at_cursor(grid), "plex");
    CHECK_INT(icongrid_command(grid, ICONGRID_UP, 4), ICONGRID_MOVED);
    CHECK_STR(at_cursor(grid), "restart");

    // A width under one is one: Down is the next icon
    CHECK_INT(icongrid_command(grid, ICONGRID_DOWN, 0), ICONGRID_MOVED);
    CHECK_STR(at_cursor(grid), "sleep");
    icongrid_free(grid);
}

// A function to test filtering: by an alias, in any case; by a part of a title or name, the groups
// with none left out; the cursor kept on its icon while it is shown; and a query nothing matches, where
// OK says why and Up still asks for the keyboard
static void test_filter(void)
{
    IconGrid *grid = icongrid_create(ICONS, ICON_COUNT);
    CHECK(icongrid_filter(grid, "XBMC"));
    CHECK_STR(listed(grid), "servers: kodi");
    CHECK_STR(at_cursor(grid), "kodi");
    CHECK_STR(icongrid_query(grid), "XBMC");
    CHECK(icongrid_filter(grid, icongrid_query(grid)));      // Its own query, given back
    CHECK_STR(icongrid_query(grid), "XBMC");
    CHECK(icongrid_filter(grid, ""));
    CHECK_INT(icongrid_count(grid), 11);
    CHECK_STR(at_cursor(grid), "kodi");                     // Still shown: the cursor stays on it

    CHECK(icongrid_select(grid, "plex"));
    CHECK(icongrid_filter(grid, "e"));
    CHECK_STR(listed(grid), "system: power restart sleep settings | servers: plex jellyfin emby | streaming: netflix");
    CHECK_STR(at_cursor(grid), "plex");
    CHECK(icongrid_filter(grid, "Ix"));
    CHECK_STR(listed(grid), "streaming: netflix");
    CHECK_STR(at_cursor(grid), "netflix");                  // Plex is not shown: the first icon

    CHECK(icongrid_filter(grid, "zzz"));
    CHECK_INT(icongrid_count(grid), 0);
    CHECK_INT(icongrid_groups(grid), 0);
    CHECK_INT(icongrid_cursor(grid), -1);
    CHECK_INT(icongrid_command(grid, ICONGRID_OK, 3), ICONGRID_NONE);
    CHECK_STR(icongrid_why(grid), "No icon matches what was typed");
    CHECK_INT(icongrid_command(grid, ICONGRID_DOWN, 3), ICONGRID_NONE);
    CHECK(icongrid_why(grid) == NULL);
    CHECK_INT(icongrid_command(grid, ICONGRID_UP, 3), ICONGRID_FILTER);
    CHECK(icongrid_filter(grid, NULL));
    CHECK_INT(icongrid_count(grid), 11);
    CHECK_STR(at_cursor(grid), "power");
    icongrid_free(grid);
}

// A function to test choosing: OK on the icon the cursor was put on by name, a name not shown leaving
// the cursor where it was, and Back
static void test_choose(void)
{
    IconGrid *grid = icongrid_create(ICONS, ICON_COUNT);
    CHECK(icongrid_select(grid, "jellyfin"));
    CHECK_INT(icongrid_cursor(grid), 6);
    CHECK(!icongrid_select(grid, "nope"));
    CHECK(!icongrid_select(grid, NULL));
    CHECK_INT(icongrid_cursor(grid), 6);
    CHECK_INT(icongrid_command(grid, ICONGRID_OK, 3), ICONGRID_CHOSEN);
    CHECK_STR(icongrid_chosen(grid)->name, "jellyfin");
    CHECK_INT(icongrid_command(grid, ICONGRID_BACK, 3), ICONGRID_CANCELED);
    icongrid_free(grid);

    // An icon with no group is in the group "", which the screen names; an empty library shows nothing
    static const LibraryInfo loose[] = { { "a", "A", NULL, NULL, 0 }, { "b", "B", "", NULL, 0 } };
    grid = icongrid_create(loose, 2);
    CHECK_INT(icongrid_groups(grid), 1);
    CHECK_STR(icongrid_group(grid, 0), "");
    CHECK_INT(icongrid_group_size(grid, 0), 2);
    icongrid_free(grid);
    grid = icongrid_create(NULL, 0);
    CHECK(grid != NULL);
    CHECK_INT(icongrid_count(grid), 0);
    CHECK_INT(icongrid_command(grid, ICONGRID_RIGHT, 3), ICONGRID_NONE);
    CHECK_INT(icongrid_command(grid, ICONGRID_BACK, 3), ICONGRID_CANCELED);
    icongrid_free(grid);
    icongrid_free(NULL);
}

int main(void)
{
    test_groups();
    test_moves();
    test_filter();
    test_choose();
    return check_report();
}
