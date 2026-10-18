#include <stdbool.h>
#include <string.h>
#include "osk.h"
#include "alloc.h"

#define BOTTOM_ROW 4                   // Every page has four rows of characters, then the bottom row
#define BOTTOM_KEYS 9
#define KEY(text, shifted) { OSK_KEY_CHAR, text, text, shifted, 1.0F }
#define WIDE(text, shifted) { OSK_KEY_CHAR, text, text, shifted, 1.5F }   // The ABC grid's: seven fill its row
#define ROW(keys) { keys, (int) (sizeof(keys) / sizeof(keys[0])) }
#define LETTERS_LABEL "abc"            // The ?123 and accents keys on their own page: back to the letters
#define ACCENTS_LABEL "\xC3\xA0\xC3\xA9"   // a grave, e acute
#define BACKSPACE_LABEL "\xE2\x8C\xAB"     // U+232B, erase to the left
#define LEFT_LABEL "\xE2\x97\x80"          // U+25C0, the text cursor left
#define RIGHT_LABEL "\xE2\x96\xB6"         // U+25B6, the text cursor right

// A row of a page's keys
typedef struct {
    const OskKey *keys;
    int count;
} OskRow;

static const OskKey DIGITS[] = { KEY("1", NULL), KEY("2", NULL), KEY("3", NULL), KEY("4", NULL), KEY("5", NULL),
    KEY("6", NULL), KEY("7", NULL), KEY("8", NULL), KEY("9", NULL), KEY("0", NULL) };
static const OskKey QWERTY_TOP[] = { KEY("q", "Q"), KEY("w", "W"), KEY("e", "E"), KEY("r", "R"), KEY("t", "T"),
    KEY("y", "Y"), KEY("u", "U"), KEY("i", "I"), KEY("o", "O"), KEY("p", "P") };
static const OskKey QWERTY_MIDDLE[] = { KEY("a", "A"), KEY("s", "S"), KEY("d", "D"), KEY("f", "F"), KEY("g", "G"),
    KEY("h", "H"), KEY("j", "J"), KEY("k", "K"), KEY("l", "L") };
static const OskKey QWERTY_LOW[] = { KEY("z", "Z"), KEY("x", "X"), KEY("c", "C"), KEY("v", "V"), KEY("b", "B"),
    KEY("n", "N"), KEY("m", "M") };

static const OskKey ABC_1[] = { WIDE("a", "A"), WIDE("b", "B"), WIDE("c", "C"), WIDE("d", "D"), WIDE("e", "E"),
    WIDE("f", "F"), WIDE("g", "G") };
static const OskKey ABC_2[] = { WIDE("h", "H"), WIDE("i", "I"), WIDE("j", "J"), WIDE("k", "K"), WIDE("l", "L"),
    WIDE("m", "M"), WIDE("n", "N") };
static const OskKey ABC_3[] = { WIDE("o", "O"), WIDE("p", "P"), WIDE("q", "Q"), WIDE("r", "R"), WIDE("s", "S"),
    WIDE("t", "T"), WIDE("u", "U") };
static const OskKey ABC_4[] = { WIDE("v", "V"), WIDE("w", "W"), WIDE("x", "X"), WIDE("y", "Y"), WIDE("z", "Z"),
    WIDE("-", NULL), WIDE(".", NULL) };

static const OskKey SYMBOLS_1[] = { KEY("!", NULL), KEY("@", NULL), KEY("#", NULL), KEY("$", NULL), KEY("%", NULL),
    KEY("^", NULL), KEY("&", NULL), KEY("*", NULL), KEY("(", NULL), KEY(")", NULL) };
static const OskKey SYMBOLS_2[] = { KEY("-", NULL), KEY("_", NULL), KEY("=", NULL), KEY("+", NULL), KEY("[", NULL),
    KEY("]", NULL), KEY("{", NULL), KEY("}", NULL), KEY(";", NULL), KEY(":", NULL) };
static const OskKey SYMBOLS_3[] = { KEY("'", NULL), KEY("\"", NULL), KEY(",", NULL), KEY(".", NULL), KEY("/", NULL),
    KEY("?", NULL), KEY("\\", NULL), KEY("|", NULL), KEY("~", NULL), KEY("<", NULL), KEY(">", NULL), KEY("`", NULL) };

