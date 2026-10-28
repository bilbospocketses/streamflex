// The media-keys codec: the desktops' key and config names, Plasma's key sequences as value text and
// back, whether a value holds a key (Plasma's ints, GNOME's GVariant string arrays), next and previous
// over a whole state (GNOME's static keys too), and the values config.ini cannot read back.
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include "check.h"
#include "mediakeys.h"

// A function to test the name tables: each desktop's keys in order, their [Desktop Keys] names, and the
// desktops' own names
static void test_names(void)
{
    CHECK_INT(mediakeys_key_count(MEDIAKEYS_PLASMA), 2);
    CHECK_INT(mediakeys_key_count(MEDIAKEYS_GNOME), 4);
    CHECK_INT(mediakeys_key_count(MEDIAKEYS_NONE), 0);

    CHECK_STR(mediakeys_key_name(MEDIAKEYS_PLASMA, 0), "nextmedia");
    CHECK_STR(mediakeys_key_name(MEDIAKEYS_PLASMA, 1), "previousmedia");
    CHECK(mediakeys_key_name(MEDIAKEYS_PLASMA, 2) == NULL);
    CHECK(mediakeys_key_name(MEDIAKEYS_PLASMA, -1) == NULL);
    CHECK_STR(mediakeys_key_name(MEDIAKEYS_GNOME, 0), "next");
    CHECK_STR(mediakeys_key_name(MEDIAKEYS_GNOME, 1), "previous");
    CHECK_STR(mediakeys_key_name(MEDIAKEYS_GNOME, 2), "next-static");
    CHECK_STR(mediakeys_key_name(MEDIAKEYS_GNOME, 3), "previous-static");
    CHECK(mediakeys_key_name(MEDIAKEYS_GNOME, 4) == NULL);
    CHECK(mediakeys_key_name(MEDIAKEYS_NONE, 0) == NULL);

    CHECK_STR(mediakeys_config_key(MEDIAKEYS_PLASMA, 0), "PlasmaNext");
    CHECK_STR(mediakeys_config_key(MEDIAKEYS_PLASMA, 1), "PlasmaPrevious");
    CHECK(mediakeys_config_key(MEDIAKEYS_PLASMA, 2) == NULL);
    CHECK_STR(mediakeys_config_key(MEDIAKEYS_GNOME, 0), "GnomeNext");
    CHECK_STR(mediakeys_config_key(MEDIAKEYS_GNOME, 1), "GnomePrevious");
    CHECK_STR(mediakeys_config_key(MEDIAKEYS_GNOME, 2), "GnomeNextStatic");
    CHECK_STR(mediakeys_config_key(MEDIAKEYS_GNOME, 3), "GnomePreviousStatic");
    CHECK(mediakeys_config_key(MEDIAKEYS_GNOME, 4) == NULL);
    CHECK(mediakeys_config_key(MEDIAKEYS_NONE, 0) == NULL);

    CHECK_STR(mediakeys_desktop_name(MEDIAKEYS_PLASMA), "KDE Plasma");
    CHECK_STR(mediakeys_desktop_name(MEDIAKEYS_GNOME), "GNOME");
    CHECK(mediakeys_desktop_name(MEDIAKEYS_NONE) == NULL);
    CHECK_STR(mediakeys_desktop_short_name(MEDIAKEYS_PLASMA), "Plasma");
    CHECK_STR(mediakeys_desktop_short_name(MEDIAKEYS_GNOME), "GNOME");
    CHECK(mediakeys_desktop_short_name(MEDIAKEYS_NONE) == NULL);
    CHECK_STR(mediakeys_desktop_config_name(MEDIAKEYS_PLASMA), "plasma");
    CHECK_STR(mediakeys_desktop_config_name(MEDIAKEYS_GNOME), "gnome");
    CHECK(mediakeys_desktop_config_name(MEDIAKEYS_NONE) == NULL);

    // Desktop's value back, in the case a hand edit gave it; anything else is no desktop
    CHECK(mediakeys_desktop_from_config("plasma") == MEDIAKEYS_PLASMA);
    CHECK(mediakeys_desktop_from_config("gnome") == MEDIAKEYS_GNOME);
    CHECK(mediakeys_desktop_from_config("Plasma") == MEDIAKEYS_PLASMA);
    CHECK(mediakeys_desktop_from_config("GNOME") == MEDIAKEYS_GNOME);
    CHECK(mediakeys_desktop_from_config("kde") == MEDIAKEYS_NONE);
    CHECK(mediakeys_desktop_from_config("plasm") == MEDIAKEYS_NONE);
    CHECK(mediakeys_desktop_from_config("plasmas") == MEDIAKEYS_NONE);
    CHECK(mediakeys_desktop_from_config("") == MEDIAKEYS_NONE);
    CHECK(mediakeys_desktop_from_config(NULL) == MEDIAKEYS_NONE);
}

