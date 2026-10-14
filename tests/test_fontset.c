// Unit test 7, its real half (spec section 9): font sets over SDL_ttf and the bundled fonts. This file
// stands in for fontcand.c (fontset_candidates() below), so no system font is read: the candidates are
// the bundled DejaVu Sans, and FreeSans where a chain's order must show. It stands in for the launcher's
// log too, and reads what fontset.c wrote there.
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "check.h"
#include "launcher.h"
#include "debug.h"
#include "fontset.h"

#define OPEN_SANS FONTSET_TEST_FONTS "/OpenSans-Regular.ttf"
#define DEJAVU FONTSET_TEST_FONTS "/DejaVuSans.ttf"
#define FREESANS FONTSET_TEST_FONTS "/FreeSans.ttf"
#define STAR "\xE2\x98\x85"     // U+2605: DejaVu Sans has it; Open Sans and FreeSans do not
#define ARROW "\xE2\x86\x92"    // U+2192: DejaVu Sans (0.838 em) and FreeSans (0.987 em) have it; Open Sans does not
#define ZHONG "\xE4\xB8\xAD"    // U+4E2D: none of the three has it
#define NNBSP "\xE2\x80\xAF"    // U+202F, the narrow no-break space: DejaVu Sans has it, with no ink
#define BRAILLE "\xE2\xA0\x80"  // U+2800, the blank Braille pattern: DejaVu Sans has it, with no ink, and it is no space
#define LINES 64
#define CANDIDATES 6            // The most candidates a case lists (test_many_fallbacks)

static char logged[LINES][512];
static int logged_count = 0;
static FontCandidate list[CANDIDATES];
static int list_count = 0;
static int lists_built = 0;    // Calls to fontset_candidates()
static int covers_calls = 0;   // Calls to never_covers()

// The launcher's log, kept for the checks to read
void output_log(LogLevel log_level, const char *format, ...)
{
    (void) log_level;
    if (logged_count == LINES)
        return;
    va_list args;
    va_start(args, format);
    vsnprintf(logged[logged_count], sizeof(logged[0]), format, args);
    va_end(args);
    logged[logged_count][strcspn(logged[logged_count], "\r\n")] = '\0';
    logged_count++;
}

// A function to count the lines logged that hold a text
static int lines_with(const char *text)
{
    int n = 0;
    for (int i = 0; i < logged_count; i++)
        n += strstr(logged[i], text) != NULL;
    return n;
}

// The candidates, in place of fontcand.c's
const FontCandidate *fontset_candidates(int *count)
{
    lists_built++;
    *count = list_count;
    return list_count > 0 ? list : NULL;
}

// Nothing to let go of: the list is this file's
void fontset_candidates_free(void)
{
}

// A coverage that answers no to everything, counting the questions
static bool never_covers(const FontCandidate *candidate, Uint32 ch)
{
    (void) candidate;
    (void) ch;
    covers_calls++;
    return false;
}

// A function to begin a case: these candidates (NULL for no second), nothing logged or counted yet
static void begin(const char *first, const char *second)
{
    list[0] = (FontCandidate) { .path = (char *) first, .face = 0 };
    list[1] = (FontCandidate) { .path = (char *) second, .face = 0 };
    list_count = second != NULL ? 2 : 1;
    logged_count = 0;
    lists_built = 0;
    covers_calls = 0;
}

// A function to measure a text in a font
static int width(TTF_Font *font, const char *text)
{
    int w = 0;
    int h = 0;
    CHECK(TTF_GetStringSize(font, text, 0, &w, &h));
    return w;
}

// A function to measure a text in a file's own font at a size, with no fallbacks
static int plain_width(const char *path, float size, const char *text)
{
    TTF_Font *font = TTF_OpenFont(path, size);
    CHECK(font != NULL);
    int w = font != NULL ? width(font, text) : -1;
    TTF_CloseFont(font);
    return w;
}

// A function to test that a text the font covers builds no candidate list, opens nothing, logs nothing
static void test_covered_text(void)
{
    begin(DEJAVU, NULL);
    FontSet *set = fontset_open(OPEN_SANS, 36.0f, 0, "titles");
    CHECK(set != NULL);
    fontset_prepare(set, "Kodi Media Center \xE2\x80\xBA 12 \xC3\x97 10");   // Open Sans has the › and the ×
    CHECK_INT(lists_built, 0);
    CHECK_INT(logged_count, 0);
    fontset_close(set);
    fontset_quit();
    CHECK_INT(logged_count, 0);   // No list, so nothing to let go of
}

