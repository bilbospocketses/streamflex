#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "osk.h"
#include "menuedit.h"

#define BOTTOM 4                      // The bottom row, on every page
#define ACCENTS "\xC3\xA0\xC3\xA9"    // The accents key's label
#define ERASE "\xE2\x8C\xAB"          // U+232B
#define LEFT "\xE2\x97\x80"           // U+25C0
#define RIGHT "\xE2\x96\xB6"          // U+25B6

// A function to join a row's keys' texts, or their shifted forms, as typing each in turn gives them
static const char *row_text(const Osk *osk, int row, bool shifted)
{
    static char text[128];
    text[0] = '\0';
    for (int c = 0; c < osk_row_length(osk, row); c++) {
        const OskKey *key = osk_key(osk, row, c);
        const char *typed = shifted && key->shifted != NULL ? key->shifted : key->text;
        size_t used = strlen(text);
        snprintf(text + used, sizeof(text) - used, "%s", typed != NULL ? typed : "");
    }
    return text;
}

// A function to join a row's labels, between bars
static const char *row_labels(const Osk *osk, int row)
{
    static char text[128];
    text[0] = '\0';
    for (int c = 0; c < osk_row_length(osk, row); c++) {
        size_t used = strlen(text);
        snprintf(text + used, sizeof(text) - used, c == 0 ? "%s" : "|%s", osk_key(osk, row, c)->label);
    }
    return text;
}

// A function to tell whether the highlight is on a key
static bool highlighted(const Osk *osk, int row, int column)
{
    int r = -1;
    int c = -1;
    osk_highlight(osk, &r, &c);
    return r == row && c == column;
}

// A function to move the highlight to a key with the remote's arrows, as a user would: down to the
// bottom row, up to the key's row, then along it; false when it never got there
static bool go(Osk *osk, int row, int column)
{
    while (osk_command(osk, OSK_DOWN) == OSK_MOVED)
        continue;
    int r = 0;
    int c = 0;
    osk_highlight(osk, &r, &c);
    for (; r > row; osk_highlight(osk, &r, &c))
        osk_command(osk, OSK_UP);
    for (int i = 0; i < 16 && c != column; i++, osk_highlight(osk, &r, &c))
        osk_command(osk, OSK_RIGHT);
    return highlighted(osk, row, column);
}

// A function to test the QWERTY layout: the number row, three rows of letters, and the bottom row,
// with the highlight starting on q
static void test_qwerty(void)
{
    Osk *osk = osk_create("Title for Kodi", "", 64, NULL, NULL, OSK_QWERTY);
    CHECK(osk != NULL);
    CHECK_STR(osk_label(osk), "Title for Kodi");
    CHECK_INT(osk_layout(osk), OSK_QWERTY);
    CHECK_INT(osk_page(osk), OSK_PAGE_LETTERS);
    CHECK_INT(osk_rows(osk), 5);
    CHECK_STR(row_text(osk, 0, false), "1234567890");
    CHECK_STR(row_text(osk, 0, true), "1234567890");             // Shift leaves the digits as they are
    CHECK_STR(row_text(osk, 1, false), "qwertyuiop");
    CHECK_STR(row_text(osk, 1, true), "QWERTYUIOP");
    CHECK_STR(row_text(osk, 2, false), "asdfghjkl");
    CHECK_STR(row_text(osk, 3, false), "zxcvbnm");
    CHECK_STR(row_text(osk, 3, true), "ZXCVBNM");
    CHECK_STR(row_labels(osk, BOTTOM), "ABC|?123|" ACCENTS "|Shift|Space|" ERASE "|" LEFT "|" RIGHT "|Done");
    static const OskKeyKind kinds[] = { OSK_KEY_LAYOUT, OSK_KEY_SYMBOLS, OSK_KEY_ACCENTS, OSK_KEY_SHIFT, OSK_KEY_SPACE,
                                        OSK_KEY_BACKSPACE, OSK_KEY_CURSOR_LEFT, OSK_KEY_CURSOR_RIGHT, OSK_KEY_DONE };
    CHECK_INT(osk_row_length(osk, BOTTOM), 9);
    for (int i = 0; i < 9; i++)
        CHECK_INT(osk_key(osk, BOTTOM, i)->kind, kinds[i]);
    CHECK_INT(osk_key(osk, 1, 0)->kind, OSK_KEY_CHAR);
    CHECK(osk_key(osk, BOTTOM, 9) == NULL);
    CHECK(osk_key(osk, 5, 0) == NULL);
    CHECK(osk_key(osk, -1, 0) == NULL);
    CHECK(osk_key(osk, 0, -1) == NULL);
    CHECK_INT(osk_row_length(osk, 5), 0);
    CHECK(highlighted(osk, 1, 0));                                // On q
    CHECK_STR(osk_text(osk), "");
    CHECK_INT(osk_cursor(osk), 0);
    osk_free(osk);
}

