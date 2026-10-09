#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include "launcher.h"
#include <launcher_config.h>
#include "settings_fonts.h"
#include "fontlist.h"
#include "fontscan.h"
#include "fileio.h"
#include "alloc.h"
#include "image.h"
#include "clock.h"
#include "test_hooks.h"
#include "debug.h"

extern SDL_Renderer *renderer;
extern TextInfo title_info;
extern Clock *clk;

#define FONT_SAMPLE_PERCENT 70 // A family's sample: its size in points, as a share of the row font's line height
#define FONT_FILES_PER_FRAME 6 // Files read per frame while the list loads
#define FONT_CACHE_SIZE 32     // Families drawn in their own face, kept while the picker is open

static FontScan *font_scan = NULL;      // The listing thread, until its files are read
static FontList *fonts = NULL;          // The families, kept for the session once read
static int font_files_read = 0;
static int font_files_skipped = 0;
static bool font_read_failed = false;   // A face could not be added (out of memory): the list is let go
static Uint32 font_scan_start = 0;
static bool ready = false;              // The fonts were read, and are kept for the session
static struct {
    char *path;                         // The face drawn; NULL for an empty place
    int face;
    SDL_Texture *texture;               // The family's name drawn in its own face; NULL when it would not draw
    int w;
    int h;
    Uint32 used;
} font_cache[FONT_CACHE_SIZE];
static Uint32 font_clock = 0;

// A function to tell whether the fonts were read, and are kept for the session
bool fonts_ready(void)
{
    return ready;
}

// A function to count the files read so far, for "Loading fonts... (N)"
int fonts_files_read(void)
{
    return font_files_read;
}

// A function to read the file from a font row's value
const char *fonts_value_path(const char *value)
{
    const char *bar = strchr(value, '|');
    return bar != NULL ? bar + 1 : value;
}

// A function to get a family's name drawn in its own face, from the cache. A face that cannot be
// opened or drawn is kept as such, so it is tried once: NULL, and its row draws in the settings'
// font. Each draw is logged, as it happens once per family on show.
SDL_Texture *fonts_sample(TTF_Font *row_font, const char *value, const char *name, int *w, int *h)
{
    const char *path = fonts_value_path(value);
    int face = atoi(value);
    font_clock++;
    int oldest = 0;
    for (int i = 0; i < FONT_CACHE_SIZE; i++) {
        if (font_cache[i].path != NULL && font_cache[i].face == face && strcmp(font_cache[i].path, path) == 0) {
            font_cache[i].used = font_clock;
            *w = font_cache[i].w;
            *h = font_cache[i].h;
            return font_cache[i].texture;
        }
        if (font_cache[i].used < font_cache[oldest].used)
            oldest = i;
    }
    if (font_cache[oldest].texture != NULL)
        SDL_DestroyTexture(font_cache[oldest].texture);
    alloc_free(font_cache[oldest].path);
    memset(&font_cache[oldest], 0, sizeof(font_cache[oldest]));
    // A sample whose place cannot be had (out of memory) is not drawn: an entry with no path would
    // never be found again, and the face would be opened and drawn each frame
    test_fail("sample", true);
    font_cache[oldest].path = alloc_strdup(path);
    test_fail("sample", false);
    if (font_cache[oldest].path == NULL)
        return NULL;
    font_cache[oldest].face = face;
    font_cache[oldest].used = font_clock;
    TTF_Font *font = TTF_OpenFontIndex(path, TTF_FontHeight(row_font) * FONT_SAMPLE_PERCENT / 100, face);
    SDL_Surface *surface = NULL;
    if (font != NULL) {
        SDL_Color white = { 0xFF, 0xFF, 0xFF, 0xFF };
        surface = TTF_RenderUTF8_Blended(font, name, white);
        TTF_CloseFont(font);
    }
    if (surface != NULL) {
        font_cache[oldest].texture = SDL_CreateTextureFromSurface(renderer, surface);
        font_cache[oldest].w = surface->w;
        font_cache[oldest].h = surface->h;
        SDL_FreeSurface(surface);
    }
    if (font_cache[oldest].texture == NULL) {
        log_debug("Settings: the font picker could not draw %s in its own face", name);
        return NULL;
    }
    log_debug("Settings: the font picker drew %s in its own face", name);
    *w = font_cache[oldest].w;
    *h = font_cache[oldest].h;
    return font_cache[oldest].texture;
}

// A function to empty the family samples' cache
void fonts_clear_samples(void)
{
    for (int i = 0; i < FONT_CACHE_SIZE; i++) {
        if (font_cache[i].texture != NULL)
            SDL_DestroyTexture(font_cache[i].texture);
        alloc_free(font_cache[i].path);
    }
    memset(font_cache, 0, sizeof(font_cache));
}

// A function to find the bundled fonts' folder (caller frees): the one the bundled title font is
// in, beside the executable or where the packages put it; NULL when it is not there
static char *bundled_fonts_folder(void)
{
    char *font = find_default_font(FILENAME_DEFAULT_FONT);
    if (font == NULL)
        return NULL;
    size_t length = strlen(font);
    while (length > 0 && !fileio_is_separator(font[length - 1]))
        length--;
    font[length > 0 ? length - 1 : 0] = '\0';
    return font;
}

// A function to write a face as a font row's value: "<face>|<path>"
static void font_value(const char *path, int face, char *out, size_t size)
{
    snprintf(out, size, "%d|%s", face, path);
}