// The accents page (spec): the a's and c cedilla; the e's and i's; n tilde, the o's and the oe ligature;
// the u's, the y's and sharp s, which has no capital
static const OskKey ACCENTS_1[] = { KEY("\xC3\xA0", "\xC3\x80"), KEY("\xC3\xA1", "\xC3\x81"), KEY("\xC3\xA2", "\xC3\x82"),
    KEY("\xC3\xA4", "\xC3\x84"), KEY("\xC3\xA3", "\xC3\x83"), KEY("\xC3\xA5", "\xC3\x85"), KEY("\xC3\xA6", "\xC3\x86"),
    KEY("\xC3\xA7", "\xC3\x87") };
static const OskKey ACCENTS_2[] = { KEY("\xC3\xA8", "\xC3\x88"), KEY("\xC3\xA9", "\xC3\x89"), KEY("\xC3\xAA", "\xC3\x8A"),
    KEY("\xC3\xAB", "\xC3\x8B"), KEY("\xC3\xAC", "\xC3\x8C"), KEY("\xC3\xAD", "\xC3\x8D"), KEY("\xC3\xAE", "\xC3\x8E"),
    KEY("\xC3\xAF", "\xC3\x8F") };
static const OskKey ACCENTS_3[] = { KEY("\xC3\xB1", "\xC3\x91"), KEY("\xC3\xB2", "\xC3\x92"), KEY("\xC3\xB3", "\xC3\x93"),
    KEY("\xC3\xB4", "\xC3\x94"), KEY("\xC3\xB6", "\xC3\x96"), KEY("\xC3\xB5", "\xC3\x95"), KEY("\xC3\xB8", "\xC3\x98"),
    KEY("\xC5\x93", "\xC5\x92") };
static const OskKey ACCENTS_4[] = { KEY("\xC3\xB9", "\xC3\x99"), KEY("\xC3\xBA", "\xC3\x9A"), KEY("\xC3\xBB", "\xC3\x9B"),
    KEY("\xC3\xBC", "\xC3\x9C"), KEY("\xC3\xBD", "\xC3\x9D"), KEY("\xC3\xBF", "\xC5\xB8"), KEY("\xC3\x9F", NULL) };

static const OskRow QWERTY_ROWS[BOTTOM_ROW] = { ROW(DIGITS), ROW(QWERTY_TOP), ROW(QWERTY_MIDDLE), ROW(QWERTY_LOW) };
static const OskRow ABC_ROWS[BOTTOM_ROW] = { ROW(ABC_1), ROW(ABC_2), ROW(ABC_3), ROW(ABC_4) };
static const OskRow SYMBOL_ROWS[BOTTOM_ROW] = { ROW(DIGITS), ROW(SYMBOLS_1), ROW(SYMBOLS_2), ROW(SYMBOLS_3) };
static const OskRow ACCENT_ROWS[BOTTOM_ROW] = { ROW(ACCENTS_1), ROW(ACCENTS_2), ROW(ACCENTS_3), ROW(ACCENTS_4) };

// The bottom row, every page's; label_bottom() labels its first three keys for the layout and the page
static const OskKey BOTTOM[BOTTOM_KEYS] = {
    { OSK_KEY_LAYOUT, "ABC", NULL, NULL, 1.5F },
    { OSK_KEY_SYMBOLS, "?123", NULL, NULL, 1.5F },
    { OSK_KEY_ACCENTS, ACCENTS_LABEL, NULL, NULL, 1.0F },
    { OSK_KEY_SHIFT, "Shift", NULL, NULL, 1.5F },
    { OSK_KEY_SPACE, "Space", " ", NULL, 2.0F },
    { OSK_KEY_BACKSPACE, BACKSPACE_LABEL, NULL, NULL, 1.0F },
    { OSK_KEY_CURSOR_LEFT, LEFT_LABEL, NULL, NULL, 1.0F },
    { OSK_KEY_CURSOR_RIGHT, RIGHT_LABEL, NULL, NULL, 1.0F },
    { OSK_KEY_DONE, "Done", NULL, NULL, 1.5F }
};

struct Osk {
    char *label;
    char *initial;              // The text it opened with: Back cancels at once while the text is that again
    char *text;                 // Room for max_bytes, or for the text it opened with when that is longer
    size_t max_bytes;
    size_t cursor;              // A byte offset on a character's first byte, or the end
    OskCheck check;
    void *context;
    OskLayout layout;
    OskPage page;
    bool shift;                 // One-shot: the next key typed is shifted, then it goes off
    bool caps;                  // Caps lock: every key typed is shifted until Shift is pressed again
    bool asking;                // "Back again to discard" is showing
    int row;                    // The highlighted key
    int column;
    OskKey bottom[BOTTOM_KEYS]; // The bottom row, labeled for the layout and the page on show
};

