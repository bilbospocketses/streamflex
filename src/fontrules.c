#include <stdio.h>
#include <string.h>
#include "fontrules.h"

// Unicode's Default_Ignorable_Code_Point ranges (DerivedCoreProperties.txt, Unicode 16.0): no font is
// expected to draw them, ZWJ, ZWNJ, the variation selectors and the soft hyphen among them
static const uint32_t IGNORABLE[][2] = {
    { 0x00AD, 0x00AD }, { 0x034F, 0x034F }, { 0x061C, 0x061C }, { 0x115F, 0x1160 }, { 0x17B4, 0x17B5 },
    { 0x180B, 0x180F }, { 0x200B, 0x200F }, { 0x202A, 0x202E }, { 0x2060, 0x206F }, { 0x3164, 0x3164 },
    { 0xFE00, 0xFE0F }, { 0xFEFF, 0xFEFF }, { 0xFFA0, 0xFFA0 }, { 0xFFF0, 0xFFF8 }, { 0x1BCA0, 0x1BCA3 },
    { 0x1D173, 0x1D17A }, { 0xE0000, 0xE0FFF }
};

// What a font draws as nothing by design: the spaces, Unicode's Zs less the Ogham space mark, which has
// ink, plus the line and paragraph separators (Zl and Zp); and the blank Braille pattern, which is So,
// not Zs, but has no dots, and is common as an invisible filler in names
static const uint32_t BLANK[][2] = {
    { 0x0020, 0x0020 }, { 0x00A0, 0x00A0 }, { 0x2000, 0x200A }, { 0x2028, 0x2029 }, { 0x202F, 0x202F },
    { 0x205F, 0x205F }, { 0x2800, 0x2800 }, { 0x3000, 0x3000 }
};

// A function to tell whether a code point lies in one of a table's ranges
static bool in_ranges(const uint32_t (*ranges)[2], size_t count, uint32_t code_point)
{
    for (size_t i = 0; i < count; i++) {
        if (code_point >= ranges[i][0] && code_point <= ranges[i][1])
            return true;
    }
    return false;
}

// A function to tell whether a font set never looks a code point up: a default-ignorable, or a
// control character (a wrapped note's line breaks, which SDL_ttf breaks on and never draws)
bool fontrules_skip(uint32_t code_point)
{
    if (code_point < 0x20 || (code_point >= 0x7F && code_point < 0xA0))
        return true;
    return in_ranges(IGNORABLE, sizeof(IGNORABLE) / sizeof(IGNORABLE[0]), code_point);
}

// A function to tell whether a code point is blank, a space or the blank Braille pattern: a fallback
// that has it is not asked for ink, or a CJK font would be skipped for the session over its ideographic
// space
bool fontrules_blank(uint32_t code_point)
{
    return in_ranges(BLANK, sizeof(BLANK) / sizeof(BLANK[0]), code_point);
}

// A function to find where a provider of candidate rank `rank` goes among a set's fallbacks, whose
// ranks rise: `count` when it ranks after every one (it is appended), else the index it takes (the
// chain is rebuilt)
int fontrules_insert_at(const int *ranks, int count, int rank)
{
    int at = count;
    while (at > 0 && ranks[at - 1] > rank)
        at--;
    return at;
}

// A function to write a face as the log names it. fontconfig's index for a variable font's named
// instance is (instance << 16) | face, which SDL3_ttf hands to FreeType unchanged.
void fontrules_face_text(int index, char *out, size_t size)
{
    int instance = index >> 16;
    if (instance > 0)
        snprintf(out, size, "face %i, named instance %i", index & 0xFFFF, instance);
    else
        snprintf(out, size, "face %i", index & 0xFFFF);
}

// Windows' Han and Hangul families. YaHei stays behind Yu Gothic, which lacks simplified Han.
static const char *const CJK[] = { "Yu Gothic", "Microsoft YaHei", "Microsoft JhengHei", "Malgun Gothic" };