// A function to test the other layouts and pages: the ABC grid, ?123 and the accents, each one press
// away on the bottom row, whose keys say where they go
static void test_layouts_and_pages(void)
{
    Osk *osk = osk_create("Name", "", 64, NULL, NULL, OSK_ABC);
    CHECK_INT(osk_layout(osk), OSK_ABC);
    CHECK_INT(osk_rows(osk), 5);
    CHECK_STR(row_text(osk, 0, false), "abcdefg");
    CHECK_STR(row_text(osk, 1, false), "hijklmn");
    CHECK_STR(row_text(osk, 2, false), "opqrstu");
    CHECK_STR(row_text(osk, 3, false), "vwxyz-.");
    CHECK_STR(row_text(osk, 3, true), "VWXYZ-.");
    CHECK_STR(row_labels(osk, BOTTOM), "QWERTY|?123|" ACCENTS "|Shift|Space|" ERASE "|" LEFT "|" RIGHT "|Done");
    CHECK(highlighted(osk, 0, 0));                                // On a

    // The layout key: QWERTY's letters, the highlight still on the key
    CHECK(go(osk, BOTTOM, 0));
    CHECK_INT(osk_command(osk, OSK_OK), OSK_MOVED);
    CHECK_INT(osk_layout(osk), OSK_QWERTY);
    CHECK_INT(osk_page(osk), OSK_PAGE_LETTERS);
    CHECK(highlighted(osk, BOTTOM, 0));
    CHECK_STR(osk_key(osk, BOTTOM, 0)->label, "ABC");

    // ?123: digits and symbols, its key reading abc, and pressed again, the letters
    osk_command(osk, OSK_RIGHT);
    CHECK_INT(osk_command(osk, OSK_OK), OSK_MOVED);
    CHECK_INT(osk_page(osk), OSK_PAGE_SYMBOLS);
    CHECK_STR(row_text(osk, 0, false), "1234567890");
    CHECK_STR(row_text(osk, 1, false), "!@#$%^&*()");
    CHECK_STR(row_text(osk, 2, false), "-_=+[]{};:");
    CHECK_STR(row_text(osk, 3, false), "'\",./?\\|~<>`");
    CHECK_STR(row_text(osk, 3, true), "'\",./?\\|~<>`");
    CHECK_STR(row_labels(osk, BOTTOM), "ABC|abc|" ACCENTS "|Shift|Space|" ERASE "|" LEFT "|" RIGHT "|Done");

    // The backtick, the row's last key, types itself: with it the page has every printable ASCII character
    CHECK(go(osk, 3, 11));
    CHECK_INT(osk_command(osk, OSK_OK), OSK_CHANGED);
    CHECK_STR(osk_text(osk), "`");
    CHECK(go(osk, BOTTOM, 1));
    CHECK_INT(osk_command(osk, OSK_OK), OSK_MOVED);
    CHECK_INT(osk_page(osk), OSK_PAGE_LETTERS);
    CHECK_STR(osk_key(osk, BOTTOM, 1)->label, "?123");

    // The accents, from ?123 straight: exactly the spec's 31, in its order, with their capitals
    osk_command(osk, OSK_OK);
    osk_command(osk, OSK_RIGHT);
    CHECK_INT(osk_command(osk, OSK_OK), OSK_MOVED);
    CHECK_INT(osk_page(osk), OSK_PAGE_ACCENTS);
    CHECK_STR(row_text(osk, 0, false), "\xC3\xA0\xC3\xA1\xC3\xA2\xC3\xA4\xC3\xA3\xC3\xA5\xC3\xA6\xC3\xA7");
    CHECK_STR(row_text(osk, 1, false), "\xC3\xA8\xC3\xA9\xC3\xAA\xC3\xAB\xC3\xAC\xC3\xAD\xC3\xAE\xC3\xAF");
    CHECK_STR(row_text(osk, 2, false), "\xC3\xB1\xC3\xB2\xC3\xB3\xC3\xB4\xC3\xB6\xC3\xB5\xC3\xB8\xC5\x93");
    CHECK_STR(row_text(osk, 3, false), "\xC3\xB9\xC3\xBA\xC3\xBB\xC3\xBC\xC3\xBD\xC3\xBF\xC3\x9F");
    CHECK_STR(row_text(osk, 0, true), "\xC3\x80\xC3\x81\xC3\x82\xC3\x84\xC3\x83\xC3\x85\xC3\x86\xC3\x87");
    CHECK_STR(row_text(osk, 1, true), "\xC3\x88\xC3\x89\xC3\x8A\xC3\x8B\xC3\x8C\xC3\x8D\xC3\x8E\xC3\x8F");
    CHECK_STR(row_text(osk, 2, true), "\xC3\x91\xC3\x92\xC3\x93\xC3\x94\xC3\x96\xC3\x95\xC3\x98\xC5\x92");
    CHECK_STR(row_text(osk, 3, true), "\xC3\x99\xC3\x9A\xC3\x9B\xC3\x9C\xC3\x9D\xC5\xB8\xC3\x9F");
    CHECK_STR(row_labels(osk, BOTTOM), "ABC|?123|abc|Shift|Space|" ERASE "|" LEFT "|" RIGHT "|Done");

    // The layout key from a page: the other layout's letters
    osk_command(osk, OSK_LEFT);
    osk_command(osk, OSK_LEFT);
    CHECK_INT(osk_command(osk, OSK_OK), OSK_MOVED);
    CHECK_INT(osk_layout(osk), OSK_ABC);
    CHECK_INT(osk_page(osk), OSK_PAGE_LETTERS);
    osk_free(osk);
}

