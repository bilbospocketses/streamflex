// The shell's AppsFolder on a real Windows (3c Task 8): the scan lists apps through COM, each with a
// name and an ID the shell finds the app by again, as explorer.exe does when a tile launches it; an app's
// image comes at 256 px or less as a straight-alpha surface holding the shell's own bitmap as it is, which
// is straight alpha already (a Windows that premultiplied it would fail here), and a bitmap with no alpha
// at all comes opaque; no ID, no size, an ID too long, an ID the folder does not have, one that is not
// UTF-8, and a web link give no image and say why; the scan and the image each end the COM they started; a
// stop asked for before the first item lists nothing, and is no failure; and the whole scan runs on its own
// thread, COM started there, as settings run it. What the runner has installed is not known, so no app is
// named.
#define COBJMACROS
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <shobjidl.h>
#include <SDL3/SDL.h>
#include "check.h"
#include "applist.h"
#include "appscan.h"
#include "platform/appsfolder.h"

#define TRIES 40              // Apps whose image is asked for, at most, before the test gives up on one
#define HANG_GUARD_MS 120000  // The scan's thread, given up on as HUNG; a correct one takes a second or two

// A function to read one pixel of an ARGB8888 surface
static Uint32 pixel_at(const SDL_Surface *surface, int x, int y)
{
    return ((const Uint32 *) ((const Uint8 *) surface->pixels + (size_t) y * (size_t) surface->pitch))[x];
}

// A function to test appicon_image()'s pixel copy on bitmaps made here, without the shell: a straight,
// partly transparent one is kept as it is, colors above their alpha included; one with no alpha anywhere
// (an old icon) is made opaque, its colors kept
static void test_pixels(void)
{
    static const Uint32 straight_bits[4] = { 0x80402010u, 0xFFFFFFFFu, 0x00000000u, 0x01FF8000u };
    static const Uint32 no_alpha_bits[4] = { 0x00402010u, 0x00FFFFFFu, 0x00000000u, 0x00FF8000u };
    SDL_Surface *surface = SDL_CreateSurface(2, 2, SDL_PIXELFORMAT_ARGB8888);
    CHECK(surface != NULL);
    if (surface == NULL)
        return;
    appicon_pixels(straight_bits, surface);
    CHECK(pixel_at(surface, 0, 0) == 0x80402010u);
    CHECK(pixel_at(surface, 1, 0) == 0xFFFFFFFFu);
    CHECK(pixel_at(surface, 0, 1) == 0x00000000u);
    CHECK(pixel_at(surface, 1, 1) == 0x01FF8000u);
    appicon_pixels(no_alpha_bits, surface);
    CHECK(pixel_at(surface, 0, 0) == 0xFF402010u);
    CHECK(pixel_at(surface, 1, 0) == 0xFFFFFFFFu);
    CHECK(pixel_at(surface, 0, 1) == 0xFF000000u);
    CHECK(pixel_at(surface, 1, 1) == 0xFFFF8000u);
    SDL_DestroySurface(surface);
}

// A function to test appicon_image()'s refusals, each made before the shell is asked and each saying its
// own reason: no ID, no size, and an ID too long to name an item by
static void test_refusals(void)
{
    static char long_id[2001];   // 2000 bytes of 'A', far past the item's name the shell can be given
    memset(long_id, 'A', sizeof(long_id) - 1);
    CHECK(appicon_image(NULL, 256) == NULL);
    CHECK_STR(SDL_GetError(), "There is no app ID to get an image for");
    CHECK(appicon_image("", 256) == NULL);
    CHECK_STR(SDL_GetError(), "There is no app ID to get an image for");
    CHECK(appicon_image("StreamFlex.Test.NoSuchApp!App", 0) == NULL);
    CHECK_STR(SDL_GetError(), "An app's image cannot be 0 px a side");
    CHECK(appicon_image(long_id, 256) == NULL);
    CHECK_STR(SDL_GetError(), "The app ID is 2000 bytes, too long to get an image for");
}

// A function to tell whether COM is ended on this thread, as appsfolder_scan() and appicon_image() each
// leave it once they have started it themselves: starting it here is then a first start
static bool com_ended(void)
{
    HRESULT started = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(started))
        CoUninitialize();
    return started == S_OK;
}

