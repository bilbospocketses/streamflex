#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "launcher.h"
#include <launcher_config.h>
#include "image.h"
#include "chroma.h"
#include "colorpick.h"
#include "fileio.h"
#include "util.h"
#include "debug.h"
#include <ini.h>
// Vendored code is not held to our warning level (see src/external/README.md)
#ifdef _MSC_VER
#pragma warning(push, 0)
// Level 0 does not reach the back end's C4702, which needs its own disable
#pragma warning(disable: 4702)
#endif
#define NANOSVG_IMPLEMENTATION
#include <nanosvg.h>
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvgrast.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

extern Config config;
extern State state;
extern SDL_Renderer *renderer;
extern TextInfo title_info;
extern Effective eff;
NSVGrasterizer *rasterizer = NULL;

// What render_buttons' icons cost in its current render, in performance-counter counts: their reads,
// their decodes (with the chroma pass, which a decode worker would run too) and their textures. Main
// thread only: render_buttons zeroes them, and the slideshow's thread never touches them.
Uint64 timing_read = 0, timing_decode = 0, timing_texture = 0;

// A function to turn performance-counter counts into whole milliseconds, rounded
unsigned timing_ms(Uint64 counts)
{
    Uint64 frequency = SDL_GetPerformanceFrequency();
    return (unsigned) ((counts * 1000 + frequency / 2) / frequency);
}

// A function to load an image file as IMG_Load does, in two timed parts: the read (SDL_LoadFile)
// and the decode (IMG_LoadTyped_IO from memory, with the file's extension as the type, as IMG_Load
// passes it; IMG_Load_IO would pass none), adding each part's counts to *read and *decode. NULL,
// with SDL's reason in SDL_GetError(), when either fails. It touches nothing else, so the
// slideshow's thread can use it.
static SDL_Surface *load_surface_timed(const char *path, Uint64 *read, Uint64 *decode)
{
    Uint64 start = SDL_GetPerformanceCounter();
    size_t size = 0;
    void *data = SDL_LoadFile(path, &size);
    Uint64 loaded = SDL_GetPerformanceCounter();
    *read += loaded - start;
    if (data == NULL)
        return NULL;
    const char *dot = SDL_strrchr(path, '.');
    SDL_Surface *surface = IMG_LoadTyped_IO(SDL_IOFromConstMem(data, size), true, dot != NULL ? dot + 1 : NULL);
    SDL_free(data);
    *decode += SDL_GetPerformanceCounter() - loaded;
    return surface;
}

// A function to initalize SVG rasterization
int init_svg()
{
    rasterizer = nsvgCreateRasterizer();
    if (rasterizer == NULL) {
        log_fatal("Could not initialize SVG rasterizer.");
        return 1;
    }
    return 0;
}

// A function to quit the SVG subsystem
void quit_svg()
{
    nsvgDeleteRasterizer(rasterizer);
}

// Three 1 x 1 images, one per format SDL_image decodes for StreamFlex: a mid-gray PNG (RGB), a
// baseline grayscale JPEG with the standard Huffman tables, and a lossless WebP. Made with
// ImageMagick 7.1.2 and decoded back by Pillow before they were pasted here.
static const unsigned char PRIME_PNG[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53,
    0xDE, 0x00, 0x00, 0x00, 0x0C, 0x49, 0x44, 0x41, 0x54, 0x08, 0xD7, 0x63, 0x68, 0x68, 0x68, 0x00,
    0x00, 0x03, 0x04, 0x01, 0x81, 0x9A, 0xF0, 0xCD, 0x4F, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E,
    0x44, 0xAE, 0x42, 0x60, 0x82
};
static const unsigned char PRIME_JPEG[] = {
    0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01,
    0x00, 0x01, 0x00, 0x00, 0xFF, 0xDB, 0x00, 0x43, 0x00, 0x10, 0x0B, 0x0C, 0x0E, 0x0C, 0x0A, 0x10,
    0x0E, 0x0D, 0x0E, 0x12, 0x11, 0x10, 0x13, 0x18, 0x28, 0x1A, 0x18, 0x16, 0x16, 0x18, 0x31, 0x23,
    0x25, 0x1D, 0x28, 0x3A, 0x33, 0x3D, 0x3C, 0x39, 0x33, 0x38, 0x37, 0x40, 0x48, 0x5C, 0x4E, 0x40,
    0x44, 0x57, 0x45, 0x37, 0x38, 0x50, 0x6D, 0x51, 0x57, 0x5F, 0x62, 0x67, 0x68, 0x67, 0x3E, 0x4D,
    0x71, 0x79, 0x70, 0x64, 0x78, 0x5C, 0x65, 0x67, 0x63, 0xFF, 0xC0, 0x00, 0x0B, 0x08, 0x00, 0x01,
    0x00, 0x01, 0x01, 0x01, 0x11, 0x00, 0xFF, 0xC4, 0x00, 0x1F, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04,
    0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0xFF, 0xC4, 0x00, 0xB5, 0x10, 0x00, 0x02, 0x01, 0x03,
    0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7D, 0x01, 0x02, 0x03, 0x00,
    0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32,
    0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0, 0x24, 0x33, 0x62, 0x72,
    0x82, 0x09, 0x0A, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x34, 0x35,
    0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55,
    0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75,
    0x76, 0x77, 0x78, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94,
    0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2,
    0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9,
    0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6,
    0xE7, 0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFF, 0xDA,
    0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x3F, 0x00, 0x2B, 0xFF, 0xD9
};
static const unsigned char PRIME_WEBP[] = {
    0x52, 0x49, 0x46, 0x46, 0x1E, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50, 0x56, 0x50, 0x38, 0x4C,
    0x11, 0x00, 0x00, 0x00, 0x2F, 0x00, 0x00, 0x00, 0x00, 0x07, 0x50, 0xC0, 0x02, 0x16, 0xB0, 0xFF,
    0x81, 0x88, 0xE8, 0x7F, 0x00, 0x00
};

