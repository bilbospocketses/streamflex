#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "inidoc.h"
#include "fileio.h"

// A function to parse a file's text, write it back out and give the result (caller frees)
static char *round_trip(const char *text, size_t length, size_t *out_length)
{
    IniDoc *doc = inidoc_parse(text, length);
    char *out = inidoc_serialize(doc, out_length);
    inidoc_free(doc);
    return out;
}

// A function to test that a file read and written back without edits is byte-identical
static void test_round_trip_is_identical(void)
{
    static const char *const texts[] = {
        "[General]\nDefaultMenu=Main\n",
        "[General]\r\nDefaultMenu=Main\r\n\r\n; comment\r\n",
        "\xEF\xBB\xBF[General]\nDefaultMenu=Main\n",
        "[General]\nDefaultMenu=Main",
        "# hash comment\n; semicolon comment\n[Layout]\nColumns = 4 ; four across\nRows: 2\n",
        "[Main]\nEntry1=A;apps;:quit\n[Main]\nEntry1=B;apps;:quit\n",
        "[Layout]\nRows=1\n  continued\n",
        "[Caf\xC3\xA9]\nEntry1=Th\xC3\xA9;apps;:quit\n",
        "",
        "\n\n",
        "[Broken\nNoSeparator\n"
    };
    for (size_t i = 0; i < sizeof(texts) / sizeof(texts[0]); i++) {
        size_t length = 0;
        char *out = round_trip(texts[i], strlen(texts[i]), &length);
        CHECK(out != NULL && length == strlen(texts[i]) && memcmp(out, texts[i], length) == 0);
        free(out);
    }

    // The sample config the build generates, exactly as shipped
    size_t sample_length = 0;
    char *sample = fileio_read_all(SAMPLE_CONFIG, &sample_length);
    CHECK(sample != NULL);
    if (sample != NULL) {
        size_t length = 0;
        char *out = round_trip(sample, sample_length, &length);
        CHECK(out != NULL && length == sample_length && memcmp(out, sample, length) == 0);
        free(out);
        free(sample);
    }
}

// A function to test that a key is found, and its value read, the way inih reads it
static void test_get_reads_like_inih(void)
{
    const char *text =
        "Loose=before any section\n"
        "[Background]\n"
        "Mode = Slideshow ; the photos\n"
        "Semi=;x\n"
        "Spaced= ;x\n"
        "[ Games ]\n"
        "Rows: 3\n"
        "  Columns=6\n"
        "[Layout]\n"
        "Rows=1\n"
        "Rows=2\n";
    IniDoc *doc = inidoc_parse(text, strlen(text));
    CHECK_STR(inidoc_get(doc, "", "Loose"), "before any section");
    CHECK_STR(inidoc_get(doc, "Background", "Mode"), "Slideshow");
    CHECK_STR(inidoc_get(doc, "Background", "Semi"), ";x");       // No space before ';': not a comment
    CHECK_STR(inidoc_get(doc, "Background", "Spaced"), "");       // A space before ';': a comment
    CHECK_STR(inidoc_get(doc, " Games ", "Rows"), "3");           // The name keeps its spaces, as inih's does
    CHECK(inidoc_get(doc, "Games", "Rows") == NULL);
    CHECK(inidoc_get(doc, " Games ", "Columns") == NULL);         // Indented after a key: a continuation
    CHECK_STR(inidoc_get(doc, "Layout", "Rows"), "2");            // The last one wins, as in the parser
    CHECK(inidoc_get(doc, "layout", "Rows") == NULL);             // Names match exactly, case included
    inidoc_free(doc);
}

// A function to apply one set or remove (value NULL) and compare the whole file with what is expected
static void check_edit(int line, const char *text, const char *section, const char *key, const char *value,
                       IniDocPlacement placement, bool expected_ok, const char *expected)
{
    IniDoc *doc = inidoc_parse(text, strlen(text));
    bool ok = value != NULL ? inidoc_set(doc, section, key, value, placement) : inidoc_remove(doc, section, key);
    char *out = inidoc_serialize(doc, NULL);
    check_count++;
    if (ok != expected_ok || out == NULL || strcmp(out, expected) != 0) {
        check_failures++;
        fprintf(stderr, "test_inidoc.c:%d: edit gave %s and\n[%s]\nexpected %s and\n[%s]\n", line,
            ok ? "true" : "false", out != NULL ? out : "(null)", expected_ok ? "true" : "false", expected);
    }
    free(out);
    inidoc_free(doc);
}