// The families after them: Indic, Thai and Lao, African, Myanmar, Javanese, Tibetan, Mongolian,
// Armenian and Georgian, and historic scripts
static const char *const AFTER_CJK[] = {
    "Nirmala UI", "Leelawadee UI", "Ebrima", "Gadugi", "Myanmar Text", "Javanese Text", "Microsoft Himalaya",
    "Mongolian Baiti", "Sylfaen", "Segoe UI Historic"
};

// A function to compare two ASCII strings without case; NULL matches nothing
static bool same(const char *a, const char *b)
{
    if (a == NULL || b == NULL)
        return false;
    for (; *a != '\0' && *b != '\0'; a++, b++) {
        char x = (*a >= 'A' && *a <= 'Z') ? (char) (*a - 'A' + 'a') : *a;
        char y = (*b >= 'A' && *b <= 'Z') ? (char) (*b - 'A' + 'a') : *b;
        if (x != y)
            return false;
    }
    return *a == *b;
}

// A function to choose the CJK family Windows' list tries first, from the user's preferred locale
// (SDL_GetPreferredLocales' first): ja Yu Gothic, ko Malgun Gothic, zh with TW, HK or MO Microsoft
// JhengHei, anything else Microsoft YaHei
const char *fontrules_cjk_first(const char *language, const char *country)
{
    if (same(language, "ja"))
        return "Yu Gothic";
    if (same(language, "ko"))
        return "Malgun Gothic";
    if (same(language, "zh") && (same(country, "TW") || same(country, "HK") || same(country, "MO")))
        return "Microsoft JhengHei";
    return "Microsoft YaHei";
}

// A function to list Windows' fallback families in the order they are tried, FONTRULES_WINDOWS_FAMILIES
// of them: Segoe UI; Segoe UI Emoji before Symbol, so emoji draw in color; Segoe UI Symbol; the four
// CJK families, the locale's first; then the other scripts. Returns how many it wrote.
int fontrules_windows_families(const char *language, const char *country, const char **out)
{
    int n = 0;
    out[n++] = "Segoe UI";
    out[n++] = "Segoe UI Emoji";
    out[n++] = "Segoe UI Symbol";
    const char *first = fontrules_cjk_first(language, country);
    out[n++] = first;
    for (size_t i = 0; i < sizeof(CJK) / sizeof(CJK[0]); i++) {
        if (strcmp(CJK[i], first) != 0)
            out[n++] = CJK[i];
    }
    for (size_t i = 0; i < sizeof(AFTER_CJK) / sizeof(AFTER_CJK[0]); i++)
        out[n++] = AFTER_CJK[i];
    return n;
}

// A function to find a family's face in a Fonts registry value name. "Yu Gothic Regular & Yu Gothic UI
// Semilight (TrueType)" lists a collection's faces in order, and "Segoe UI (TrueType)" a file's one
// face. A face is the family when it reads the family's name alone, or with " Regular" after it, so
// Segoe UI Bold is never Segoe UI. -1 when no face is.
int fontrules_registry_face(const char *value_name, const char *family)
{
    size_t length = strlen(value_name);
    const char *paren = strrchr(value_name, '(');   // " (TrueType)" or " (OpenType)"
    if (paren != NULL && paren > value_name && paren[-1] == ' ')
        length = (size_t) (paren - 1 - value_name);
    size_t family_length = strlen(family);
    const char *start = value_name;
    const char *end = value_name + length;
    for (int face = 0; start < end; face++) {
        const char *amp = strstr(start, " & ");
        const char *stop = amp != NULL && amp < end ? amp : end;
        size_t n = (size_t) (stop - start);
        if ((n == family_length && strncmp(start, family, n) == 0)
            || (n == family_length + 8 && strncmp(start, family, family_length) == 0
                && strncmp(start + family_length, " Regular", 8) == 0))
            return face;
        start = stop == end ? end : stop + 3;
    }
    return -1;
}