// A function to set SDL_image's PNG, JPEG and WebP loaders up on the main thread, before any thread
// that decodes starts. SDL_image sets each format up on its first load, with nothing to stop two
// threads doing it at once, and the slideshow's loader and the settings preview's decode load beside
// the main thread. Each format decodes its tiny image once; one that cannot says why.
void prime_image_formats(void)
{
    static const struct { const char *name; const char *type; const unsigned char *data; size_t size; } formats[] = {
        { "PNG", "png", PRIME_PNG, sizeof(PRIME_PNG) },
        { "JPEG", "jpg", PRIME_JPEG, sizeof(PRIME_JPEG) },
        { "WebP", "webp", PRIME_WEBP, sizeof(PRIME_WEBP) }
    };
    size_t primed = 0;
    for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); i++) {
        SDL_Surface *surface = IMG_LoadTyped_IO(SDL_IOFromConstMem(formats[i].data, formats[i].size), true, formats[i].type);
        if (surface == NULL)
            log_error("Could not prime SDL_image's %s loader\n%s", formats[i].name, SDL_GetError());
        else {
            SDL_DestroySurface(surface);
            primed++;
        }
    }
    if (primed == sizeof(formats) / sizeof(formats[0]))
        log_debug("Images: primed the PNG, JPEG and WebP loaders");
}

// A function to make a texture from a surface, filtered linearly when it is drawn scaled, as SDL2's
// SDL_HINT_RENDER_SCALE_QUALITY "1" asked for every texture. SDL3 has no such hint, and is linear by
// default, but each texture is set anyway, whatever the default becomes (spec section 1, item 8).
// NULL when the texture could not be made, with SDL's reason still in SDL_GetError()
SDL_Texture *surface_texture(SDL_Surface *surface)
{
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture != NULL)
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    return texture;
}

// A function to open a font file at a point size and face: SDL3_ttf takes the face as a property
// (TTF_OpenFontIndex is gone, spec section 1, item 5)
TTF_Font *open_font_face(const char *path, int size, int face)
{
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, TTF_PROP_FONT_CREATE_FILENAME_STRING, path);
    SDL_SetFloatProperty(props, TTF_PROP_FONT_CREATE_SIZE_FLOAT, (float) size);
    SDL_SetNumberProperty(props, TTF_PROP_FONT_CREATE_FACE_NUMBER, face);
    TTF_Font *font = TTF_OpenFontWithProperties(props);
    SDL_DestroyProperties(props);
    return font;
}

// The layout is in whole pixels and SDL3 draws with float rectangles. These convert at the draw call
// only, a cast of whole numbers, so nothing lands half a pixel off (spec section 1, item 3). A NULL
// rectangle is the whole texture, or the whole target, as in SDL.
void draw_texture(SDL_Renderer *r, SDL_Texture *texture, const SDL_Rect *src, const SDL_Rect *dst)
{
    SDL_FRect from, to;
    if (src != NULL)
        SDL_RectToFRect(src, &from);
    if (dst != NULL)
        SDL_RectToFRect(dst, &to);
    SDL_RenderTexture(r, texture, src != NULL ? &from : NULL, dst != NULL ? &to : NULL);
}

void draw_texture_rotated(SDL_Renderer *r, SDL_Texture *texture, const SDL_Rect *dst, double angle, SDL_FlipMode flip)
{
    SDL_FRect to;
    SDL_RectToFRect(dst, &to);
    SDL_RenderTextureRotated(r, texture, NULL, &to, angle, NULL, flip);
}