// A function to ask the shell for the item an app ID names, as explorer.exe does when a tile runs
// "shell:AppsFolder\<ID>": the launch's own round trip, with no image asked for. S_OK when it is found,
// else why not; COM must be started on this thread.
static HRESULT parse_app(const char *app_id)
{
    char text[1100];
    wchar_t path[1100];
    int written = snprintf(text, sizeof(text), "shell:AppsFolder\\%s", app_id);
    if (written < 0 || (size_t) written >= sizeof(text) ||
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, path, (int) (sizeof(path) / sizeof(path[0]))) == 0)
        return E_INVALIDARG;
    IShellItem *item = NULL;
    HRESULT hr = SHCreateItemFromParsingName(path, NULL, &IID_IShellItem, (void **) &item);
    if (SUCCEEDED(hr))
        IShellItem_Release(item);
    return hr;
}

// A function to read the shell's own bitmap for an app, asked for as appicon_image() asks for it: its
// pixels, the top row first, which the caller frees; NULL when no image came
static Uint32 *shell_bitmap(const char *app_id, int *width, int *height)
{
    char text[1100];
    wchar_t path[1100];
    snprintf(text, sizeof(text), "shell:AppsFolder\\%s", app_id);
    if (MultiByteToWideChar(CP_UTF8, 0, text, -1, path, (int) (sizeof(path) / sizeof(path[0]))) == 0)
        return NULL;
    HRESULT started = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    IShellItemImageFactory *factory = NULL;
    HBITMAP bitmap = NULL;
    if (SUCCEEDED(SHCreateItemFromParsingName(path, NULL, &IID_IShellItemImageFactory, (void **) &factory))) {
        SIZE want = { 256, 256 };
        if (FAILED(IShellItemImageFactory_GetImage(factory, want, SIIGBF_ICONONLY, &bitmap)))
            bitmap = NULL;
        IShellItemImageFactory_Release(factory);
    }
    Uint32 *bits = NULL;
    BITMAP info;
    if (bitmap != NULL && GetObjectW(bitmap, (int) sizeof(info), &info) != 0 && info.bmWidth > 0 && info.bmHeight != 0) {
        *width = info.bmWidth;
        *height = info.bmHeight < 0 ? -info.bmHeight : info.bmHeight;
        bits = malloc((size_t) *width * (size_t) *height * sizeof(Uint32));
        BITMAPINFO header;
        memset(&header, 0, sizeof(header));
        header.bmiHeader.biSize = (DWORD) sizeof(BITMAPINFOHEADER);
        header.bmiHeader.biWidth = *width;
        header.bmiHeader.biHeight = -*height;
        header.bmiHeader.biPlanes = 1;
        header.bmiHeader.biBitCount = 32;
        header.bmiHeader.biCompression = BI_RGB;
        HDC screen = GetDC(NULL);
        if (bits != NULL && GetDIBits(screen, bitmap, 0, (UINT) *height, bits, &header, DIB_RGB_COLORS) != *height) {
            free(bits);
            bits = NULL;
        }
        ReleaseDC(NULL, screen);
    }
    if (bitmap != NULL)
        DeleteObject(bitmap);
    if (SUCCEEDED(started))
        CoUninitialize();
    return bits;
}

// A function to judge a bitmap's alpha by its partly transparent pixels, the only ones the question is
// about: 1 when a color is above its alpha in one of them (straight alpha; a premultiplied bitmap cannot
// hold one), 0 when every such color is at most its alpha (premultiplied, or straight with dark edges), -1
// when no pixel is partly transparent (there is nothing to judge by). A fully transparent pixel's color
// says nothing: GDI leaves stray color in some, in a premultiplied bitmap too.
static int judge_alpha(const Uint32 *bits, size_t count)
{
    bool partial = false;
    bool above = false;
    for (size_t i = 0; i < count; i++) {
        Uint32 alpha = bits[i] >> 24;
        bool part = alpha > 0 && alpha < 255;
        partial = partial || part;
        above = above || (part && (((bits[i] >> 16) & 0xFF) > alpha || ((bits[i] >> 8) & 0xFF) > alpha || (bits[i] & 0xFF) > alpha));
    }
    return !partial ? -1 : above ? 1 : 0;
}