// A function to test moving the highlight: Up and Down to the nearest key by its center, the left one of
// two as near, across rows of different widths; Left and Right wrapping within a row; nothing past the
// top or the bottom
static void test_moves(void)
{
    // QWERTY, every row centered on the 12-unit bottom row: q at 1.5, a at 2.0, z at 3.0, ?123 at 2.25,
    // the accents key at 3.5, Shift at 4.75, Space at 6.5, the left arrow at 9.0, Done at 11.25
    Osk *osk = osk_create("Title", "", 64, NULL, NULL, OSK_QWERTY);
    CHECK_INT(osk_command(osk, OSK_DOWN), OSK_MOVED);
    CHECK(highlighted(osk, 2, 0));                                // q to a
    osk_command(osk, OSK_DOWN);
    CHECK(highlighted(osk, 3, 0));                                // z
    osk_command(osk, OSK_DOWN);
    CHECK(highlighted(osk, BOTTOM, 2));                           // The accents key (3.5) is nearer than ?123 (2.25)
    CHECK_INT(osk_command(osk, OSK_DOWN), OSK_NONE);
    CHECK(highlighted(osk, BOTTOM, 2));
    osk_command(osk, OSK_UP);
    CHECK(highlighted(osk, 3, 0));                                // A tie, z (3.0) or x (4.0) from 3.5: the left one
    osk_command(osk, OSK_UP);
    CHECK(highlighted(osk, 2, 1));                                // s (3.0), straight above z
    osk_command(osk, OSK_UP);
    CHECK(highlighted(osk, 1, 1));                                // A tie, w (2.5) or e (3.5): w
    osk_command(osk, OSK_LEFT);
    CHECK(highlighted(osk, 1, 0));                                // q

    // Along a row, wrapping at either end
    CHECK_INT(osk_command(osk, OSK_LEFT), OSK_MOVED);
    CHECK(highlighted(osk, 1, 9));                                // p
    osk_command(osk, OSK_RIGHT);
    CHECK(highlighted(osk, 1, 0));
    osk_command(osk, OSK_LEFT);

    // Down the right-hand side from p (10.5): l (10.0), m (9.0), the left arrow (9.0)
    osk_command(osk, OSK_DOWN);
    CHECK(highlighted(osk, 2, 8));
    osk_command(osk, OSK_DOWN);
    CHECK(highlighted(osk, 3, 6));
    osk_command(osk, OSK_DOWN);
    CHECK(highlighted(osk, BOTTOM, 6));
    osk_command(osk, OSK_RIGHT);
    osk_command(osk, OSK_RIGHT);
    CHECK(highlighted(osk, BOTTOM, 8));                           // Done
    osk_command(osk, OSK_UP);
    CHECK(highlighted(osk, 3, 6));                                // m, the nearest to Done (11.25)
    osk_command(osk, OSK_DOWN);
    CHECK(highlighted(osk, BOTTOM, 6));
    osk_command(osk, OSK_RIGHT);
    osk_command(osk, OSK_RIGHT);
    osk_command(osk, OSK_RIGHT);
    CHECK(highlighted(osk, BOTTOM, 0));                           // The bottom row wraps too

    // From Space (6.5), as near v (6.0) as b (7.0): v; then up the middle to the top, where Up does nothing
    for (int i = 0; i < 4; i++)
        osk_command(osk, OSK_RIGHT);
    CHECK(highlighted(osk, BOTTOM, 4));
    osk_command(osk, OSK_UP);
    CHECK(highlighted(osk, 3, 3));
    osk_command(osk, OSK_UP);
    CHECK(highlighted(osk, 2, 4));                                // g (6.0)
    osk_command(osk, OSK_UP);
    CHECK(highlighted(osk, 1, 4));                                // A tie, t (5.5) or y (6.5): t
    osk_command(osk, OSK_UP);
    CHECK(highlighted(osk, 0, 4));                                // 5
    CHECK_INT(osk_command(osk, OSK_UP), OSK_NONE);
    osk_free(osk);

    // The ABC grid, 10.5 units wide on the 12-unit bottom row: . (10.5) is nearer the right arrow (10.0)
    // than Done (11.25), and from Space (6.5), y (6.0) is nearer than z (7.5)
    osk = osk_create("Title", "", 64, NULL, NULL, OSK_ABC);
    osk_command(osk, OSK_DOWN);
    osk_command(osk, OSK_DOWN);
    osk_command(osk, OSK_DOWN);
    CHECK(highlighted(osk, 3, 0));                                // a, h, o, v: one column
    osk_command(osk, OSK_LEFT);
    CHECK(highlighted(osk, 3, 6));                                // .
    osk_command(osk, OSK_DOWN);
    CHECK(highlighted(osk, BOTTOM, 7));
    osk_command(osk, OSK_LEFT);
    osk_command(osk, OSK_LEFT);
    osk_command(osk, OSK_LEFT);
    osk_command(osk, OSK_UP);
    CHECK(highlighted(osk, 3, 3));
    osk_free(osk);
}