// A function to test that a star attaches DejaVu Sans once, measures wider than Open Sans's own box
// for it, and that a second character DejaVu Sans has attaches nothing more
static void test_star(void)
{
    begin(DEJAVU, NULL);
    int boxed = plain_width(OPEN_SANS, 36.0f, "A" STAR);
    FontSet *set = fontset_open(OPEN_SANS, 36.0f, 0, "titles");
    fontset_prepare(set, "A" STAR);
    CHECK(width(fontset_font(set), "A" STAR) > boxed);
    CHECK_INT(lines_with("Fonts: the 36 pt titles fall back to " DEJAVU " (face 0), first for U+2605"), 1);
    fontset_prepare(set, STAR STAR ARROW);
    CHECK_INT(lines_with(" fall back to "), 1);
    CHECK_INT(lists_built, 1);
    fontset_close(set);
    fontset_quit();
    // DejaVu Sans's probe, opened to learn its coverage, and Open Sans's own, which said it lacks the arrow
    CHECK_INT(lines_with("Fonts: let go of 1 fallback candidates and 2 probe fonts at quit"), 1);
}

// A function to test that order is kept. FreeSans ranks first and DejaVu Sans second. A star (FreeSans
// lacks it) attaches DejaVu Sans; an arrow then attaches FreeSans in front of it, rebuilding the chain,
// so the arrow is FreeSans's and DejaVu Sans still draws stars. Only the primary's own probe can say
// Open Sans lacks the arrow: Open Sans would answer through DejaVu Sans, which has it.
static void test_chain_order(void)
{
    begin(FREESANS, DEJAVU);
    FontSet *set = fontset_open(OPEN_SANS, 36.0f, 0, "titles");
    fontset_prepare(set, STAR);
    fontset_prepare(set, ARROW);
    int free_arrow = plain_width(FREESANS, 36.0f, ARROW);
    CHECK(free_arrow != plain_width(DEJAVU, 36.0f, ARROW));
    CHECK_INT(width(fontset_font(set), ARROW), free_arrow);
    CHECK_INT(width(fontset_font(set), STAR STAR), plain_width(DEJAVU, 36.0f, STAR STAR));
    CHECK_INT(lines_with("Fonts: the 36 pt titles fall back to " DEJAVU " (face 0), first for U+2605"), 1);
    CHECK_INT(lines_with("Fonts: the 36 pt titles fall back to " FREESANS " (face 0), first for U+2192"), 1);
    fontset_close(set);
    fontset_quit();
}

// A function to test that a resized set resizes its fallbacks: a text measured after the resize
// matches the same text in a set opened at that size
static void test_resize(void)
{
    begin(DEJAVU, NULL);
    FontSet *set = fontset_open(OPEN_SANS, 36.0f, 0, "titles");
    fontset_prepare(set, "A" STAR);
    CHECK(fontset_resize(set, 18.0f));
    FontSet *fresh = fontset_open(OPEN_SANS, 18.0f, 0, "titles");
    fontset_prepare(fresh, "A" STAR STAR);
    CHECK_INT(width(fontset_font(set), "A" STAR STAR), width(fontset_font(fresh), "A" STAR STAR));
    fontset_close(fresh);
    fontset_close(set);
    fontset_quit();
}

// A function to test that a set looks at each code point once: the same character again asks no candidate
static void test_seen_once(void)
{
    begin(DEJAVU, DEJAVU);
    list[0].covers = never_covers;
    FontSet *set = fontset_open(OPEN_SANS, 36.0f, 0, "titles");
    fontset_prepare(set, ZHONG);
    CHECK_INT(covers_calls, 1);
    fontset_prepare(set, ZHONG ZHONG);
    CHECK_INT(covers_calls, 1);
    fontset_close(set);
    fontset_quit();
}

// A function to test that a character no font has is logged once a session, not once a set
static void test_missing_once(void)
{
    begin(DEJAVU, NULL);
    FontSet *first = fontset_open(OPEN_SANS, 36.0f, 0, "titles");
    FontSet *second = fontset_open(OPEN_SANS, 24.0f, 0, "settings rows");
    fontset_prepare(first, "A" ZHONG);
    fontset_prepare(second, ZHONG);
    CHECK_INT(lines_with("Fonts: no installed font draws U+4E2D; it shows as a box"), 1);
    CHECK_INT(lines_with(" fall back to "), 0);
    fontset_close(second);
    fontset_close(first);
    fontset_quit();
}

