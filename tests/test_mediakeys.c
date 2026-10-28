// The media-keys codec: the desktops' key and config names, Plasma's key sequences as value text and
// back, whether a value holds a key (Plasma's ints, GNOME's GVariant string arrays), next and previous
// over a whole state (GNOME's static keys too), the values config.ini cannot read back, and the
// [Desktop Keys] edits, alone and as config_save_with() writes them.
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "config_save.h"
#include "fileio.h"
#include "mediakeys.h"

#define DIR "mediakeys-fixture"
#define CONFIG DIR "/config.ini"

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
static bool round_trip(int (*seqs)[4], int nseq, const char *expected)
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
    int none[1][4] = { { 0, 0, 0, 0 } };
    int next[1][4] = { { 16777347, 0, 0, 0 } };
    int two[2][4] = { { 16777347, 0, 0, 0 }, { 67108864 + 16777347, 0, 0, 0 } };
    int signs[1][4] = { { INT_MAX, INT_MIN, -1, 0 } };
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
    int two[2][4] = { { 16777347, 0, 0, 0 }, { 16777346, 0, 0, 0 } };
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
        snprintf(state.values[i], sizeof(state.values[i]), "%s", values[i]);
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

    // inidoc's 199-byte line cap under the longest key, "GnomePreviousStatic=" (20 bytes): 179 bytes of
    // value fill the line exactly
    char text[MEDIAKEYS_VALUE_MAX];
    memset(text, 'x', sizeof(text));
    text[178] = '\0';
    CHECK(mediakeys_value_safe(text));                     // One byte under
    text[178] = 'x';
    text[179] = '\0';
    CHECK(mediakeys_value_safe(text));                     // At the cap
    text[179] = 'x';
    text[180] = '\0';
    CHECK(!mediakeys_value_safe(text));                    // One byte over
}

// A function to tell whether an edit is one [Desktop Keys] edit: its key, its value (NULL removes it),
// no alias, and a new key placed after the section's last
static bool is_edit(const ConfigEdit *edit, const char *key, const char *value)
{
    return strcmp(edit->section, "Desktop Keys") == 0 && strcmp(edit->key, key) == 0 && edit->alias == NULL &&
           (value == NULL ? edit->value == NULL : edit->value != NULL && strcmp(edit->value, value) == 0) &&
           edit->placement == INIDOC_AFTER_LAST_KEY;
}

// A function to test the edits that save a desktop's shortcuts: Desktop, its values pointing into the
// state, the other desktop's keys removed; refused for no desktop, a value config.ini cannot hold, or
// too little room
static void test_save_edits(void)
{
    ConfigEdit edits[MEDIAKEYS_EDITS_MAX + 1];
    MediaKeysState s = state_of(MEDIAKEYS_PLASMA, "16777347,0,0,0", "", "", "");
    CHECK_INT(mediakeys_save_edits(&s, edits, MEDIAKEYS_EDITS_MAX), 7);
    CHECK(is_edit(&edits[0], "Desktop", "plasma"));
    CHECK(is_edit(&edits[1], "PlasmaNext", "16777347,0,0,0"));
    CHECK(is_edit(&edits[2], "PlasmaPrevious", ""));
    CHECK(edits[1].value == s.values[0]);
    CHECK(edits[2].value == s.values[1]);
    CHECK(is_edit(&edits[3], "GnomeNext", NULL));
    CHECK(is_edit(&edits[4], "GnomePrevious", NULL));
    CHECK(is_edit(&edits[5], "GnomeNextStatic", NULL));
    CHECK(is_edit(&edits[6], "GnomePreviousStatic", NULL));

    s = state_of(MEDIAKEYS_GNOME, "['XF86AudioNext', '<Ctrl>XF86AudioNext']", "@as []", "['']", "['XF86AudioPrev']");
    CHECK_INT(mediakeys_save_edits(&s, edits, MEDIAKEYS_EDITS_MAX), 7);
    CHECK(is_edit(&edits[0], "Desktop", "gnome"));
    CHECK(is_edit(&edits[1], "GnomeNext", "['XF86AudioNext', '<Ctrl>XF86AudioNext']"));
    CHECK(is_edit(&edits[2], "GnomePrevious", "@as []"));
    CHECK(is_edit(&edits[3], "GnomeNextStatic", "['']"));
    CHECK(is_edit(&edits[4], "GnomePreviousStatic", "['XF86AudioPrev']"));
    CHECK(edits[4].value == s.values[3]);
    CHECK(is_edit(&edits[5], "PlasmaNext", NULL));
    CHECK(is_edit(&edits[6], "PlasmaPrevious", NULL));

    // Too little room, or nothing to save: -1, and out untouched
    ConfigEdit marker = { "x", "x", NULL, "x", INIDOC_UNDER_HEADER };
    edits[0] = marker;
    CHECK_INT(mediakeys_save_edits(&s, edits, MEDIAKEYS_EDITS_MAX - 1), -1);
    CHECK(edits[0].section == marker.section);
    s = state_of(MEDIAKEYS_PLASMA, "16777347,0,0,0", "16777346,0,0,0", "", "");
    CHECK_INT(mediakeys_save_edits(&s, edits, MEDIAKEYS_EDITS_MAX - 1), -1);
    s = state_of(MEDIAKEYS_NONE, "16777347,0,0,0", "", "", "");
    CHECK_INT(mediakeys_save_edits(&s, edits, MEDIAKEYS_EDITS_MAX), -1);
    CHECK_INT(mediakeys_save_edits(NULL, edits, MEDIAKEYS_EDITS_MAX), -1);
    CHECK_INT(mediakeys_save_edits(&s, NULL, MEDIAKEYS_EDITS_MAX), -1);
    CHECK(edits[0].section == marker.section);

    // A value config.ini cannot hold refuses the whole save; past the desktop's keys nothing is read
    s = state_of(MEDIAKEYS_GNOME, "['XF86AudioNext']", "@as []", "@as []", "['a ;b']");
    CHECK_INT(mediakeys_save_edits(&s, edits, MEDIAKEYS_EDITS_MAX), -1);
    s = state_of(MEDIAKEYS_PLASMA, "16777347,0,0,0", " 16777346,0,0,0", "", "");
    CHECK_INT(mediakeys_save_edits(&s, edits, MEDIAKEYS_EDITS_MAX), -1);
    CHECK(edits[0].section == marker.section);
    s = state_of(MEDIAKEYS_PLASMA, "16777347,0,0,0", "16777346,0,0,0", "x\ny", " ");
    CHECK_INT(mediakeys_save_edits(&s, edits, MEDIAKEYS_EDITS_MAX), 7);
}

