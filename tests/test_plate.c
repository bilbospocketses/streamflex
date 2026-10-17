// Plating an app's own icon (3c Task 9): the library's rounded square plate in #07606C, the icon centered
// in its 0.60 box, scaled premultiplied and by area; the file's name, a clash numbered, and the folder.
// Run as "test_plate png", it is the png_roundtrip test: a plated icon saved with IMG_SavePNG and read
// back with IMG_Load is the same, pixel for pixel, which proves the build's SDL3_image writes PNG.
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include "check.h"
#include "plate.h"
#include "fileio.h"

#define DIR "plate-fixture"
#ifdef _WIN32
#define SEP "\\"
#else
#define SEP "/"
#endif
#define ARGB(a, r, g, b) ((Uint32) (a) << 24 | (Uint32) (r) << 16 | (Uint32) (g) << 8 | (Uint32) (b))
#define PLATE ARGB(0xFF, 0x07, 0x60, 0x6C)   // The library's general plate, opaque
#define CLEAR ARGB(0x00, 0x07, 0x60, 0x6C)   // Outside its corners: the plate's color at alpha 0
#define RED ARGB(0xFF, 0xFF, 0x00, 0x00)
#define CHECK_PIXEL(surface, x, y, expected) check_pixel(surface, x, y, expected, __LINE__)

// A function to read one pixel of an ARGB8888 surface
static Uint32 pixel(const SDL_Surface *surface, int x, int y)
{
    return ((const Uint32 *) ((const Uint8 *) surface->pixels + (size_t) y * (size_t) surface->pitch))[x];
}

// A function to write one pixel of an ARGB8888 surface
static void put(SDL_Surface *surface, int x, int y, Uint32 value)
{
    ((Uint32 *) ((Uint8 *) surface->pixels + (size_t) y * (size_t) surface->pitch))[x] = value;
}

// A function to check one pixel, naming it and both values in hex when it differs
static void check_pixel(const SDL_Surface *surface, int x, int y, Uint32 expected, int line)
{
    Uint32 found = pixel(surface, x, y);
    check_count++;
    if (found != expected) {
        check_failures++;
        fprintf(stderr, "test_plate.c:%d: pixel %d,%d is %08X, expected %08X\n", line, x, y, (unsigned) found, (unsigned) expected);
    }
}

// A function to make an icon of one color (straight ARGB)
static SDL_Surface *solid(int width, int height, Uint32 color)
{
    SDL_Surface *surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_ARGB8888);
    CHECK(surface != NULL);
    for (int y = 0; surface != NULL && y < height; y++) {
        for (int x = 0; x < width; x++)
            put(surface, x, y, color);
    }
    return surface;
}

// A function to plate an icon at 100 px and free the icon; NULL (a failed check) when it could not
static SDL_Surface *plated_at_100(SDL_Surface *icon)
{
    SDL_Surface *plated = icon != NULL ? plate_icon(icon, 100) : NULL;
    CHECK(plated != NULL);
    SDL_DestroySurface(icon);
    return plated;
}

