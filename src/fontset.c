// Fallback fonts (spec section 4): font sets, their fallbacks and the order they are kept in. See
// fontset.h. What SDL3_ttf 3.2.2 does that this file relies on is read in its source: adding or
// removing a fallback flushes nothing (TTF_AddFallbackFont, TTF_RemoveFallbackFont), TTF_FontHasGlyph
// follows a font's fallbacks (get_char_index_fallback), and TTF_SetFontSize resizes only its font.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "launcher.h"
#include "fontset.h"
#include "fontrules.h"
#include "fileio.h"
#include "debug.h"

#define PRIMARY_PROBES 4   // Primaries whose own coverage is kept: the titles' file and the settings', with room for a reload

struct FontSet {
    TTF_Font *font;        // The primary: what the set's users measure and draw with
    char *path;            // Its file and face, for its probe
    int face;
    float size;
    const char *role;      // For the log: "titles", "settings rows", "settings headers" or "settings notes"
    TTF_Font **fallbacks;  // Attached to the primary in candidate order...
    int *ranks;            // ...with each one's place in the candidate list
    int count;
    int capacity;
    Uint32 *seen;          // The code points looked at already: open addressing, 0 a free slot
    int seen_count;
    int seen_capacity;     // A power of two; 0 before the first
};

// What the session has learned of a candidate, beside the list fontcand.c built
typedef struct {
    TTF_Font *probe;       // At FONTSET_PROBE_SIZE with no fallbacks: the coverage of a candidate with no charset
    bool probed;           // Its probe was tried
    bool vetted;           // It draws the first character it was picked for that is no space
    bool rejected;         // Skipped for the session
} CandidateState;

void (SDLCALL *fontset_close_font)(TTF_Font *font) = NULL;

static const FontCandidate *candidates = NULL;   // NULL until a character first needs one
static CandidateState *states = NULL;
static int candidate_count = 0;
static struct {
    char *path;
    int face;
    TTF_Font *probe;       // NULL when it would not open: the primary is then taken to have everything
    Uint32 used;
} primaries[PRIMARY_PROBES];
static Uint32 primary_clock = 0;
static Uint32 *missing = NULL;                   // Code points no font draws, each logged once a session
static int missing_count = 0;
static int missing_capacity = 0;

// A function to open a face of a font file at a size, the face being a property in SDL3_ttf.
// Something there that is not a regular file (a pipe, say, whose read would wait for good) is never
// opened, as open_font_file() refuses it, and SDL_GetError() says why.
static TTF_Font *open_face(const char *path, float size, int face)
{
    if (fileio_not_a_file(path)) {
        SDL_SetError("%s", fileio_last_error());
        return NULL;
    }
    SDL_PropertiesID props = SDL_CreateProperties();
    if (props == 0)
        return NULL;
    SDL_SetStringProperty(props, TTF_PROP_FONT_CREATE_FILENAME_STRING, path);
    SDL_SetFloatProperty(props, TTF_PROP_FONT_CREATE_SIZE_FLOAT, size);
    SDL_SetNumberProperty(props, TTF_PROP_FONT_CREATE_FACE_NUMBER, face);
    TTF_Font *font = TTF_OpenFontWithProperties(props);
    SDL_DestroyProperties(props);
    return font;
}

// A function to close one of a set's fonts
static void close_font(TTF_Font *font)
{
    if (font == NULL)
        return;
    if (fontset_close_font != NULL)
        fontset_close_font(font);
    else
        TTF_CloseFont(font);
}

// A function to put a code point in a table of seen ones that has room for it
static void seen_insert(Uint32 *table, int capacity, Uint32 ch)
{
    int i = (int) ((ch * 2654435761u) & (Uint32) (capacity - 1));
    while (table[i] != 0)
        i = (i + 1) & (capacity - 1);
    table[i] = ch;
}

