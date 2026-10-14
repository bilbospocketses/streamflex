// Fallback fonts (spec section 4). Each font StreamFlex opens for titles or the settings screen is a
// set: the font itself and the fallbacks attached to it at its size, all owned by the set. Before a
// text is first measured, fontset_prepare() gives each character the font lacks its provider, the
// first usable candidate that has it, attached in candidate order; SDL_ttf then measures, wraps and
// draws through the fallbacks. A text the font covers costs one lookup per character and opens
// nothing. Main thread only, as SDL_ttf's one FreeType library is.
#ifndef FONTSET_H
#define FONTSET_H

#include <stdbool.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#define FILENAME_FALLBACK_FONT "DejaVuSans.ttf"   // The bundled font every candidate list ends with
#define FONTSET_PROBE_SIZE 12.0f                  // The size a font's own coverage is learned at, with no fallbacks

typedef struct FontSet FontSet;

// A font a character the primary lacks may come from; fontcand.c lists them in the order they are tried
typedef struct FontCandidate {
    char *path;            // The font file
    int face;              // TTF_PROP_FONT_CREATE_FACE_NUMBER: fontconfig's (instance << 16) | face
    const char *family;    // Windows: the family it was found as; NULL elsewhere
    const void *charset;   // Linux: fontconfig's character set for it, which covers() reads
    bool (*covers)(const struct FontCandidate *candidate, Uint32 ch);   // NULL: a probe learns its coverage
} FontCandidate;

FontSet *fontset_open(const char *path, float size, int face, const char *role);   // role: "titles", "settings rows", "settings headers", "settings notes"
void fontset_prepare(FontSet *set, const char *text);   // Attach a provider for each character the font lacks, before the first measure
TTF_Font *fontset_font(FontSet *set);                    // The primary, fallbacks attached; NULL for no set
bool fontset_resize(FontSet *set, float size);          // Fallbacks first, then the primary; a set that fails is closed by its caller
void fontset_close(FontSet *set);                       // The font first, then the fallbacks (#549)
void fontset_quit(void);                                // Before TTF_Quit: the probes, the candidate list

const FontCandidate *fontset_candidates(int *count);   // fontcand.c: built once, logged once
void fontset_candidates_free(void);                    // fontcand.c: the list, and fontconfig's sort on Linux

// The close every font of a set goes through: TTF_CloseFont, unless tests/test_fontset.c records the order
extern void (SDLCALL *fontset_close_font)(TTF_Font *font);

#endif
