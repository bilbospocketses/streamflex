// An app's name and each library icon's name, title and match = aliases are compared normalized: in lower
// case, with punctuation dropped. The app's name is tried as it is first, then with its vendor's name
// dropped ("Google Chrome" is Chrome); an icon's side always keeps its vendor words, so "Apple TV" never
// stands for any "TV".
#include <stdbool.h>
#include <string.h>
#include "iconmatch.h"

// The words that name an app's maker rather than the app: the spec's five, and the two in front of the
// library's own brands as their apps install ("Amazon Prime Video", "NVIDIA GeForce NOW"). Whole words
// only: "Googleplex" keeps its name.
static const char *const VENDORS[] = { "microsoft", "google", "mozilla", "apple", "valve", "amazon", "nvidia" };

// A function to tell how many bytes at `p` are an apostrophe (' or U+2019), which a name drops within its
// word ("Assassin's" is "assassins"); 0 for none
static size_t apostrophe(const char *p)
{
    if (*p == '\'')
        return 1;
    if (strncmp(p, "\xE2\x80\x99", 3) == 0)
        return 3;
    return 0;
}

// A function to tell how many bytes at `p` are a sign that ends a word though it is not ASCII: the
// registered, copyright and trade mark signs ("Microsoft Edge(R)"); 0 for none
static size_t mark(const char *p)
{
    static const char *const MARKS[] = { "\xC2\xAE", "\xC2\xA9", "\xE2\x84\xA2" };
    for (size_t i = 0; i < sizeof(MARKS) / sizeof(MARKS[0]); i++) {
        size_t length = strlen(MARKS[i]);
        if (strncmp(p, MARKS[i], length) == 0)
            return length;
    }
    return 0;
}

// A function to tell whether a word names a vendor
static bool vendor(const char *word)
{
    for (size_t i = 0; i < sizeof(VENDORS) / sizeof(VENDORS[0]); i++) {
        if (strcmp(word, VENDORS[i]) == 0)
            return true;
    }
    return false;
}

// A function to add a word to a normalized name, after a space when one is there already, as much of it as
// fits
static void add_word(char *out, size_t size, size_t *used, const char *word, size_t length)
{
    if (length == 0)
        return;
    if (*used > 0) {
        if (*used + 1 >= size)
            return;
        out[(*used)++] = ' ';
    }
    size_t room = size - 1 - *used;
    size_t take = length < room ? length : room;
    memcpy(out + *used, word, take);
    *used += take;
    out[*used] = '\0';
}

// A function to normalize a name: ASCII letters in lower case (other letters stay as they are), words of
// letters and digits joined by one space, every other ASCII character and the signs mark() knows a break
// between words, an apostrophe dropped within its word; with `drop_vendors`, a vendor's name left out.
// Cut at `size`, both sides alike.
static void normalize(const char *name, char *out, size_t size, bool drop_vendors)
{
    if (size == 0)
        return;
    out[0] = '\0';
    size_t used = 0;
    char word[ICONMATCH_KEY_MAX];
    size_t length = 0;
    const char *p = name;
    for (;;) {
        size_t skip = apostrophe(p);
        if (skip > 0) {
            p += skip;
            continue;
        }
        unsigned char c = (unsigned char) *p;
        size_t ends = c != '\0' ? mark(p) : 0;
        bool letter = ends == 0 && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c >= 0x80);
        if (letter) {
            if (length + 1 < sizeof(word))
                word[length++] = (char) (c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
            p++;
            continue;
        }
        word[length] = '\0';
        if (!drop_vendors || !vendor(word))
            add_word(out, size, &used, word, length);
        length = 0;
        if (c == '\0')
            break;
        p += ends > 0 ? ends : 1;
    }
}

// A function to normalize an app's name as iconmatch_find() compares it once its vendor's name is dropped
void iconmatch_normalize(const char *name, char *out, size_t size)
{
    normalize(name != NULL ? name : "", out, size, true);
}

// A function to tell whether an icon's name, title or one of its aliases normalizes to `key`; the icon's
// side keeps its vendor words
static bool matches(const LibraryInfo *icon, const char *key)
{
    char other[ICONMATCH_KEY_MAX];
    normalize(icon->name, other, sizeof(other), false);
    if (strcmp(other, key) == 0)
        return true;
    if (icon->title != NULL) {
        normalize(icon->title, other, sizeof(other), false);
        if (strcmp(other, key) == 0)
            return true;
    }
    for (int a = 0; a < icon->match_count; a++) {
        normalize(icon->match[a], other, sizeof(other), false);
        if (strcmp(other, key) == 0)
            return true;
    }
    return false;
}

// A function to find the library icon an app's name stands for: the first in the manifest's order whose
// name, title or alias is the app's name, normalized; failing that, the first that is the app's name
// without its vendor's ("Google Chrome" is Chrome, while "Apple Music" stays Apple Music, never Music).
// -1 for none: the caller falls back to the library's apps icon (LIBRARY_FALLBACK_ICON).
int iconmatch_find(const char *app_name, const LibraryInfo *icons, int count)
{
    if (app_name == NULL)
        return -1;
    char key[ICONMATCH_KEY_MAX];
    for (int pass = 0; pass < 2; pass++) {
        normalize(app_name, key, sizeof(key), pass == 1);
        for (int i = 0; key[0] != '\0' && i < count; i++) {
            if (matches(&icons[i], key))
                return i;
        }
    }
    return -1;
}
