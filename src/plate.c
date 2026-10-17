#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <launcher_config.h>
#include "plate.h"
#include "fileio.h"
#ifdef _WIN32
#define SEPARATOR "\\"
#else
#define SEPARATOR "/"
#endif

#define PLATE_RED 0x07       // The general group's plate, #07606C (build-library.py GENERAL_HEX): the generic
#define PLATE_GREEN 0x60     // apps icon's, which an app with no icon of its own gets
#define PLATE_BLUE 0x6C
#define SAMPLES 4            // Points across and down a corner pixel is sampled at, as libtools.outline_mask()
                             // draws at 4x and box-filters down
#define MAX_CLASH 999        // name-2.png to name-999.png, then no new name

// A function to round a channel's value to a byte, held to 0 to 255
static Uint32 channel(double value)
{
    if (value <= 0.0)
        return 0;
    if (value >= 255.0)
        return 255;
    return (Uint32) (value + 0.5);
}

// A function to count the sample points of a pixel inside the plate: a square of side `size` whose corners
// are rounded to `radius`, sampled at SAMPLES x SAMPLES points
static int coverage(int size, double radius, int x, int y)
{
    int inside = 0;
    for (int j = 0; j < SAMPLES; j++) {
        for (int i = 0; i < SAMPLES; i++) {
            double px = x + (i + 0.5) / SAMPLES;
            double py = y + (j + 0.5) / SAMPLES;
            double dx = px < radius ? radius - px : px > size - radius ? px - (size - radius) : 0.0;
            double dy = py < radius ? radius - py : py > size - radius ? py - (size - radius) : 0.0;
            if (dx * dx + dy * dy <= radius * radius)
                inside++;
        }
    }
    return inside;
}

// A function to draw the plate: the library's rounded square in its color, row by row. The rows and the
// columns clear of the corners are wholly inside, so only the corners' pixels are sampled, each given the
// share of its points inside as its alpha, which smooths the curve as the library's own outline is
// smoothed. Every pixel has the plate's color, so filtering never blends another into the edge.
static void draw_plate(SDL_Surface *plated, int size)
{
    double radius = size * PLATE_RADIUS;
    int corner = (int) radius;
    if (corner < radius)
        corner++;
    const Uint32 color = (Uint32) PLATE_RED << 16 | (Uint32) PLATE_GREEN << 8 | (Uint32) PLATE_BLUE;
    for (int y = 0; y < size; y++) {
        Uint32 *row = (Uint32 *) ((Uint8 *) plated->pixels + (size_t) y * (size_t) plated->pitch);
        bool clear_row = y >= corner && y < size - corner;
        for (int x = 0; x < size; x++) {
            bool clear = clear_row || (x >= corner && x < size - corner);
            int inside = clear ? SAMPLES * SAMPLES : coverage(size, radius, x, y);
            Uint32 alpha = (Uint32) ((inside * 255 + SAMPLES * SAMPLES / 2) / (SAMPLES * SAMPLES));
            row[x] = alpha << 24 | color;
        }
    }
}

// A function to read an icon into premultiplied channels, four floats a pixel (alpha, red, green, blue,
// each 0 to 255): scaled that way, a transparent pixel's color, whatever it is, adds nothing to its
// neighbors. NULL, with SDL's error set, when out of memory or the icon cannot be converted.
static float *premultiplied(SDL_Surface *icon)
{
    SDL_Surface *argb = SDL_ConvertSurface(icon, SDL_PIXELFORMAT_ARGB8888);
    if (argb == NULL)
        return NULL;
    float *out = malloc((size_t) argb->w * (size_t) argb->h * 4 * sizeof(float));
    if (out == NULL)
        SDL_OutOfMemory();
    for (int y = 0; out != NULL && y < argb->h; y++) {
        const Uint32 *row = (const Uint32 *) ((const Uint8 *) argb->pixels + (size_t) y * (size_t) argb->pitch);
        for (int x = 0; x < argb->w; x++) {
            float *p = out + ((size_t) y * (size_t) argb->w + (size_t) x) * 4;
            float alpha = (float) (row[x] >> 24);
            float share = alpha / 255.0F;
            p[0] = alpha;
            p[1] = (float) ((row[x] >> 16) & 0xFF) * share;
            p[2] = (float) ((row[x] >> 8) & 0xFF) * share;
            p[3] = (float) (row[x] & 0xFF) * share;
        }
    }
    SDL_DestroySurface(argb);
    return out;
}