// A function to tell whether a set has looked at a code point already, and remember it if not, so the
// settings screen, which measures its rows every frame, pays one lookup per character. A table that
// cannot grow (out of memory) only costs the lookup again.
static bool seen_before(FontSet *set, Uint32 ch)
{
    if (set->seen_capacity > 0) {
        int i = (int) ((ch * 2654435761u) & (Uint32) (set->seen_capacity - 1));
        for (; set->seen[i] != 0; i = (i + 1) & (set->seen_capacity - 1)) {
            if (set->seen[i] == ch)
                return true;
        }
    }
    if (2 * (set->seen_count + 1) > set->seen_capacity) {
        int capacity = set->seen_capacity > 0 ? 2 * set->seen_capacity : 64;
        Uint32 *table = calloc((size_t) capacity, sizeof(Uint32));
        if (table == NULL)
            return false;
        for (int i = 0; i < set->seen_capacity; i++) {
            if (set->seen[i] != 0)
                seen_insert(table, capacity, set->seen[i]);
        }
        free(set->seen);
        set->seen = table;
        set->seen_capacity = capacity;
    }
    seen_insert(set->seen, set->seen_capacity, ch);
    set->seen_count++;
    return false;
}

// A function to get a primary's own font at FONTSET_PROBE_SIZE with no fallbacks: once a fallback is
// attached, TTF_FontHasGlyph() on the primary answers through it, so only this can say what the
// primary itself lacks. Kept for the session, a few files at most; never attached to anything.
static TTF_Font *primary_probe(const FontSet *set)
{
    int oldest = 0;
    primary_clock++;
    for (int i = 0; i < PRIMARY_PROBES; i++) {
        if (primaries[i].path != NULL && primaries[i].face == set->face && strcmp(primaries[i].path, set->path) == 0) {
            primaries[i].used = primary_clock;
            return primaries[i].probe;
        }
        if (primaries[i].used < primaries[oldest].used)
            oldest = i;
    }
    if (primaries[oldest].probe != NULL)
        TTF_CloseFont(primaries[oldest].probe);
    free(primaries[oldest].path);
    primaries[oldest].path = strdup(set->path);
    primaries[oldest].face = set->face;
    primaries[oldest].probe = primaries[oldest].path != NULL ? open_face(set->path, FONTSET_PROBE_SIZE, set->face) : NULL;
    primaries[oldest].used = primary_clock;
    return primaries[oldest].probe;
}

// A function to tell whether a set's primary has a glyph of its own for a code point
static bool primary_has(FontSet *set, Uint32 ch)
{
    if (set->count == 0)
        return TTF_FontHasGlyph(set->font, ch);   // No fallback yet: it answers for itself
    TTF_Font *probe = primary_probe(set);
    return probe == NULL || TTF_FontHasGlyph(probe, ch);
}

// A function to get the candidates, built the first time a character needs one, with room for what
// this session learns of each; returns how many there are (0 for none, or no memory for them)
static int candidate_list(void)
{
    if (candidates == NULL) {
        int found = 0;
        const FontCandidate *list = fontset_candidates(&found);
        CandidateState *learned = found > 0 ? calloc((size_t) found, sizeof(CandidateState)) : NULL;
        if (list != NULL && learned != NULL) {
            candidates = list;
            states = learned;
            candidate_count = found;
        }
        else
            free(learned);
    }
    return candidate_count;
}

// A function to skip a candidate for the session, saying why, once
static void reject(int rank, const char *why)
{
    states[rank].rejected = true;
    log_debug("Fonts: skipped %s: %s", candidates[rank].path, why);
    if (states[rank].probe != NULL) {
        TTF_CloseFont(states[rank].probe);
        states[rank].probe = NULL;
    }
}

// A function to tell whether a candidate has a glyph for a code point: from fontconfig's character set,
// or, for a candidate with none (Windows' and the bundled font), from its probe, opened once, when the
// walk first reaches it
static bool covers(int rank, Uint32 ch)
{
    if (candidates[rank].covers != NULL)
        return candidates[rank].covers(&candidates[rank], ch);
    CandidateState *state = &states[rank];
    if (!state->probed) {
        state->probed = true;
        state->probe = open_face(candidates[rank].path, FONTSET_PROBE_SIZE, candidates[rank].face);
        if (state->probe == NULL) {
            reject(rank, SDL_GetError());
            return false;
        }
    }
    return state->probe != NULL && TTF_FontHasGlyph(state->probe, ch);
}