void fill_rect(SDL_Renderer *r, const SDL_Rect *rect)
{
    SDL_FRect area;
    if (rect != NULL)
        SDL_RectToFRect(rect, &area);
    SDL_RenderFillRect(r, rect != NULL ? &area : NULL);
}

void outline_rect(SDL_Renderer *r, const SDL_Rect *rect)
{
    SDL_FRect area;
    SDL_RectToFRect(rect, &area);
    SDL_RenderRect(r, &area);
}

void draw_line(SDL_Renderer *r, int x1, int y1, int x2, int y2)
{
    SDL_RenderLine(r, (float) x1, (float) y1, (float) x2, (float) y2);
}

#define MAX_TITLE_FONTS 16

// Title fonts by point size, for menus whose titles scale with their buttons
static struct {
    int size;
    TTF_Font *font;
} title_fonts[MAX_TITLE_FONTS];
static int title_font_count = 0;

// A function to get the title font at a point size, opening it the first time. Each size is its own
// font; the cache keeps MAX_TITLE_FONTS of them and closes the oldest to make room.
TTF_Font *title_font(int size)
{
    for (int i = 0; i < title_font_count; i++) {
        if (title_fonts[i].size == size)
            return title_fonts[i].font;
    }
    TTF_Font *font = open_font_face(title_info.font_path, size, title_info.font_face);
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: STREAMFLEX_TEST_FAIL_TITLE_SIZE names a size that fails
    const char *fail = getenv("STREAMFLEX_TEST_FAIL_TITLE_SIZE");
    if (fail != NULL && atoi(fail) == size && font != NULL) {
        TTF_CloseFont(font);
        font = NULL;
        SDL_SetError("the test hook failed it");
    }
#endif
    if (font == NULL) {
        log_error("Could not open the title font at %i pt\n%s", size, SDL_GetError());
        return NULL;
    }
    if (title_font_count == MAX_TITLE_FONTS) {
        TTF_CloseFont(title_fonts[0].font);
        memmove(&title_fonts[0], &title_fonts[1], (MAX_TITLE_FONTS - 1) * sizeof(title_fonts[0]));
        title_font_count--;
    }
    title_fonts[title_font_count].size = size;
    title_fonts[title_font_count].font = font;
    title_font_count++;
    return font;
}

// A function to close every cached title font
void title_fonts_free(void)
{
    for (int i = 0; i < title_font_count; i++)
        TTF_CloseFont(title_fonts[i].font);
    title_font_count = 0;
}

// A function to close the cached title fonts at sizes not in a list: when settings close, the
// sizes no menu uses any more
void title_fonts_keep(const int *sizes, int count)
{
    int kept = 0;
    for (int i = 0; i < title_font_count; i++) {
        bool used = false;
        for (int j = 0; j < count && !used; j++)
            used = title_fonts[i].size == sizes[j];
        if (used)
            title_fonts[kept++] = title_fonts[i];
        else {
            log_debug("Titles: closed the %i pt title font, which no menu uses now", title_fonts[i].size);
            TTF_CloseFont(title_fonts[i].font);
        }
    }
    title_font_count = kept;
}

// A function to load the next slideshow image that loads. It also runs on the slideshow thread, so
// it touches nothing but the slideshow: textures, the draw color and what is shown belong to the
// main thread. It returns NULL when no image in the folder loads, and sets slideshow->only_one when
// the only one that does is the image already on show; the main thread falls back from either. The
// read and the decode of each try are added to *read and *decode.
SDL_Surface *load_next_slideshow_background(Slideshow *slideshow, bool transition, Uint64 *read, Uint64 *decode)
{
    SDL_Surface *surface = NULL;
    int initial_index = slideshow->i;

    // Try each image once at most, starting after the one on show (i is -1 before the first)
    for (int attempts = 0; surface == NULL && attempts < slideshow->num_images; attempts++) {
        slideshow->i = (slideshow->i + 1) % slideshow->num_images;
        surface = load_surface_timed(slideshow->images[slideshow->order[slideshow->i]], read, decode);

        // If the loaded image has no alpha channel (e.g. JPEG), create one
        // so that we can have transparency for the background transition
        if (surface != NULL && surface->format == SDL_PIXELFORMAT_RGB24 && transition) {
            SDL_Surface *tmp = SDL_CreateSurface(surface->w, surface->h, SDL_PIXELFORMAT_ARGB8888);
            if (tmp != NULL) {
                Uint32 color = SDL_MapSurfaceRGBA(tmp, 0, 0, 0, 0xFF);
                SDL_FillSurfaceRect(tmp, NULL, color);
                SDL_BlitSurface(surface, NULL, tmp, NULL);
                SDL_DestroySurface(surface);
                surface = tmp;
            }
        }
    }
    slideshow->only_one = surface != NULL && slideshow->i == initial_index;
    return surface;
}

