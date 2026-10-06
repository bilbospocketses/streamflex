#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "check.h"
#include "listpick.h"

// A function to make a picker of the rows a small command picker holds
static ListPick *sample(void)
{
    ListPick *pick = listpick_create();
    CHECK(listpick_add(pick, "None", "", true, NULL));
    CHECK(listpick_add(pick, "Left", ":left", true, NULL));
    CHECK(listpick_add(pick, "Right", ":right", true, NULL));
    CHECK(listpick_add(pick, "Quit StreamFlex", ":quit", true, NULL));
    CHECK(listpick_add(pick, "Close the app on show", ":exit", false, "Only on Windows"));
    CHECK(listpick_add(pick, "Kodi", "kodi --standalone", true, NULL));
    return pick;
}

// A function to test moving, paging and the ends
static void test_moves(void)
{
    ListPick *pick = sample();
    CHECK_INT(listpick_count(pick), 6);
    CHECK_INT(listpick_cursor(pick), 0);
    CHECK_INT(listpick_command(pick, LISTPICK_UP, 3), LISTPICK_NONE);
    CHECK_INT(listpick_command(pick, LISTPICK_DOWN, 3), LISTPICK_MOVED);
    CHECK_INT(listpick_cursor(pick), 1);
    CHECK_INT(listpick_command(pick, LISTPICK_PAGE_DOWN, 3), LISTPICK_MOVED);
    CHECK_INT(listpick_cursor(pick), 4);
    CHECK_INT(listpick_command(pick, LISTPICK_PAGE_DOWN, 3), LISTPICK_MOVED);
    CHECK_INT(listpick_cursor(pick), 5);                     // The last row, not past it
    CHECK_INT(listpick_command(pick, LISTPICK_PAGE_DOWN, 3), LISTPICK_NONE);
    CHECK_INT(listpick_command(pick, LISTPICK_PAGE_UP, 3), LISTPICK_MOVED);
    CHECK_INT(listpick_cursor(pick), 2);
    CHECK_INT(listpick_command(pick, LISTPICK_PAGE_UP, 0), LISTPICK_MOVED);   // A page is at least one row
    CHECK_INT(listpick_cursor(pick), 1);
    CHECK_INT(listpick_command(pick, LISTPICK_UP, 3), LISTPICK_MOVED);
    CHECK_INT(listpick_cursor(pick), 0);
    listpick_free(pick);
}

// A function to test choosing, a row that cannot be chosen, and cancelling
static void test_choose(void)
{
    ListPick *pick = sample();
    CHECK(listpick_select(pick, ":quit", "Custom: :quit"));
    CHECK_INT(listpick_cursor(pick), 3);
    CHECK_INT(listpick_command(pick, LISTPICK_OK, 3), LISTPICK_CHOSEN);
    CHECK_STR(listpick_chosen(pick), ":quit");

    // A row that cannot be chosen takes the cursor, and OK says why
    listpick_command(pick, LISTPICK_DOWN, 3);
    CHECK_INT(listpick_command(pick, LISTPICK_OK, 3), LISTPICK_NONE);
    CHECK_STR(listpick_why(pick), "Only on Windows");
    listpick_command(pick, LISTPICK_DOWN, 3);
    CHECK(listpick_why(pick) == NULL);                        // Moving clears the reason
    CHECK_INT(listpick_command(pick, LISTPICK_BACK, 3), LISTPICK_CANCELLED);

    // None chooses the empty value
    CHECK(listpick_select(pick, "", "Custom: "));
    CHECK_INT(listpick_cursor(pick), 0);
    CHECK_INT(listpick_command(pick, LISTPICK_OK, 3), LISTPICK_CHOSEN);
    CHECK_STR(listpick_chosen(pick), "");
    listpick_free(pick);
}

