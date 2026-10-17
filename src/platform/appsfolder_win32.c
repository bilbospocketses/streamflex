#define COBJMACROS   // IShellItem_GetDisplayName() and the rest, in C
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <SDL3/SDL.h>
#include "appsfolder.h"
#include "../applist.h"
#include "../fileio.h"
#include "../alloc.h"

#define NAME_BYTES 1024
#define ID_BYTES 1024
#define COMMAND_BYTES 1200

// The AppsFolder known folder (knownfolders.h FOLDERID_AppsFolder), the handler that lists a folder's
// items (shlguid.h BHID_EnumItems) and the app's AppUserModelID (propkey.h PKEY_AppUserModel_ID), as the
// SDK defines them; defined here, so no INITGUID and no library has to carry them
static const GUID APPS_FOLDER = { 0x1E87508D, 0x89C2, 0x42F0, { 0x8A, 0x7E, 0x64, 0x5A, 0x0F, 0x50, 0xCA, 0x58 } };
static const GUID ENUM_ITEMS = { 0x94F60519, 0x2850, 0x4924, { 0xAA, 0x5A, 0xD1, 0x5E, 0x84, 0x86, 0x80, 0x39 } };
static const PROPERTYKEY APP_USER_MODEL_ID = { { 0x9F4C2855, 0x9F79, 0x4B39, { 0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3 } }, 5 };

// A function to turn a UTF-16 string into UTF-8, into a buffer; false when it does not fit
static bool to_utf8(const wchar_t *wide, char *out, int size)
{
    return WideCharToMultiByte(CP_UTF8, 0, wide, -1, out, size, NULL, NULL) > 0;
}

// A function to get an AppsFolder item's app ID: its AppUserModelID, else its name in the folder, which
// for an app with none is the ID Windows made for it. NULL when it has neither; CoTaskMemFree() frees it.
static wchar_t *app_id_of(IShellItem *item)
{
    wchar_t *id = NULL;
    IShellItem2 *item2 = NULL;
    if (SUCCEEDED(IShellItem_QueryInterface(item, &IID_IShellItem2, (void **) &item2))) {
        if (FAILED(IShellItem2_GetString(item2, &APP_USER_MODEL_ID, &id)))
            id = NULL;
        IShellItem2_Release(item2);
    }
    if (id != NULL && id[0] != L'\0')
        return id;
    CoTaskMemFree(id);
    id = NULL;
    if (FAILED(IShellItem_GetDisplayName(item, SIGDN_PARENTRELATIVEPARSING, &id)))
        id = NULL;
    return id;
}

// A function to add one AppsFolder item to the list: its display name, the command that launches it,
// and its app ID, which appicon_image() finds its image by. An item the list leaves out (an uninstaller,
// a document) or that cannot be written adds nothing; out of memory, the list says so itself.
static void add_item(AppList *list, IShellItem *item, SDL_AtomicInt *found)
{
    wchar_t *name = NULL;
    if (FAILED(IShellItem_GetDisplayName(item, SIGDN_NORMALDISPLAY, &name)))
        return;
    wchar_t *id = app_id_of(item);
    char name_text[NAME_BYTES];
    char id_text[ID_BYTES];
    char command[COMMAND_BYTES];
    if (id != NULL && to_utf8(name, name_text, (int) sizeof(name_text)) && to_utf8(id, id_text, (int) sizeof(id_text)) &&
        applist_appsfolder_command(id_text, command, sizeof(command)) && applist_add(list, name_text, command, id_text))
        SDL_AddAtomicInt(found, 1);
    CoTaskMemFree(name);
    CoTaskMemFree(id);
}

// A function to list the shell's AppsFolder into the list, on the scan's thread, counting each app added
// in `found` as it goes, and ending at the next item once `stop` is set (settings closed mid-scan), true
// with what it listed. COM is started here, apartment-threaded, and ended here: nothing else started it
// on this thread. False, with SDL's error saying why, when the folder cannot be opened or listed, or its
// listing fails part way (an error from Next(), not its S_FALSE at the end): a short list would misstate
// the apps, as appscan.c's finish() judges a Linux scan short of memory.
bool appsfolder_scan(AppList *list, SDL_AtomicInt *found, SDL_AtomicInt *stop)
{
    HRESULT started = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    IShellItem *folder = NULL;
    IEnumShellItems *items = NULL;
    HRESULT hr = SHGetKnownFolderItem(&APPS_FOLDER, KF_FLAG_DEFAULT, NULL, &IID_IShellItem, (void **) &folder);
    if (SUCCEEDED(hr)) {
        hr = IShellItem_BindToHandler(folder, NULL, &ENUM_ITEMS, &IID_IEnumShellItems, (void **) &items);
        IShellItem_Release(folder);
    }
    if (SUCCEEDED(hr)) {
        IShellItem *item = NULL;
        HRESULT next = S_OK;   // Stays S_OK when a stop ends the loop before it asks again
        while (SDL_GetAtomicInt(stop) == 0 && (next = IEnumShellItems_Next(items, 1, &item, NULL)) == S_OK) {
            add_item(list, item, found);
            IShellItem_Release(item);
        }
        IEnumShellItems_Release(items);
        if (FAILED(next)) {
            hr = next;
            SDL_SetError("Windows could not list its apps folder (error 0x%08lX)", (unsigned long) next);
        }
    }
    else
        SDL_SetError("Windows could not open its apps folder (error 0x%08lX)", (unsigned long) hr);
    if (SUCCEEDED(started))
        CoUninitialize();
    return SUCCEEDED(hr);
}