// A function to test Shift: one-shot, twice for caps lock, a third time off; ended by typing a key or
// Space, and on the accents page too; left as it is by a real keyboard's text
static void test_shift(void)
{
    Osk *osk = osk_create("Title", "", 64, NULL, NULL, OSK_QWERTY);
    CHECK_INT(osk_command(osk, OSK_OK), OSK_CHANGED);            // q
    CHECK(go(osk, BOTTOM, 3));                                   // Shift
    CHECK_INT(osk_command(osk, OSK_OK), OSK_MOVED);
    CHECK(osk_shift(osk) && !osk_caps(osk));
    osk_command(osk, OSK_UP);
    CHECK(highlighted(osk, 3, 2));                               // c (5.0), the nearest to Shift (4.75)
    osk_command(osk, OSK_OK);
    CHECK_STR(osk_text(osk), "qC");
    CHECK(!osk_shift(osk));                                      // One shot
    osk_command(osk, OSK_OK);
    CHECK_STR(osk_text(osk), "qCc");

    // Twice: caps lock, until Shift is pressed again
    osk_command(osk, OSK_DOWN);
    CHECK(highlighted(osk, BOTTOM, 3));
    osk_command(osk, OSK_OK);
    osk_command(osk, OSK_OK);
    CHECK(osk_caps(osk) && !osk_shift(osk));
    osk_command(osk, OSK_UP);
    osk_command(osk, OSK_OK);
    osk_command(osk, OSK_OK);
    CHECK_STR(osk_text(osk), "qCcCC");
    CHECK(osk_caps(osk));
    osk_command(osk, OSK_DOWN);
    osk_command(osk, OSK_OK);
    CHECK(!osk_caps(osk) && !osk_shift(osk));

    // A digit has no capital, and the one-shot ends with it as with any key; Space (the pad's Y) ends it too
    osk_command(osk, OSK_OK);
    CHECK(go(osk, 0, 0));
    osk_command(osk, OSK_OK);
    CHECK_STR(osk_text(osk), "qCcCC1");
    CHECK(!osk_shift(osk));
    CHECK(go(osk, BOTTOM, 3));
    osk_command(osk, OSK_OK);
    CHECK_INT(osk_command(osk, OSK_SPACE), OSK_CHANGED);
    CHECK(!osk_shift(osk));

    // On the accents page: a capital E acute
    osk_command(osk, OSK_OK);
    osk_command(osk, OSK_LEFT);
    osk_command(osk, OSK_OK);
    CHECK_INT(osk_page(osk), OSK_PAGE_ACCENTS);
    CHECK(osk_shift(osk));                                       // A page change is no key typed
    CHECK(go(osk, 1, 1));
    osk_command(osk, OSK_OK);
    CHECK_STR(osk_text(osk), "qCcCC1 \xC3\x89");

    // A real keyboard's text leaves the on-screen Shift as it is: that keyboard has its own
    CHECK(go(osk, BOTTOM, 3));
    osk_command(osk, OSK_OK);
    CHECK_INT(osk_insert(osk, "x"), OSK_CHANGED);
    CHECK(osk_shift(osk));
    osk_free(osk);
}