// A function to load a new slideshow background in a separate thread
int load_next_slideshow_background_async(void *data)
{
    Slideshow *slideshow = (Slideshow*) data;
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: STREAMFLEX_TEST_SLIDESHOW_HOLD holds the loader back
    // before its read, as a slow disk would, until the file it names exists, so a mode step or a quit
    // lands while the loader is still running and the check lets it go when it has looked. A check
    // that never lets it go is not waited on for more than a minute.
    const char *hold = getenv("STREAMFLEX_TEST_SLIDESHOW_HOLD");
    for (int waited = 0; hold != NULL && waited < 60000 && !fileio_present(hold); waited += 50)
        SDL_Delay(50);
#endif
    Uint64 read = 0, decode = 0;   // Timed only on the main thread's first load
    slideshow->transition_surface = load_next_slideshow_background(slideshow, true, &read, &decode);
    slideshow->transition_luminance = slideshow->transition_surface != NULL
                                      ? surface_luminance(slideshow->transition_surface) : -1.0;
    SDL_SetAtomicInt(&state.slideshow_background_rendering, 0);
    SDL_SetAtomicInt(&state.slideshow_background_ready, 1);
    return 0;
}

// A function to measure a decoded image's mean relative luminance, for the contrast warning: a copy
// converted to RGBA32 bytes (R at byte 0) is read at most 64 points across and down, by its own
// pitch. -1 when it cannot be converted. It touches only the surface and logs nothing, so the
// slideshow's thread can use it; each image is measured once, as it is decoded.
double surface_luminance(SDL_Surface *surface)
{
    SDL_Surface *rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
#ifdef STREAMFLEX_TEST_HOOKS
    // Only the headless harness builds this: STREAMFLEX_TEST_NO_LUMINANCE fails the conversion
    if (rgba != NULL && getenv("STREAMFLEX_TEST_NO_LUMINANCE") != NULL) {
        SDL_DestroySurface(rgba);
        rgba = NULL;
    }
#endif
    if (rgba == NULL)
        return -1.0;
    // A surface converted without flags is never RLE-encoded, so its pixels can be read unlocked
    double luminance = color_mean_luminance(rgba->pixels, rgba->w, rgba->h, rgba->pitch);
    SDL_DestroySurface(rgba);
    return luminance;
}

// A function to load a texture from a file, measuring its mean luminance when asked (NULL: not). A
// path to something that is not a regular file (a pipe, say, whose read would wait for good) is never
// opened; a path to nothing is, so SDL says why it failed.
SDL_Texture *load_texture_measured(const char *path, double *luminance)
{
    if (luminance != NULL)
        *luminance = -1.0;
    if (path == NULL)
        return NULL;
    if (fileio_not_a_file(path)) {
        log_error("Could not load image %s\n%s", path, fileio_last_error());
        return NULL;
    }
    Uint64 start = SDL_GetPerformanceCounter(), read = 0, decode = 0;
    SDL_Surface *surface = load_surface_timed(path, &read, &decode);
    if (surface == NULL) {
        log_error("Could not load image %s\n%s", path, SDL_GetError());
        return NULL;
    }
    Uint64 decoded = SDL_GetPerformanceCounter();
    if (luminance != NULL)
        *luminance = surface_luminance(surface);
    Uint64 lit = SDL_GetPerformanceCounter();
    SDL_Texture *texture = load_texture(surface);
    Uint64 end = SDL_GetPerformanceCounter();
    log_debug("Timing: background: image in %u ms (read %u, decode %u, luminance %u, texture %u)",
        timing_ms(end - start), timing_ms(read), timing_ms(decode), timing_ms(lit - decoded), timing_ms(end - lit));
    return texture;
}

// A function to load a texture from a file
SDL_Texture *load_texture_from_file(const char *path)
{
    return load_texture_measured(path, NULL);
}

// A function to load a texture from a    SDL surface
SDL_Texture *load_texture(SDL_Surface *surface)
{
    SDL_Texture *texture = NULL;
    if (surface == NULL)
        return NULL;

    //Convert surface to screen format
    texture = surface_texture(surface);
    if (texture == NULL)
        log_error("Could not create texture %s", SDL_GetError());
    SDL_DestroySurface(surface);
    return texture;
}

