// The folder browser behind the settings screen's Image, Folder and Mappings file rows: a folder's
// rows, a cursor, and moving between folders and the places it can start from. It lists folders
// through a function it is given, so the tests hand it a pretend file system; paths in either style
// ("/home/me", "C:\Users\me", "\\server\share") work on any platform. Pure: no SDL, no globals;
// memory comes from alloc.h.
#ifndef BROWSER_H
#define BROWSER_H

#include <stdbool.h>
#include <stddef.h>
#include "fileio.h"

#define BROWSER_PATH_MAX 1024

typedef enum {
    BROWSER_IMAGE,   // Choosing one image
    BROWSER_FOLDER,  // Choosing a folder of images (a slideshow)
    BROWSER_FILE     // Choosing one file of any kind
} BrowserMode;

typedef enum {
    BROWSER_ROW_PLACE,
    BROWSER_ROW_USE_FOLDER,
    BROWSER_ROW_FOLDER,
    BROWSER_ROW_IMAGE,
    BROWSER_ROW_FILE
} BrowserRowKind;

typedef struct {
    BrowserRowKind kind;
    char *name;          // What the row shows
    char *path;          // The full path it stands for (the folder on show for USE_FOLDER)
    bool enabled;        // False: shown, but OK does nothing
    const char *why;     // Why a row is disabled, when it says; NULL otherwise. Not a copy: see BrowserCheck
    int image_count;     // USE_FOLDER: the images in the folder on show
} BrowserRow;

typedef struct {
    const char *label;
    const char *path;
    bool network;        // It may be on a network share (FileioPlace's flag): never opened unasked, since
                         // it could keep the browser waiting
} BrowserPlace;

typedef int (*BrowserList)(const char *folder, FileioEntry **entries, void *context);

// Says why config.ini cannot hold a path, or NULL when it can. The reason is kept in the rows as it
// is given, so it must outlive them: a string literal, or one the caller keeps while the browser is open.
typedef const char *(*BrowserCheck)(const char *path, void *context);

typedef enum {
    BROWSER_UP,
    BROWSER_DOWN,
    BROWSER_PAGE_UP,
    BROWSER_PAGE_DOWN,
    BROWSER_OK,
    BROWSER_BACK
} BrowserCommand;

typedef enum {
    BROWSER_NONE,
    BROWSER_MOVED,
    BROWSER_CHOSEN,
    BROWSER_CLOSED
} BrowserResult;

typedef struct Browser Browser;

// NULL when the browser cannot be opened, with the reason in *why when `why` is not NULL
Browser *browser_open(BrowserMode mode, const char *start, const BrowserPlace *places, int place_count,
                      BrowserList list, BrowserCheck check, void *context, const char **why);
void browser_free(Browser *browser);
BrowserResult browser_command(Browser *browser, BrowserCommand command, int page_rows);
int browser_row_count(const Browser *browser);
const BrowserRow *browser_row(const Browser *browser, int index);
int browser_cursor(const Browser *browser);
const char *browser_folder(const Browser *browser);
const char *browser_chosen(const Browser *browser);
const char *browser_why(const Browser *browser);
bool browser_first_image(const Browser *browser, const char *folder, char *out, size_t size);
bool browser_is_image(const char *name);
bool browser_is_image_file(const FileioEntry *entry);   // The one rule for every image scan
bool browser_parent(const char *path, char *out, size_t size);

#endif
