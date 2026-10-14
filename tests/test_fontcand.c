// fontcand.c (spec section 4, Candidates). On Linux, fontconfig's sort over a fixture fontconfig file
// whose one folder is the bundled fonts (CMake writes it), so no machine's own fonts are read; on
// Windows, the walk over the runner's own Fonts registry key. Both end with the bundled DejaVu Sans,
// which this file's find_default_font() stands in for, as it stands in for the launcher's log.
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "check.h"
#include "launcher.h"
#include "image.h"
#include "debug.h"
#include "fontset.h"
#include "fontrules.h"

#define BUNDLED FONTCAND_TEST_FONTS "/DejaVuSans.ttf"
#define LINES 16

static char logged[LINES][512];
static int logged_count = 0;

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

// The bundled font, where the launcher would find it
char *find_default_font(const char *font)
{
    CHECK_STR(font, FILENAME_FALLBACK_FONT);
    return strdup(BUNDLED);
}

#ifdef _WIN32
// A function to check a registry walk: every family found once and in fontrules' order for the
// preferred locale, each with no charset, so a probe learns its coverage, then the bundled DejaVu Sans
static void check_walk(const FontCandidate *list, int count)
{
    int locales = 0;
    SDL_Locale **preferred = SDL_GetPreferredLocales(&locales);
    const char *families[FONTRULES_WINDOWS_FAMILIES];
    fontrules_windows_families(preferred != NULL && locales > 0 ? preferred[0]->language : NULL,
                               preferred != NULL && locales > 0 ? preferred[0]->country : NULL, families);
    SDL_free(preferred);
    int last = -1;
    for (int i = 0; i < count - 1; i++) {
        CHECK(list[i].covers == NULL && list[i].charset == NULL);
        int at = -1;
        for (int f = 0; f < FONTRULES_WINDOWS_FAMILIES && at < 0; f++)
            at = list[i].family != NULL && strcmp(list[i].family, families[f]) == 0 ? f : -1;
        CHECK(at > last);
        last = at;
    }
    CHECK_STR(list[count - 1].path, BUNDLED);
    CHECK(list[count - 1].family == NULL && list[count - 1].covers == NULL);
}

// A function to test the registry walk: Segoe UI first, every family found once and in fontrules'
// order for the runner's locale, each with no charset, so a probe learns its coverage
static void test_registry(void)
{
    int count = 0;
    const FontCandidate *list = fontset_candidates(&count);
    CHECK(count >= 2);   // Segoe UI at least, then DejaVu Sans
    char want[64];
    snprintf(want, sizeof(want), "Fonts: %i fallback candidates in ", count - 1);
    CHECK_INT(logged_count, 1);
    CHECK(strncmp(logged[0], want, strlen(want)) == 0 && strstr(logged[0], "\\Fonts") != NULL);
    CHECK_STR(list[0].family, "Segoe UI");
    CHECK(strstr(list[0].path, "\\segoeui.ttf") != NULL);
    CHECK_INT(list[0].face, 0);
    check_walk(list, count);
    int again = 0;
    CHECK(fontset_candidates(&again) == list);   // Built once...
    CHECK_INT(logged_count, 1);                  // ...and logged once
    fontset_candidates_free();
}

// A function to test that the walk follows the preferred locale on any runner: Korean puts Malgun
// Gothic first among the CJK families, where an English runner's walk puts Microsoft YaHei first
static void test_registry_locale(void)
{
    SDL_SetHint(SDL_HINT_PREFERRED_LOCALES, "ko_KR");
    int locales = 0;
    SDL_Locale **preferred = SDL_GetPreferredLocales(&locales);
    CHECK(preferred != NULL && locales > 0 && strcmp(preferred[0]->language, "ko") == 0);
    SDL_free(preferred);
    int count = 0;
    const FontCandidate *list = fontset_candidates(&count);
    CHECK(count >= 2);
    check_walk(list, count);
    fontset_candidates_free();
    SDL_ResetHint(SDL_HINT_PREFERRED_LOCALES);
}
#else
// A function to test fontconfig's sort over the bundled fonts: all seven, each with fontconfig's
// character set and face 0, then the bundled DejaVu Sans again with none; FcFini() at the end leaves
// LeakSanitizer no fontconfig cache
static void test_fontconfig(void)
{
    int count = 0;
    const FontCandidate *list = fontset_candidates(&count);
    CHECK_INT(count, 8);
    CHECK_INT(logged_count, 1);
    CHECK_STR(logged[0], "Fonts: 7 fallback candidates from fontconfig");
    int dejavu = -1;
    int open_sans = -1;
    for (int i = 0; i < count - 1; i++) {
        CHECK(list[i].covers != NULL && list[i].charset != NULL);
        CHECK_INT(list[i].face, 0);
        CHECK(list[i].covers != NULL && list[i].covers(&list[i], 'A'));
        if (strstr(list[i].path, "/DejaVuSans.ttf") != NULL)
            dejavu = i;
        if (strstr(list[i].path, "/OpenSans-Regular.ttf") != NULL)
            open_sans = i;
    }
    CHECK(dejavu >= 0 && open_sans >= 0);
    if (dejavu >= 0 && open_sans >= 0) {
        CHECK(list[dejavu].covers(&list[dejavu], 0x2605));          // DejaVu Sans has the star...
        CHECK(!list[open_sans].covers(&list[open_sans], 0x2605));   // ...and Open Sans does not
    }
    CHECK_STR(list[count - 1].path, BUNDLED);
    CHECK(list[count - 1].covers == NULL && list[count - 1].charset == NULL);
    int again = 0;
    CHECK(fontset_candidates(&again) == list);   // Built once...
    CHECK_INT(again, count);
    CHECK_INT(logged_count, 1);                  // ...and logged once
    fontset_candidates_free();
}
#endif

int main(void)
{
#ifdef _WIN32
    test_registry();
    test_registry_locale();
#else
    test_fontconfig();
#endif
    SDL_Quit();
    return check_report();
}