// A function to test the alpha judge on bitmaps made here: a premultiplied one with stray color at a
// fully transparent pixel, which GDI's mask conversions leave, still reads as premultiplied; a straight one
// reads as straight; one with no partly transparent pixel cannot be judged
static void test_judge(void)
{
    static const Uint32 stray[3] = { 0x80402010u, 0x00FFFFFFu, 0xFFFFFFFFu };
    static const Uint32 straight_edge[2] = { 0x80FF2010u, 0xFFFFFFFFu };
    static const Uint32 none_partial[2] = { 0x00FFFFFFu, 0xFF102030u };
    CHECK_INT(judge_alpha(stray, 3), 0);
    CHECK_INT(judge_alpha(straight_edge, 2), 1);
    CHECK_INT(judge_alpha(none_partial, 2), -1);
}

// A function to judge the shell's own bitmap for an app, as judge_alpha() does; -1 when no image came. At
// 1, `same` says whether appicon_image()'s surface for the app holds the bitmap's pixels as they are.
static int straight_alpha(const char *app_id, bool *same)
{
    int width = 0;
    int height = 0;
    Uint32 *bits = shell_bitmap(app_id, &width, &height);
    *same = false;
    if (bits == NULL)
        return -1;
    int verdict = judge_alpha(bits, (size_t) width * (size_t) height);
    if (verdict == 1) {
        SDL_Surface *image = appicon_image(app_id, 256);
        *same = image != NULL && image->w == width && image->h == height;
        for (int y = 0; y < height && *same; y++) {
            for (int x = 0; x < width && *same; x++)
                *same = pixel_at(image, x, y) == bits[(size_t) y * (size_t) width + (size_t) x];
        }
        SDL_DestroySurface(image);
    }
    free(bits);
    return verdict;
}