// A function to move an image's opaque pixels off the chroma key (RGBA bytes), always, since
// Transparent mode can be chosen in settings after the image is loaded: on Linux a compositor's
// shader makes every pixel of the key color see-through. Windows' Transparent mode is per-pixel
// alpha, with no key, and ChromaKeyColor is ignored there, so nothing moves. `what` names the image
// for the log.
static void keep_off_chroma_key(unsigned char *rgba, int width, int height, int pitch, const char *what)
{
#ifdef _WIN32
    UNUSED(rgba);
    UNUSED(width);
    UNUSED(height);
    UNUSED(pitch);
    UNUSED(what);
#else
    int moved = chroma_keep_off(rgba, width, height, pitch,
                    config.chroma_key_color.r, config.chroma_key_color.g, config.chroma_key_color.b);
    if (moved > 0)
        log_debug("%s: moved %i pixel(s) off the chroma key #%02X%02X%02X", what, moved,
            config.chroma_key_color.r, config.chroma_key_color.g, config.chroma_key_color.b);
#endif
}

// A function to keep a loaded image's opaque pixels off the chroma key. The pixels are read as
// RGBA bytes, so another format is converted first. Returns the surface to use: this one, or the
// converted copy, which replaces it.
static SDL_Surface *keep_surface_off_chroma_key(SDL_Surface *surface, const char *what)
{
#ifdef _WIN32
    UNUSED(what);
    return surface;   // No key on Windows (keep_off_chroma_key())
#else
    if (surface->format != SDL_PIXELFORMAT_RGBA32) {
        SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        if (converted == NULL) {
            log_error("Could not convert %s to keep it off the chroma key\n%s", what, SDL_GetError());
            return surface;
        }
        SDL_DestroySurface(surface);
        surface = converted;
    }
    if (SDL_LockSurface(surface)) {
        keep_off_chroma_key(surface->pixels, surface->w, surface->h, surface->pitch, what);
        SDL_UnlockSurface(surface);
    }
    else {
        // The pixels cannot be read, but the texture can still be made from the surface: the icon
        // is drawn as it is, and only a Transparent window may show through its near-key pixels
        log_debug("%s: could not be kept off the chroma key, so it is drawn as it is: %s", what, SDL_GetError());
    }
    return surface;
#endif
}

// A function to rasterize an SVG from a text buffer, keeping its pixels off the chroma key on Linux;
// `what` names it for the log
static SDL_Texture *rasterize(char *buffer, int w, int h, SDL_Rect *rect, const char *what)
{
    NSVGimage *image = NULL;
    unsigned char *pixel_buffer = NULL;
    int width, height, pitch;
    float scale;

    // Parse SVG to NSVGimage struct
    Uint64 start = SDL_GetPerformanceCounter();
    image = nsvgParse(buffer, "px", 96.0f);
    if (image == NULL) {
        log_error("could not open SVG image.");
        return NULL;
    }

    // Calculate scaling and dimensions
    if (w == -1 && h == -1) {
        scale = 1.0f;
        width = (int) image->width;
        height = (int) image->height;
    }
    else if (w == -1 && h != -1) {
        scale = (float) h / (float) image->height;
        width = (int) ceil((double) image->width * (double) scale);
        height = h;
    }
    else if (w != -1 && h == -1) {
        scale = (float) w / (float) image->width;
        width = w;
        height = (int) ceil((double) image->height * (double) scale);
    }
    else {
        scale = (float) w / (float) image->width;
        width = w;
        height = h;
    }
    
    // Allocate memory
    pitch = 4*width;
    pixel_buffer = malloc((size_t) (4*width*height));
    if (pixel_buffer == NULL) {
        log_error("Could not alloc SVG pixel buffer.");
        return NULL;
    }

    // Rasterize image
    nsvgRasterize(rasterizer, image, 0, 0, scale, pixel_buffer, width, height, pitch);
    keep_off_chroma_key(pixel_buffer, width, height, pitch, what);
    Uint64 rasterized = SDL_GetPerformanceCounter();
    timing_decode += rasterized - start;
    SDL_Surface *surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, pixel_buffer, pitch);
    SDL_Texture *texture = surface_texture(surface);
    timing_texture += SDL_GetPerformanceCounter() - rasterized;
    if (rect != NULL) {
        rect->w = width;
        rect->h = height;
    }
    free(pixel_buffer);
    SDL_DestroySurface(surface);
    nsvgDelete(image);
    return texture;
}

// A function to rasterize an SVG from an existing text buffer
SDL_Texture *rasterize_svg(char *buffer, int w, int h, SDL_Rect *rect)
{
    return rasterize(buffer, w, h, rect, "An SVG image");
}