// A function to test editing UTF-8 at the cursor: whole characters at a time, inserted, deleted and
// stepped over, the cursor always on a character's first byte; what is not a character, or is a control
// character, is refused
static void test_editing(void)
{
    Osk *osk = osk_create("Title", "Kodi", 64, NULL, NULL, OSK_QWERTY);
    CHECK_INT(osk_cursor(osk), 4);                               // At the end
    CHECK_INT(osk_insert(osk, "\xC3\xA9"), OSK_CHANGED);
    CHECK_STR(osk_text(osk), "Kodi\xC3\xA9");
    CHECK_INT(osk_cursor(osk), 6);
    CHECK(go(osk, BOTTOM, 6));                                   // The left arrow
    CHECK_INT(osk_command(osk, OSK_OK), OSK_MOVED);
    CHECK_INT(osk_cursor(osk), 4);                               // Over e acute's two bytes at once
    CHECK_INT(osk_insert(osk, "\xF0\x9F\x98\x80"), OSK_CHANGED);   // U+1F600, four bytes, in the middle
    CHECK_STR(osk_text(osk), "Kodi\xF0\x9F\x98\x80\xC3\xA9");
    CHECK_INT(osk_cursor(osk), 8);
    CHECK_INT(osk_command(osk, OSK_BACKSPACE), OSK_CHANGED);     // The pad's X: the whole character
    CHECK_STR(osk_text(osk), "Kodi\xC3\xA9");
    CHECK_INT(osk_cursor(osk), 4);
    osk_command(osk, OSK_RIGHT);                                 // The right arrow
    CHECK_INT(osk_command(osk, OSK_OK), OSK_MOVED);
    CHECK_INT(osk_cursor(osk), 6);
    CHECK_INT(osk_command(osk, OSK_OK), OSK_NONE);               // At the end already
    osk_command(osk, OSK_LEFT);
    osk_command(osk, OSK_LEFT);                                  // The erase key
    CHECK_INT(osk_command(osk, OSK_OK), OSK_CHANGED);
    CHECK_STR(osk_text(osk), "Kodi");
    for (int i = 0; i < 4; i++)
        CHECK_INT(osk_command(osk, OSK_OK), OSK_CHANGED);
    CHECK_STR(osk_text(osk), "");
    CHECK_INT(osk_command(osk, OSK_OK), OSK_NONE);               // Nothing left to erase
    CHECK_INT(osk_command(osk, OSK_BACKSPACE), OSK_NONE);
    osk_command(osk, OSK_RIGHT);                                 // The left arrow
    CHECK_INT(osk_command(osk, OSK_OK), OSK_NONE);

    // The Space key on the bottom row types a space, as the pad's Y does
    CHECK(go(osk, BOTTOM, 4));
    CHECK_INT(osk_command(osk, OSK_OK), OSK_CHANGED);
    CHECK_STR(osk_text(osk), " ");
    CHECK_INT(osk_command(osk, OSK_BACKSPACE), OSK_CHANGED);

    // Not inserted: nothing; a lone lead or continuation byte; a control character; U+007F; a surrogate;
    // an overlong form; past U+10FFFF
    static const char *const bad[] = { "", "\xC3", "\x80", "a\tb", "\x7F", "\xED\xA0\x80", "\xC0\xAF", "\xF4\x90\x80\x80" };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
        CHECK_INT(osk_insert(osk, bad[i]), OSK_NONE);
    CHECK_INT(osk_insert(osk, NULL), OSK_NONE);
    CHECK_STR(osk_text(osk), "");
    osk_free(osk);
}