int main(void)
{
    test_pixels();
    test_judge();
    test_refusals();

    // The scan, on this thread: apps, each with a name and an ID the shell finds the app by
    SDL_AtomicInt found;
    SDL_AtomicInt stop;
    SDL_SetAtomicInt(&found, 0);
    SDL_SetAtomicInt(&stop, 0);
    AppList *list = applist_create();
    CHECK(list != NULL);
    bool scanned = list != NULL && appsfolder_scan(list, &found, &stop);
    CHECK(scanned);
    if (!scanned)
        fprintf(stderr, "appsfolder_scan: %s\n", SDL_GetError());
    int count = list != NULL ? applist_count(list) : 0;
    printf("AppsFolder: %d apps\n", count);
    CHECK(count > 0);
    CHECK_INT(SDL_GetAtomicInt(&found), count);
    CHECK(list != NULL && !applist_failed(list));
    // Every listed app's ID names an item the shell finds, as the launch asks it to; COM is started for
    // the parsing and ended after it
    int bad = 0;
    HRESULT started = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    for (int i = 0; i < count; i++) {
        const AppItem *item = applist_item(list, i);
        const char *id = item->icon != NULL ? item->icon : "none";
        HRESULT parsed = item->icon != NULL ? parse_app(item->icon) : E_POINTER;
        if (item->name[0] == '\0') {
            bad++;
            fprintf(stderr, "app %d (ID '%s') has no name\n", i, id);
        }
        if (FAILED(parsed)) {
            bad++;
            fprintf(stderr, "app %d: '%s': the shell does not find its ID '%s' (error 0x%08lX)\n", i, item->name, id,
                    (unsigned long) parsed);
        }
        if (i < 5)
            printf("  %s: %s\n", item->name, item->command);
    }
    if (SUCCEEDED(started))
        CoUninitialize();
    CHECK_INT(bad, 0);
    CHECK(com_ended());

    // Asked to stop before its first item, as closing settings asks mid-scan: it lists nothing, counts
    // nothing, and that is no failure
    SDL_SetAtomicInt(&stop, 1);
    SDL_SetAtomicInt(&found, 0);
    AppList *stopped = applist_create();
    CHECK(stopped != NULL);
    CHECK(stopped != NULL && appsfolder_scan(stopped, &found, &stop));
    CHECK_INT(stopped != NULL ? applist_count(stopped) : -1, 0);
    CHECK_INT(SDL_GetAtomicInt(&found), 0);
    applist_free(stopped);

    // An image: the first app's whose image comes, at most 256 px a side, straight ARGB, not empty
    SDL_Surface *image = NULL;
    const AppItem *imaged = NULL;
    for (int i = 0; i < count && i < TRIES && image == NULL; i++) {
        imaged = applist_item(list, i);
        image = appicon_image(imaged->icon, 256);
    }
    CHECK(image != NULL);
    if (image != NULL) {
        printf("Image of %s: %d x %d\n", imaged->name, image->w, image->h);
        CHECK(image->format == SDL_PIXELFORMAT_ARGB8888);
        CHECK(image->w > 0 && image->w <= 256 && image->h > 0 && image->h <= 256);
        bool seen = false;
        for (int y = 0; y < image->h && !seen; y++) {
            for (int x = 0; x < image->w && !seen; x++)
                seen = (pixel_at(image, x, y) >> 24) != 0;
        }
        CHECK(seen);
        SDL_DestroySurface(image);
    }
    CHECK(com_ended());

    // The shell's bitmap is straight alpha, and appicon_image() keeps its pixels as they are: an app among
    // the first TRIES whose image holds a color above its alpha (a premultiplied bitmap never does; a
    // straight icon with dark edges shows none, so the search goes on past it), its surface the same pixels
    int verdict = -1;
    bool same = false;
    const AppItem *judged = NULL;
    for (int i = 0; i < count && i < TRIES && verdict < 1; i++) {
        judged = applist_item(list, i);
        int seen = straight_alpha(judged->icon, &same);
        verdict = seen > verdict ? seen : verdict;
    }
    if (verdict == 1)
        printf("Straight alpha: %s, its surface %s\n", judged->name, same ? "the same pixels" : "different pixels");
    else if (verdict == 0)
        fprintf(stderr, "The shell's bitmap reads as premultiplied: no color above its alpha in the first %d apps' images\n", TRIES);
    else
        fprintf(stderr, "No image to judge the alpha by: none of the first %d apps' images has partly transparent pixels\n", TRIES);
    CHECK_INT(verdict, 1);
    CHECK(verdict != 1 || same);

    // An ID the folder does not have: no image, and why; nor for an ID that is not UTF-8
    SDL_ClearError();
    CHECK(appicon_image("StreamFlex.Test.NoSuchApp!App", 256) == NULL);
    CHECK(SDL_GetError()[0] != '\0');
    CHECK(appicon_image("\xFF", 256) == NULL);
    CHECK(strstr(SDL_GetError(), "UTF-8") != NULL);

    // A web link is no app: refused before the shell is asked (asking can reach the Internet Shortcut
    // code, which set off a Controlled Folder Access block on Favorites), whatever the case of ".url"
    CHECK(appicon_image("https://streamflex.invalid/", 256) == NULL);
    CHECK(strstr(SDL_GetError(), "web link") != NULL);
    CHECK(appicon_image("C:\\StreamFlex\\No such link.URL", 256) == NULL);
    CHECK(strstr(SDL_GetError(), "web link") != NULL);

    // The scan as settings run it: its own thread, COM started there, the list sorted and taken
    AppScan *scan = appscan_start();
    CHECK(scan != NULL);
    int listed = 0;
    bool hung = false;
    Uint64 began = SDL_GetTicks();
    while (scan != NULL && !appscan_poll(scan, &listed)) {
        if (SDL_GetTicks() - began > HANG_GUARD_MS) {
            fprintf(stderr, "HUNG: the app scan's thread had not finished after %d s, with %d apps listed\n",
                    HANG_GUARD_MS / 1000, listed);
            hung = true;
            break;
        }
        SDL_Delay(10);
    }
    CHECK(!hung);
    if (scan != NULL && !hung) {
        CHECK(appscan_why(scan) == NULL);
        AppList *taken = appscan_take(scan);
        CHECK(taken != NULL && applist_count(taken) == count);
        applist_free(taken);
        appscan_free(scan);
    }
    applist_free(list);
    return check_report();
}