// A function to test the plate's shape on a 100 px plate: clear outside its corners, opaque along its
// edges and through its middle, the corners' curve partly covered and the same at all four; a 10 px red
// icon grown to fill the 60 px box at 20 to 79 on both axes; and the plate's color under every pixel the
// icon does not cover, whatever its alpha
static void test_shape(void)
{
    SDL_Surface *plated = plated_at_100(solid(10, 10, RED));
    if (plated == NULL)
        return;
    CHECK(plated->w == 100 && plated->h == 100 && plated->format == SDL_PIXELFORMAT_ARGB8888);
    CHECK_PIXEL(plated, 0, 0, CLEAR);
    CHECK_PIXEL(plated, 99, 0, CLEAR);
    CHECK_PIXEL(plated, 0, 99, CLEAR);
    CHECK_PIXEL(plated, 99, 99, CLEAR);
    CHECK_PIXEL(plated, 50, 0, PLATE);
    CHECK_PIXEL(plated, 0, 50, PLATE);
    CHECK_PIXEL(plated, 99, 50, PLATE);
    CHECK_PIXEL(plated, 50, 99, PLATE);
    CHECK_PIXEL(plated, 10, 50, PLATE);
    CHECK_PIXEL(plated, 19, 50, PLATE);
    CHECK_PIXEL(plated, 50, 19, PLATE);
    CHECK_PIXEL(plated, 20, 50, RED);
    CHECK_PIXEL(plated, 50, 20, RED);
    CHECK_PIXEL(plated, 50, 50, RED);
    CHECK_PIXEL(plated, 79, 79, RED);
    CHECK_PIXEL(plated, 80, 50, PLATE);
    CHECK_PIXEL(plated, 50, 80, PLATE);
    int edge = (int) (pixel(plated, 6, 6) >> 24);
    CHECK(edge > 0 && edge < 255);
    CHECK_PIXEL(plated, 93, 6, pixel(plated, 6, 6));
    CHECK_PIXEL(plated, 6, 93, pixel(plated, 6, 6));
    CHECK_PIXEL(plated, 93, 93, pixel(plated, 6, 6));
    int stray = 0;
    for (int y = 0; y < 100; y++) {
        for (int x = 0; x < 100; x++) {
            bool icon = x >= 20 && x < 80 && y >= 20 && y < 80;
            if (!icon && (pixel(plated, x, y) & 0xFFFFFFu) != 0x07606Cu)
                stray++;
        }
    }
    CHECK_INT(stray, 0);
    SDL_DestroySurface(plated);
}

// A function to test an icon's alpha over the plate: half-transparent white shows the plate through
// what it leaves, (255 x 128/255) + 0x07 x 127/255 and so on, rounded: 131, 176, 182, opaque
static void test_composite(void)
{
    SDL_Surface *plated = plated_at_100(solid(2, 2, ARGB(128, 255, 255, 255)));
    if (plated == NULL)
        return;
    CHECK_PIXEL(plated, 50, 50, ARGB(0xFF, 131, 176, 182));
    SDL_DestroySurface(plated);
}

// A function to test that the color under transparent pixels never reaches the icon: 300 px, white in
// its first 152 columns, transparent red after, shrunk five to one. The output column over columns 150 to
// 154 holds two white and three transparent: a premultiplied average of 102, which over the plate is
// 106, 160, 167. A straight average would bring the red in (and 98 for green).
static void test_no_halo(void)
{
    SDL_Surface *icon = SDL_CreateSurface(300, 300, SDL_PIXELFORMAT_ARGB8888);
    CHECK(icon != NULL);
    for (int y = 0; icon != NULL && y < 300; y++) {
        for (int x = 0; x < 300; x++)
            put(icon, x, y, x < 152 ? ARGB(255, 255, 255, 255) : ARGB(0, 255, 0, 0));
    }
    SDL_Surface *plated = plated_at_100(icon);
    if (plated == NULL)
        return;
    CHECK_PIXEL(plated, 49, 50, ARGB(0xFF, 0xFF, 0xFF, 0xFF));
    CHECK_PIXEL(plated, 50, 50, ARGB(0xFF, 106, 160, 167));
    CHECK_PIXEL(plated, 51, 50, PLATE);
    SDL_DestroySurface(plated);
}

// A function to test that shrinking averages by area: a 300 px checkerboard of single black and white
// pixels, shrunk five to one, is gray everywhere (122 or 133); sampling a pixel per output pixel would
// give black and white
static void test_shrink_by_area(void)
{
    SDL_Surface *icon = SDL_CreateSurface(300, 300, SDL_PIXELFORMAT_ARGB8888);
    CHECK(icon != NULL);
    for (int y = 0; icon != NULL && y < 300; y++) {
        for (int x = 0; x < 300; x++)
            put(icon, x, y, (x + y) % 2 != 0 ? ARGB(255, 255, 255, 255) : ARGB(255, 0, 0, 0));
    }
    SDL_Surface *plated = plated_at_100(icon);
    if (plated == NULL)
        return;
    int outside = 0;
    for (int y = 20; y < 80; y++) {
        for (int x = 20; x < 80; x++) {
            Uint32 p = pixel(plated, x, y);
            Uint32 r = (p >> 16) & 0xFF;
            if (r < 115 || r > 140 || r != ((p >> 8) & 0xFF) || r != (p & 0xFF))
                outside++;
        }
    }
    CHECK_INT(outside, 0);
    SDL_DestroySurface(plated);
}