// A function to test the field's byte limit: an insert past it changes nothing, from a key or Space or a
// real keyboard; a text longer than the limit to begin with stays whole and can be shortened
static void test_limit(void)
{
    Osk *osk = osk_create("Title", "abcd", 5, NULL, NULL, OSK_QWERTY);
    CHECK_INT(osk_insert(osk, "\xC3\xA9"), OSK_NONE);           // 4 + 2 > 5
    CHECK_INT(osk_insert(osk, "e"), OSK_CHANGED);
    CHECK_INT(osk_command(osk, OSK_OK), OSK_NONE);               // q: full
    CHECK_INT(osk_command(osk, OSK_SPACE), OSK_NONE);
    CHECK_STR(osk_text(osk), "abcde");
    osk_free(osk);

    osk = osk_create("Title", "abcdefgh", 5, NULL, NULL, OSK_QWERTY);
    CHECK_STR(osk_text(osk), "abcdefgh");
    CHECK_INT(osk_insert(osk, "x"), OSK_NONE);
    CHECK_INT(osk_command(osk, OSK_BACKSPACE), OSK_CHANGED);
    CHECK_STR(osk_text(osk), "abcdefg");
    osk_free(osk);
}

// A tile's other parts, for its title's validator, as the tile's page gives them
typedef struct {
    const char *icon;
    const char *command;
} TileParts;

// The title field's validator: the tile's rules, with its other parts
static const char *check_title(const char *text, void *context)
{
    const TileParts *parts = context;
    return menuedit_check_tile(text, parts->icon, parts->command);
}

// A new submenu's name field's validator
static const char *check_name(const char *text, void *context)
{
    return menuedit_check_name(context, text, -1);
}

// A function to erase every character before the cursor
static void erase_all(Osk *osk)
{
    while (osk_command(osk, OSK_BACKSPACE) == OSK_CHANGED)
        continue;
}