// A function to tell whether formatting key sequences and parsing the text gives them back
static bool round_trip(const int (*seqs)[4], int nseq, const char *expected)
{
    char text[MEDIAKEYS_VALUE_MAX];
    if (!mediakeys_plasma_format(seqs, nseq, text, sizeof(text)) || strcmp(text, expected) != 0)
        return false;
    int back[8][4];
    if (mediakeys_plasma_parse(text, back, 8) != nseq)
        return false;
    return nseq == 0 || memcmp(back, seqs, sizeof(int[4]) * (size_t) nseq) == 0;
}

// A function to test Plasma's value text: no sequence, one and two, both ways
static void test_plasma_round_trip(void)
{
    static const int none[1][4] = { { 0, 0, 0, 0 } };
    static const int next[1][4] = { { 16777347, 0, 0, 0 } };
    static const int two[2][4] = { { 16777347, 0, 0, 0 }, { 67108864 + 16777347, 0, 0, 0 } };
    static const int signs[1][4] = { { INT_MAX, INT_MIN, -1, 0 } };
    CHECK(round_trip(none, 0, ""));
    CHECK(round_trip(next, 1, "16777347,0,0,0"));
    CHECK(round_trip(two, 2, "16777347,0,0,0|83886211,0,0,0"));
    CHECK(round_trip(signs, 1, "2147483647,-2147483648,-1,0"));
    CHECK(round_trip(none, 1, "0,0,0,0"));

    int seqs[2][4];
    CHECK_INT(mediakeys_plasma_parse("16777347,0,0,0", seqs, 2), 1);
    CHECK_INT(seqs[0][0], 16777347);
    CHECK_INT(seqs[0][1], 0);
    CHECK_INT(mediakeys_plasma_parse("1,2,3,4|5,6,7,8", seqs, 2), 2);
    CHECK_INT(seqs[1][0], 5);
    CHECK_INT(seqs[1][3], 8);
}

