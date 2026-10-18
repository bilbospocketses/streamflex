#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "iconfind.h"
#include "fileio.h"

#define PATH_BYTES 4096
#define NAME_BYTES 256

// The extensions an Icon= name may carry, though the Desktop Entry Specification asks for none; the
// pixmaps folders are looked in for each, in this order
static const char *const EXTENSIONS[] = { ".png", ".svg", ".xpm", NULL };

// A function to tell whether a path is absolute: from the root, a share, or a drive
static bool is_absolute(const char *path)
{
    bool drive = ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':' &&
                 fileio_is_separator(path[2]);
    return fileio_is_separator(path[0]) || drive;
}

// A function to tell whether snprintf() wrote its whole text: a path cut short names another file
static bool fits(int written, size_t size)
{
    return written >= 0 && (size_t) written < size;
}

// A function to copy a path found into the caller's buffer; false when it does not fit
static bool give(const char *path, char *out, size_t size)
{
    size_t length = strlen(path);
    if (length + 1 > size)
        return false;
    memcpy(out, path, length + 1);
    return true;
}

// A function to read a hicolor size folder's name, "<N>x<N>": N, or -1 for any other name (scalable, a
// HiDPI "256x256@2", a size that is not square)
static int square_size(const char *name)
{
    const char *p = name;
    int n = 0;
    int m = 0;
    if (*p < '0' || *p > '9')
        return -1;
    for (; *p >= '0' && *p <= '9'; p++) {
        if (n > 100000)
            return -1;
        n = n * 10 + (*p - '0');
    }
    if (*p != 'x' || p[1] < '0' || p[1] > '9')
        return -1;
    for (p++; *p >= '0' && *p <= '9'; p++) {
        if (m > 100000)
            return -1;
        m = m * 10 + (*p - '0');
    }
    return *p == '\0' && n == m && n > 0 ? n : -1;
}

// A function to drop an Icon= name's .png, .svg or .xpm, in any case; other dots stay (org.gnome.Calculator)
static void strip_extension(char *name)
{
    size_t length = strlen(name);
    for (int i = 0; EXTENSIONS[i] != NULL; i++) {
        size_t end = strlen(EXTENSIONS[i]);
        if (length <= end)
            continue;
        bool same = true;
        for (size_t k = 0; k < end && same; k++)
            same = fileio_lower(name[length - end + k]) == EXTENSIONS[i][k];
        if (same) {
            name[length - end] = '\0';
            return;
        }
    }
}

// A function to find the file a .desktop file's Icon= value names (see iconfind.h). In the hicolor theme
// the largest <N>x<N> PNG across the folders is kept (the first folder's on a tie) beside the first
// folder's scalable SVG; the PNG wins when it is ICONFIND_GOOD_SIZE or larger, else the SVG, else the PNG
// of any size. Then each folder's pixmaps, folder by folder, .png, .svg, then .xpm. False when nothing is
// found, or the path does not fit `out`.
bool iconfind_path(const char *icon_value, const char *const *dirs, int dir_count, char *out, size_t size)
{
    if (icon_value == NULL || icon_value[0] == '\0')
        return false;
    if (is_absolute(icon_value))
        return fileio_is_file(icon_value) && give(icon_value, out, size);
    size_t length = strlen(icon_value);
    if (strchr(icon_value, '/') != NULL || strchr(icon_value, '\\') != NULL || length >= NAME_BYTES)
        return false;
    char name[NAME_BYTES];
    memcpy(name, icon_value, length + 1);
    strip_extension(name);

    char png[PATH_BYTES] = "";
    char svg[PATH_BYTES] = "";
    char path[PATH_BYTES];
    int png_size = 0;
    for (int d = 0; d < dir_count; d++) {
        char theme[PATH_BYTES];
        if (!fits(snprintf(theme, sizeof(theme), "%s/icons/hicolor", dirs[d]), sizeof(theme)))
            continue;
        FileioEntry *entries = NULL;
        int count = fileio_list(theme, &entries);
        for (int i = 0; i < count; i++) {
            int n = entries[i].is_dir ? square_size(entries[i].name) : -1;
            if (n > png_size && fits(snprintf(path, sizeof(path), "%s/%s/apps/%s.png", theme, entries[i].name, name), sizeof(path)) &&
                fileio_is_file(path)) {
                png_size = n;
                memcpy(png, path, strlen(path) + 1);
            }
        }
        fileio_free_list(entries, count > 0 ? count : 0);
        if (svg[0] == '\0' && fits(snprintf(path, sizeof(path), "%s/scalable/apps/%s.svg", theme, name), sizeof(path)) &&
            fileio_is_file(path))
            memcpy(svg, path, strlen(path) + 1);
    }
    if (png_size >= ICONFIND_GOOD_SIZE || (png_size > 0 && svg[0] == '\0'))
        return give(png, out, size);
    if (svg[0] != '\0')
        return give(svg, out, size);
    for (int d = 0; d < dir_count; d++) {
        for (int e = 0; EXTENSIONS[e] != NULL; e++) {
            if (fits(snprintf(path, sizeof(path), "%s/pixmaps/%s%s", dirs[d], name, EXTENSIONS[e]), sizeof(path)) &&
                fileio_is_file(path))
                return give(path, out, size);
        }
    }
    return false;
}
