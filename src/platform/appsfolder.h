// The shell's AppsFolder on Windows (spec "Installed apps"): every app the Start menu lists, desktop and
// Store apps alike, and an app's own image (spec "Icons"). A file apart from win32.c, which needs the
// launcher, so the Windows unit test links these two alone.
#ifndef APPSFOLDER_H
#define APPSFOLDER_H

#include <stdbool.h>
#include <SDL3/SDL.h>
#include "../applist.h"

bool appsfolder_scan(AppList *list, SDL_AtomicInt *found, SDL_AtomicInt *stop);   // On the scan's thread; ends at the next item once *stop is set; false with SDL's error set
SDL_Surface *appicon_image(const char *app_id, int size);    // ARGB8888, straight alpha, at most size x size; NULL with SDL's error set
void appicon_pixels(const Uint32 *bits, SDL_Surface *surface);   // appicon_image()'s pixel copy, declared for the unit test: the shell's pixels into an ARGB8888 surface of their size, as they are; no alpha anywhere is opaque

#endif