// A function to label the bottom row for the layout and the page on show: the layout key names the layout
// it switches to, and the ?123 and accents keys read "abc" on their own page, where they go back to the
// letters
static void label_bottom(Osk *osk)
{
    memcpy(osk->bottom, BOTTOM, sizeof(BOTTOM));
    osk->bottom[0].label = osk->layout == OSK_QWERTY ? "ABC" : "QWERTY";
    osk->bottom[1].label = osk->page == OSK_PAGE_SYMBOLS ? LETTERS_LABEL : "?123";
    osk->bottom[2].label = osk->page == OSK_PAGE_ACCENTS ? LETTERS_LABEL : ACCENTS_LABEL;
}

// A function to give a row of the page on show
static OskRow row_of(const Osk *osk, int row)
{
    if (row == BOTTOM_ROW)
        return (OskRow) { osk->bottom, BOTTOM_KEYS };
    const OskRow *rows = osk->page == OSK_PAGE_SYMBOLS ? SYMBOL_ROWS
                       : osk->page == OSK_PAGE_ACCENTS ? ACCENT_ROWS
                       : osk->layout == OSK_ABC ? ABC_ROWS : QWERTY_ROWS;
    return rows[row];
}

// A function to measure a row: its keys' widths, end to end
static float row_width(OskRow row)
{
    float width = 0.0F;
    for (int c = 0; c < row.count; c++)
        width += row.keys[c].width;
    return width;
}

// A function to find a key's center, in key widths from the keyboard's left edge: every row is centered
// on the page's widest
static float key_center(const Osk *osk, int row, int column)
{
    float widest = 0.0F;
    for (int r = 0; r <= BOTTOM_ROW; r++) {
        float width = row_width(row_of(osk, r));
        if (width > widest)
            widest = width;
    }
    OskRow keys = row_of(osk, row);
    float x = (widest - row_width(keys)) / 2.0F;
    for (int c = 0; c < column; c++)
        x += keys.keys[c].width;
    return x + keys.keys[column].width / 2.0F;
}

// A function to find the key in a row whose center is nearest a point; the left one of two as near
static int nearest(const Osk *osk, int row, float x)
{
    int best = 0;
    float best_distance = 0.0F;
    for (int c = 0; c < row_of(osk, row).count; c++) {
        float distance = key_center(osk, row, c) - x;
        if (distance < 0.0F)
            distance = -distance;
        if (c == 0 || distance < best_distance) {
            best = c;
            best_distance = distance;
        }
    }
    return best;
}

// A function to tell a byte that continues a UTF-8 character
static bool continues(char c)
{
    return ((unsigned char) c & 0xC0) == 0x80;
}

// A function to step back from a byte offset to the first byte of the character before it
static size_t previous_character(const char *text, size_t at)
{
    do {
        at--;
    } while (at > 0 && continues(text[at]));
    return at;
}

// A function to step from a byte offset past the character there
static size_t next_character(const char *text, size_t at)
{
    do {
        at++;
    } while (text[at] != '\0' && continues(text[at]));
    return at;
}

// A function to tell whether a text is UTF-8 that types something: well formed (no stray continuation
// byte, no character cut short, overlong, a surrogate or past U+10FFFF) and with no control character
static bool typeable(const char *text)
{
    const unsigned char *p = (const unsigned char *) text;
    if (*p == '\0')
        return false;
    while (*p != '\0') {
        unsigned int c = *p;
        int more = c < 0x80 ? 0 : c >= 0xC2 && c <= 0xDF ? 1 : c >= 0xE0 && c <= 0xEF ? 2 : c >= 0xF0 && c <= 0xF4 ? 3 : -1;
        if (more < 0 || c < 0x20 || c == 0x7F)
            return false;
        unsigned int code = more == 0 ? c : c & (0x3Fu >> more);
        p++;
        for (int i = 0; i < more; i++, p++) {
            if ((*p & 0xC0) != 0x80)
                return false;
            code = (code << 6) | (*p & 0x3Fu);
        }
        if ((more == 2 && (code < 0x800 || (code >= 0xD800 && code <= 0xDFFF))) ||
            (more == 3 && (code < 0x10000 || code > 0x10FFFF)))
            return false;
    }
    return true;
}