// A function to resample one line of premultiplied pixels, four floats each and `in_step` or `out_step`
// pixels apart, from `from` pixels to `to`. Shrinking averages by area: each pixel made is the mean of
// the pixels it covers, one it covers in part weighed by that part, so none is skipped. Growing
// interpolates between the pixels' centers, the edge pixels held.
static void resample_line(const float *in, int from, int in_step, float *out, int to, int out_step)
{
    if (to < from) {
        double scale = (double) from / to;
        for (int o = 0; o < to; o++) {
            double start = o * scale;
            double end = start + scale;
            double sum[4] = { 0.0, 0.0, 0.0, 0.0 };
            for (int i = (int) start; i < from && i < end; i++) {
                double left = i > start ? i : start;
                double right = i + 1 < end ? i + 1 : end;
                const float *p = in + (size_t) i * (size_t) in_step * 4;
                for (int c = 0; c < 4; c++)
                    sum[c] += (right - left) * p[c];
            }
            float *q = out + (size_t) o * (size_t) out_step * 4;
            for (int c = 0; c < 4; c++)
                q[c] = (float) (sum[c] / scale);
        }
        return;
    }
    for (int o = 0; o < to; o++) {
        double at = (o + 0.5) * from / to - 0.5;
        if (at < 0.0)
            at = 0.0;
        int i = (int) at;
        if (i > from - 1)
            i = from - 1;
        int next = i + 1 < from ? i + 1 : i;
        double t = at - i;
        const float *p = in + (size_t) i * (size_t) in_step * 4;
        const float *n = in + (size_t) next * (size_t) in_step * 4;
        float *q = out + (size_t) o * (size_t) out_step * 4;
        for (int c = 0; c < 4; c++)
            q[c] = (float) (p[c] * (1.0 - t) + n[c] * t);
    }
}

// A function to resample premultiplied pixels to a new size, across then down; NULL, with SDL's error
// set, when out of memory
static float *resample(const float *in, int width, int height, int to_width, int to_height)
{
    float *across = malloc((size_t) to_width * (size_t) height * 4 * sizeof(float));
    float *out = malloc((size_t) to_width * (size_t) to_height * 4 * sizeof(float));
    if (across == NULL || out == NULL) {
        free(across);
        free(out);
        SDL_OutOfMemory();
        return NULL;
    }
    for (int y = 0; y < height; y++)
        resample_line(in + (size_t) y * (size_t) width * 4, width, 1, across + (size_t) y * (size_t) to_width * 4, to_width, 1);
    for (int x = 0; x < to_width; x++)
        resample_line(across + (size_t) x * 4, height, to_width, out + (size_t) x * 4, to_height, to_width);
    free(across);
    return out;
}

// A function to put an app's own icon on the library's plate (spec "Icons", choice 2): a new ARGB8888
// surface, `size` square, the plate drawn by draw_plate() and the icon centered in its PLATE_GLYPH_BOX
// box, its longer side filling the box and its aspect kept, scaled premultiplied and by area. The box
// lies inside the plate's opaque part (it starts 0.20 of the side in; the corners' curves reach 0.22), so
// each of its pixels is the icon's color plus the plate's through what the icon leaves, opaque. NULL,
// with SDL's error set, for no icon, a plate too small to sample, or out of memory.
SDL_Surface *plate_icon(SDL_Surface *icon, int size)
{
    if (icon == NULL || icon->w <= 0 || icon->h <= 0 || size < SAMPLES) {
        SDL_SetError("There is no icon to put on a plate");
        return NULL;
    }
    int box = (int) (size * PLATE_GLYPH_BOX + 0.5);
    int longest = icon->w > icon->h ? icon->w : icon->h;
    int width = (int) ((double) icon->w * box / longest + 0.5);
    int height = (int) ((double) icon->h * box / longest + 0.5);
    if (width < 1)
        width = 1;
    if (height < 1)
        height = 1;
    float *pixels = premultiplied(icon);
    float *scaled = pixels != NULL ? resample(pixels, icon->w, icon->h, width, height) : NULL;
    free(pixels);
    SDL_Surface *plated = scaled != NULL ? SDL_CreateSurface(size, size, SDL_PIXELFORMAT_ARGB8888) : NULL;
    if (plated == NULL) {
        free(scaled);
        return NULL;
    }
    draw_plate(plated, size);
    int left = (size - width) / 2;
    int top = (size - height) / 2;
    for (int y = 0; y < height; y++) {
        Uint32 *row = (Uint32 *) ((Uint8 *) plated->pixels + (size_t) (top + y) * (size_t) plated->pitch);
        for (int x = 0; x < width; x++) {
            const float *p = scaled + ((size_t) y * (size_t) width + (size_t) x) * 4;
            double through = 1.0 - p[0] / 255.0;
            row[left + x] = 0xFF000000u | channel(p[1] + PLATE_RED * through) << 16 |
                            channel(p[2] + PLATE_GREEN * through) << 8 | channel(p[3] + PLATE_BLUE * through);
        }
    }
    free(scaled);
    return plated;
}

