// The fallback candidates (spec section 4, Candidates): the fonts a character the chosen font lacks
// may come from, in the order fontset.c tries them. On Linux, fontconfig's sort for sans-serif, which
// follows the desktop's own choices, fonts the user installed among them, and carries each font's
// character set, so no font is opened to ask what it covers. On Windows, a fixed list of Windows' own
// families, found by their Fonts registry value names, never by file name, as file names drift
// between builds; each one's coverage is learned by a probe (fontset.c). The bundled DejaVu Sans comes
// last on both. Built once, the first time a character needs it, and logged once.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "launcher.h"
#include "image.h"
#include "fontset.h"
#include "fontrules.h"
#include "debug.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <fontconfig/fontconfig.h>
#endif

#define PATH_BYTES 2048

static FontCandidate *list = NULL;
static int count = 0;
static int capacity = 0;
static bool built = false;
#ifndef _WIN32
static FcFontSet *sorted = NULL;       // Holds the character sets the candidates point to, until they are let go
#endif

// A function to add a candidate; false when out of memory (it is then left out)
static bool add(const char *path, int face, const char *family, const void *charset,
                bool (*covers)(const FontCandidate *candidate, Uint32 ch))
{
    if (count == capacity) {
        int grown = capacity > 0 ? 2 * capacity : 32;
        FontCandidate *bigger = realloc(list, (size_t) grown * sizeof(FontCandidate));
        if (bigger == NULL)
            return false;
        list = bigger;
        capacity = grown;
    }
    char *copy = strdup(path);
    if (copy == NULL)
        return false;
    list[count++] = (FontCandidate) { .path = copy, .face = face, .family = family, .charset = charset, .covers = covers };
    return true;
}

#ifdef _WIN32
// A Fonts registry value: the faces its name lists ("Segoe UI (TrueType)") and the file that holds them
typedef struct {
    char name[1024];
    char file[PATH_BYTES];
} RegistryFont;

// A function to turn a UTF-16 string into UTF-8, into a buffer; false when it does not fit
static bool to_utf8(const wchar_t *wide, char *out, int size)
{
    return WideCharToMultiByte(CP_UTF8, 0, wide, -1, out, size, NULL, NULL) > 0;
}

// A function to read the machine's Fonts registry key, where Windows' own fonts are named (fontscan.c
// reads it, and the user's, for the font picker): each value's name and file. Caller frees; NULL,
// with a count of 0, when it cannot be read.
static RegistryFont *read_registry(int *found)
{
    *found = 0;
    HKEY key;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0, KEY_READ, &key) != ERROR_SUCCESS)
        return NULL;
    RegistryFont *fonts = NULL;
    int room = 0;
    wchar_t name[512];
    wchar_t data[1024];
    for (DWORD i = 0;; i++) {
        DWORD name_size = (DWORD) (sizeof(name) / sizeof(name[0]));
        DWORD data_size = (DWORD) (sizeof(data) - sizeof(wchar_t));
        DWORD type = 0;
        LONG result = RegEnumValueW(key, i, name, &name_size, NULL, &type, (LPBYTE) data, &data_size);
        if (result == ERROR_NO_MORE_ITEMS)
            break;
        if (result != ERROR_SUCCESS || type != REG_SZ)
            continue;
        data[data_size / sizeof(wchar_t)] = L'\0';
        if (*found == room) {
            int grown = room > 0 ? 2 * room : 256;
            RegistryFont *bigger = realloc(fonts, (size_t) grown * sizeof(RegistryFont));
            if (bigger == NULL)
                break;
            fonts = bigger;
            room = grown;
        }
        if (to_utf8(name, fonts[*found].name, (int) sizeof(fonts[*found].name))
            && to_utf8(data, fonts[*found].file, (int) sizeof(fonts[*found].file)))
            (*found)++;
    }
    RegCloseKey(key);
    return fonts;
}