// A function to rasterize an SVG file; pass -1 for w or h to keep the aspect ratio. A path to
// something that is not a regular file is never opened.
SDL_Texture *rasterize_svg_from_file(const char *path, int w, int h, SDL_Rect *rect)
{
    if (fileio_not_a_file(path)) {
        log_error("Could not load image %s\n%s", path, fileio_last_error());
        return NULL;
    }
    Uint64 start = SDL_GetPerformanceCounter();
    char *buffer = SDL_LoadFile(path, NULL);
    timing_read += SDL_GetPerformanceCounter() - start;
    if (buffer == NULL) {
        log_error("Could not load image %s\n%s", path, SDL_GetError());
        return NULL;
    }
    SDL_Texture *texture = rasterize(buffer, w, h, rect, path);
    SDL_free(buffer);
    return texture;
}

// A function to load a menu icon. SVGs are rasterized at the button size so they stay sharp
// at any size; other formats load at their own size and the renderer scales them. Either way, on
// Linux, its pixels are kept off the chroma key. A path to something that is not a regular file is
// never opened.
SDL_Texture *load_icon(const char *path, int size)
{
    if (path == NULL)
        return NULL;
    size_t length = strlen(path);
    if (length > 4 && SDL_strcasecmp(path + length - 4, ".svg") == 0)
        return rasterize_svg_from_file(path, size, -1, NULL);
    if (fileio_not_a_file(path)) {
        log_error("Could not load image %s\n%s", path, fileio_last_error());
        return NULL;
    }
    SDL_Surface *surface = load_surface_timed(path, &timing_read, &timing_decode);
    if (surface == NULL) {
        log_error("Could not load image %s\n%s", path, SDL_GetError());
        return NULL;
    }
    // The chroma pass counts as decode: a decode worker would run it too (spec section 8)
    Uint64 decoded = SDL_GetPerformanceCounter();
    surface = keep_surface_off_chroma_key(surface, path);
    Uint64 kept = SDL_GetPerformanceCounter();
    timing_decode += kept - decoded;
    SDL_Texture *texture = load_texture(surface);
    timing_texture += SDL_GetPerformanceCounter() - kept;
    return texture;
}

// A function to render the highlight for the buttons
SDL_Texture *render_highlight(int width, int height, SDL_Rect *rect)
{
    // Insert user config variables into SVG-formatted text buffer
    char *buffer = NULL;
    char *outline_buffer = NULL;
    if (eff.highlight_outline_size) {
        float stroke_opacity = ((float) eff.highlight_outline.a) / 255.0f;
        format_highlight_outline(&outline_buffer,
            eff.highlight_outline_size,
            eff.highlight_outline,
            stroke_opacity
        );
    }
    else
        outline_buffer = "";

    float fill_opacity = ((float) eff.highlight_fill.a) / 255.0f;
    format_highlight(&buffer,
        width,
        height,
        eff.highlight_rx,
        eff.highlight_fill,
        fill_opacity,
        outline_buffer
    );

    // Rasterize the SVG
    SDL_Texture *texture = rasterize(buffer, -1, -1, rect, "The highlight");
    
    // Cleanup
    free(buffer);
    if (eff.highlight_outline_size)
        free(outline_buffer);

    return texture;
}

// A function to render the scroll indicators. It returns non-zero when the arrow cannot be
// drawn; `scroll` belongs to the caller, which decides what to do then.
int render_scroll_indicators(Scroll *scroll, int height, Geometry *geo)
{
    // Format the SVG
    char *buffer = NULL;
    float opacity = (float) eff.scroll_fill.a / 255.0f;
    format_scroll_indicator(&buffer,
        eff.scroll_fill,
        eff.scroll_outline_size,
        eff.scroll_outline,
        opacity
    );

    // Rasterize the SVG
    scroll->texture = rasterize(buffer,
                          -1,
                          height,
                          &scroll->rect_right,
                          "The scroll arrow"
                      );
    free(buffer);
    if (scroll->texture == NULL)
        return 1;
    scroll->rect_left.w = scroll->rect_right.w;
    scroll->rect_left.h = scroll->rect_right.h;

    // Calculate screen position
    scroll->rect_right.y = geo->screen_height - geo->screen_margin - scroll->rect_right.h;
    scroll->rect_right.x = geo->screen_width - geo->screen_margin - scroll->rect_right.w;
    scroll->rect_left.y = scroll->rect_right.y;
    scroll->rect_left.x = geo->screen_margin;

    // Grid indicators: the same arrow drawn a quarter turn round (see draw_screen), sized so
    // its on-screen height is the screen margin and centered in the top and bottom margins.
    // SDL rotates about the rect's center, so the rect keeps the unrotated arrow's proportions,
    // and its width becomes the on-screen height.
    int grid_w = geo->screen_margin;
    int grid_h = grid_w * scroll->rect_right.h / scroll->rect_right.w;
    scroll->rect_up = (SDL_Rect) {
        .x = geo->screen_width / 2 - grid_w / 2,
        .y = geo->screen_margin / 2 - grid_h / 2,
        .w = grid_w,
        .h = grid_h
    };
    scroll->rect_down = scroll->rect_up;
    scroll->rect_down.y = geo->screen_height - geo->screen_margin / 2 - grid_h / 2;
    return 0;
}