// A function to tell whether a font draws any ink for a code point: a pixel of its blended glyph
// that is not transparent
static bool has_ink(TTF_Font *font, Uint32 ch)
{
    SDL_Color white = { 0xFF, 0xFF, 0xFF, 0xFF };
    SDL_Surface *glyph = TTF_RenderGlyph_Blended(font, ch, white);
    bool ink = false;
    for (int y = 0; glyph != NULL && y < glyph->h && !ink; y++) {
        for (int x = 0; x < glyph->w && !ink; x++) {
            Uint8 r, g, b, a;
            ink = SDL_ReadSurfacePixel(glyph, x, y, &r, &g, &b, &a) && a > 0;
        }
    }
    SDL_DestroySurface(glyph);
    return ink;
}

// A function to check, the first time a candidate is picked, that it can draw the character it was
// picked for. One that cannot be scaled (Noto Color Emoji, a CBDT bitmap font drawn 128 px tall at any
// size) or that draws no ink for it (Fedora's Noto COLRv1 emoji, at SDL_ttf 3.2.2) is skipped for the
// session. A space draws no ink by design, so it is not asked for any, and proves nothing: a candidate
// picked for one is asked again the next time it is picked. False when it was skipped.
static bool vetted(int rank, Uint32 ch)
{
    CandidateState *state = &states[rank];
    if (state->vetted)
        return true;
    bool own = state->probe == NULL;   // A candidate with a charset has no probe: one is opened for this
    TTF_Font *font = own ? open_face(candidates[rank].path, FONTSET_PROBE_SIZE, candidates[rank].face) : state->probe;
    char why[256];
    bool ok = false;
    if (font == NULL)
        snprintf(why, sizeof(why), "%s", SDL_GetError());
    else if (!TTF_FontIsScalable(font))
        snprintf(why, sizeof(why), "a bitmap font cannot be scaled to the text");
    else if (!fontrules_blank(ch) && !has_ink(font, ch))
        snprintf(why, sizeof(why), "it draws nothing for U+%04X", (unsigned) ch);
    else
        ok = true;
    if (own && font != NULL)
        TTF_CloseFont(font);
    if (!ok) {
        reject(rank, why);
        return false;
    }
    state->vetted = !fontrules_blank(ch);
    return true;
}

// A function to make room for one more fallback; false when out of memory
static bool grow(FontSet *set)
{
    if (set->count < set->capacity)
        return true;
    int capacity = set->capacity > 0 ? 2 * set->capacity : 4;
    TTF_Font **fallbacks = realloc(set->fallbacks, (size_t) capacity * sizeof(TTF_Font *));
    if (fallbacks == NULL)
        return false;
    set->fallbacks = fallbacks;
    int *ranks = realloc(set->ranks, (size_t) capacity * sizeof(int));
    if (ranks == NULL)
        return false;
    set->ranks = ranks;
    set->capacity = capacity;
    return true;
}

