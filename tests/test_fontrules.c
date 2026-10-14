#include <stdint.h>
#include "check.h"
#include "fontrules.h"

// Unit test 7's pure half (spec section 9): the code points a font set never looks up, the spaces whose
// fallback is not asked for ink, where a provider goes in a chain, and the face as the log writes it

// A function to test the code points never looked up: Unicode's default-ignorables and the controls
static void test_skip(void)
{
    CHECK(fontrules_skip(0x200D));    // ZWJ
    CHECK(fontrules_skip(0xFE0F));    // VS16, emoji presentation
    CHECK(fontrules_skip(0xE0100));   // VS17, in the variation selectors supplement
    CHECK(fontrules_skip(0x200C));    // ZWNJ
    CHECK(fontrules_skip(0x00AD));    // The soft hyphen
    CHECK(fontrules_skip(0x000A));    // A wrapped note's line break
    CHECK(!fontrules_skip(0x0041));   // A
    CHECK(!fontrules_skip(0x1F600));  // An emoji
    CHECK(!fontrules_skip(0x4E2D));   // A Han character
    CHECK(!fontrules_skip(0x0020));   // A space is drawn, as nothing
}

// A function to test the controls at their edges: C0 and C1 both (Unicode's Cc), and nothing either side
static void test_skip_controls(void)
{
    CHECK(fontrules_skip(0x001F));
    CHECK(!fontrules_skip(0x007E));   // The tilde
    CHECK(fontrules_skip(0x007F));    // DEL
    CHECK(fontrules_skip(0x0085));    // The next-line control, in C1
    CHECK(fontrules_skip(0x009F));
    CHECK(!fontrules_skip(0x00A0));   // The no-break space is looked up
}

// A function to test every default-ignorable range at both ends (DerivedCoreProperties.txt, Unicode 16.0),
// and the code points beside them a wider rule would take: format characters with ink, the spaces, and
// the replacement character a broken UTF-8 sequence draws as
static void test_skip_ranges(void)
{
    CHECK(fontrules_skip(0x034F));    // The combining grapheme joiner
    CHECK(fontrules_skip(0x061C));    // The Arabic letter mark
    CHECK(fontrules_skip(0x115F));    // The Hangul fillers
    CHECK(fontrules_skip(0x1160));
    CHECK(fontrules_skip(0x17B4));    // Khmer's inherent vowels
    CHECK(fontrules_skip(0x17B5));
    CHECK(fontrules_skip(0x180B));    // Mongolian's variation selectors and vowel separator
    CHECK(fontrules_skip(0x180F));
    CHECK(fontrules_skip(0x200B));    // The zero-width space
    CHECK(fontrules_skip(0x200F));    // The right-to-left mark
    CHECK(fontrules_skip(0x202A));    // The bidirectional embeddings and overrides
    CHECK(fontrules_skip(0x202E));
    CHECK(fontrules_skip(0x2060));    // The word joiner, through the nominal digit shapes
    CHECK(fontrules_skip(0x2065));
    CHECK(fontrules_skip(0x206F));
    CHECK(fontrules_skip(0x3164));    // The Hangul filler
    CHECK(fontrules_skip(0xFE00));    // VS1
    CHECK(fontrules_skip(0xFEFF));    // The byte order mark
    CHECK(fontrules_skip(0xFFA0));    // The halfwidth Hangul filler
    CHECK(fontrules_skip(0xFFF0));
    CHECK(fontrules_skip(0xFFF8));
    CHECK(fontrules_skip(0x1BCA0));   // The shorthand format controls
    CHECK(fontrules_skip(0x1BCA3));
    CHECK(fontrules_skip(0x1D173));   // The musical symbols' beams and phrases
    CHECK(fontrules_skip(0x1D17A));
    CHECK(fontrules_skip(0xE0000));   // The tags, through the end of the supplement's block
    CHECK(fontrules_skip(0xE0FFF));
    CHECK(!fontrules_skip(0x200A));   // The hair space, before ZWSP
    CHECK(!fontrules_skip(0x2010));   // The hyphen, after the right-to-left mark
    CHECK(!fontrules_skip(0x205F));   // The medium mathematical space, before the word joiner
    CHECK(!fontrules_skip(0x0600));   // The Arabic number sign: a format character with ink
    CHECK(!fontrules_skip(0xFFF9));   // The interlinear annotation anchor: a format character, not ignorable
    CHECK(!fontrules_skip(0xFFFD));   // The replacement character
}

// A function to test the spaces a fallback draws as nothing on purpose
static void test_blank(void)
{
    CHECK(fontrules_blank(0x0020));
    CHECK(fontrules_blank(0x00A0));   // The no-break space
    CHECK(fontrules_blank(0x202F));   // The narrow no-break space
    CHECK(fontrules_blank(0x3000));   // The ideographic space, which a CJK font draws as nothing
    CHECK(!fontrules_blank(0x0041));
    CHECK(!fontrules_blank(0x2605));
}

// A function to test the rest of the spaces, and the one space with ink: a font with no ink for the Ogham
// space mark must not be taken as its provider
static void test_blank_edges(void)
{
    CHECK(fontrules_blank(0x2000));   // The en quad
    CHECK(fontrules_blank(0x200A));   // The hair space
    CHECK(fontrules_blank(0x2028));   // The line separator
    CHECK(fontrules_blank(0x2029));   // The paragraph separator
    CHECK(fontrules_blank(0x205F));   // The medium mathematical space
    CHECK(!fontrules_blank(0x1680));  // The Ogham space mark, a stroke
    CHECK(!fontrules_blank(0x3001));  // The ideographic comma
}

// A function to test where a provider goes among a set's fallbacks, kept in candidate order
static void test_insert_order(void)
{
    int ranks[] = { 2, 5 };
    CHECK_INT(fontrules_insert_at(ranks, 2, 7), 2);   // After both: appended
    CHECK_INT(fontrules_insert_at(ranks, 2, 3), 1);   // Between them: the chain is rebuilt
    CHECK_INT(fontrules_insert_at(ranks, 2, 0), 0);   // Before both
    CHECK_INT(fontrules_insert_at(NULL, 0, 4), 0);    // The first: appended
    // The spec's case: a star attached Segoe UI Symbol (rank 2), and an emoji's provider, Segoe UI
    // Emoji (rank 1), goes in front of it, or the emoji would draw from Symbol in monochrome
    int symbol[] = { 2 };
    CHECK_INT(fontrules_insert_at(symbol, 1, 1), 0);
}

// A function to test the face as the log writes it: fontconfig's index is (instance << 16) | face
static void test_face_text(void)
{
    char text[48];
    fontrules_face_text(0, text, sizeof(text));
    CHECK_STR(text, "face 0");
    fontrules_face_text(2, text, sizeof(text));
    CHECK_STR(text, "face 2");
    fontrules_face_text(4 << 16, text, sizeof(text));
    CHECK_STR(text, "face 0, named instance 4");
    fontrules_face_text((4 << 16) | 1, text, sizeof(text));
    CHECK_STR(text, "face 1, named instance 4");
}

int main(void)
{
    test_skip();
    test_skip_controls();
    test_skip_ranges();
    test_blank();
    test_blank_edges();
    test_insert_order();
    test_face_text();
    return check_report();
}