// A function to save a plated icon as a PNG (IMG_SavePNG, SDL3_image 3.2.4 and later), at a new file's
// path (icons_new_path()). False, with SDL's error saying why (a read-only folder, a full disk), when it
// could not; a file this save left half-written is removed, and never one that was there before it.
bool plate_save(SDL_Surface *plated, const char *path)
{
    bool was_there = fileio_present(path);
    if (plated != NULL && IMG_SavePNG(plated, path))
        return true;
    char why[512];
    snprintf(why, sizeof(why), "%s", plated != NULL ? SDL_GetError() : "There is no icon to save");
    if (!was_there)
        fileio_remove(path);
    SDL_SetError("%s", why);
    return false;
}

// A function to find the folder plated icons are saved in, and make it: on Windows, icons in the folder
// `config_path` is in; on Linux, <home>/.config/streamflex/icons, beside the user's own config.ini. False
// when there is no config path (Windows) or home (Linux), the path does not fit, or it cannot be made.
bool icons_dir_at(const char *config_path, const char *home, char *out, size_t size)
{
    int written;
#ifdef _WIN32
    (void) home;
    if (config_path == NULL || config_path[0] == '\0')
        return false;
    size_t folder = strlen(config_path);
    while (folder > 0 && !fileio_is_separator(config_path[folder - 1]))
        folder--;
    written = snprintf(out, size, "%.*sicons", (int) folder, config_path);
#else
    (void) config_path;
    if (home == NULL || home[0] == '\0')
        return false;
    written = snprintf(out, size, "%s/.config/" EXECUTABLE_TITLE "/icons", home);
#endif
    if (written < 0 || (size_t) written >= size)
        return false;
    return fileio_make_dirs(out);
}

// A function to make a file name from an app's name: lower-case ASCII letters and digits kept, every
// other byte (a space, punctuation, each byte of a UTF-8 character) a '-', a run of them one '-', none at
// either end, ICONS_NAME_MAX characters at most, then ".png"; "app.png" when nothing is left. `out` holds
// ICONS_NAME_MAX + 5 bytes.
void icons_file_name(const char *app_name, char *out, size_t size)
{
    char stem[ICONS_NAME_MAX + 1];
    size_t length = 0;
    bool dash = false;
    for (const char *p = app_name != NULL ? app_name : ""; *p != '\0' && length < ICONS_NAME_MAX; p++) {
        int c = fileio_lower(*p);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            stem[length++] = (char) c;
            dash = false;
        }
        else if (!dash && length > 0) {
            stem[length++] = '-';
            dash = true;
        }
    }
    while (length > 0 && stem[length - 1] == '-')
        length--;
    stem[length] = '\0';
    snprintf(out, size, "%s.png", length > 0 ? stem : "app");
}

// A function to give a new file's path in a folder for an app's plated icon: icons_file_name()'s name,
// or, while something is at that path, the name with -2, -3 and on up to -999 before ".png". False when
// none is free or the path does not fit.
bool icons_new_path(const char *dir, const char *app_name, char *out, size_t size)
{
    char file[ICONS_NAME_MAX + 5];
    icons_file_name(app_name, file, sizeof(file));
    int stem = (int) strlen(file) - 4;   // Without ".png"
    for (int n = 1; n <= MAX_CLASH; n++) {
        int written = n == 1 ? snprintf(out, size, "%s" SEPARATOR "%s", dir, file)
                             : snprintf(out, size, "%s" SEPARATOR "%.*s-%i.png", dir, stem, file, n);
        if (written < 0 || (size_t) written >= size)
            return false;
        if (!fileio_present(out))
            return true;
    }
    return false;
}