// A function to cut a line of text with "..." to fit a width, measured in the font it is drawn in:
// utf8_truncate() estimates from the average character's width, and wide letters can leave the
// estimate too long, so one more character goes until it fits. Leaves the width and height in w, h.
static void truncate_to_fit(TTF_Font *font, char *text, int max_width, int *w, int *h)
{
    utf8_truncate(text, *w, max_width);
    TTF_GetStringSize(font, text, 0, w, h);
    while (*w > max_width && utf8_shorten(text))
        TTF_GetStringSize(font, text, 0, w, h);
}

// A function to render text
SDL_Surface *render_text(const char *text, TextInfo *info, SDL_Rect *rect, int *text_height)
{
    TTF_Font *output_font = NULL;
    TTF_Font *reduced_font = NULL; // Font for Shrink text oversize mode
    int w, h;

    // Copy text into new buffer in case we need to manipulate it
    char *text_buffer = strdup(text);

    // Calculate size of the rendered title
    TTF_GetStringSize(info->font, text_buffer, 0, &w, &h);

    // If title is too large to fit
    if (info->oversize_mode != OVERSIZE_NONE && w > info->max_width) {

        // Truncate mode:
        if (info->oversize_mode == OVERSIZE_TRUNCATE)
            truncate_to_fit(info->font, text_buffer, info->max_width, &w, &h);

        // Shrink mode: work out the size that fits from the measured width, never going below
        // the readable minimum, then cut whatever still does not fit
        else if (info->oversize_mode == OVERSIZE_SHRINK) {
            int size = info->font_size * info->max_width / w;
            if (size < info->min_size)
                size = info->min_size;
            if (size > 0 && size < info->font_size) {
                reduced_font = open_font_face(info->font_path, size, info->font_face);

                // The font's own rounding can leave it a pixel or two too wide: step down to the minimum
                while (reduced_font != NULL) {
                    TTF_GetStringSize(reduced_font, text_buffer, 0, &w, &h);
                    if (w <= info->max_width || size <= info->min_size)
                        break;
                    TTF_CloseFont(reduced_font);
                    reduced_font = open_font_face(info->font_path, --size, info->font_face);
#ifdef STREAMFLEX_TEST_HOOKS
                    // Only the headless harness builds this: every step down fails to open
                    if (getenv("STREAMFLEX_TEST_FAIL_SHRINK_STEP") != NULL && reduced_font != NULL) {
                        log_debug("Test hook: the step down to %i pt fails", size);
                        TTF_CloseFont(reduced_font);
                        reduced_font = NULL;
                    }
#endif
                }
            }

            // A smaller font that failed to open leaves the title at the menu's size, and w from
            // the font just closed; truncate_to_fit() measures it again in the font it is drawn in
            if (w > info->max_width)
                truncate_to_fit(reduced_font != NULL ? reduced_font : info->font, text_buffer, info->max_width, &w, &h);
        }
    }
    output_font = reduced_font != NULL ? reduced_font : info->font;

    // Render surface
    SDL_Surface *surface = NULL;
    if (info->shadow) {
        int shadow_offset = layout_shadow_offset(h);
        SDL_Surface *foreground = TTF_RenderText_Blended(output_font,
                                      text_buffer,
                                      0,
                                      *info->color
                                  );
        SDL_Surface *shadow = TTF_RenderText_Blended(output_font,
                                  text_buffer,
                                  0,
                                  *info->shadow_color
                              );
        surface = SDL_CreateSurface(foreground->w + shadow_offset, foreground->h + shadow_offset, SDL_PIXELFORMAT_ARGB8888);
        Uint32 color = SDL_MapSurfaceRGBA(surface, 0, 0, 0, 0);
        SDL_FillSurfaceRect(surface, NULL, color);
        SDL_Rect shadow_rect = {shadow_offset, shadow_offset, shadow->w, shadow->h};
        SDL_BlitSurface(shadow, NULL, surface, &shadow_rect);
        SDL_Rect foreground_rect = {0, 0, foreground->w, foreground->h};
        SDL_BlitSurface(foreground, NULL, surface, &foreground_rect);
        SDL_DestroySurface(foreground);
        SDL_DestroySurface(shadow);
    }
    else
        surface = TTF_RenderText_Blended(output_font,
                      text_buffer,
                      0,
                      *info->color
                  );

    // Set geometry
    rect->w = surface->w;
    rect->h = surface->h;
    if (info->oversize_mode == OVERSIZE_SHRINK && text_height != NULL)
        *text_height = h;

    // Clean up
    if (reduced_font != NULL)
        TTF_CloseFont(reduced_font);
    free(text_buffer);
    
    return surface;
}