// A function to test an icon that is not square: 20 x 10 fills the box's width (60) and keeps its aspect
// (30), centered at rows 35 to 64
static void test_aspect(void)
{
    SDL_Surface *plated = plated_at_100(solid(20, 10, RED));
    if (plated == NULL)
        return;
    CHECK_PIXEL(plated, 50, 34, PLATE);
    CHECK_PIXEL(plated, 50, 35, RED);
    CHECK_PIXEL(plated, 50, 64, RED);
    CHECK_PIXEL(plated, 50, 65, PLATE);
    CHECK_PIXEL(plated, 19, 50, PLATE);
    CHECK_PIXEL(plated, 20, 50, RED);
    CHECK_PIXEL(plated, 79, 50, RED);
    CHECK_PIXEL(plated, 80, 50, PLATE);
    SDL_DestroySurface(plated);
}

// A function to test the refusals: no icon, and a plate too small to sample; each says why
static void test_refused(void)
{
    SDL_ClearError();
    CHECK(plate_icon(NULL, 100) == NULL);
    CHECK(SDL_GetError()[0] != '\0');
    SDL_Surface *icon = solid(4, 4, RED);
    CHECK(plate_icon(icon, 3) == NULL);
    SDL_DestroySurface(icon);
}

// A function to give the file name icons_file_name() makes of an app's name
static const char *file_name(const char *app_name)
{
    static char out[64];
    icons_file_name(app_name, out, sizeof(out));
    return out;
}

// A function to test the file names: lower-case letters and digits, anything else one '-' however many
// there were and none at either end, 48 characters at most, "app" when nothing is left
static void test_file_names(void)
{
    CHECK_STR(file_name("Microsoft Edge"), "microsoft-edge.png");
    CHECK_STR(file_name("VLC media player"), "vlc-media-player.png");
    CHECK_STR(file_name("  GIMP 2.10 (64-bit)!"), "gimp-2-10-64-bit.png");
    CHECK_STR(file_name("Caf\xC3\xA9 \xC3\x9Cnicode"), "caf-nicode.png");
    CHECK_STR(file_name("\xE6\x97\xA5\xE6\x9C\xAC"), "app.png");
    CHECK_STR(file_name("!!!"), "app.png");
    CHECK_STR(file_name(""), "app.png");
    CHECK_STR(file_name(NULL), "app.png");

    char name[80];
    char expected[80];
    memset(name, 'A', 60);
    name[60] = '\0';
    memset(expected, 'a', ICONS_NAME_MAX);
    snprintf(expected + ICONS_NAME_MAX, sizeof(expected) - ICONS_NAME_MAX, ".png");
    CHECK_STR(file_name(name), expected);
    memset(name, 'a', 47);
    snprintf(name + 47, sizeof(name) - 47, " b");          // The '-' is the 48th: cut, and dropped
    memset(expected, 'a', 47);
    snprintf(expected + 47, sizeof(expected) - 47, ".png");
    CHECK_STR(file_name(name), expected);
}

// A function to test a new file's path: the name itself, then -2 and -3 as each before it is there;
// false when the path does not fit
static void test_new_path(void)
{
    static const char *const dir = DIR SEP "icons";
    CHECK(fileio_make_dirs(dir));
    fileio_remove(DIR SEP "icons" SEP "kodi.png");
    fileio_remove(DIR SEP "icons" SEP "kodi-2.png");
    fileio_remove(DIR SEP "icons" SEP "kodi-3.png");
    char path[256];
    CHECK(icons_new_path(dir, "Kodi", path, sizeof(path)));
    CHECK_STR(path, DIR SEP "icons" SEP "kodi.png");
    CHECK(fileio_write_all(path, "x", 1));
    CHECK(icons_new_path(dir, "KODI", path, sizeof(path)));
    CHECK_STR(path, DIR SEP "icons" SEP "kodi-2.png");
    CHECK(fileio_write_all(path, "x", 1));
    CHECK(icons_new_path(dir, "kodi", path, sizeof(path)));
    CHECK_STR(path, DIR SEP "icons" SEP "kodi-3.png");
    CHECK(!icons_new_path(dir, "Kodi", path, 10));
}