// A function to put text in at the cursor, the cursor after it; false, with nothing changed, when the
// field would hold more than its limit
static bool insert(Osk *osk, const char *text, size_t length)
{
    size_t used = strlen(osk->text);
    if (length == 0 || used + length > osk->max_bytes)
        return false;
    memmove(osk->text + osk->cursor + length, osk->text + osk->cursor, used - osk->cursor + 1);
    memcpy(osk->text + osk->cursor, text, length);
    osk->cursor += length;
    return true;
}

// A function to take out the character before the cursor, all of it; false at the start
static bool delete_before(Osk *osk)
{
    if (osk->cursor == 0)
        return false;
    size_t start = previous_character(osk->text, osk->cursor);
    memmove(osk->text + start, osk->text + osk->cursor, strlen(osk->text + osk->cursor) + 1);
    osk->cursor = start;
    return true;
}

// A function to move the cursor a character left (direction -1) or right; false at either end
static bool step_cursor(Osk *osk, int direction)
{
    if (direction < 0 ? osk->cursor == 0 : osk->text[osk->cursor] == '\0')
        return false;
    osk->cursor = direction < 0 ? previous_character(osk->text, osk->cursor) : next_character(osk->text, osk->cursor);
    return true;
}

// A function to type text at the cursor as the keyboard's own keys type it: a one-shot Shift ends once
// something is typed
static OskResult type(Osk *osk, const char *text)
{
    if (!insert(osk, text, strlen(text)))
        return OSK_NONE;
    osk->shift = false;
    return OSK_CHANGED;
}

// A function to finish, when the validator lets Done: nothing while it says why not
static OskResult finish(const Osk *osk)
{
    return osk_why(osk) == NULL ? OSK_FINISHED : OSK_NONE;
}

// A function to answer Back: with the text as it opened, it cancels at once; after a change, the first
// Back asks ("Back again to discard"), and a second, with nothing between, cancels
static OskResult back(Osk *osk)
{
    if (strcmp(osk->text, osk->initial) == 0 || osk->asking) {
        osk->asking = false;
        return OSK_CANCELED;
    }
    osk->asking = true;
    return OSK_ASKED;
}

// A function to move the highlight a row up (-1) or down, to the key nearest the one it leaves; there is
// no row past the top or the bottom
static OskResult move_row(Osk *osk, int direction)
{
    int to = osk->row + direction;
    if (to < 0 || to > BOTTOM_ROW)
        return OSK_NONE;
    osk->column = nearest(osk, to, key_center(osk, osk->row, osk->column));
    osk->row = to;
    return OSK_MOVED;
}

// A function to press a key, as OK on the highlighted one does
static OskResult press(Osk *osk, const OskKey *key)
{
    switch (key->kind) {
        case OSK_KEY_CHAR:
            return type(osk, (osk->shift || osk->caps) && key->shifted != NULL ? key->shifted : key->text);
        case OSK_KEY_SPACE:
            return type(osk, " ");
        case OSK_KEY_BACKSPACE:
            return delete_before(osk) ? OSK_CHANGED : OSK_NONE;
        case OSK_KEY_CURSOR_LEFT:
            return step_cursor(osk, -1) ? OSK_MOVED : OSK_NONE;
        case OSK_KEY_CURSOR_RIGHT:
            return step_cursor(osk, 1) ? OSK_MOVED : OSK_NONE;
        case OSK_KEY_DONE:
            return finish(osk);
        case OSK_KEY_SHIFT:
            // Off, then one-shot, then caps lock, then off again
            if (osk->caps)
                osk->caps = false;
            else if (osk->shift) {
                osk->shift = false;
                osk->caps = true;
            }
            else
                osk->shift = true;
            return OSK_MOVED;
        case OSK_KEY_LAYOUT:
            osk->layout = osk->layout == OSK_QWERTY ? OSK_ABC : OSK_QWERTY;
            osk->page = OSK_PAGE_LETTERS;
            break;
        case OSK_KEY_SYMBOLS:
            osk->page = osk->page == OSK_PAGE_SYMBOLS ? OSK_PAGE_LETTERS : OSK_PAGE_SYMBOLS;
            break;
        case OSK_KEY_ACCENTS:
            osk->page = osk->page == OSK_PAGE_ACCENTS ? OSK_PAGE_LETTERS : OSK_PAGE_ACCENTS;
            break;
    }

    // Another layout or page: the bottom row is labeled again, and the highlight stays on its key
    label_bottom(osk);
    int length = row_of(osk, osk->row).count;
    if (osk->column >= length)
        osk->column = length - 1;
    return OSK_MOVED;
}