// A function to attach a candidate to a set as a fallback, opened at the set's size and kept in
// candidate order: one ranking after every attached one is appended, one ranking earlier rebuilds the
// chain, as SDL_ttf takes the first fallback that has a glyph. Without this, a star that attached
// Segoe UI Symbol first would make a later emoji draw from it, in monochrome. No fallback is shared
// between sets, or made with TTF_CopyFont, which copies no fallbacks and shares one file stream.
// False when it could not be opened (it is then skipped for the session).
static bool attach(FontSet *set, int rank, Uint32 ch)
{
    for (int i = 0; i < set->count; i++) {
        if (set->ranks[i] == rank)
            return true;   // Attached already, for another character
    }
    if (!grow(set))
        return false;
    TTF_Font *fallback = open_face(candidates[rank].path, set->size, candidates[rank].face);
    if (fallback == NULL) {
        reject(rank, SDL_GetError());
        return false;
    }
    int at = fontrules_insert_at(set->ranks, set->count, rank);
    memmove(&set->fallbacks[at + 1], &set->fallbacks[at], (size_t) (set->count - at) * sizeof(TTF_Font *));
    memmove(&set->ranks[at + 1], &set->ranks[at], (size_t) (set->count - at) * sizeof(int));
    set->fallbacks[at] = fallback;
    set->ranks[at] = rank;
    set->count++;
    if (at == set->count - 1)
        TTF_AddFallbackFont(set->font, fallback);
    else {
        TTF_ClearFallbackFonts(set->font);
        for (int i = 0; i < set->count; i++)
            TTF_AddFallbackFont(set->font, set->fallbacks[i]);
    }
    char face[48];
    fontrules_face_text(candidates[rank].face, face, sizeof(face));
    log_debug("Fonts: the %i pt %s fall back to %s (%s), first for U+%04X", (int) (set->size + 0.5f), set->role,
        candidates[rank].path, face, (unsigned) ch);
    return true;
}

// A function to say, once a session for each code point, that no font draws it. Not a debug line:
// the user can install a font that has it.
static void report_missing(Uint32 ch)
{
    for (int i = 0; i < missing_count; i++) {
        if (missing[i] == ch)
            return;
    }
    if (missing_count == missing_capacity) {
        int capacity = missing_capacity > 0 ? 2 * missing_capacity : 16;
        Uint32 *grown = realloc(missing, (size_t) capacity * sizeof(Uint32));
        if (grown != NULL) {
            missing = grown;
            missing_capacity = capacity;
        }
    }
    if (missing_count < missing_capacity)
        missing[missing_count++] = ch;
    log_error("Fonts: no installed font draws U+%04X; it shows as a box", (unsigned) ch);
}

// A function to give a character the primary lacks its provider: the first candidate, in order, that
// is not skipped, has it, and draws it. A character none has draws as the primary's box.
static void find_provider(FontSet *set, Uint32 ch)
{
    int count = candidate_list();
    for (int rank = 0; rank < count; rank++) {
        if (!states[rank].rejected && covers(rank, ch) && vetted(rank, ch) && attach(set, rank, ch))
            return;
    }
    report_missing(ch);
}

// A function to read the code point a UTF-8 text starts with, and step past it: U+FFFD, one byte on,
// where the bytes are no UTF-8 (overlong, a surrogate, past U+10FFFF), and 0, staying put, at its end.
// Not SDL_StepUTF8(): through SDL 3.4.18 it drops a bit of a four-byte sequence's second byte, so it
// reads VS17 (U+E0100) as U+C0100 and U+20000 as U+FFFD (fixed upstream on 2026-10-08, 57f22e6e43),
// while SDL_ttf shapes the text with HarfBuzz, which reads them right.
static Uint32 next_code_point(const char **text)
{
    static const Uint32 least[5] = { 0, 0, 0x80, 0x800, 0x10000 };   // The smallest code point each length may hold
    const Uint8 *bytes = (const Uint8 *) *text;
    if (bytes[0] == 0)
        return 0;
    if (bytes[0] < 0x80) {
        *text += 1;
        return bytes[0];
    }
    int length = (bytes[0] & 0xE0) == 0xC0 ? 2 : (bytes[0] & 0xF0) == 0xE0 ? 3 : (bytes[0] & 0xF8) == 0xF0 ? 4 : 0;
    Uint32 ch = bytes[0] & (0xFFu >> (length + 1));   // The lead byte's bits: 5, 4 or 3
    for (int i = 1; i < length; i++) {
        if ((bytes[i] & 0xC0) != 0x80)
            length = 0;   // Not a continuation byte (the text's end is not one either): no code point
        else
            ch = (ch << 6) | (bytes[i] & 0x3Fu);
    }
    if (length == 0 || ch < least[length] || ch > 0x10FFFF || (ch >= 0xD800 && ch <= 0xDFFF)) {
        *text += 1;
        return 0xFFFD;
    }
    *text += length;
    return ch;
}