// A function to test a value that matches no row: pinned first as Custom, chosen as itself
static void test_custom(void)
{
    ListPick *pick = sample();
    CHECK(!listpick_has(pick, "retroarch -f"));
    CHECK(listpick_has(pick, ":left"));
    CHECK(listpick_select(pick, "retroarch -f", "Custom: retroarch -f"));
    CHECK_INT(listpick_count(pick), 7);
    CHECK(!listpick_has(pick, "retroarch -f"));                // The Custom row is not one of the rows

    const ListPickRow *row = listpick_row(pick, 0);
    CHECK(row->custom);
    CHECK_STR(row->label, "Custom: retroarch -f");
    CHECK_STR(row->value, "retroarch -f");
    CHECK_INT(listpick_cursor(pick), 0);
    CHECK_STR(listpick_row(pick, 1)->label, "None");        // The rest keep their order
    CHECK_INT(listpick_command(pick, LISTPICK_OK, 3), LISTPICK_CHOSEN);
    CHECK_STR(listpick_chosen(pick), "retroarch -f");

    // Selecting again does not pin a second one, and a value that matches unpins it
    CHECK(listpick_select(pick, "retroarch -f", "Custom: retroarch -f"));
    CHECK_INT(listpick_count(pick), 7);
    CHECK(listpick_select(pick, ":left", "Custom: :left"));
    CHECK_INT(listpick_count(pick), 6);
    CHECK_STR(listpick_row(pick, listpick_cursor(pick))->value, ":left");
    CHECK(listpick_row(pick, 6) == NULL);
    CHECK_STR(listpick_chosen(pick), "");                     // The unpinned row's value is gone, and so is the choice
    listpick_free(pick);
}

// A function to test moving the cursor to a value: a Custom row pinned before stays pinned, and a
// value no row gives (the Custom row's own included) leaves the cursor where it was
static void test_move_to(void)
{
    ListPick *pick = sample();
    CHECK(listpick_select(pick, "retroarch -f", "Custom: retroarch -f"));
    CHECK(listpick_move_to(pick, ":quit"));
    CHECK_INT(listpick_cursor(pick), 4);
    CHECK_INT(listpick_count(pick), 7);                      // The Custom row is still pinned
    CHECK(listpick_row(pick, 0)->custom);
    CHECK_STR(listpick_row(pick, 0)->value, "retroarch -f");
    CHECK(!listpick_move_to(pick, "not listed"));
    CHECK_INT(listpick_cursor(pick), 4);
    CHECK(!listpick_move_to(pick, "retroarch -f"));          // The Custom row is not one of the rows
    CHECK_INT(listpick_cursor(pick), 4);
    CHECK(listpick_move_to(pick, ""));
    CHECK_INT(listpick_cursor(pick), 1);
    CHECK_INT(listpick_command(pick, LISTPICK_OK, 3), LISTPICK_CHOSEN);
    CHECK_STR(listpick_chosen(pick), "");
    listpick_free(pick);
}

// A function to test a picker that outgrows its first rows, as the command and font pickers do
static void test_grow(void)
{
    ListPick *pick = listpick_create();
    char label[32];
    char value[32];
    for (int i = 0; i < 40; i++) {
        snprintf(label, sizeof(label), "Row %d", i);
        snprintf(value, sizeof(value), "value-%d", i);
        CHECK(listpick_add(pick, label, value, true, NULL));
    }
    CHECK_INT(listpick_count(pick), 40);
    CHECK_STR(listpick_row(pick, 0)->label, "Row 0");
    CHECK_STR(listpick_row(pick, 39)->label, "Row 39");

    // A Custom row goes in at the top, moving all 40 down
    CHECK(listpick_select(pick, "not listed", "Custom: not listed"));
    CHECK_INT(listpick_count(pick), 41);
    CHECK(listpick_row(pick, 0)->custom);
    CHECK_STR(listpick_row(pick, 1)->label, "Row 0");
    CHECK_STR(listpick_row(pick, 40)->label, "Row 39");
    CHECK_STR(listpick_row(pick, 40)->value, "value-39");

    // Paging reaches the last row and stops there
    for (int i = 0; i < 10; i++)
        listpick_command(pick, LISTPICK_PAGE_DOWN, 8);
    CHECK_INT(listpick_cursor(pick), 40);
    CHECK_INT(listpick_command(pick, LISTPICK_OK, 8), LISTPICK_CHOSEN);
    CHECK_STR(listpick_chosen(pick), "value-39");
    listpick_free(pick);
}

// A function to test an empty picker, as the font picker is while it loads
static void test_empty(void)
{
    ListPick *pick = listpick_create();
    CHECK_INT(listpick_count(pick), 0);
    CHECK_INT(listpick_command(pick, LISTPICK_DOWN, 3), LISTPICK_NONE);
    CHECK_INT(listpick_command(pick, LISTPICK_OK, 3), LISTPICK_NONE);
    CHECK_INT(listpick_command(pick, LISTPICK_BACK, 3), LISTPICK_CANCELLED);
    CHECK(listpick_row(pick, 0) == NULL);
    listpick_free(pick);
    listpick_free(NULL);
}

int main(void)
{
    test_moves();
    test_choose();
    test_custom();
    test_move_to();
    test_grow();
    test_empty();
    return check_report();
}