// A function to open the keyboard on a field: its label, the text it holds, its limit in bytes (a longer
// text, written by hand, is kept whole and can be shortened), and its validator (NULL: Done is always
// enabled) with what it needs. The cursor starts at the end, the highlight on the first letter.
Osk *osk_create(const char *label, const char *initial, int max_bytes, OskCheck check, void *context, OskLayout layout)
{
    if (label == NULL)
        label = "";
    if (initial == NULL)
        initial = "";
    Osk *osk = alloc_calloc(1, sizeof(Osk));
    if (osk == NULL)
        return NULL;
    size_t length = strlen(initial);
    osk->max_bytes = max_bytes > 0 ? (size_t) max_bytes : 0;
    osk->label = alloc_strdup(label);
    osk->initial = alloc_strdup(initial);
    osk->text = alloc_malloc((length > osk->max_bytes ? length : osk->max_bytes) + 1);
    if (osk->label == NULL || osk->initial == NULL || osk->text == NULL) {
        osk_free(osk);
        return NULL;
    }
    memcpy(osk->text, initial, length + 1);
    osk->cursor = length;
    osk->check = check;
    osk->context = context;
    osk->layout = layout == OSK_ABC ? OSK_ABC : OSK_QWERTY;
    osk->page = OSK_PAGE_LETTERS;
    osk->row = osk->layout == OSK_QWERTY ? 1 : 0;   // q, or a
    osk->column = 0;
    label_bottom(osk);
    return osk;
}

// A function to free the keyboard
void osk_free(Osk *osk)
{
    if (osk == NULL)
        return;
    alloc_free(osk->label);
    alloc_free(osk->initial);
    alloc_free(osk->text);
    alloc_free(osk);
}

// A function to act on a command: the remote's arrows, OK and Back, and the pad's shortcuts, X for
// backspace, Y for space and Start for Done (Enter on a real keyboard is Done too)
OskResult osk_command(Osk *osk, OskCommand command)
{
    if (command != OSK_BACK)
        osk->asking = false;
    switch (command) {
        case OSK_UP:
            return move_row(osk, -1);
        case OSK_DOWN:
            return move_row(osk, 1);
        case OSK_LEFT:
        case OSK_RIGHT: {
            int length = row_of(osk, osk->row).count;
            osk->column = (osk->column + (command == OSK_RIGHT ? 1 : length - 1)) % length;
            return OSK_MOVED;
        }
        case OSK_OK:
            return press(osk, &row_of(osk, osk->row).keys[osk->column]);
        case OSK_BACK:
            return back(osk);
        case OSK_BACKSPACE:
            return delete_before(osk) ? OSK_CHANGED : OSK_NONE;
        case OSK_SPACE:
            return type(osk, " ");
        case OSK_DONE:
            return finish(osk);
    }
    return OSK_NONE;
}

// A function to put in what a real keyboard's key typed (osk_key_char()), at the cursor: only UTF-8 that
// types something, and only within the limit. An on-screen Shift stays as it is: that keyboard has its own.
OskResult osk_insert(Osk *osk, const char *utf8)
{
    osk->asking = false;
    if (utf8 == NULL || !typeable(utf8))
        return OSK_NONE;
    return insert(osk, utf8, strlen(utf8)) ? OSK_CHANGED : OSK_NONE;
}

// A function to give the modifiers a real keyboard's key is looked up with (SDL_GetKeyFromScancode()'s
// `modstate`): Shift, Caps Lock and AltGr, and no other. SDL on Windows reports AltGr as Right Alt while its
// keymap keeps the AltGr characters under SDL_KMOD_MODE, so on Windows (`windows`) Right Alt, or Ctrl and Alt
// held together as Windows itself takes them, is AltGr too (user ruling 2026-10-17, "Right Alt and
// Ctrl+Alt"). X11 and Wayland report AltGr as SDL_KMOD_MODE.
unsigned int osk_key_mods(unsigned int mod, bool windows)
{
    unsigned int mods = mod & (OSK_MOD_SHIFT | OSK_MOD_CAPS | OSK_MOD_MODE);
    if (windows && ((mod & OSK_MOD_RALT) != 0 || ((mod & OSK_MOD_CTRL) != 0 && (mod & OSK_MOD_ALT) != 0)))
        mods |= OSK_MOD_MODE;
    return mods;
}