// A function to test that default-ignorables are never looked up. DejaVu Sans maps ZWJ to a glyph with
// no ink, which would have it skipped for the session, and the star after it would find no font.
static void test_ignorable(void)
{
    begin(DEJAVU, NULL);
    FontSet *set = fontset_open(OPEN_SANS, 36.0f, 0, "titles");
    fontset_prepare(set, "A\xE2\x80\x8D\xEF\xB8\x8F\xF3\xA0\x84\x80");   // ZWJ, VS16, VS17
    fontset_prepare(set, STAR);
    CHECK_INT(lines_with("Fonts: skipped "), 0);
    CHECK_INT(lines_with("Fonts: no installed font draws "), 0);
    CHECK_INT(lines_with("Fonts: the 36 pt titles fall back to " DEJAVU " (face 0), first for U+2605"), 1);
    fontset_close(set);
    fontset_quit();
}

// A function to test that a space the font lacks attaches the font that has it, though it draws no
// ink: DejaVu Sans's narrow no-break space does not get it taken for a font that draws nothing
static void test_blank_space(void)
{
    begin(DEJAVU, NULL);
    FontSet *set = fontset_open(OPEN_SANS, 36.0f, 0, "titles");
    fontset_prepare(set, "12" NNBSP "h");
    fontset_prepare(set, STAR);
    CHECK_INT(lines_with("Fonts: skipped "), 0);
    CHECK_INT(lines_with("Fonts: the 36 pt titles fall back to " DEJAVU " (face 0), first for U+202F"), 1);
    CHECK_INT(lines_with("Fonts: no installed font draws "), 0);
    fontset_close(set);
    fontset_quit();
}

static TTF_Font *primary = NULL;   // The set's font, which record_close() measures again while it lives
static bool primary_open = false;
static int closes = 0;
static bool primary_first = false; // The first font the set closed was its own

// The close fontset.c is pointed at: it records the order, and after each close it measures the
// primary again while that lives. A fallback closed before its font leaves the font's cached
// positions for the text pointing into the fallback's freed glyphs, and that measure reads them
// (upstream #549): ASan reports it in the "Unit tests (ASan, UBSan)" job, and the order check fails
// it everywhere.
static void SDLCALL record_close(TTF_Font *font)
{
    if (closes++ == 0)
        primary_first = font == primary;
    if (font == primary)
        primary_open = false;
    TTF_CloseFont(font);
    if (primary_open) {
        int w = 0;
        int h = 0;
        TTF_GetStringSize(primary, "A" STAR, 0, &w, &h);
    }
}

// A function to test the closing order of upstream #549: the font first, then its fallbacks; opened
// again, the same text measures the same
static void test_close_order(void)
{
    begin(DEJAVU, NULL);
    FontSet *set = fontset_open(OPEN_SANS, 36.0f, 0, "titles");
    fontset_prepare(set, "A" STAR);
    int first = width(fontset_font(set), "A" STAR);   // The font now caches the text's positions, in DejaVu Sans
    primary = fontset_font(set);
    primary_open = true;
    closes = 0;
    primary_first = false;
    fontset_close_font = record_close;
    fontset_close(set);
    fontset_close_font = NULL;
    CHECK_INT(closes, 2);
    CHECK(primary_first);
    set = fontset_open(OPEN_SANS, 36.0f, 0, "titles");
    fontset_prepare(set, "A" STAR);
    CHECK_INT(width(fontset_font(set), "A" STAR), first);
    fontset_close(set);
    fontset_quit();
}

// A function to close a set through record_close(), returning how many fonts it closed
static int closed_fonts(FontSet *set)
{
    primary = fontset_font(set);
    primary_open = true;
    closes = 0;
    primary_first = false;
    fontset_close_font = record_close;
    fontset_close(set);
    fontset_close_font = NULL;
    return closes;
}

// A coverage, as fontconfig's character set gives one, that has the three code points
// test_blank_then_ink asks for: the narrow no-break space, the blank Braille pattern and the star
static bool covers_three(const FontCandidate *candidate, Uint32 ch)
{
    (void) candidate;
    return ch == 0x202F || ch == 0x2800 || ch == 0x2605;
}