// A function to render text into a texture
SDL_Texture *render_text_texture(const char *text, TextInfo *info, SDL_Rect *rect, int *text_height)
{
    SDL_Surface *surface = render_text(text, info, rect, text_height);
    return load_texture(surface);
}

// A function to tell a relative path (.\assets\fonts\x.ttf, fonts/x.ttf) from an absolute one
static bool is_relative_path(const char *path)
{
    if (path[0] == '/' || path[0] == '\\')
        return false;
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':')
        return false;
    return true;
}

// A function to find a bundled font: next to the executable, else where the packages install it
char *find_default_font(const char *font)
{
    const char *prefixes[2];
    char fonts_exe_buffer[MAX_PATH_CHARS + 1];
    // No base path (SDL_GetBasePath() failed): the folder beside the executable is unknown, so only
    // where the packages install the fonts is searched (find_file() skips a NULL prefix)
    prefixes[0] = config.exe_path != NULL
                  ? join_paths(fonts_exe_buffer, sizeof(fonts_exe_buffer), 3, config.exe_path, PATH_ASSETS_EXE, PATH_FONTS_EXE)
                  : NULL;
#ifdef __unix__
    prefixes[1] = PATH_FONTS_SYSTEM;
#else
    prefixes[1] = PATH_FONTS_RELATIVE;
#endif
    return find_file(font, 2, prefixes);
}

// A function to open a font file at a size and face. A path to something that is not a regular file
// (a pipe, say, whose read would wait for good) is never opened: it fails as a missing file does,
// and SDL_GetError() says why.
TTF_Font *open_font_file(const char *path, int size, int face)
{
    if (fileio_not_a_file(path)) {
        SDL_SetError("%s", fileio_last_error());
        return NULL;
    }
    return open_font_face(path, size, face);
}

// A function to open a text's font: the configured file and face (a relative path is also tried
// beside the executable), else the bundled font. The configured path is never changed: what was
// opened is kept in info->font_path, and a failure says so in the log. A font the TextInfo still
// holds is closed first, so a reload leaks nothing: the caller must drop any other pointer to it
// (launcher.c's fixed_title_font), and info->font must not be one of title_font()'s cached fonts.
int load_font(TextInfo *info, const char *configured, int face, const char *default_font)
{
    if (info->font != NULL)
        TTF_CloseFont(info->font);
    free(info->font_path);
    info->font_path = NULL;
    info->font = NULL;
    info->font_face = 0;
    if (configured != NULL) {
        info->font = open_font_file(configured, info->font_size, face);
        if (info->font != NULL)
            info->font_path = strdup(configured);

        // A relative path in the config means the folder StreamFlex is in, not the one it was started
        // from (the Windows config names .\assets\fonts\...; a shortcut's "Start in" folder can be anywhere)
        else if (config.exe_path != NULL && is_relative_path(configured)) {
            char exe_font_path[MAX_PATH_CHARS + 1];
            join_paths(exe_font_path, sizeof(exe_font_path), 2, config.exe_path, configured);
            info->font = open_font_file(exe_font_path, info->font_size, face);
            if (info->font != NULL)
                info->font_path = strdup(exe_font_path);
        }
        if (info->font != NULL)
            info->font_face = face;
        else
            log_error("Could not open the font %s (face %i), using the default font\n%s", configured, face, SDL_GetError());
    }
    if (info->font == NULL) {
        char *default_font_path = find_default_font(default_font);
        if (default_font_path != NULL) {
            info->font = open_font_file(default_font_path, info->font_size, 0);
            if (info->font == NULL)
                log_error("Could not open the default font %s\n%s", default_font_path, SDL_GetError());
        }
        if (info->font == NULL) {
            free(default_font_path);
            log_fatal("Could not load default font");
            return 1;
        }
        info->font_path = default_font_path;
    }
    return 0;
}

// A function to tell whether a configured font file is there, where load_font() looks for it: the
// path as given, else a relative one beside the executable
bool font_file_found(const char *configured)
{
    if (fileio_is_file(configured))
        return true;
    char exe_font_path[MAX_PATH_CHARS + 1];
    return config.exe_path != NULL && is_relative_path(configured)
        && fileio_is_file(join_paths(exe_font_path, sizeof(exe_font_path), 2, config.exe_path, configured));
}
