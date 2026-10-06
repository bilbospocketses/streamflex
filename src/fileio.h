// File access with UTF-8 paths on every platform. Windows' narrow file APIs read a path in the
// system code page, so there every call converts it to UTF-16 and uses the wide API. Pure: no
// SDL, no launcher headers, so tests/test_fileio.c builds it on its own. Memory comes from alloc.h.
// A call that fails says why in fileio_last_error(), which each thread keeps for itself.
#ifndef FILEIO_H
#define FILEIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define FILEIO_REPLACE_ATTEMPTS 10   // Windows: how often a held file is retried...
#define FILEIO_REPLACE_WAIT_MS 100   // ...and how long apart: about a second in all

typedef struct {
    char *name;   // UTF-8 name of the file or folder, without its folder
    bool is_dir;
    bool hidden;  // Windows: the hidden or system attribute; elsewhere: the name starts with '.'
} FileioEntry;

FILE *fileio_open(const char *path, const char *mode);
bool fileio_exists(const char *path);    // exists and can be read
bool fileio_present(const char *path);   // exists, whether or not it can be read
bool fileio_is_dir(const char *path);
bool fileio_is_writable(const char *path);
char *fileio_read_all(const char *path, size_t *length);
bool fileio_write_all(const char *path, const char *data, size_t length);
bool fileio_copy(const char *from, const char *to);
bool fileio_replace(const char *from, const char *to);
bool fileio_remove(const char *path);
bool fileio_make_dirs(const char *path);   // Windows: a share alone (\\server\share) is not looked at,
                                           // since looking can wait on the network, so true there does
                                           // not say the share exists; what is done there next says so
bool fileio_real_path(const char *path, char *out, size_t size);   // on Linux, follows symbolic links; on Windows, the path as given
bool fileio_full_path(const char *path, char *out, size_t size);   // absolute: on Linux the real path; on Windows from the working folder
int fileio_list(const char *folder, FileioEntry **entries);
void fileio_free_list(FileioEntry *entries, int count);
const char *fileio_last_error(void);
const char *fileio_last_warning(void);   // What the last replace could not keep, though it succeeded; "" if nothing
bool fileio_is_separator(char c);        // '/' or '\', on every platform
int fileio_lower(char c);                // An ASCII capital in lower case; any other byte as it is (0-255)
void fileio_base_name(const char *path, char *out, size_t size);   // The last name in a path, ignoring a trailing separator

#ifdef _WIN32
#include <wchar.h>
wchar_t *fileio_wide(const char *text);   // For other Windows calls that take a path or command
#endif

typedef struct {
    char *label;  // What the browser shows: "Pictures", "Home", "C:", "/", a mount's name
    char *path;
    bool network; // It may be on a network share (a network drive, or anything in /media or /mnt), so it
                  // was listed without being looked at, and is never opened unasked
} FileioPlace;

int fileio_places(FileioPlace **places);   // -1, with the reason, when finding them fails: never a list with a place missing
void fileio_free_places(FileioPlace *places, int count);
#ifndef _WIN32
int fileio_places_under(const char *folder, FileioPlace **places);   // /media's mounts, none looked at
void fileio_set_mount_table(const char *path);   // Unit tests only: a pretend /proc/self/mounts; NULL for the real one
#endif

// Unit tests only: a step made to fail on purpose, where no real file can be made to fail it
typedef enum {
    FILEIO_FAULT_NONE,
    FILEIO_FAULT_LIST_READ,   // fileio_list's read fails after `after` entries, with error `code`
    FILEIO_FAULT_KEEP,        // fileio_replace cannot keep the old file's permissions or attributes
    FILEIO_FAULT_NO_KIND      // Linux: fileio_list's reads give no entry's kind, as some file systems give none
} FileioFault;
void fileio_set_fault(FileioFault fault, int after, int code);

#endif