// A function to test text that is not Plasma's form, which parses to -1
static void test_plasma_malformed(void)
{
    int seqs[4][4];
    CHECK_INT(mediakeys_plasma_parse("1,2,3", seqs, 4), -1);            // Three ints
    CHECK_INT(mediakeys_plasma_parse("1,2,3,4,5", seqs, 4), -1);        // Five
    CHECK_INT(mediakeys_plasma_parse("a,0,0,0", seqs, 4), -1);          // Letters
    CHECK_INT(mediakeys_plasma_parse("16777347,0,0,x", seqs, 4), -1);
    CHECK_INT(mediakeys_plasma_parse("0x10,0,0,0", seqs, 4), -1);
    CHECK_INT(mediakeys_plasma_parse("1,2,3,4|", seqs, 4), -1);         // A trailing '|'
    CHECK_INT(mediakeys_plasma_parse("|1,2,3,4", seqs, 4), -1);
    CHECK_INT(mediakeys_plasma_parse("1,2,3,4||5,6,7,8", seqs, 4), -1);
    CHECK_INT(mediakeys_plasma_parse("1,,3,4", seqs, 4), -1);
    CHECK_INT(mediakeys_plasma_parse(" 1,2,3,4", seqs, 4), -1);         // Spaces and a '+', which strtol takes
    CHECK_INT(mediakeys_plasma_parse("1, 2,3,4", seqs, 4), -1);
    CHECK_INT(mediakeys_plasma_parse("1,2,3,4 ", seqs, 4), -1);
    CHECK_INT(mediakeys_plasma_parse("+1,2,3,4", seqs, 4), -1);
    CHECK_INT(mediakeys_plasma_parse("-,2,3,4", seqs, 4), -1);
    CHECK_INT(mediakeys_plasma_parse("2147483648,0,0,0", seqs, 4), -1);    // Past an int
    CHECK_INT(mediakeys_plasma_parse("-2147483649,0,0,0", seqs, 4), -1);
    CHECK_INT(mediakeys_plasma_parse("99999999999999999999999,0,0,0", seqs, 4), -1);
    CHECK_INT(mediakeys_plasma_parse(NULL, seqs, 4), -1);
    // More sequences than the caller has room for
    CHECK_INT(mediakeys_plasma_parse("1,0,0,0|2,0,0,0", seqs, 1), -1);
    CHECK_INT(mediakeys_plasma_parse("1,0,0,0", seqs, 0), -1);
    CHECK_INT(mediakeys_plasma_parse("", seqs, 0), 0);
}

// A function to test formatting into too little room: false, and nothing half written
static void test_plasma_format_room(void)
{
    static const int two[2][4] = { { 16777347, 0, 0, 0 }, { 16777346, 0, 0, 0 } };
    char text[32];
    CHECK(mediakeys_plasma_format(two, 2, text, 31));           // "16777347,0,0,0|16777346,0,0,0" is 29
    CHECK_STR(text, "16777347,0,0,0|16777346,0,0,0");
    CHECK(mediakeys_plasma_format(two, 2, text, 30));
    CHECK(!mediakeys_plasma_format(two, 2, text, 29));          // No room for the terminator
    CHECK_STR(text, "");
    CHECK(!mediakeys_plasma_format(two, 1, text, 14));
    CHECK_STR(text, "");
    CHECK(mediakeys_plasma_format(two, 0, text, 1));
    CHECK_STR(text, "");
    CHECK(!mediakeys_plasma_format(two, 0, text, 0));
}