// A function to add Windows' own families in their fixed order, the first preferred locale choosing
// the first CJK family (fontrules_windows_families()), each found by its registry value name; its
// file is in the Fonts folder. Returns how many it added.
static int add_windows(const char *fonts_folder)
{
    int locale_count = 0;
    SDL_Locale **locales = SDL_GetPreferredLocales(&locale_count);
    const char *language = locales != NULL && locale_count > 0 ? locales[0]->language : NULL;
    const char *country = locales != NULL && locale_count > 0 ? locales[0]->country : NULL;
    const char *families[FONTRULES_WINDOWS_FAMILIES];
    int family_count = fontrules_windows_families(language, country, families);
    SDL_free(locales);   // The families are fontrules.c's own strings: nothing points into the locales
    int registered = 0;
    RegistryFont *fonts = read_registry(&registered);
    int added = 0;
    char path[PATH_BYTES];
    for (int f = 0; f < family_count; f++) {
        for (int i = 0; i < registered; i++) {
            int face = fontrules_registry_face(fonts[i].name, families[f]);
            if (face < 0)
                continue;
            snprintf(path, sizeof(path), "%s\\%s", fonts_folder, fonts[i].file);
            if (add(path, face, families[f], NULL, NULL))
                added++;
            break;
        }
    }
    free(fonts);
    return added;
}
#else
// A function to read a candidate's coverage from the character set fontconfig gave it
static bool charset_covers(const FontCandidate *candidate, Uint32 ch)
{
    return FcCharSetHasChar((const FcCharSet *) candidate->charset, ch);
}

// A function to add fontconfig's sort for sans-serif, Regular, with the user's language (which
// FcDefaultSubstitute adds), untrimmed, so a font a skipped one would have shadowed is still there.
// Each pattern is a face, or a named instance of a variable one; its index is fontconfig's
// (instance << 16) | face, which SDL3_ttf 3.2.2 hands to FT_Open_Face unchanged. A variable font's own
// pattern is left out: it opens at the default instance, which for Fedora's Noto Sans CJK VF is Thin,
// and its named instances follow it in the sort, the Regular one first. Returns how many it added.
static int add_fontconfig(void)
{
    FcPattern *pattern = FcNameParse((const FcChar8 *) "sans-serif:style=Regular");
    if (pattern == NULL)
        return 0;
    FcConfigSubstitute(NULL, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);
    FcResult result;
    sorted = FcFontSort(NULL, pattern, FcFalse, NULL, &result);
    FcPatternDestroy(pattern);
    int added = 0;
    for (int i = 0; sorted != NULL && i < sorted->nfont; i++) {
        FcChar8 *file = NULL;
        FcCharSet *charset = NULL;
        int index = 0;
        FcBool variable = FcFalse;
        if (FcPatternGetString(sorted->fonts[i], FC_FILE, 0, &file) != FcResultMatch
            || FcPatternGetCharSet(sorted->fonts[i], FC_CHARSET, 0, &charset) != FcResultMatch)
            continue;
        FcPatternGetInteger(sorted->fonts[i], FC_INDEX, 0, &index);
        if (FcPatternGetBool(sorted->fonts[i], FC_VARIABLE, 0, &variable) == FcResultMatch && variable)
            continue;
        if (add((const char *) file, index, NULL, charset, charset_covers))
            added++;
    }
    return added;
}
#endif

// A function to get the candidates, building them the first time
const FontCandidate *fontset_candidates(int *found)
{
    if (!built) {
        built = true;
#ifdef _WIN32
        char windows[MAX_PATH + 1];
        UINT length = GetWindowsDirectoryA(windows, (UINT) sizeof(windows));
        char folder[PATH_BYTES];
        snprintf(folder, sizeof(folder), "%s\\Fonts", length > 0 && length < sizeof(windows) ? windows : "C:\\Windows");
        log_debug("Fonts: %i fallback candidates in %s", add_windows(folder), folder);
#else
        log_debug("Fonts: %i fallback candidates from fontconfig", add_fontconfig());
#endif
        // The bundled DejaVu Sans last: Greek, Cyrillic, Hebrew, Arabic, symbols and some emoji, in
        // monochrome. It has no character set, so a probe learns its coverage.
        char *bundled = find_default_font(FILENAME_FALLBACK_FONT);
        if (bundled != NULL)
            add(bundled, 0, NULL, NULL, NULL);
        free(bundled);
    }
    *found = count;
    return list;
}

// A function to let go of the candidates (fontset_quit()), and on Linux of fontconfig's sort
void fontset_candidates_free(void)
{
    for (int i = 0; i < count; i++)
        free(list[i].path);
    free(list);
    list = NULL;
    count = 0;
    capacity = 0;
    built = false;
#ifndef _WIN32
    if (sorted != NULL)
        FcFontSetDestroy(sorted);
    sorted = NULL;
    // fontconfig is left to process exit: FcFini() is process-wide, and GNOME Wayland's libdecor-gtk (pango) may hold it
#endif
}