// A function to test the validators the fields are given (Review Focus 2): Done is disabled while one
// says why, by its key, the pad's Start and Enter alike, and enabled again once the text is fixed
static void test_validators(void)
{
    TileParts parts = { "kodi", "kodi" };
    Osk *osk = osk_create("Title for Kodi", "Kodi", MENUEDIT_TITLE_MAX, check_title, &parts, OSK_QWERTY);
    CHECK(osk_why(osk) == NULL);
    CHECK_INT(osk_insert(osk, ";"), OSK_CHANGED);
    CHECK_STR(osk_why(osk), "A title can't contain ;");
    CHECK_INT(osk_command(osk, OSK_DONE), OSK_NONE);             // The pad's Start, and Enter
    CHECK(go(osk, BOTTOM, 8));
    CHECK_INT(osk_command(osk, OSK_OK), OSK_NONE);               // The Done key
    osk_command(osk, OSK_BACKSPACE);
    CHECK_INT(osk_insert(osk, " "), OSK_CHANGED);
    CHECK_STR(osk_why(osk), "A title can't start or end with a space");   // "Kodi ;" would begin a comment
    osk_command(osk, OSK_BACKSPACE);
    CHECK(osk_why(osk) == NULL);
    CHECK_INT(osk_command(osk, OSK_OK), OSK_FINISHED);
    erase_all(osk);
    CHECK_STR(osk_why(osk), "Type a title");
    osk_free(osk);

    // The title fits the field, but not the line beside the command: Entry99999= and Kodi;kodi; and 178
    // bytes of command are 199
    char command[200];
    memset(command, 'c', 178);
    command[178] = '\0';
    parts.command = command;
    osk = osk_create("Title for Kodi", "Kodi", MENUEDIT_TITLE_MAX, check_title, &parts, OSK_QWERTY);
    CHECK(osk_why(osk) == NULL);
    osk_insert(osk, "x");
    CHECK_STR(osk_why(osk), "It's too long for one line of config.ini (199 bytes)");
    osk_free(osk);

    // A new submenu's name: an existing menu in another case, a reserved name, ']', and 50 bytes
    static const char *const text = "[General]\nDefaultMenu=Main\n[Main]\nEntry1=Games;games;:submenu Games\n"
                                    "[Games]\nEntry1=Retro;retro;retroarch\n";
    IniDoc *doc = inidoc_parse(text, strlen(text));
    MenuEdit *edit = menuedit_load(doc, "Main");
    inidoc_free(doc);
    CHECK(edit != NULL);
    osk = osk_create("Name of the new submenu", "", 64, check_name, edit, OSK_ABC);
    CHECK_STR(osk_why(osk), "Type a name");
    osk_insert(osk, "games");
    CHECK_STR(osk_why(osk), "There's already a menu with that name");
    erase_all(osk);
    osk_insert(osk, "Clock");
    CHECK_STR(osk_why(osk), "That name is kept for settings");
    erase_all(osk);
    osk_insert(osk, "Kids]");
    CHECK_STR(osk_why(osk), "A name can't contain ]");
    erase_all(osk);
    for (int i = 0; i < 50; i++)
        osk_insert(osk, "K");
    CHECK_STR(osk_why(osk), "A name can be 49 bytes at most");
    erase_all(osk);
    osk_insert(osk, "Kids");
    CHECK(osk_why(osk) == NULL);
    CHECK_INT(osk_command(osk, OSK_DONE), OSK_FINISHED);
    osk_free(osk);
    menuedit_free(edit);
}

// A function to test Back: with the text as it opened it cancels at once; after a change it asks first,
// and the second Back cancels; anything else between ends the question, and a text changed back to how
// it opened cancels at once again
static void test_back(void)
{
    Osk *osk = osk_create("Title", "Kodi", 64, NULL, NULL, OSK_QWERTY);
    CHECK(!osk_asking(osk));
    CHECK_INT(osk_command(osk, OSK_BACK), OSK_CANCELED);
    osk_free(osk);

    osk = osk_create("Title", "Kodi", 64, NULL, NULL, OSK_QWERTY);
    osk_insert(osk, "x");
    CHECK_INT(osk_command(osk, OSK_BACK), OSK_ASKED);
    CHECK(osk_asking(osk));
    CHECK_INT(osk_command(osk, OSK_BACK), OSK_CANCELED);
    osk_free(osk);

    osk = osk_create("Title", "Kodi", 64, NULL, NULL, OSK_QWERTY);
    osk_insert(osk, "x");
    osk_command(osk, OSK_BACK);
    CHECK_INT(osk_command(osk, OSK_RIGHT), OSK_MOVED);
    CHECK(!osk_asking(osk));
    CHECK_INT(osk_command(osk, OSK_BACK), OSK_ASKED);
    osk_command(osk, OSK_BACKSPACE);
    CHECK(!osk_asking(osk));
    CHECK_STR(osk_text(osk), "Kodi");
    CHECK_INT(osk_command(osk, OSK_BACK), OSK_CANCELED);
    osk_free(osk);
}