// A function to test whether one value holds a key, on each desktop
static void test_value_holds(void)
{
    // Plasma: a sequence whose first int is not 0
    CHECK(mediakeys_value_holds(MEDIAKEYS_PLASMA, "16777347,0,0,0"));
    CHECK(mediakeys_value_holds(MEDIAKEYS_PLASMA, "0,0,0,0|16777346,0,0,0"));
    CHECK(!mediakeys_value_holds(MEDIAKEYS_PLASMA, ""));
    CHECK(!mediakeys_value_holds(MEDIAKEYS_PLASMA, "0,0,0,0"));
    CHECK(!mediakeys_value_holds(MEDIAKEYS_PLASMA, "0,16777347,0,0"));
    CHECK(!mediakeys_value_holds(MEDIAKEYS_PLASMA, "16777347,0,0"));     // Malformed holds nothing
    CHECK(!mediakeys_value_holds(MEDIAKEYS_PLASMA, "['XF86AudioNext']"));

    // GNOME: a string in the array that is not empty
    CHECK(!mediakeys_value_holds(MEDIAKEYS_GNOME, "@as []"));
    CHECK(!mediakeys_value_holds(MEDIAKEYS_GNOME, "[]"));
    CHECK(!mediakeys_value_holds(MEDIAKEYS_GNOME, "['']"));
    CHECK(!mediakeys_value_holds(MEDIAKEYS_GNOME, "['', '']"));
    CHECK(!mediakeys_value_holds(MEDIAKEYS_GNOME, "[\"\"]"));
    CHECK(mediakeys_value_holds(MEDIAKEYS_GNOME, "['XF86AudioNext', '<Ctrl>XF86AudioNext']"));
    CHECK(mediakeys_value_holds(MEDIAKEYS_GNOME, "['', 'XF86AudioPrev']"));
    CHECK(mediakeys_value_holds(MEDIAKEYS_GNOME, "['XF86AudioPrev', '']"));
    CHECK(mediakeys_value_holds(MEDIAKEYS_GNOME, "['XF86AudioNext']\n"));     // gsettings' line end
    CHECK(mediakeys_value_holds(MEDIAKEYS_GNOME, "['XF86AudioNext','<Super>n']"));
    // An escaped quote is a character, not the string's end
    CHECK(mediakeys_value_holds(MEDIAKEYS_GNOME, "['\\'']"));
    CHECK(mediakeys_value_holds(MEDIAKEYS_GNOME, "['it\\'s', '']"));
    CHECK(mediakeys_value_holds(MEDIAKEYS_GNOME, "['\\\\']"));
    CHECK(mediakeys_value_holds(MEDIAKEYS_GNOME, "[\"it's\"]"));      // GLib's double quotes around a '
    // Not a string array: holds nothing
    CHECK(!mediakeys_value_holds(MEDIAKEYS_GNOME, ""));
    CHECK(!mediakeys_value_holds(MEDIAKEYS_GNOME, "['XF86AudioNext'"));
    CHECK(!mediakeys_value_holds(MEDIAKEYS_GNOME, "['XF86AudioNext]"));
    CHECK(!mediakeys_value_holds(MEDIAKEYS_GNOME, "['a\\']"));             // The escape eats the closing quote
    CHECK(!mediakeys_value_holds(MEDIAKEYS_GNOME, "['a' 'b']"));
    CHECK(!mediakeys_value_holds(MEDIAKEYS_GNOME, "['a',]"));
    CHECK(!mediakeys_value_holds(MEDIAKEYS_GNOME, "['a'] x"));
    CHECK(!mediakeys_value_holds(MEDIAKEYS_GNOME, "[XF86AudioNext]"));
    CHECK(!mediakeys_value_holds(MEDIAKEYS_GNOME, "'XF86AudioNext'"));
    CHECK(!mediakeys_value_holds(MEDIAKEYS_GNOME, "16777347,0,0,0"));

    CHECK(!mediakeys_value_holds(MEDIAKEYS_NONE, "['XF86AudioNext']"));
    CHECK(!mediakeys_value_holds(MEDIAKEYS_NONE, "16777347,0,0,0"));
    CHECK(!mediakeys_value_holds(MEDIAKEYS_PLASMA, NULL));
    CHECK(!mediakeys_value_holds(MEDIAKEYS_GNOME, NULL));
}

// A function to make a state from its desktop and up to four values
static MediaKeysState state_of(MediaKeysDesktop desktop, const char *v0, const char *v1, const char *v2,
                               const char *v3)
{
    MediaKeysState state;
    memset(&state, 0, sizeof(state));
    state.desktop = desktop;
    const char *values[MEDIAKEYS_KEYS_MAX] = { v0, v1, v2, v3 };
    for (int i = 0; i < MEDIAKEYS_KEYS_MAX; i++)
        strcpy(state.values[i], values[i]);
    return state;
}

