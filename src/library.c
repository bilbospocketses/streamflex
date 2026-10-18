#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <ini.h>
#include "library.h"
#include "fileio.h"

typedef struct {
    char name[LIBRARY_NAME_MAX + 1];
    char *file;       // as written in the manifest
    char *path;       // the full path once the file is checked; NULL if the icon was rejected
    char *title;      // NULL when the manifest gives none
    char *group;
    char **match;     // the match = aliases, trimmed
    int match_count;
} LibraryIcon;

static LibraryIcon *icons = NULL;
static int icon_count = 0;
static int icon_capacity = 0;
static LibraryInfo *infos = NULL;   // the icons kept, in the manifest's order, for library_info()
static int info_count = 0;
static LibraryWarn warn_function = NULL;
static char *parsing = NULL;   // the manifest section being read
static int target = -1;        // its index in icons, or -1 while its keys are skipped

// A function to format a message and pass it to the warn function, if one is set
static void warn(const char *format, ...)
{
    if (warn_function == NULL)
        return;
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    warn_function(message);
}

void library_set_warn(LibraryWarn function)
{
    warn_function = function;
}

// A function to tell a library icon name (1-32 of a-z, 0-9 and '-') from a file path
bool library_is_name(const char *field)
{
    if (field == NULL)
        return false;
    size_t length = strlen(field);
    if (length == 0 || length > LIBRARY_NAME_MAX)
        return false;
    for (size_t i = 0; i < length; i++) {
        char c = field[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
            return false;
    }
    return true;
}

static int find_icon(const char *name)
{
    for (int i = 0; i < icon_count; i++) {
        if (!strcmp(icons[i].name, name))
            return i;
    }
    return -1;
}

static int add_icon(const char *name)
{
    if (icon_count == icon_capacity) {
        int capacity = icon_capacity ? icon_capacity * 2 : 64;
        LibraryIcon *grown = realloc(icons, (size_t) capacity * sizeof(LibraryIcon));
        if (grown == NULL)
            return -1;
        icons = grown;
        icon_capacity = capacity;
    }
    LibraryIcon *icon = &icons[icon_count];
    snprintf(icon->name, sizeof(icon->name), "%s", name);
    icon->file = NULL;
    icon->path = NULL;
    icon->title = NULL;
    icon->group = NULL;
    icon->match = NULL;
    icon->match_count = 0;
    return icon_count++;
}

// A function to add an icon's match = aliases: a comma-separated list, each alias trimmed of spaces and an
// empty one skipped; a repeated match key adds to them. Out of memory, an alias is left out.
static void add_aliases(LibraryIcon *icon, const char *value)
{
    const char *part = value;
    while (*part != '\0') {
        size_t length = strcspn(part, ",");
        const char *start = part;
        const char *end = part + length;
        while (start < end && isspace((unsigned char) *start))
            start++;
        while (end > start && isspace((unsigned char) end[-1]))
            end--;
        if (end > start) {
            char **grown = realloc(icon->match, (size_t) (icon->match_count + 1) * sizeof(char*));
            if (grown != NULL)
                icon->match = grown;
            char *alias = grown != NULL ? malloc((size_t) (end - start) + 1) : NULL;
            if (alias != NULL) {
                memcpy(alias, start, (size_t) (end - start));
                alias[end - start] = '\0';
                icon->match[icon->match_count++] = alias;
            }
        }
        part += length;
        if (*part == ',')
            part++;
    }
}

// The inih handler. inih reports keys, not section headers, so a change of section name marks a new
// section. A repeated section keeps the first; a repeated 'file', 'title' or 'group' keeps the first, and a
// repeated 'match' adds to it. A section with no keys at all never reaches this handler (inih's default
// build has no header callback, and Linux uses the system library), so it is skipped without the "has no
// 'file'" warning. build-library.py never writes one.
static int handler(void *user, const char *section, const char *key, const char *value)
{
    (void) user;
    if (parsing == NULL || strcmp(parsing, section)) {
        free(parsing);
        parsing = strdup(section);
        target = -1;
        if (!library_is_name(section))
            warn("Icon library: [%s] is not a valid icon name (a-z, 0-9 and '-', up to %i characters), skipping it",
                section, LIBRARY_NAME_MAX);
        else if (find_icon(section) >= 0)
            warn("Icon library: [%s] appears twice, keeping the first", section);
        else
            target = add_icon(section);
    }
    if (target < 0)
        return 1;
    LibraryIcon *icon = &icons[target];
    if (!strcmp(key, "file")) {
        if (icon->file != NULL)
            warn("Icon library: [%s] has 'file' twice, keeping the first", section);
        else
            icon->file = strdup(value);
    }
    else if (!strcmp(key, "title") && icon->title == NULL)
        icon->title = strdup(value);
    else if (!strcmp(key, "group") && icon->group == NULL)
        icon->group = strdup(value);
    else if (!strcmp(key, "match"))
        add_aliases(icon, value);
    return 1;
}

// A function to check that a manifest path stays inside the library: relative, with no '..' part
static bool inside_library(const char *file)
{
    if (file[0] == '\0' || file[0] == '/' || file[0] == '\\')
        return false;
    if (isalpha((unsigned char) file[0]) && file[1] == ':')
        return false;
    const char *part = file;
    while (*part != '\0') {
        size_t length = strcspn(part, "/\\");
        if (length == 2 && part[0] == '.' && part[1] == '.')
            return false;
        part += length;
        if (*part != '\0')
            part++;
    }
    return true;
}

// A function to read root/icons.ini, keeping every icon whose file is inside the library and is a
// regular file that exists
int library_load(const char *root)
{
    library_free();
    size_t root_length = strlen(root);
    const char *separator = (root_length > 0 && (root[root_length - 1] == '/' || root[root_length - 1] == '\\'))
                            ? "" : "/";
    size_t manifest_size = root_length + 1 + strlen(LIBRARY_MANIFEST) + 1;
    char *manifest = malloc(manifest_size);
    if (manifest == NULL)
        return -1;
    snprintf(manifest, manifest_size, "%s%s%s", root, separator, LIBRARY_MANIFEST);
    // Something there that is not a regular file (a pipe, say, whose open would wait for good) is
    // never opened, here or for an icon below
    if (fileio_not_a_file(manifest)) {
        warn("Icon library: could not open %s: %s", manifest, fileio_last_error());
        free(manifest);
        return -1;
    }
    FILE *file = fileio_open(manifest, "r");
    if (file == NULL) {
        warn("Icon library: could not open %s", manifest);
        free(manifest);
        return -1;
    }
    int error = ini_parse_file(file, handler, NULL);
    fclose(file);
    if (error > 0)
        warn("Icon library: %s line %i could not be read, skipping it", manifest, error);
    else if (error < 0)
        warn("Icon library: could not read %s", manifest);
    free(manifest);
    free(parsing);
    parsing = NULL;
    target = -1;

    int count = 0;
    for (int i = 0; i < icon_count; i++) {
        LibraryIcon *icon = &icons[i];
        if (icon->file == NULL) {
            warn("Icon library: [%s] has no 'file', skipping it", icon->name);
            continue;
        }
        if (!inside_library(icon->file)) {
            warn("Icon library: [%s] file '%s' must be a path inside the library, skipping it", icon->name, icon->file);
            continue;
        }
        size_t path_size = root_length + 1 + strlen(icon->file) + 1;
        char *path = malloc(path_size);
        if (path == NULL)
            continue;
        snprintf(path, path_size, "%s%s%s", root, separator, icon->file);
        if (fileio_not_a_file(path)) {
            warn("Icon library: [%s] file %s is %s, skipping it", icon->name, path, fileio_last_error());
            free(path);
            continue;
        }
        FILE *probe = fileio_open(path, "rb");
        if (probe == NULL) {
            warn("Icon library: [%s] file %s does not exist, skipping it", icon->name, path);
            free(path);
            continue;
        }
        fclose(probe);
        icon->path = path;
        count++;
    }
    // The icons kept, in the manifest's order and in one array, for the icon pickers (library_info())
    infos = count > 0 ? malloc((size_t) count * sizeof(LibraryInfo)) : NULL;
    if (count > 0 && infos == NULL)
        warn("Icon library: out of memory listing its icons, so the icon pickers show none");
    for (int i = 0; infos != NULL && i < icon_count; i++) {
        const LibraryIcon *icon = &icons[i];
        if (icon->path == NULL)
            continue;
        infos[info_count++] = (LibraryInfo) { icon->name, icon->title != NULL ? icon->title : icon->name,
            icon->group != NULL ? icon->group : "", (const char *const *) icon->match, icon->match_count };
    }
    return count;
}

// A function to count the icons loaded (a rejected one is not), for the icon pickers
int library_count(void)
{
    return info_count;
}

// A function to describe a loaded icon by its place in the manifest's order; NULL past either end. The
// icons sit in one array, so library_info(0) and library_count() are what iconmatch_find() searches.
const LibraryInfo *library_info(int index)
{
    return index >= 0 && index < info_count ? &infos[index] : NULL;
}

// A function to find a loaded (not rejected) icon by name
static const LibraryIcon *loaded_icon(const char *name)
{
    if (name == NULL)
        return NULL;
    for (int i = 0; i < icon_count; i++) {
        if (icons[i].path != NULL && !strcmp(icons[i].name, name))
            return &icons[i];
    }
    return NULL;
}

const char *library_lookup(const char *name)
{
    const LibraryIcon *icon = loaded_icon(name);
    return icon != NULL ? icon->path : NULL;
}

// A function to find the library icon a broken icon field most likely meant: one of the seven icons older
// versions shipped (by file name), or a library name typed with capitals or stray spaces. Returns the
// icon's path and, if name is not NULL, points it at the icon's name; returns NULL if nothing fits.
const char *library_rescue(const char *field, const char **name)
{
    if (field == NULL)
        return NULL;
    const LibraryIcon *icon = loaded_icon(library_legacy_name(field));
    if (icon == NULL) {
        while (isspace((unsigned char) *field))
            field++;
        size_t length = strlen(field);
        while (length > 0 && isspace((unsigned char) field[length - 1]))
            length--;
        if (length == 0 || length > LIBRARY_NAME_MAX)
            return NULL;
        char folded[LIBRARY_NAME_MAX + 1];
        for (size_t i = 0; i < length; i++)
            folded[i] = (char) tolower((unsigned char) field[i]);
        folded[length] = '\0';
        if (library_is_name(folded))
            icon = loaded_icon(folded);
    }
    if (icon == NULL)
        return NULL;
    if (name != NULL)
        *name = icon->name;
    return icon->path;
}

// A function to map the file name of one of the seven icons older versions shipped to its library name
const char *library_legacy_name(const char *path)
{
    static const char *const legacy[][2] = {
        { "kodi.png", "kodi" },       { "plex.png", "plex" },       { "steam.png", "steam" },
        { "retroarch.png", "retroarch" }, { "system.png", "settings" },
        { "restart.png", "restart" }, { "sleep.png", "sleep" },
    };
    if (path == NULL)
        return NULL;
    const char *base = path;
    for (const char *p = path; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\')
            base = p + 1;
    }
    char lower[16];
    size_t length = strlen(base);
    if (length == 0 || length >= sizeof(lower))
        return NULL;
    for (size_t i = 0; i <= length; i++)
        lower[i] = (char) tolower((unsigned char) base[i]);
    for (size_t i = 0; i < sizeof(legacy) / sizeof(legacy[0]); i++) {
        if (!strcmp(lower, legacy[i][0]))
            return legacy[i][1];
    }
    return NULL;
}

void library_free(void)
{
    for (int i = 0; i < icon_count; i++) {
        free(icons[i].file);
        free(icons[i].path);
        free(icons[i].title);
        free(icons[i].group);
        for (int a = 0; a < icons[i].match_count; a++)
            free(icons[i].match[a]);
        free(icons[i].match);
    }
    free(icons);
    icons = NULL;
    icon_count = 0;
    icon_capacity = 0;
    free(infos);
    infos = NULL;
    info_count = 0;
    free(parsing);
    parsing = NULL;
    target = -1;
}