// A function to test that a candidate first picked for a space is still asked for ink the next time it
// is picked, for a character that is no space: a space proves nothing about ink. DejaVu Sans, listed with a
// character set as fontconfig lists it, stands in for a color emoji font that draws nothing: its blank
// Braille pattern draws no ink, and is no space. Skipped, it is skipped for the session: another set
// does not pick it, and the log says so once.
static void test_blank_then_ink(void)
{
    begin(DEJAVU, NULL);
    list[0].covers = covers_three;
    FontSet *set = fontset_open(OPEN_SANS, 36.0f, 0, "titles");
    fontset_prepare(set, "12" NNBSP "h");
    CHECK_INT(lines_with("Fonts: the 36 pt titles fall back to " DEJAVU " (face 0), first for U+202F"), 1);
    fontset_prepare(set, BRAILLE);
    CHECK_INT(lines_with("Fonts: skipped " DEJAVU ": it draws nothing for U+2800"), 1);
    CHECK_INT(lines_with("Fonts: no installed font draws U+2800; it shows as a box"), 1);
    FontSet *other = fontset_open(OPEN_SANS, 36.0f, 0, "titles");
    fontset_prepare(other, STAR);
    CHECK_INT(lines_with("Fonts: skipped "), 1);
    CHECK_INT(lines_with(" fall back to " DEJAVU " (face 0), first for U+2605"), 0);
    CHECK_INT(lines_with("Fonts: no installed font draws U+2605; it shows as a box"), 1);
    fontset_close(other);
    fontset_close(set);
    fontset_quit();
}

// A function to test that a candidate attached already is not attached again for another character
// it has: a star and then an arrow, both DejaVu Sans's, open it once, log it once, and find it there
static void test_attached_once(void)
{
    begin(DEJAVU, NULL);
    FontSet *set = fontset_open(OPEN_SANS, 36.0f, 0, "titles");
    fontset_prepare(set, STAR);
    fontset_prepare(set, ARROW);
    CHECK_INT(lines_with(" fall back to "), 1);
    CHECK_INT(lines_with("Fonts: no installed font draws "), 0);
    CHECK_INT(closed_fonts(set), 2);   // The font and its one fallback
    fontset_quit();
}

// The six candidates of test_many_fallbacks: each answers, as fontconfig's character set would, for
// two symbols of its own, which DejaVu Sans draws and Open Sans lacks
static const Uint32 own_symbols[CANDIDATES][2] = {
    { 0x2600, 0x2622 }, { 0x2601, 0x2639 }, { 0x2602, 0x263A }, { 0x2603, 0x263C }, { 0x2614, 0x2640 }, { 0x2620, 0x2642 },
};

// A coverage of a candidate's own two symbols
static bool covers_own(const FontCandidate *candidate, Uint32 ch)
{
    int rank = (int) (candidate - list);
    return ch == own_symbols[rank][0] || ch == own_symbols[rank][1];
}

// A function to write code points as UTF-8, each candidate's symbol in the order given
static void symbols_text(char *out, const int *ranks, int count, int which)
{
    for (int i = 0; i < count; i++)
        out = SDL_UCS4ToUTF8(own_symbols[ranks[i]][which], out);
    *out = '\0';
}

// A function to test a set with more fallbacks than its first room (four), attached out of order: each
// rank in its turn goes at the end or between two attached ones. Each candidate's second symbol then
// finds it attached already, so the ranks moved with their fonts. The primary's own probe is opened
// once for the set's eleven lookups after the first, and no candidate with a character set is probed.
static void test_many_fallbacks(void)
{
    begin(DEJAVU, NULL);
    for (int i = 0; i < CANDIDATES; i++)
        list[i] = (FontCandidate) { .path = (char *) DEJAVU, .face = 0, .covers = covers_own };
    list_count = CANDIDATES;
    static const int order[CANDIDATES] = { 5, 1, 3, 0, 4, 2 };
    char text[4 * CANDIDATES + 1];
    FontSet *set = fontset_open(OPEN_SANS, 36.0f, 0, "titles");
    symbols_text(text, order, CANDIDATES, 0);
    fontset_prepare(set, text);
    CHECK_INT(lines_with(" fall back to "), CANDIDATES);
    CHECK_INT(lines_with("Fonts: the 36 pt titles fall back to " DEJAVU " (face 0), first for U+2620"), 1);
    CHECK_INT(lines_with("Fonts: the 36 pt titles fall back to " DEJAVU " (face 0), first for U+2602"), 1);
    symbols_text(text, order, CANDIDATES, 1);
    fontset_prepare(set, text);
    CHECK_INT(lines_with(" fall back to "), CANDIDATES);
    CHECK_INT(lines_with("Fonts: no installed font draws "), 0);
    CHECK_INT(closed_fonts(set), 1 + CANDIDATES);
    fontset_quit();
    CHECK_INT(lines_with("Fonts: let go of 6 fallback candidates and 1 probe fonts at quit"), 1);
}