// A function to test next and previous over a whole state: Plasma's two keys, and GNOME's, where a static
// key holds as much as the user's own
static void test_holds_next_previous(void)
{
    MediaKeysState s = state_of(MEDIAKEYS_PLASMA, "16777347,0,0,0", "16777346,0,0,0", "", "");
    CHECK(mediakeys_holds_next(&s));
    CHECK(mediakeys_holds_previous(&s));
    s = state_of(MEDIAKEYS_PLASMA, "16777347,0,0,0", "", "", "");
    CHECK(mediakeys_holds_next(&s));
    CHECK(!mediakeys_holds_previous(&s));
    s = state_of(MEDIAKEYS_PLASMA, "", "16777346,0,0,0", "", "");
    CHECK(!mediakeys_holds_next(&s));
    CHECK(mediakeys_holds_previous(&s));
    s = state_of(MEDIAKEYS_PLASMA, "", "", "16777347,0,0,0", "16777346,0,0,0");   // Past Plasma's two keys
    CHECK(!mediakeys_holds_next(&s));
    CHECK(!mediakeys_holds_previous(&s));

    s = state_of(MEDIAKEYS_GNOME, "['XF86AudioNext']", "['XF86AudioPrev']", "@as []", "@as []");
    CHECK(mediakeys_holds_next(&s));
    CHECK(mediakeys_holds_previous(&s));
    s = state_of(MEDIAKEYS_GNOME, "@as []", "@as []", "['XF86AudioNext']", "@as []");     // next-static alone
    CHECK(mediakeys_holds_next(&s));
    CHECK(!mediakeys_holds_previous(&s));
    s = state_of(MEDIAKEYS_GNOME, "@as []", "@as []", "@as []", "['XF86AudioPrev']");     // previous-static alone
    CHECK(!mediakeys_holds_next(&s));
    CHECK(mediakeys_holds_previous(&s));
    s = state_of(MEDIAKEYS_GNOME, "['']", "[]", "@as []", "['']");
    CHECK(!mediakeys_holds_next(&s));
    CHECK(!mediakeys_holds_previous(&s));

    s = state_of(MEDIAKEYS_NONE, "['XF86AudioNext']", "16777346,0,0,0", "['x']", "['y']");
    CHECK(!mediakeys_holds_next(&s));
    CHECK(!mediakeys_holds_previous(&s));
    CHECK(!mediakeys_holds_next(NULL));
    CHECK(!mediakeys_holds_previous(NULL));
}

// A function to test which values config.ini reads back as written
static void test_value_safe(void)
{
    CHECK(mediakeys_value_safe(""));
    CHECK(mediakeys_value_safe("16777347,0,0,0|16777346,0,0,0"));
    CHECK(mediakeys_value_safe("['XF86AudioNext', '<Ctrl>XF86AudioNext']"));
    CHECK(mediakeys_value_safe("@as []"));
    CHECK(mediakeys_value_safe("['a;b', 'c#d']"));       // A ';' or '#' after anything but whitespace
    CHECK(!mediakeys_value_safe("['a']\n"));               // A line break
    CHECK(!mediakeys_value_safe("['a',\n'b']"));
    CHECK(!mediakeys_value_safe("['a']\r"));
    CHECK(!mediakeys_value_safe(" ['a']"));                // Leading or trailing whitespace
    CHECK(!mediakeys_value_safe("['a'] "));
    CHECK(!mediakeys_value_safe("\t['a']"));
    CHECK(mediakeys_value_safe("['a', ';b']"));            // After a quote: no comment
    CHECK(!mediakeys_value_safe("['a ;b']"));              // A ';' or '#' after whitespace: a comment
    CHECK(!mediakeys_value_safe("['a #b']"));
    CHECK(!mediakeys_value_safe("['a',\t;b']"));
    CHECK(!mediakeys_value_safe(";['a']"));                // Or at the start
    CHECK(!mediakeys_value_safe("#['a']"));
    CHECK(!mediakeys_value_safe(NULL));
}

int main(void)
{
    test_names();
    test_plasma_round_trip();
    test_plasma_malformed();
    test_plasma_format_room();
    test_value_holds();
    test_holds_next_previous();
    test_value_safe();
    return check_report();
}