// A function to test the edits that forget the saved shortcuts, and the one that stops the prompt
static void test_forget_and_never_edits(void)
{
    ConfigEdit edits[MEDIAKEYS_EDITS_MAX];
    CHECK_INT(mediakeys_forget_edits(edits, MEDIAKEYS_EDITS_MAX), 7);
    CHECK(is_edit(&edits[0], "Desktop", NULL));
    CHECK(is_edit(&edits[1], "PlasmaNext", NULL));
    CHECK(is_edit(&edits[2], "PlasmaPrevious", NULL));
    CHECK(is_edit(&edits[3], "GnomeNext", NULL));
    CHECK(is_edit(&edits[4], "GnomePrevious", NULL));
    CHECK(is_edit(&edits[5], "GnomeNextStatic", NULL));
    CHECK(is_edit(&edits[6], "GnomePreviousStatic", NULL));
    CHECK_INT(mediakeys_forget_edits(edits, MEDIAKEYS_EDITS_MAX - 1), -1);
    CHECK_INT(mediakeys_forget_edits(NULL, MEDIAKEYS_EDITS_MAX), -1);

    CHECK_INT(mediakeys_never_edits(edits, 1), 1);
    CHECK(is_edit(&edits[0], "Ask", "no"));
    CHECK_INT(mediakeys_never_edits(edits, 0), -1);
    CHECK_INT(mediakeys_never_edits(NULL, 1), -1);
}

// A function to start a check from a known file, with no backup lying about
static void reset(const char *text)
{
    fileio_remove(CONFIG);
    fileio_remove(CONFIG ".bak");
    CHECK(fileio_make_dirs(DIR));
    CHECK(fileio_write_all(CONFIG, text, strlen(text)));
}

// A function to tell whether the config holds exactly a text, printing it when it does not
static bool holds(const char *path, const char *text)
{
    char *content = fileio_read_all(path, NULL);
    bool same = content != NULL && strcmp(content, text) == 0;
    if (!same)
        fprintf(stderr, "%s holds:\n%s\n", path, content != NULL ? content : "(nothing)");
    free(content);
    return same;
}

// A function to save edits as the settings screen does, through config_save_with()
static bool save(const ConfigEdit *edits, int count)
{
    ConfigSaveResult result;
    bool ok = count > 0 && config_save_with(CONFIG, NULL, NULL, edits, count, NULL, 0, NULL, NULL, &result);
    if (ok && result.notes[0] != '\0')
        fprintf(stderr, "notes: %s\n", result.notes);
    return ok && result.notes[0] == '\0';
}