// A function to test that a set keeps what it saw when its table of seen code points grows: 中 first,
// then 94 more code points (the table grows twice), then 中 again, which asks no candidate
static void test_seen_grows(void)
{
    begin(DEJAVU, NULL);
    list[0].covers = never_covers;
    FontSet *set = fontset_open(OPEN_SANS, 36.0f, 0, "titles");
    fontset_prepare(set, ZHONG);
    char ascii[95];
    for (int i = 0; i < 94; i++)
        ascii[i] = (char) ('!' + i);
    ascii[94] = '\0';
    fontset_prepare(set, ascii);
    fontset_prepare(set, ZHONG);
    CHECK_INT(covers_calls, 1);
    fontset_close(set);
    fontset_quit();
}

// A function to test that a fallback attached after a resize opens at the new size: the log names it,
// and the set measures as one opened at that size
static void test_resize_then_attach(void)
{
    begin(DEJAVU, NULL);
    FontSet *set = fontset_open(OPEN_SANS, 36.0f, 0, "titles");
    fontset_prepare(set, "A");
    CHECK(fontset_resize(set, 18.0f));
    fontset_prepare(set, STAR);
    CHECK_INT(lines_with("Fonts: the 18 pt titles fall back to " DEJAVU " (face 0), first for U+2605"), 1);
    FontSet *fresh = fontset_open(OPEN_SANS, 18.0f, 0, "titles");
    fontset_prepare(fresh, STAR);
    CHECK_INT(width(fontset_font(set), "A" STAR), width(fontset_font(fresh), "A" STAR));
    fontset_close(fresh);
    fontset_close(set);
    fontset_quit();
}

// A function to test that something that is not a regular file (here a folder) is never opened, as a
// set's font or as a candidate: the set does not open, saying why, and the candidate is skipped for the
// session, once, while the next one provides
static void test_not_a_file(void)
{
    begin(FONTSET_TEST_FONTS, DEJAVU);
    CHECK(fontset_open(FONTSET_TEST_FONTS, 36.0f, 0, "titles") == NULL);
    CHECK_STR(SDL_GetError(), "not a regular file");
    FontSet *set = fontset_open(OPEN_SANS, 36.0f, 0, "titles");
    fontset_prepare(set, STAR);
    fontset_prepare(set, ARROW);
    CHECK_INT(lines_with("Fonts: skipped " FONTSET_TEST_FONTS ": not a regular file"), 1);
    CHECK_INT(lines_with("Fonts: the 36 pt titles fall back to " DEJAVU " (face 0), first for U+2605"), 1);
    fontset_close(set);
    fontset_quit();
}

// A function to test that a text is read as UTF-8 with its four-byte sequences whole: U+20000 and
// U+30000, which SDL 3.2.10's and 3.4.18's SDL_StepUTF8() read as U+FFFD and U+10000, are each looked
// up and found nowhere. An overlong star, a surrogate and a code point past U+10FFFF are no UTF-8:
// each reads as U+FFFD, which Open Sans has, so nothing is logged for them.
static void test_four_bytes(void)
{
    begin(DEJAVU, NULL);
    FontSet *set = fontset_open(OPEN_SANS, 36.0f, 0, "titles");
    fontset_prepare(set, "\xF0\xA0\x80\x80 \xF0\xB0\x80\x80");
    CHECK_INT(lines_with("Fonts: no installed font draws U+20000; it shows as a box"), 1);
    CHECK_INT(lines_with("Fonts: no installed font draws U+30000; it shows as a box"), 1);
    fontset_prepare(set, "\xF0\x82\x98\x85 \xED\xA0\x80 \xF4\x90\x80\x80");
    CHECK_INT(logged_count, 2);
    fontset_close(set);
    fontset_quit();
}

int main(void)
{
    if (!TTF_Init()) {
        fprintf(stderr, "TTF_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    test_covered_text();
    test_star();
    test_chain_order();
    test_resize();
    test_seen_once();
    test_missing_once();
    test_ignorable();
    test_blank_space();
    test_close_order();
    test_blank_then_ink();
    test_attached_once();
    test_many_fallbacks();
    test_seen_grows();
    test_resize_then_attach();
    test_not_a_file();
    test_four_bytes();
    TTF_Quit();
    SDL_Quit();
    return check_report();
}