// A function to open a font as a set, with no fallbacks yet; NULL, with SDL's reason, when it does not open
FontSet *fontset_open(const char *path, float size, int face, const char *role)
{
    FontSet *set = calloc(1, sizeof(FontSet));
    char *copy = strdup(path);
    TTF_Font *font = set != NULL && copy != NULL ? open_face(path, size, face) : NULL;
    if (font == NULL) {
        if (set == NULL || copy == NULL)
            SDL_OutOfMemory();
        free(copy);
        free(set);
        return NULL;
    }
    set->font = font;
    set->path = copy;
    set->face = face;
    set->size = size;
    set->role = role;
    return set;
}

// A function to attach, before a text is first measured, a provider for each character its font
// lacks. A set remembers what it has looked at, so a text it has seen costs a lookup per character.
void fontset_prepare(FontSet *set, const char *text)
{
    if (set == NULL || text == NULL)
        return;
    Uint32 ch;
    while ((ch = next_code_point(&text)) != 0) {
        if (fontrules_skip(ch) || seen_before(set, ch) || primary_has(set, ch))
            continue;
        find_provider(set, ch);
    }
}

// A function to get a set's font, its fallbacks attached
TTF_Font *fontset_font(FontSet *set)
{
    return set != NULL ? set->font : NULL;
}

// A function to resize a set: its fallbacks first, as TTF_SetFontSize() resizes only the font it is
// given, then its font, whose resize flushes its cached glyph positions. Nothing measures between the
// calls. False, with SDL's reason, when one fails; its caller then closes the set.
bool fontset_resize(FontSet *set, float size)
{
    for (int i = 0; i < set->count; i++) {
        if (!TTF_SetFontSize(set->fallbacks[i], size))
            return false;
    }
    if (!TTF_SetFontSize(set->font, size))
        return false;
    set->size = size;
    return true;
}

// A function to close a set. The font first: its close flushes its cache of glyph positions, which
// point into its fallbacks' glyphs, and clears its fallbacks. At SDL_ttf 3.2.2 removing a fallback
// flushes nothing, so a fallback closed while its font lives leaves that cache pointing at freed
// memory for the next measure of the same text (upstream #549, fixed only after 3.2.2).
void fontset_close(FontSet *set)
{
    if (set == NULL)
        return;
    close_font(set->font);
    for (int i = 0; i < set->count; i++)
        close_font(set->fallbacks[i]);
    free(set->fallbacks);
    free(set->ranks);
    free(set->seen);
    free(set->path);
    free(set);
}

// A function to let go, before TTF_Quit and once every set is closed, of what the session kept: the
// probes, what was learned of each candidate, and the list itself, whose free calls FcFini() on Linux.
// The log says what went, as LeakSanitizer cannot see what a static still points to.
void fontset_quit(void)
{
    int probes = 0;
    for (int i = 0; i < PRIMARY_PROBES; i++) {
        if (primaries[i].probe != NULL) {
            TTF_CloseFont(primaries[i].probe);
            probes++;
        }
        free(primaries[i].path);
    }
    memset(primaries, 0, sizeof(primaries));
    primary_clock = 0;
    for (int i = 0; i < candidate_count; i++) {
        if (states[i].probe != NULL) {
            TTF_CloseFont(states[i].probe);
            probes++;
        }
    }
    if (candidates != NULL)
        log_debug("Fonts: let go of %i fallback candidates and %i probe fonts at quit", candidate_count, probes);
    free(states);
    states = NULL;
    candidates = NULL;
    candidate_count = 0;
    free(missing);
    missing = NULL;
    missing_count = 0;
    missing_capacity = 0;
    fontset_candidates_free();
}
