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