// A function to give the character a real keyboard's key types, as UTF-8 in `out` (spec, "A real keyboard
// types into the field"): `keycode` is SDL_GetKeyFromScancode()'s, with Shift, Caps Lock and AltGr applied.
// The keypad's digits, point and operators type their character as the top row's keys do (user ruling
// 2026-10-16, "Type the digit"), though SDL masks their keycodes.
// False, with `out` empty, for a key that types nothing: any other keycode with SDLK_SCANCODE_MASK set
// (Keypad Enter among them), a control character below U+0020, U+007F, U+FFFD (SDL's keycode for a key that
// types several code points), and what is no character at all (a surrogate, or past U+10FFFF).
bool osk_key_char(unsigned int keycode, char out[5])
{
    // What each key from OSK_KEYPAD_FIRST to OSK_KEYPAD_LAST types, in SDL's order; Keypad Enter nothing
    static const char KEYPAD[] = { '/', '*', '-', '+', '\0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '.' };
    out[0] = '\0';
    if (keycode >= OSK_KEYPAD_FIRST && keycode <= OSK_KEYPAD_LAST && KEYPAD[keycode - OSK_KEYPAD_FIRST] != '\0') {
        out[0] = KEYPAD[keycode - OSK_KEYPAD_FIRST];
        out[1] = '\0';
        return true;
    }
    if ((keycode & OSK_SCANCODE_MASK) != 0 || keycode < 0x20 || keycode == 0x7F || keycode == 0xFFFD ||
        (keycode >= 0xD800 && keycode <= 0xDFFF) || keycode > 0x10FFFF)
        return false;
    if (keycode < 0x80) {
        out[0] = (char) keycode;
        out[1] = '\0';
    }
    else if (keycode < 0x800) {
        out[0] = (char) (0xC0 | (keycode >> 6));
        out[1] = (char) (0x80 | (keycode & 0x3F));
        out[2] = '\0';
    }
    else if (keycode < 0x10000) {
        out[0] = (char) (0xE0 | (keycode >> 12));
        out[1] = (char) (0x80 | ((keycode >> 6) & 0x3F));
        out[2] = (char) (0x80 | (keycode & 0x3F));
        out[3] = '\0';
    }
    else {
        out[0] = (char) (0xF0 | (keycode >> 18));
        out[1] = (char) (0x80 | ((keycode >> 12) & 0x3F));
        out[2] = (char) (0x80 | ((keycode >> 6) & 0x3F));
        out[3] = (char) (0x80 | (keycode & 0x3F));
        out[4] = '\0';
    }
    return true;
}

const char *osk_text(const Osk *osk)
{
    return osk->text;
}

int osk_cursor(const Osk *osk)
{
    return (int) osk->cursor;
}

const char *osk_label(const Osk *osk)
{
    return osk->label;
}

// A function to say why Done is disabled: the validator's reason for the text now, or NULL
const char *osk_why(const Osk *osk)
{
    return osk->check != NULL ? osk->check(osk->text, osk->context) : NULL;
}

OskLayout osk_layout(const Osk *osk)
{
    return osk->layout;
}

OskPage osk_page(const Osk *osk)
{
    return osk->page;
}

bool osk_shift(const Osk *osk)
{
    return osk->shift;
}

bool osk_caps(const Osk *osk)
{
    return osk->caps;
}

int osk_rows(const Osk *osk)
{
    (void) osk;
    return BOTTOM_ROW + 1;
}

int osk_row_length(const Osk *osk, int row)
{
    return row >= 0 && row <= BOTTOM_ROW ? row_of(osk, row).count : 0;
}

const OskKey *osk_key(const Osk *osk, int row, int column)
{
    if (row < 0 || row > BOTTOM_ROW)
        return NULL;
    OskRow keys = row_of(osk, row);
    return column >= 0 && column < keys.count ? &keys.keys[column] : NULL;
}

void osk_highlight(const Osk *osk, int *row, int *column)
{
    *row = osk->row;
    *column = osk->column;
}

bool osk_asking(const Osk *osk)
{
    return osk->asking;
}