// A function to make one of the shell's pixels a surface's: kept as it is, since the shell's icon bitmap
// is straight alpha already; in an image with no alpha at all, opaque
static Uint32 straight(Uint32 pixel, bool has_alpha)
{
    return has_alpha ? pixel : 0xFF000000u | (pixel & 0xFFFFFFu);
}

// A function to copy the shell's 32-bit pixels (the top row first) into an ARGB8888 surface of their size.
// The shell's icon bitmap is straight alpha, as a surface holds it and plate_icon() and IMG_SavePNG() read
// it, so its pixels are copied as they are: measured on Windows 10.0.26300, 195 of 221 apps' icons
// (Calculator's among them) hold a color above its alpha, which a premultiplied bitmap cannot. An image
// with no alpha at all (an old icon) is opaque.
void appicon_pixels(const Uint32 *bits, SDL_Surface *surface)
{
    size_t count = (size_t) surface->w * (size_t) surface->h;
    bool has_alpha = false;
    for (size_t i = 0; i < count && !has_alpha; i++)
        has_alpha = (bits[i] >> 24) != 0;
    for (int y = 0; y < surface->h; y++) {
        Uint32 *row = (Uint32 *) ((Uint8 *) surface->pixels + (size_t) y * (size_t) surface->pitch);
        for (int x = 0; x < surface->w; x++)
            row[x] = straight(bits[(size_t) y * (size_t) surface->w + (size_t) x], has_alpha);
    }
}

// A function to copy the shell's 32-bit bitmap into a surface, its pixels as appicon_pixels() copies them.
// NULL, with SDL's error set, when it cannot be read.
static SDL_Surface *surface_of(HBITMAP bitmap)
{
    BITMAP info;
    if (GetObjectW(bitmap, (int) sizeof(info), &info) == 0 || info.bmWidth <= 0 || info.bmHeight == 0) {
        SDL_SetError("Windows gave an image that cannot be read");
        return NULL;
    }
    int width = info.bmWidth;
    int height = info.bmHeight < 0 ? -info.bmHeight : info.bmHeight;
    size_t count = (size_t) width * (size_t) height;
    Uint32 *bits = malloc(count * sizeof(Uint32));
    SDL_Surface *surface = bits != NULL ? SDL_CreateSurface(width, height, SDL_PIXELFORMAT_ARGB8888) : NULL;
    if (surface == NULL) {
        if (bits == NULL)
            SDL_OutOfMemory();
        free(bits);
        return NULL;
    }
    BITMAPINFO header;
    memset(&header, 0, sizeof(header));
    header.bmiHeader.biSize = (DWORD) sizeof(BITMAPINFOHEADER);
    header.bmiHeader.biWidth = width;
    header.bmiHeader.biHeight = -height;   // The top row first, as a surface holds its rows
    header.bmiHeader.biPlanes = 1;
    header.bmiHeader.biBitCount = 32;
    header.bmiHeader.biCompression = BI_RGB;
    HDC screen = GetDC(NULL);
    int rows = GetDIBits(screen, bitmap, 0, (UINT) height, bits, &header, DIB_RGB_COLORS);
    ReleaseDC(NULL, screen);
    if (rows != height) {
        free(bits);
        SDL_DestroySurface(surface);
        SDL_SetError("Windows could not copy the image's pixels");
        return NULL;
    }
    appicon_pixels(bits, surface);
    free(bits);
    return surface;
}

// A function to get an AppsFolder app's own image (spec "Icons", choice 2), from its app ID: the icon at
// size x size or less (IShellItemImageFactory, icon only), as an ARGB8888 surface with straight alpha. Run
// on demand on the caller's thread, for the one app whose icon is being chosen; COM is started for the
// call and ended after it when this call started it. NULL, with SDL's error saying why, when there is none.
SDL_Surface *appicon_image(const char *app_id, int size)
{
    char path[ID_BYTES + 32];
    int written = app_id != NULL ? snprintf(path, sizeof(path), "shell:AppsFolder\\%s", app_id) : -1;
    if (app_id == NULL || app_id[0] == '\0' || written < 0 || (size_t) written >= sizeof(path) || size <= 0) {
        SDL_SetError("There is no app ID to get an image for");
        return NULL;
    }
    // A web link (an ID holding "://" or naming a .url file) is no app and gets no image: imaging every
    // AppsFolder item, its web links among them, set off a Controlled Folder Access block on the Favorites
    // folder (Defender event 1123, D:\OneDrive\Favorites, 2026-10-16). applist leaves these IDs out
    // already, so no listed app is refused.
    size_t length = strlen(app_id);
    if (strstr(app_id, "://") != NULL || (length >= 4 && SDL_strcasecmp(app_id + length - 4, ".url") == 0)) {
        SDL_SetError("%s is a web link, which has no app image", app_id);
        return NULL;
    }
    wchar_t *wide = fileio_wide(path);
    if (wide == NULL) {
        SDL_SetError("%s", fileio_last_error());
        return NULL;
    }
    HRESULT started = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    IShellItemImageFactory *factory = NULL;
    HBITMAP bitmap = NULL;
    HRESULT hr = SHCreateItemFromParsingName(wide, NULL, &IID_IShellItemImageFactory, (void **) &factory);
    if (SUCCEEDED(hr)) {
        SIZE want = { size, size };
        hr = IShellItemImageFactory_GetImage(factory, want, SIIGBF_ICONONLY, &bitmap);
        IShellItemImageFactory_Release(factory);
    }
    SDL_Surface *image = NULL;
    if (SUCCEEDED(hr))
        image = surface_of(bitmap);
    else
        SDL_SetError("Windows has no image for %s (error 0x%08lX)", app_id, (unsigned long) hr);
    if (bitmap != NULL)
        DeleteObject(bitmap);
    if (SUCCEEDED(started))
        CoUninitialize();
    alloc_free(wide);
    return image;
}
