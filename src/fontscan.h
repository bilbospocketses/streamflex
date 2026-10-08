// Finding the installed font files for the font picker, on a thread of their own: on Windows the
// Fonts registry keys (the machine's and the user's), on Linux the system and user font folders,
// searched folder by folder; and the bundled fonts' folder first on both. The thread only lists
// files: SDL_ttf's one FreeType library must not open fonts on two threads at once, so the main
// thread reads the faces (settings_fonts.c).
#ifndef FONTSCAN_H
#define FONTSCAN_H

#include <stdbool.h>

typedef struct FontScan FontScan;

FontScan *fontscan_start(const char *bundled_folder);  // Lists font files on a thread
bool fontscan_done(FontScan *scan);                    // True once listed; the thread is then waited for
bool fontscan_failed(const FontScan *scan);            // A file could not be added: the list is short
int fontscan_count(const FontScan *scan);              // These four only once it is done
const char *fontscan_file(const FontScan *scan, int index);
bool fontscan_bundled(const FontScan *scan, int index);
void fontscan_free(FontScan *scan);                    // Waits for the thread

#endif