// A function to test every way a value is set or removed
static void test_edits(void)
{
    // An existing key: only the value changes; spacing and the trailing comment stay
    check_edit(__LINE__, "[Layout]\nIconSize = 256 ; cap\n", "Layout", "IconSize", "128", INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nIconSize = 128 ; cap\n");

    // An empty value before a comment: the value goes straight after '=', so the comment stays a comment
    check_edit(__LINE__, "[Background]\nImage=   ; pick one\n", "Background", "Image", "/pics/a.png", INIDOC_AFTER_LAST_KEY, true,
               "[Background]\nImage=/pics/a.png   ; pick one\n");

    // A repeated key: the last one, which the parser uses, is the one edited
    check_edit(__LINE__, "[Layout]\nRows=1\nRows=2\n", "Layout", "Rows", "3", INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nRows=1\nRows=3\n");

    // A new key after the section's last key, before its trailing blank and comment lines
    check_edit(__LINE__, "[Layout]\nRows=1\nColumns=4\n\n; the menus\n[Main]\nEntry1=A;apps;:quit\n",
               "Layout", "IconSize", "256", INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nRows=1\nColumns=4\nIconSize=256\n\n; the menus\n[Main]\nEntry1=A;apps;:quit\n");

    // A new key in a menu section goes directly under its header, above the entries
    check_edit(__LINE__, "[Games]\nEntry1=A;apps;:quit\n", "Games", "Rows", "3", INIDOC_UNDER_HEADER, true,
               "[Games]\nRows=3\nEntry1=A;apps;:quit\n");

    // An indented key under the header would become the new key's continuation, so the new key goes after it
    check_edit(__LINE__, "[Games]\n  Rows=3\n", "Games", "Columns", "6", INIDOC_UNDER_HEADER, true,
               "[Games]\n  Rows=3\nColumns=6\n");

    // A continuation stays with its key
    check_edit(__LINE__, "[Layout]\nRows=1\n  more\n", "Layout", "Columns", "4", INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nRows=1\n  more\nColumns=4\n");

    // A missing section is added at the end after a blank line, in the file's own line endings, and a
    // file without a final newline still has none
    check_edit(__LINE__, "[General]\r\nDefaultMenu=Main", "Layout", "Columns", "5", INIDOC_AFTER_LAST_KEY, true,
               "[General]\r\nDefaultMenu=Main\r\n\r\n[Layout]\r\nColumns=5");
    check_edit(__LINE__, "[General]\nDefaultMenu=Main\n", "Layout", "Rows", "2", INIDOC_AFTER_LAST_KEY, true,
               "[General]\nDefaultMenu=Main\n\n[Layout]\nRows=2\n");

    // The BOM is kept
    check_edit(__LINE__, "\xEF\xBB\xBF[General]\nX=1\n", "General", "X", "2", INIDOC_AFTER_LAST_KEY, true,
               "\xEF\xBB\xBF[General]\nX=2\n");

    // Removing takes every occurrence, so the key is truly gone; the last line keeps "no newline"
    check_edit(__LINE__, "[Games]\nRows=3\nRows=2\nEntry1=A;apps;:quit\n", "Games", "Rows", NULL, INIDOC_UNDER_HEADER, true,
               "[Games]\nEntry1=A;apps;:quit\n");
    check_edit(__LINE__, "[Games]\nEntry1=A;apps;:quit\nRows=3", "Games", "Rows", NULL, INIDOC_UNDER_HEADER, true,
               "[Games]\nEntry1=A;apps;:quit");
    check_edit(__LINE__, "[Games]\nEntry1=A;apps;:quit\n", "Games", "Rows", NULL, INIDOC_UNDER_HEADER, false,
               "[Games]\nEntry1=A;apps;:quit\n");

    // Values the parser would read back differently are refused, and the file is left alone
    check_edit(__LINE__, "[Background]\n", "Background", "Image", "/a ;b", INIDOC_AFTER_LAST_KEY, false, "[Background]\n");
    check_edit(__LINE__, "[Background]\n", "Background", "Image", " /a", INIDOC_AFTER_LAST_KEY, false, "[Background]\n");
    check_edit(__LINE__, "[Background]\n", "Background", "Image", "/a\n", INIDOC_AFTER_LAST_KEY, false, "[Background]\n");
    check_edit(__LINE__, "[Background]\n", "Background", "Image", ";x", INIDOC_AFTER_LAST_KEY, false, "[Background]\n");
}

// A function to test that a key is edited together with its continuation lines. inih reads each
// indented line after a key as that key's value again, even past blank, comment and unreadable
// lines, and the launcher keeps the last one.
static void test_continuations(void)
{
    // Setting an existing key removes its continuation lines, so it reads exactly the new value
    check_edit(__LINE__, "[Layout]\nRows=2\n  4\n", "Layout", "Rows", "3", INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nRows=3\n");
    check_edit(__LINE__, "[Layout]\nRows=2\n\n; note\n  4\nColumns=5\n", "Layout", "Rows", "3", INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nRows=3\n\n; note\nColumns=5\n");
    check_edit(__LINE__, "[Layout]\nRows=2\n  4", "Layout", "Rows", "3", INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nRows=3");
    const char *text = "[Layout]\nRows=2\n  4\n";
    IniDoc *doc = inidoc_parse(text, strlen(text));
    CHECK(inidoc_set(doc, "Layout", "Rows", "3", INIDOC_AFTER_LAST_KEY));
    CHECK_STR(inidoc_get(doc, "Layout", "Rows"), "3");
    inidoc_free(doc);

    // A new key that would take an indented line as its continuation goes after it instead, at the
    // end of the section when the section has no key to follow
    check_edit(__LINE__, "[Games]\n   junk", "Games", "Rows", "3", INIDOC_UNDER_HEADER, true, "[Games]\n   junk\nRows=3");

    // A new key goes after the last key's continuation lines, even one past a blank line
    check_edit(__LINE__, "[Layout]\nRows=1\n\n  more\n", "Layout", "Columns", "4", INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nRows=1\n\n  more\nColumns=4\n");

    // Removing a key removes its continuation lines, leaving none to join the key before it
    check_edit(__LINE__, "[Layout]\nColumns=4\nRows=2\n  4\n", "Layout", "Rows", NULL, INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nColumns=4\n");
    check_edit(__LINE__, "[Layout]\nColumns=4\nRows=2\n\n  4\n", "Layout", "Rows", NULL, INIDOC_AFTER_LAST_KEY, true,
               "[Layout]\nColumns=4\n\n");
}

// A function to test inih's 199-byte line limit: a longer line would be misread
static void test_line_limit(void)
{
    char value[256];
    memset(value, 'a', 193);
    value[193] = '\0';                                   // "Image=" + 193 bytes = 199
    CHECK(inidoc_check("Image", value) == NULL);
    IniDoc *doc = inidoc_parse("[Background]\n", 13);
    CHECK(inidoc_set(doc, "Background", "Image", value, INIDOC_AFTER_LAST_KEY));
    value[193] = 'a';
    value[194] = '\0';                                   // 200 bytes
    CHECK(inidoc_check("Image", value) != NULL);
    CHECK(!inidoc_set(doc, "Background", "Image", value, INIDOC_AFTER_LAST_KEY));
    const char *kept = inidoc_get(doc, "Background", "Image");
    CHECK(kept != NULL && strlen(kept) == 193);
    inidoc_free(doc);

    // An existing line's trailing comment counts too
    const char *commented = "[Background]\nImage=x ; a long comment that takes up the room on this line\n";
    doc = inidoc_parse(commented, strlen(commented));
    value[150] = '\0';
    CHECK(!inidoc_set(doc, "Background", "Image", value, INIDOC_AFTER_LAST_KEY));
    CHECK_STR(inidoc_get(doc, "Background", "Image"), "x");
    inidoc_free(doc);
}

// A function to test that a section's name is matched on its first 49 bytes, as inih keeps it,
// while the header line itself is written back exactly as it was
static void test_long_section_name(void)
{
    char name[61];
    memset(name, 'M', 60);
    name[60] = '\0';
    char prefix[INIDOC_MAX_SECTION + 1];                 // The name as inih gives it to the launcher
    memcpy(prefix, name, INIDOC_MAX_SECTION);
    prefix[INIDOC_MAX_SECTION] = '\0';
    char text[128];
    char expected[128];
    snprintf(text, sizeof(text), "[%s]\nRows=3\n", name);

    IniDoc *doc = inidoc_parse(text, strlen(text));
    CHECK_STR(inidoc_get(doc, prefix, "Rows"), "3");
    CHECK_STR(inidoc_get(doc, name, "Rows"), "3");       // Agrees for 49 bytes: the same section to inih

    // Setting changes the existing key and adds no header; the 60-byte header line stays
    CHECK(inidoc_set(doc, prefix, "Rows", "4", INIDOC_UNDER_HEADER));
    snprintf(expected, sizeof(expected), "[%s]\nRows=4\n", name);
    char *out = inidoc_serialize(doc, NULL);
    CHECK_STR(out, expected);
    free(out);

    // Removing takes the key away, and the header line still stays
    CHECK(inidoc_remove(doc, prefix, "Rows"));
    CHECK(inidoc_get(doc, prefix, "Rows") == NULL);
    snprintf(expected, sizeof(expected), "[%s]\n", name);
    out = inidoc_serialize(doc, NULL);
    CHECK_STR(out, expected);
    free(out);
    inidoc_free(doc);

    // A new section with a long name is read back under the name inih gives it
    doc = inidoc_parse("[General]\n", 10);
    CHECK(inidoc_set(doc, name, "Rows", "2", INIDOC_AFTER_LAST_KEY));
    CHECK_STR(inidoc_get(doc, prefix, "Rows"), "2");
    snprintf(expected, sizeof(expected), "[General]\n\n[%s]\nRows=2\n", name);
    out = inidoc_serialize(doc, NULL);
    CHECK_STR(out, expected);
    free(out);
    inidoc_free(doc);
}

// A function to test the reasons given for a value that cannot be written
static void test_check(void)
{
    CHECK(inidoc_check("Image", "/home/me/Pictures/a b.png") == NULL);
    CHECK(inidoc_check("Image", "C:\\Pics\\a;b.png") == NULL);   // ';' after a non-space is fine
    CHECK(inidoc_check("Image", "/a ;b") != NULL);
    CHECK(inidoc_check("Image", "\t/a") != NULL);
    CHECK(inidoc_check("Image", "/a ") != NULL);
    CHECK(inidoc_check("Image", "/a\r") != NULL);
}

// A function to test that two different 60-byte section names that agree for 49 bytes are one
// section, as inih reads them: a key under either header is found, and set, through either name
static void test_long_names_agreeing_are_one_section(void)
{
    char first[61];
    char second[61];
    memset(first, 'M', 60);
    memset(second, 'M', 60);
    memset(first + INIDOC_MAX_SECTION, 'A', 60 - INIDOC_MAX_SECTION);
    memset(second + INIDOC_MAX_SECTION, 'B', 60 - INIDOC_MAX_SECTION);
    first[60] = '\0';
    second[60] = '\0';
    char text[256];
    char expected[256];
    snprintf(text, sizeof(text), "[%s]\nRows=3\n[%s]\nColumns=4\n", first, second);
    IniDoc *doc = inidoc_parse(text, strlen(text));
    CHECK_STR(inidoc_get(doc, second, "Rows"), "3");
    CHECK_STR(inidoc_get(doc, first, "Columns"), "4");
    CHECK(inidoc_set(doc, first, "Columns", "5", INIDOC_AFTER_LAST_KEY));
    snprintf(expected, sizeof(expected), "[%s]\nRows=3\n[%s]\nColumns=5\n", first, second);
    char *out = inidoc_serialize(doc, NULL);
    CHECK_STR(out, expected);
    free(out);
    inidoc_free(doc);
}

// A function to test that a new key finds a safe place when the one asked for is not: under the
// header an indented key would become its continuation, so it goes after the section's last key
static void test_safe_places(void)
{
    check_edit(__LINE__, "[Games]\n\n  Rows=3\nEntry1=A;apps;:quit\n", "Games", "Columns", "6", INIDOC_UNDER_HEADER, true,
               "[Games]\n\n  Rows=3\nEntry1=A;apps;:quit\nColumns=6\n");
    check_edit(__LINE__, "[Games]\n\n  Rows=3\n", "Games", "Columns", "6", INIDOC_UNDER_HEADER, true,
               "[Games]\n\n  Rows=3\nColumns=6\n");

    // At the end of a section with no key, before the next header
    check_edit(__LINE__, "[Games]\n  junk\n[Main]\nEntry1=A;apps;:quit\n", "Games", "Rows", "3", INIDOC_UNDER_HEADER, true,
               "[Games]\n  junk\nRows=3\n[Main]\nEntry1=A;apps;:quit\n");
}

// A function to test a key with an empty name ("=x"): inih empties its prev_name for it, so an
// indented line after it is a line of its own, not its continuation
static void test_empty_key_name(void)
{
    const char *text = "[Layout]\n=x\n  Rows=3\n";
    IniDoc *doc = inidoc_parse(text, strlen(text));
    CHECK_STR(inidoc_get(doc, "Layout", "Rows"), "3");
    inidoc_free(doc);
    check_edit(__LINE__, text, "Layout", "Rows", "4", INIDOC_AFTER_LAST_KEY, true, "[Layout]\n=x\n  Rows=4\n");

    // Removing it is refused: the indented line after it would join the key before it
    check_edit(__LINE__, "[Layout]\nRows=1\n=x\n  2\n", "Layout", "", NULL, INIDOC_AFTER_LAST_KEY, false,
               "[Layout]\nRows=1\n=x\n  2\n");
}

// A function to test that a new section's header must fit on one line too
static void test_new_header_limit(void)
{
    char section[256];
    memset(section, 'S', 197);
    section[197] = '\0';                                 // "[" + 197 + "]" = 199 bytes: it fits
    IniDoc *doc = inidoc_parse("[General]\n", 10);
    CHECK(inidoc_set(doc, section, "Rows", "2", INIDOC_AFTER_LAST_KEY));
    inidoc_free(doc);
    section[197] = 'S';
    section[198] = '\0';                                 // 200 bytes: refused, and nothing added
    doc = inidoc_parse("[General]\n", 10);
    CHECK(!inidoc_set(doc, section, "Rows", "2", INIDOC_AFTER_LAST_KEY));
    CHECK(strstr(inidoc_why(doc), "section") != NULL);
    char *out = inidoc_serialize(doc, NULL);
    CHECK_STR(out, "[General]\n");
    free(out);
    inidoc_free(doc);
}

// A function to test that a NUL inside a line survives: the line is kept by its length, so the
// bytes after the NUL are written back, whether its own key or another is set
static void test_embedded_nul(void)
{
    static const char text[] = "[S]\nA=1\nB=x\0tail\nC=3\n";
    size_t length = sizeof(text) - 1;
    size_t out_length = 0;
    char *out = round_trip(text, length, &out_length);
    CHECK(out != NULL && out_length == length && memcmp(out, text, length) == 0);
    free(out);

    IniDoc *doc = inidoc_parse(text, length);
    CHECK_STR(inidoc_get(doc, "S", "B"), "x");           // inih stops at the NUL too
    CHECK(inidoc_set(doc, "S", "A", "2", INIDOC_AFTER_LAST_KEY));
    CHECK(inidoc_set(doc, "S", "B", "y", INIDOC_AFTER_LAST_KEY));
    static const char expected[] = "[S]\nA=2\nB=y\0tail\nC=3\n";
    out = inidoc_serialize(doc, &out_length);
    CHECK(out != NULL && out_length == sizeof(expected) - 1 && memcmp(out, expected, out_length) == 0);
    free(out);
    inidoc_free(doc);
}

// A function to test continuation lines after a key of 50 bytes or more: inih gives them the key's
// name cut to 49 bytes, so they set that other key, not the one they follow
static void test_long_key_continuations(void)
{
    char long_key[61];
    char short_key[INIDOC_MAX_NAME + 1];
    memset(long_key, 'K', 60);
    long_key[60] = '\0';
    memcpy(short_key, long_key, INIDOC_MAX_NAME);
    short_key[INIDOC_MAX_NAME] = '\0';
    char text[256];
    char expected[256];

    // Setting the 49-byte key removes the continuation that inih would read as its value last
    snprintf(text, sizeof(text), "[S]\n%s=0\n%s=1\n  2\n", short_key, long_key);
    snprintf(expected, sizeof(expected), "[S]\n%s=5\n%s=1\n", short_key, long_key);
    check_edit(__LINE__, text, "S", short_key, "5", INIDOC_AFTER_LAST_KEY, true, expected);

    // Setting the long key keeps it: it sets the 49-byte key, which the edit is not about
    snprintf(text, sizeof(text), "[S]\n%s=1\n  2\n", long_key);
    snprintf(expected, sizeof(expected), "[S]\n%s=3\n  2\n", long_key);
    check_edit(__LINE__, text, "S", long_key, "3", INIDOC_AFTER_LAST_KEY, true, expected);

    // Removing the 49-byte key removes it too, though no line of its own exists
    snprintf(expected, sizeof(expected), "[S]\n%s=1\n", long_key);
    check_edit(__LINE__, text, "S", short_key, NULL, INIDOC_AFTER_LAST_KEY, true, expected);

    // A new 49-byte key goes where no such continuation comes after it
    snprintf(text, sizeof(text), "[S]\n%s=1\n  2\n", long_key);
    snprintf(expected, sizeof(expected), "[S]\n%s=1\n  2\n%s=7\n", long_key, short_key);
    check_edit(__LINE__, text, "S", short_key, "7", INIDOC_UNDER_HEADER, true, expected);
}

// A function to test the reasons inidoc_why() gives for a set that failed
static void test_why(void)
{
    const char *commented = "[Background]\nImage=x ; a long comment that takes up the room on this line\n";
    IniDoc *doc = inidoc_parse(commented, strlen(commented));
    char value[256];
    memset(value, 'a', 150);
    value[150] = '\0';
    CHECK(!inidoc_set(doc, "Background", "Image", value, INIDOC_AFTER_LAST_KEY));
    CHECK(strstr(inidoc_why(doc), "too long for one line of config.ini") != NULL);
    CHECK(strstr(inidoc_why(doc), "with its comment") != NULL);
    CHECK(!inidoc_set(doc, "Background", "Image", "/a ;b", INIDOC_AFTER_LAST_KEY));
    CHECK_STR(inidoc_why(doc), inidoc_check("Image", "/a ;b"));
    CHECK(inidoc_set(doc, "Background", "Image", "/a.png", INIDOC_AFTER_LAST_KEY));
    CHECK_STR(inidoc_why(doc), "");
    inidoc_free(doc);

    // Spacing alone, with no comment, is named as such
    char spaced[256];
    snprintf(spaced, sizeof(spaced), "[Background]\nImage                                                  =   x\n");
    doc = inidoc_parse(spaced, strlen(spaced));
    CHECK(!inidoc_set(doc, "Background", "Image", value, INIDOC_AFTER_LAST_KEY));
    CHECK(strstr(inidoc_why(doc), "spacing") != NULL);
    inidoc_free(doc);
}

// A function to test that asking first (inidoc_check_in) agrees with setting: for every length of
// value, it refuses exactly what the set would refuse, with the same reason
static void test_check_in_agrees_with_set(void)
{
    static const char *const texts[] = {
        "[Background]\nImage=x ; a long comment that takes up the room on this line\n",
        "[Background]\nImage                                                  =   x\n",
        "[Background]\nMode=Image\n",
        "[General]\n"
    };
    char value[256];
    for (size_t t = 0; t < sizeof(texts) / sizeof(texts[0]); t++) {
        for (size_t length = 1; length <= 200; length++) {
            memset(value, 'a', length);
            value[length] = '\0';
            IniDoc *doc = inidoc_parse(texts[t], strlen(texts[t]));
            const char *asked = inidoc_check_in(doc, "Background", "Image", value);
            bool set = inidoc_set(doc, "Background", "Image", value, INIDOC_AFTER_LAST_KEY);
            check_count++;
            if ((asked == NULL) != set || (asked != NULL && strcmp(asked, inidoc_why(doc)) != 0)) {
                check_failures++;
                fprintf(stderr, "test_inidoc.c:%d: text %d, %d bytes: asked \"%s\", the set said %s \"%s\"\n", __LINE__,
                    (int) t, (int) length, asked != NULL ? asked : "(null)", set ? "yes" : "no", inidoc_why(doc));
            }
            inidoc_free(doc);
        }
    }

    // A section too long for its header is refused before it is added
    char section[256];
    memset(section, 'S', 198);
    section[198] = '\0';
    IniDoc *doc = inidoc_parse("[General]\n", 10);
    CHECK(inidoc_check_in(doc, section, "Rows", "2") != NULL);
    CHECK(!inidoc_set(doc, section, "Rows", "2", INIDOC_AFTER_LAST_KEY));
    CHECK_STR(inidoc_check_in(doc, section, "Rows", "2"), inidoc_why(doc));
    inidoc_free(doc);
}

static const char *const GAMEPAD_SETTINGS[] = { "Enabled", "DeviceIndex", "ControllerMappingsFile", NULL };

// A function to parse a text, failing the check when it does not parse
static IniDoc *parse(const char *text)
{
    IniDoc *doc = inidoc_parse(text, strlen(text));
    CHECK(doc != NULL);
    return doc;
}

// A function to write a document out for comparing (a static buffer)
static const char *written(const IniDoc *doc)
{
    static char text[4096];
    char *out = inidoc_serialize(doc, NULL);
    snprintf(text, sizeof(text), "%s", out != NULL ? out : "");
    free(out);
    return text;
}

// A function to test listing a section's lines: in order, duplicates kept, settings and
// continuations left out
static void test_list(void)
{
    IniDoc *doc = parse("[Gamepad]\nEnabled=true\nButtonA=:select ; confirm\n  :back\nButtonA=:up\n; a comment\n"
                        "DeviceIndex=0\n\n[Hotkeys]\nHotkey1=#4000003A;:quit\nHotkey1=#40000045;:settings\n[Main]\n"
                        "Entry1=One;apps;:quit\n");
    IniDocItem items[8];
    int count = inidoc_list(doc, "Gamepad", GAMEPAD_SETTINGS, items, 8);
    CHECK_INT(count, 2);
    CHECK_STR(items[0].key, "ButtonA");
    CHECK_STR(items[0].value, ":select");
    CHECK_STR(items[0].text, "ButtonA=:select ; confirm");
    CHECK_STR(items[1].value, ":up");
    count = inidoc_list(doc, "Hotkeys", NULL, items, 8);
    CHECK_INT(count, 2);                                    // One key name, two bindings: inih reads both
    CHECK_STR(items[1].value, "#40000045;:settings");
    CHECK_INT(inidoc_list(doc, "Hotkeys", NULL, items, 1), 2);   // The count, though only one fits
    CHECK_INT(inidoc_list(doc, "Nowhere", NULL, items, 8), 0);
    CHECK_INT(inidoc_find_line(doc, "Hotkeys", "Hotkey1=#40000045;:settings"), items[1].line);
    CHECK_INT(inidoc_find_line(doc, "Hotkeys", "Hotkey1=#40000045;:quit"), -1);
    CHECK_INT(inidoc_find_line(doc, "Gamepad", "  :back"), -1);  // A continuation is not a line of the list
    inidoc_free(doc);
}

// A function to test setting a line: the same key keeps its spacing and comment, and another key
// is written in its place; only the line asked for changes, though another has the same key
static void test_list_set(void)
{
    IniDoc *doc = parse("[Gamepad]\nButtonA = :select ; confirm\nButtonA=:up\n");
    IniDocItem items[4];
    inidoc_list(doc, "Gamepad", GAMEPAD_SETTINGS, items, 4);
    CHECK(inidoc_list_set(doc, items[1].line, "ButtonA", ":down"));
    CHECK_STR(written(doc), "[Gamepad]\nButtonA = :select ; confirm\nButtonA=:down\n");
    CHECK(inidoc_list_set(doc, items[0].line, "ButtonA", ":home"));
    CHECK_STR(written(doc), "[Gamepad]\nButtonA = :home ; confirm\nButtonA=:down\n");
    CHECK(inidoc_list_set(doc, items[0].line, "ButtonB", ":back"));
    CHECK_STR(written(doc), "[Gamepad]\nButtonB=:back ; confirm\nButtonA=:down\n");
    inidoc_free(doc);

    // Refused: not a key line; a value config.ini cannot hold; another key over continuation lines
    doc = parse("[Hotkeys]\n; keys\nHotkey1=#4000003A;:quit\n  :home\n");
    CHECK(!inidoc_list_set(doc, 1, "Hotkey1", "#4000003A;:up"));
    CHECK_STR(inidoc_why(doc), "the line is not a key");
    CHECK(!inidoc_list_set(doc, 2, "Hotkey1", " ;x"));
    CHECK(!inidoc_list_set(doc, 2, "Hotkey9", "#4000003A;:up"));
    CHECK_STR(inidoc_why(doc), "it would change how other lines read");
    CHECK(inidoc_list_set(doc, 2, "Hotkey1", "#4000003A;:up"));   // The same key: its continuation stays its own
    CHECK_STR(written(doc), "[Hotkeys]\n; keys\nHotkey1=#4000003A;:up\n  :home\n");
    inidoc_free(doc);
}

// A function to test adding a line: after the section's last key (before a comment or blank line
// that follows it), under the header of a section with no keys, or in a new section at the end
static void test_list_add(void)
{
    IniDoc *doc = parse("[Hotkeys]\r\nHotkey1=#4000003A;:quit\r\n\r\n; next\r\n[Main]\r\nEntry1=One;apps;:quit\r\n");
    CHECK(inidoc_list_add(doc, "Hotkeys", "Hotkey2", "#40000045;:home"));
    CHECK_STR(written(doc), "[Hotkeys]\r\nHotkey1=#4000003A;:quit\r\nHotkey2=#40000045;:home\r\n\r\n; next\r\n"
                            "[Main]\r\nEntry1=One;apps;:quit\r\n");
    inidoc_free(doc);

    doc = parse("[Gamepad]\n; none yet\n");
    CHECK(inidoc_list_add(doc, "Gamepad", "ButtonA", ":select"));
    CHECK(inidoc_list_add(doc, "Gamepad", "ButtonA", ":up"));        // A label twice is allowed
    CHECK_STR(written(doc), "[Gamepad]\nButtonA=:select\nButtonA=:up\n; none yet\n");
    inidoc_free(doc);

    doc = parse("[Main]\nEntry1=One;apps;:quit");
    CHECK(inidoc_list_add(doc, "Hotkeys", "Hotkey1", "#4000003A;:quit"));
    CHECK_STR(written(doc), "[Main]\nEntry1=One;apps;:quit\n\n[Hotkeys]\nHotkey1=#4000003A;:quit");
    CHECK(!inidoc_list_add(doc, "Hotkeys", "Hotkey2", "x\ny"));
    inidoc_free(doc);
}

// A function to test removing a line: only it, with its continuation lines, and the file's last
// line keeping its missing line ending
static void test_list_remove(void)
{
    IniDoc *doc = parse("[Gamepad]\nButtonA=:select\n  :back\n; keep\nButtonA=:up");
    IniDocItem items[4];
    inidoc_list(doc, "Gamepad", GAMEPAD_SETTINGS, items, 4);
    CHECK(inidoc_list_remove(doc, items[0].line));
    CHECK_STR(written(doc), "[Gamepad]\n; keep\nButtonA=:up");
    inidoc_list(doc, "Gamepad", GAMEPAD_SETTINGS, items, 4);
    CHECK(inidoc_list_remove(doc, items[0].line));
    CHECK_STR(written(doc), "[Gamepad]\n; keep");
    CHECK(!inidoc_list_remove(doc, 1));                   // A comment is not a key line
    CHECK(!inidoc_list_remove(doc, 7));
    inidoc_free(doc);
}

// A function to test the edges of listing and finding: a list too short for every item is written
// no further than its end, and a line is found only by its whole text, in its own section
static void test_list_edges(void)
{
    IniDoc *doc = parse("[Gamepad]\nButtonA=:select\n[Hotkeys]\nHotkey1=#4000003A;:quit\nHotkey1=#40000045;:settings\n");
    IniDocItem items[3];
    memset(items, 0, sizeof(items));
    items[1].line = -7;
    CHECK_INT(inidoc_list(doc, "Hotkeys", NULL, items, 1), 2);
    CHECK_INT(items[0].line, 3);
    CHECK_INT(items[1].line, -7);                                // Past `max`: untouched
    CHECK_INT(inidoc_list(doc, "Hotkeys", NULL, NULL, 0), 2);    // Only the count
    CHECK_INT(inidoc_find_line(doc, "Hotkeys", "Hotkey1=#4000003A"), -1);             // A part of a line is not it
    CHECK_INT(inidoc_find_line(doc, "Gamepad", "Hotkey1=#40000045;:settings"), -1);   // Another section's line
    CHECK_INT(inidoc_find_line(doc, "Gamepad", "ButtonA=:select"), 1);
    inidoc_free(doc);
}

// A function to test the edges of setting a line: an index outside the document, a line too long for
// config.ini with its comment or spacing (199 bytes fits), a key that would not read back, and a key
// with an empty name, whose indented lines would become the new key's continuations
static void test_list_set_edges(void)
{
    // An index past the last line, though the slot after it once held a key (removed since)
    IniDoc *doc = parse("[S]\nA=1\nB=2\n");
    CHECK(inidoc_list_remove(doc, 2));
    CHECK(!inidoc_list_set(doc, 2, "B", "3"));
    CHECK_STR(inidoc_why(doc), "the line is not a key");
    CHECK(!inidoc_list_set(doc, -1, "A", "3"));
    CHECK_STR(inidoc_why(doc), "the line is not a key");
    CHECK(!inidoc_list_remove(doc, 2));
    CHECK(!inidoc_list_remove(doc, -1));
    CHECK_STR(written(doc), "[S]\nA=1\n");
    inidoc_free(doc);

    // A value config.ini cannot hold is refused with inidoc_check()'s own reason
    doc = parse("[Hotkeys]\nHotkey1=#4000003A;:quit\n");
    CHECK(!inidoc_list_set(doc, 1, "Hotkey1", " ;x"));
    CHECK_STR(inidoc_why(doc), inidoc_check("Hotkey1", " ;x"));
    inidoc_free(doc);

    // Too long with the line's comment: "K = " + value + " ; comment" is 14 bytes and the value
    char value[256];
    memset(value, 'a', 186);
    value[185] = '\0';
    doc = parse("[S]\nK = x ; comment\n");
    CHECK(inidoc_list_set(doc, 1, "K", value));                 // 199 bytes: it fits
    value[185] = 'a';
    value[186] = '\0';
    CHECK(!inidoc_list_set(doc, 1, "K", value));                // 200 bytes
    CHECK(strstr(inidoc_why(doc), "with its comment") != NULL);
    value[187] = '\0';
    memset(value, 'b', 187);
    CHECK(inidoc_list_set(doc, 1, "J", value));                 // Another key: "J=" + 187 + " ; comment" is 199
    CHECK_STR(inidoc_why(doc), "");                             // The refusal before it is not kept
    value[187] = 'b';
    value[188] = '\0';
    CHECK(!inidoc_list_set(doc, 1, "I", value));                // 200
    CHECK(strstr(inidoc_why(doc), "with its comment") != NULL);
    inidoc_free(doc);

    // Too long with the line's spacing alone
    char spaced[128];
    snprintf(spaced, sizeof(spaced), "[S]\nK%20s=%20sx\n", "", "");
    doc = parse(spaced);
    memset(value, 'c', 158);
    value[158] = '\0';                                          // 42 bytes before the value: 200
    CHECK(!inidoc_list_set(doc, 1, "K", value));
    CHECK(strstr(inidoc_why(doc), "spacing") != NULL);
    value[157] = '\0';
    CHECK(inidoc_list_set(doc, 1, "K", value));
    inidoc_free(doc);

    // A key that would not read back as itself: refused, and the line is as it was
    doc = parse("[Gamepad]\nButtonA=:select ; confirm\n");
    CHECK(!inidoc_list_set(doc, 1, "Bad=key", ":x"));
    CHECK_STR(inidoc_why(doc), "it would change how other lines read");
    CHECK(!inidoc_list_set(doc, 1, ";c", ":x"));                // Not a key at all: a comment
    CHECK_STR(inidoc_why(doc), "it would change how other lines read");
    CHECK(!inidoc_list_set(doc, 1, "ButtonB ", ":x"));          // inih reads the key without its space
    CHECK_STR(inidoc_why(doc), "it would change how other lines read");
    CHECK_STR(written(doc), "[Gamepad]\nButtonA=:select ; confirm\n");
    IniDocItem items[2];
    CHECK_INT(inidoc_list(doc, "Gamepad", NULL, items, 2), 1);
    CHECK_STR(items[0].value, ":select");
    inidoc_free(doc);

    // A key with an empty name ends the key before it, so the indented line after it is a key of its
    // own; another key there would read it as a continuation, so it is refused
    doc = parse("[Gamepad]\n=x\n  ButtonA=:a\n");
    CHECK(!inidoc_list_set(doc, 1, "ButtonB", ":b"));
    CHECK_STR(inidoc_why(doc), "it would change how other lines read");
    CHECK_STR(written(doc), "[Gamepad]\n=x\n  ButtonA=:a\n");
    CHECK_INT(inidoc_list(doc, "Gamepad", NULL, items, 2), 2);
    CHECK(inidoc_list_set(doc, 1, "", "y"));                    // The same empty key: nothing changes how it reads
    CHECK_STR(written(doc), "[Gamepad]\n=y\n  ButtonA=:a\n");
    inidoc_free(doc);

    // An indented line that starts with '#' or ';' is a comment to inih, not a continuation, so
    // another key may take the line before it
    doc = parse("[Hotkeys]\nHotkey1=#4000003A;:quit\n  #40000045;:home\n");
    CHECK(inidoc_list_set(doc, 1, "Hotkey2", "#4000003A;:up"));
    CHECK_STR(written(doc), "[Hotkeys]\nHotkey2=#4000003A;:up\n  #40000045;:home\n");
    inidoc_free(doc);
}

// A function to test the edges of adding a line: after the last key's continuation lines, at the
// end of the section when under the header an indented line would become its continuation, and
// refused, with the file as it was, when no place reads it back
static void test_list_add_edges(void)
{
    IniDoc *doc = parse("[Gamepad]\nButtonA=:select\n  :back\n\n[Main]\nEntry1=One;apps;:quit\n");
    CHECK(inidoc_list_add(doc, "Gamepad", "ButtonB", ":x"));
    CHECK_STR(written(doc), "[Gamepad]\nButtonA=:select\n  :back\nButtonB=:x\n\n[Main]\nEntry1=One;apps;:quit\n");
    inidoc_free(doc);

    doc = parse("[Gamepad]\n  junk\n[Main]\nEntry1=One;apps;:quit\n");
    CHECK(inidoc_list_add(doc, "Gamepad", "ButtonA", ":select"));
    CHECK_STR(inidoc_why(doc), "");                             // The first place failed; the second did not
    CHECK_STR(written(doc), "[Gamepad]\n  junk\nButtonA=:select\n[Main]\nEntry1=One;apps;:quit\n");
    inidoc_free(doc);

    static const char *const keyed = "[Gamepad]\nButtonA=:select\n; end\n[Main]\nEntry1=One;apps;:quit\n";
    doc = parse(keyed);
    CHECK(!inidoc_list_add(doc, "Gamepad", "Bad=key", ":x"));
    CHECK_STR(inidoc_why(doc), "it would change how other lines read");
    CHECK(!inidoc_list_add(doc, "Gamepad", ";c", ":x"));
    CHECK_STR(inidoc_why(doc), "it would change how other lines read");
    CHECK(!inidoc_list_add(doc, "Gamepad", "ButtonB ", ":x"));  // inih reads the key without its space
    CHECK_STR(inidoc_why(doc), "it would change how other lines read");
    CHECK(!inidoc_list_add(doc, "Gamepad", "ButtonB", "x\ny"));
    CHECK_STR(inidoc_why(doc), inidoc_check("ButtonB", "x\ny"));
    CHECK_STR(written(doc), keyed);
    inidoc_free(doc);

    doc = parse("[Gamepad]\n");                                 // One place only: under the header, at the end
    CHECK(!inidoc_list_add(doc, "Gamepad", "Bad=key", ":x"));
    CHECK_STR(written(doc), "[Gamepad]\n");
    inidoc_free(doc);
}

// A function to test the lines that are not removed: a key with an empty name (the indented lines
// after it are keys of their own, and with it gone they would become the key before it's
// continuations), a section header, and a continuation line that reads like a key
static void test_list_remove_edges(void)
{
    IniDoc *doc = parse("[Gamepad]\nButtonA=:a\n=x\n  :b\n");
    CHECK(!inidoc_list_remove(doc, 2));
    CHECK_STR(written(doc), "[Gamepad]\nButtonA=:a\n=x\n  :b\n");
    inidoc_free(doc);

    doc = parse("[Gamepad]\nButtonA=:a\n  ButtonB=:b\n");
    CHECK(!inidoc_list_remove(doc, 0));
    CHECK(!inidoc_list_remove(doc, 2));
    CHECK_STR(written(doc), "[Gamepad]\nButtonA=:a\n  ButtonB=:b\n");
    inidoc_free(doc);
}

int main(void)
{
    test_round_trip_is_identical();
    test_get_reads_like_inih();
    test_edits();
    test_continuations();
    test_line_limit();
    test_long_section_name();
    test_check();
    test_long_names_agreeing_are_one_section();
    test_safe_places();
    test_empty_key_name();
    test_new_header_limit();
    test_embedded_nul();
    test_long_key_continuations();
    test_why();
    test_check_in_agrees_with_set();
    test_list();
    test_list_set();
    test_list_add();
    test_list_remove();
    test_list_edges();
    test_list_set_edges();
    test_list_add_edges();
    test_list_remove_edges();
    return check_report();
}