// A function to test the real-keyboard rule (Review Focus 5): a key's character, with Shift, Caps Lock
// and AltGr already applied by SDL_GetKeyFromScancode(), as UTF-8; nothing for a key that types nothing
static void test_key_char(void)
{
    char out[5];
    CHECK(osk_key_char('a', out));
    CHECK_STR(out, "a");
    CHECK(osk_key_char('A', out));
    CHECK_STR(out, "A");
    CHECK(osk_key_char(' ', out));
    CHECK_STR(out, " ");
    CHECK(osk_key_char('^', out));                               // A dead key types its own character
    CHECK_STR(out, "^");
    CHECK(osk_key_char(0xE9, out));                              // e acute, on a French keyboard
    CHECK_STR(out, "\xC3\xA9");
    CHECK(osk_key_char(0xDF, out));                              // sharp s, on a German one
    CHECK_STR(out, "\xC3\x9F");
    CHECK(osk_key_char(0x20AC, out));                            // The euro sign, AltGr+E there
    CHECK_STR(out, "\xE2\x82\xAC");
    CHECK(osk_key_char(0x3B1, out));                             // Greek alpha
    CHECK_STR(out, "\xCE\xB1");
    CHECK(osk_key_char(0x1F600, out));
    CHECK_STR(out, "\xF0\x9F\x98\x80");

    // Nothing: SDLK_LEFT and SDLK_F1 (masked), the mask alone, Enter, Backspace, Tab, Esc, NUL, U+001F,
    // Delete (U+007F), SDL's U+FFFD for a key that types several code points, surrogates, past U+10FFFF
    static const unsigned int nothing[] = { 0x40000050, 0x4000003A, OSK_SCANCODE_MASK, 0x0D, 0x08, 0x09, 0x1B, 0x00,
                                            0x1F, 0x7F, 0xFFFD, 0xD800, 0xDFFF, 0x110000 };
    for (size_t i = 0; i < sizeof(nothing) / sizeof(nothing[0]); i++) {
        CHECK(!osk_key_char(nothing[i], out));
        CHECK_STR(out, "");
    }
}

// A function to test the keypad's keys (user ruling 2026-10-16, "Type the digit"): its digits, its point and
// its four operators type their character, as the top row's keys do, though SDL masks their keycodes;
// Keypad Enter (Done, which the screen gives it), Keypad =, the keypad's comma and the keys either side of
// the range type nothing
static void test_keypad_char(void)
{
    static const struct {
        unsigned int keycode;
        const char *text;
    } typed[] = {
        { 0x40000062, "0" }, { 0x40000059, "1" }, { 0x4000005A, "2" }, { 0x4000005B, "3" }, { 0x4000005C, "4" },
        { 0x4000005D, "5" }, { 0x4000005E, "6" }, { 0x4000005F, "7" }, { 0x40000060, "8" }, { 0x40000061, "9" },
        { 0x40000063, "." },
        { 0x40000054, "/" }, { 0x40000055, "*" }, { 0x40000056, "-" }, { 0x40000057, "+" }
    };
    char out[5];
    for (size_t i = 0; i < sizeof(typed) / sizeof(typed[0]); i++) {
        CHECK(osk_key_char(typed[i].keycode, out));
        CHECK_STR(out, typed[i].text);
    }

    // Keypad Enter, Keypad =, the keypad's comma, Num Lock (just below the range) and the non-US backslash
    // (just past it)
    static const unsigned int nothing[] = { 0x40000058, 0x40000067, 0x40000085, 0x40000053, 0x40000064 };
    for (size_t i = 0; i < sizeof(nothing) / sizeof(nothing[0]); i++) {
        CHECK(!osk_key_char(nothing[i], out));
        CHECK_STR(out, "");
    }

    // What a keypad digit types goes into the field as a top-row digit's does
    Osk *osk = osk_create("Command", "", 8, NULL, NULL, OSK_QWERTY);
    CHECK(osk_key_char(0x40000059, out));
    CHECK_INT(osk_insert(osk, out), OSK_CHANGED);
    CHECK_STR(osk_text(osk), "1");
    osk_free(osk);
}

int main(void)
{
    test_qwerty();
    test_layouts_and_pages();
    test_moves();
    test_shift();
    test_editing();
    test_limit();
    test_validators();
    test_back();
    test_key_char();
    test_keypad_char();
    return check_report();
}