// A function to test the icons folder's rule, made as it is found: beside the config on Windows, in
// ~/.config/streamflex on Linux; none without a config (Windows) or a home (Linux); false when the path
// does not fit
static void test_icons_dir(void)
{
    char out[512];
#ifdef _WIN32
    CHECK(icons_dir_at(DIR "\\cfg\\config.ini", NULL, out, sizeof(out)));
    CHECK_STR(out, DIR "\\cfg\\icons");
    CHECK(fileio_is_dir(out));
    CHECK(!icons_dir_at(NULL, NULL, out, sizeof(out)));
    CHECK(!icons_dir_at("", NULL, out, sizeof(out)));
#else
    CHECK(icons_dir_at("/anywhere/config.ini", DIR "/home", out, sizeof(out)));
    CHECK_STR(out, DIR "/home/.config/streamflex/icons");
    CHECK(fileio_is_dir(out));
    CHECK(!icons_dir_at("/anywhere/config.ini", NULL, out, sizeof(out)));
    CHECK(!icons_dir_at("/anywhere/config.ini", "", out, sizeof(out)));
#endif
    CHECK(!icons_dir_at(DIR "\\cfg\\config.ini", DIR "/home", out, 8));
}

// A function to test that the build's SDL3_image writes PNG: a plated icon (partly transparent corners,
// an icon of many colors and alphas) saved and read back is the same, pixel for pixel (a clear pixel by
// its alpha alone); and a save into a folder that is not there is refused, saying why, with no file left
static void test_png_roundtrip(void)
{
    SDL_Surface *icon = SDL_CreateSurface(9, 7, SDL_PIXELFORMAT_ARGB8888);
    CHECK(icon != NULL);
    for (int y = 0; icon != NULL && y < 7; y++) {
        for (int x = 0; x < 9; x++)
            put(icon, x, y, ARGB(x * 28, y * 36, (x * y * 4) & 0xFF, 200));
    }
    SDL_Surface *plated = icon != NULL ? plate_icon(icon, 64) : NULL;
    SDL_DestroySurface(icon);
    CHECK(plated != NULL);
    if (plated == NULL)
        return;
    fileio_remove(DIR SEP "round.png");
    bool saved = plate_save(plated, DIR SEP "round.png");
    CHECK(saved);
    if (!saved)
        fprintf(stderr, "plate_save: %s\n", SDL_GetError());
    SDL_Surface *loaded = saved ? IMG_Load(DIR SEP "round.png") : NULL;
    CHECK(!saved || loaded != NULL);
    if (saved && loaded == NULL)
        fprintf(stderr, "IMG_Load: %s\n", SDL_GetError());
    SDL_Surface *read = loaded != NULL ? SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_ARGB8888) : NULL;
    if (read != NULL) {
        CHECK(read->w == 64 && read->h == 64);
        int differ = 0;
        int partial = 0;
        for (int y = 0; y < 64 && read->w == 64 && read->h == 64; y++) {
            for (int x = 0; x < 64; x++) {
                Uint32 written = pixel(plated, x, y);
                Uint32 back = pixel(read, x, y);
                bool same = (written >> 24) == 0 ? (back >> 24) == 0 : written == back;
                differ += same ? 0 : 1;
                partial += (written >> 24) > 0 && (written >> 24) < 255 ? 1 : 0;
            }
        }
        CHECK_INT(differ, 0);
        CHECK(partial > 0);
        printf("png_roundtrip: 4096 pixels read back as written, %d of them partly transparent\n", partial);
    }
    SDL_DestroySurface(read);
    SDL_DestroySurface(loaded);

    SDL_ClearError();
    CHECK(!plate_save(plated, DIR SEP "missing" SEP "round.png"));
    CHECK(SDL_GetError()[0] != '\0');
    CHECK(!fileio_present(DIR SEP "missing" SEP "round.png"));
    SDL_DestroySurface(plated);
}

int main(int argc, char **argv)
{
    if (!SDL_Init(0)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    CHECK(fileio_make_dirs(DIR));
    if (argc > 1 && strcmp(argv[1], "png") == 0)
        test_png_roundtrip();
    else {
        test_shape();
        test_composite();
        test_no_halo();
        test_shrink_by_area();
        test_aspect();
        test_refused();
        test_file_names();
        test_new_path();
        test_icons_dir();
    }
    SDL_Quit();
    return check_report();
}