// A function to write the value of the font a setting uses now: the file and face the titles or
// the clock opened (the configured one, or the bundled font they fell back to), as the family that
// holds that face writes it; a face no family holds (a file outside the folders listed) as itself.
// A file load_font() opened by a relative path (the Windows config's .\assets\fonts\..., from the
// working folder, or joined to the executable's folder) is found by the full path it opened, which
// is how the list names the bundled fonts. The clock's row is grayed while the clock is off, and a
// clock that is on has opened its font (init_clock() failing quits through log_fatal), so clk is there.
void fonts_value_in_use(SettingId id, char *out, size_t size)
{
    const TextInfo *info = id == SET_ID_TITLE_FONT ? &title_info : &clk->text_info;
    const char *path = info->font_path != NULL ? info->font_path : "";
    int family = fontlist_find(fonts, path, info->font_face);
    char full[FONT_VALUE_MAX];
    if (family < 0 && fileio_full_path(path, full, sizeof(full)))
        family = fontlist_find(fonts, full, info->font_face);
    if (family >= 0)
        font_value(fontlist_path(fonts, family), fontlist_face(fonts, family), out, size);
    else
        font_value(path, info->font_face, out, size);
}

// A function to fill the font picker from the font list, one row a family; false when out of memory
bool fonts_fill(ListPick *to)
{
    char value[FONT_VALUE_MAX];
    for (int i = 0; i < fontlist_count(fonts); i++) {
        font_value(fontlist_path(fonts, i), fontlist_face(fonts, i), value, sizeof(value));
        if (!listpick_add(to, fontlist_family(fonts, i), value, true, NULL))
            return false;
    }
    return true;
}

// A function to read one file's faces into the font list, on the main thread (SDL_ttf's one
// FreeType library is not safe on two): every face with a family name, and a glyph for each of
// "Aa0" (symbol and emoji fonts have none), is added. A face that could not be added for want of
// memory marks the list as failed.
static void read_font_file(const char *path, bool bundled)
{
    TTF_Font *font = TTF_OpenFontIndex(path, 12, 0);
    if (font == NULL) {
        font_files_skipped++;
        return;
    }
    long faces = TTF_FontFaces(font);
    TTF_CloseFont(font);
    for (long i = 0; i < faces && i < 64; i++) {
        TTF_Font *face = TTF_OpenFontIndex(path, 12, i);
        if (face == NULL)
            continue;
        const char *family = TTF_FontFaceFamilyName(face);
        const char *style = TTF_FontFaceStyleName(face);
        if (family != NULL && TTF_GlyphIsProvided(face, 'A') && TTF_GlyphIsProvided(face, 'a') &&
            TTF_GlyphIsProvided(face, '0')) {
            test_fail("faces", true);
            bool added = fontlist_add(fonts, path, (int) i, family, style != NULL ? style : "", bundled);
            test_fail("faces", false);
            if (!added)
                font_read_failed = true;
        }
        TTF_CloseFont(face);
    }
}

// A function to start listing the font files on a thread, unless a listing is under way. One that
// cannot start leaves no list behind, and says so.
bool fonts_start(void)
{
    if (font_scan != NULL)
        return true;
    char *folder = bundled_fonts_folder();
    test_fail("fontlist", true);
    fonts = fontlist_create();
    test_fail("fontlist", false);
    test_fail("fontscan", true);
    font_scan = fonts != NULL ? fontscan_start(folder) : NULL;
    test_fail("fontscan", false);
    free(folder);
    if (font_scan == NULL) {
        fontlist_free(fonts);
        fonts = NULL;
        log_error("Settings: the fonts cannot be listed: out of memory, or no thread");
        return false;
    }
    font_scan_start = SDL_GetTicks();
    font_files_read = 0;
    font_files_skipped = 0;
    font_read_failed = false;
    log_debug("Fonts: listing the font files");
    return true;
}

// A function to let go of a font list that ran out of memory, and say where: the next opening lists
// the files anew, as a short list kept for the session would misstate the fonts
static void let_go(const char *where)
{
    fontscan_free(font_scan);
    font_scan = NULL;
    fontlist_free(fonts);
    fonts = NULL;
    log_error("Fonts: out of memory while %s, so the list was let go", where);
}

// A function to read more of the font files each frame once the thread has listed them (and been
// waited for, by fontscan_done()), then put the families in order and keep them for the session. A
// list a file or a face could not be added to is let go. True on the frame the reading ended,
// either way.
bool fonts_tick(void)
{
    if (font_scan == NULL || !fontscan_done(font_scan))
        return false;
    if (fontscan_failed(font_scan)) {
        let_go("listing the font files");
        return true;
    }
    int total = fontscan_count(font_scan);
    for (int n = 0; n < FONT_FILES_PER_FRAME && font_files_read < total; n++, font_files_read++)
        read_font_file(fontscan_file(font_scan, font_files_read), fontscan_bundled(font_scan, font_files_read));
    if (font_files_read < total)
        return false;
    if (font_read_failed) {
        let_go("reading the faces");
        return true;
    }
    fontscan_free(font_scan);
    font_scan = NULL;
    fontlist_finish(fonts);
    ready = true;
    log_debug("Fonts: found %i families in %i files, skipped %i (%u ms)", fontlist_count(fonts), total,
        font_files_skipped, SDL_GetTicks() - font_scan_start);
    return true;
}

// A function to let go of the font list and its listing at quit: they are kept while the launcher
// runs. A listing under way is waited for. The log says what went, as LeakSanitizer cannot see
// memory a static still points to.
void fonts_quit(void)
{
    if (font_scan != NULL) {
        fontscan_free(font_scan);
        font_scan = NULL;
        log_debug("Fonts: waited for the font scan at quit");
    }
    if (fonts != NULL) {
        fontlist_free(fonts);
        fonts = NULL;
        log_debug("Fonts: let go of the font list at quit");
    }
    ready = false;
}