// A function to test the edits as config_save_with() writes them: the section made at the end of the file
// when absent, kept where it is when present, Ask kept by a save and a forget, and the other desktop's
// keys gone when a desktop is saved
static void test_edits_written(void)
{
    static const char *const ORIGINAL =
        "[General]\n"
        "DefaultMenu=Main\n"
        "\n"
        "[Main]\n"
        "Entry1=A;apps;:quit\n";
    ConfigEdit edits[MEDIAKEYS_EDITS_MAX];

    // No section yet: it is made at the end, after a blank line, and the old file is kept
    reset(ORIGINAL);
    MediaKeysState s = state_of(MEDIAKEYS_PLASMA, "16777347,0,0,0", "", "", "");
    CHECK(save(edits, mediakeys_save_edits(&s, edits, MEDIAKEYS_EDITS_MAX)));
    CHECK(holds(CONFIG,
        "[General]\n"
        "DefaultMenu=Main\n"
        "\n"
        "[Main]\n"
        "Entry1=A;apps;:quit\n"
        "\n"
        "[Desktop Keys]\n"
        "Desktop=plasma\n"
        "PlasmaNext=16777347,0,0,0\n"
        "PlasmaPrevious=\n"));
    CHECK(holds(CONFIG ".bak", ORIGINAL));

    // Don't ask again joins the section; GNOME's save then puts its keys after Ask and drops Plasma's
    CHECK(save(edits, mediakeys_never_edits(edits, MEDIAKEYS_EDITS_MAX)));
    s = state_of(MEDIAKEYS_GNOME, "['XF86AudioNext', '<Ctrl>XF86AudioNext']", "@as []", "['']", "['XF86AudioPrev']");
    CHECK(save(edits, mediakeys_save_edits(&s, edits, MEDIAKEYS_EDITS_MAX)));
    CHECK(holds(CONFIG,
        "[General]\n"
        "DefaultMenu=Main\n"
        "\n"
        "[Main]\n"
        "Entry1=A;apps;:quit\n"
        "\n"
        "[Desktop Keys]\n"
        "Desktop=gnome\n"
        "Ask=no\n"
        "GnomeNext=['XF86AudioNext', '<Ctrl>XF86AudioNext']\n"
        "GnomePrevious=@as []\n"
        "GnomeNextStatic=['']\n"
        "GnomePreviousStatic=['XF86AudioPrev']\n"));

    // Forgetting leaves Ask alone
    CHECK(save(edits, mediakeys_forget_edits(edits, MEDIAKEYS_EDITS_MAX)));
    CHECK(holds(CONFIG,
        "[General]\n"
        "DefaultMenu=Main\n"
        "\n"
        "[Main]\n"
        "Entry1=A;apps;:quit\n"
        "\n"
        "[Desktop Keys]\n"
        "Ask=no\n"));

    // A section before a menu keeps its place: new keys go after its last key, not at the file's end, and
    // its comment stays
    static const char *const MIDDLE =
        "[Desktop Keys]\n"
        "; saved by StreamFlex\n"
        "Ask=yes\n"
        "PlasmaNext=1,0,0,0\n"
        "\n"
        "[Main]\n"
        "Entry1=A;apps;:quit\n";
    reset(MIDDLE);
    s = state_of(MEDIAKEYS_GNOME, "['XF86AudioNext']", "['XF86AudioPrev']", "@as []", "@as []");
    CHECK(save(edits, mediakeys_save_edits(&s, edits, MEDIAKEYS_EDITS_MAX)));
    CHECK(holds(CONFIG,
        "[Desktop Keys]\n"
        "; saved by StreamFlex\n"
        "Ask=yes\n"
        "Desktop=gnome\n"
        "GnomeNext=['XF86AudioNext']\n"
        "GnomePrevious=['XF86AudioPrev']\n"
        "GnomeNextStatic=@as []\n"
        "GnomePreviousStatic=@as []\n"
        "\n"
        "[Main]\n"
        "Entry1=A;apps;:quit\n"));
    CHECK(save(edits, mediakeys_never_edits(edits, MEDIAKEYS_EDITS_MAX)));
    CHECK(save(edits, mediakeys_forget_edits(edits, MEDIAKEYS_EDITS_MAX)));
    CHECK(holds(CONFIG,
        "[Desktop Keys]\n"
        "; saved by StreamFlex\n"
        "Ask=no\n"
        "\n"
        "[Main]\n"
        "Entry1=A;apps;:quit\n"));

    // Don't ask again on a file with no section makes it with Ask alone
    reset(ORIGINAL);
    CHECK(save(edits, mediakeys_never_edits(edits, MEDIAKEYS_EDITS_MAX)));
    CHECK(holds(CONFIG,
        "[General]\n"
        "DefaultMenu=Main\n"
        "\n"
        "[Main]\n"
        "Entry1=A;apps;:quit\n"
        "\n"
        "[Desktop Keys]\n"
        "Ask=no\n"));

    fileio_remove(CONFIG);
    fileio_remove(CONFIG ".bak");
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
    test_save_edits();
    test_forget_and_never_edits();
    test_edits_written();
    return check_report();
}
