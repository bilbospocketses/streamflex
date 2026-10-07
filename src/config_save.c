#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config_save.h"
#include "fileio.h"
#include "alloc.h"

// A function to tell whether a path lies under a folder
static bool starts_with(const char *path, const char *prefix)
{
    return strncmp(path, prefix, strlen(prefix)) == 0;
}

// A function to cut a file's path down to its folder, in place
static void cut_to_folder(char *path)
{
    size_t length = strlen(path);
    while (length > 0 && path[length - 1] != '/' && path[length - 1] != '\\')
        length--;
    if (length > 1)
        length--;
    path[length] = '\0';
}

// A function to apply the edits to a config's text; false (with the reason) when one cannot be written.
// A key and its older alias never both survive an edit: the alias alone is edited in its place,
// and beside the key it is removed, so the file never holds two lines for one setting.
static bool apply_edits(IniDoc *doc, const ConfigEdit *edits, int count, ConfigSaveResult *result)
{
    for (int i = 0; i < count; i++) {
        const ConfigEdit *edit = &edits[i];
        const char *key = edit->key;
        if (edit->alias != NULL && inidoc_get(doc, edit->section, edit->alias) != NULL) {
            if (inidoc_get(doc, edit->section, key) == NULL && edit->value != NULL)
                key = edit->alias;
            else
                inidoc_remove(doc, edit->section, edit->alias);
        }
        if (edit->value == NULL)
            inidoc_remove(doc, edit->section, key);
        else if (!inidoc_set(doc, edit->section, key, edit->value, edit->placement)) {
            snprintf(result->why, sizeof(result->why), "the %s value in [%s] cannot be written: %s",
                key, edit->section, inidoc_why(doc));
            return false;
        }
    }
    return true;
}

// A function to turn the save to the user's own config, which the launcher searches before the
// system copy. One that exists already (an earlier save made it, say) is the file the launcher
// reads next, so it is read and changed like any other, and fails the save when it cannot be read
// or written, rather than being replaced without a backup; otherwise its folder is made and the
// system copy in `source` is its starting point. `source` and `result->path` end up naming the
// file to read and the file to write.
static bool use_user_config(const char *user_config, char *source, size_t size, ConfigSaveResult *result)
{
    if (strlen(user_config) >= sizeof(result->path)) {
        snprintf(result->why, sizeof(result->why), "the user config's path is too long");
        return false;
    }
    snprintf(result->path, sizeof(result->path), "%s", user_config);
    bool present = fileio_present(user_config);
    if (!present && strcmp(fileio_last_error(), "not found") != 0) {
        snprintf(result->why, sizeof(result->why), "%s", fileio_last_error());
        return false;
    }
    if (present) {
        if (!fileio_real_path(user_config, source, size) || !fileio_is_writable(source)) {
            snprintf(result->why, sizeof(result->why), "%s", fileio_last_error());
            return false;
        }
        snprintf(result->path, sizeof(result->path), "%s", source);
        return true;
    }
    char folder[CONFIG_SAVE_PATH_MAX];
    snprintf(folder, sizeof(folder), "%s", user_config);
    cut_to_folder(folder);
    if (!fileio_make_dirs(folder)) {
        snprintf(result->why, sizeof(result->why), "could not make the folder %.300s: %s", folder, fileio_last_error());
        return false;
    }
    return true;
}

// A function to keep a copy of the file about to be replaced as <file>.bak. The copy is written
// beside it first and then moved over the old backup, so the old one survives until the new one
// is ready, and Windows lets a move replace a hidden backup, which a plain write cannot.
static bool keep_backup(ConfigSaveResult *result)
{
    char temporary[CONFIG_SAVE_PATH_MAX];
    int written = snprintf(result->backup, sizeof(result->backup), "%s.bak", result->path);
    int temporary_written = snprintf(temporary, sizeof(temporary), "%s.bak.tmp", result->path);
    if (written < 0 || written >= (int) sizeof(result->backup) ||
        temporary_written < 0 || temporary_written >= (int) sizeof(temporary)) {
        snprintf(result->why, sizeof(result->why), "could not write the backup: the path is too long");
        result->backup[0] = '\0';
        return false;
    }
    if (!fileio_copy(result->path, temporary) || !fileio_replace(temporary, result->backup)) {
        snprintf(result->why, sizeof(result->why), "could not write the backup %.300s: %s", result->backup, fileio_last_error());
        fileio_remove(temporary);
        result->backup[0] = '\0';
        return false;
    }
    return true;
}

// The notes a save gathers, each line with its kind; they reach the result only when it succeeds
typedef struct {
    char text[CONFIG_SAVE_NOTES_SIZE];
    ConfigNoteKind kinds[CONFIG_SAVE_NOTES_SIZE / 2];
    int count;
} SaveNotes;

// A function to add a note of a kind to the notes, one per line; one that does not fit is cut short,
// and one with no room left at all adds no line, and so no kind
static void add_note(SaveNotes *notes, ConfigNoteKind kind, const char *format, const char *line)
{
    size_t size = sizeof(notes->text);
    size_t used = strlen(notes->text);
    if (used > 0 && used + 1 < size)
        notes->text[used++] = '\n';
    if (used + 1 < size)
        notes->kinds[notes->count++] = kind;
    snprintf(notes->text + used, size - used, format, line);
}

// A function to list a section's key lines into new memory (caller frees); NULL when out of memory.
// An empty section still gets its own block: alloc gives one byte for none.
static IniDocItem *list_items(const IniDoc *doc, const char *section, int *count)
{
    *count = inidoc_list(doc, section, NULL, NULL, 0);
    IniDocItem *items = alloc_calloc((size_t) *count, sizeof(IniDocItem));
    if (items != NULL)
        inidoc_list(doc, section, NULL, items, *count);
    return items;
}

// A function to number a key one above the highest <stem>N the section holds (Hotkey1 and Hotkey4
// give Hotkey5), into `out`; false (with the reason) when out of memory, or when the highest number
// is too large to go one above (a hand edit's Hotkey99999999999999999999), which would wrap round
static bool next_key(const IniDoc *doc, const char *section, const char *stem, char *out, size_t size,
                     ConfigSaveResult *result)
{
    int count = 0;
    IniDocItem *items = list_items(doc, section, &count);
    if (items == NULL) {
        snprintf(result->why, sizeof(result->why), "out of memory");
        return false;
    }
    size_t stem_length = strlen(stem);
    long long highest = 0;
    for (int i = 0; i < count; i++) {
        if (strncmp(items[i].key, stem, stem_length) != 0)
            continue;
        const char *digits = items[i].key + stem_length;
        if (*digits == '\0' || strspn(digits, "0123456789") != strlen(digits))
            continue;
        long long n = strtoll(digits, NULL, 10);
        if (n > highest)
            highest = n;
    }
    alloc_free(items);
    if (highest == LLONG_MAX) {
        snprintf(result->why, sizeof(result->why), "a new %s line in [%s] cannot be numbered: the highest number there is too large",
            stem, section);
        return false;
    }
    snprintf(out, size, "%s%lld", stem, highest + 1);
    return true;
}

// A function to copy a list line's own key into `out`; false (with the reason) when out of memory
static bool line_key(const IniDoc *doc, const char *section, int line, char *out, size_t size, ConfigSaveResult *result)
{
    int count = 0;
    IniDocItem *items = list_items(doc, section, &count);
    if (items == NULL) {
        snprintf(result->why, sizeof(result->why), "out of memory");
        return false;
    }
    out[0] = '\0';
    for (int i = 0; i < count; i++) {
        if (items[i].line == line)
            snprintf(out, size, "%s", items[i].key);
    }
    alloc_free(items);
    return true;
}

// A function to add one list line: its key numbered when asked; false (with the reason) when it
// cannot be written. A key is held to a line's length: one longer could never be written (inidoc
// refuses the line as too long), so cutting it there never writes another key.
static bool add_list_line(IniDoc *doc, const ConfigListEdit *edit, ConfigSaveResult *result)
{
    char key[INIDOC_MAX_LINE + 1];
    if (!edit->numbered)
        snprintf(key, sizeof(key), "%s", edit->key);
    else if (!next_key(doc, edit->section, edit->key, key, sizeof(key), result))
        return false;
    if (inidoc_list_add(doc, edit->section, key, edit->value))
        return true;
    snprintf(result->why, sizeof(result->why), "the %s value in [%s] cannot be written: %s", key, edit->section, inidoc_why(doc));
    return false;
}

// A function to apply the list edits to the fresh file: changes and removals first, found by the line
// each had when settings opened, then the additions. A line a hand edit changed or removed meanwhile
// takes its change as a new line, and its removal is skipped; both say so in `notes`. A removal
// inidoc refuses (a key with an empty name) fails the save with its reason, as a line that cannot be
// written does.
static bool apply_list_edits(IniDoc *doc, const ConfigListEdit *lists, int count, SaveNotes *notes,
                             ConfigSaveResult *result)
{
    for (int i = 0; i < count; i++) {
        const ConfigListEdit *edit = &lists[i];
        if (edit->op == CONFIG_LIST_ADD)
            continue;
        int line = inidoc_find_line(doc, edit->section, edit->original);
        if (edit->op == CONFIG_LIST_REMOVE) {
            if (line < 0)
                add_note(notes, CONFIG_NOTE_REMOVAL_SKIPPED, "'%s' is not there any more, so its removal is skipped",
                         edit->original);
            else if (!inidoc_list_remove(doc, line)) {
                snprintf(result->why, sizeof(result->why), "the line '%s' in [%s] cannot be removed: %s", edit->original,
                    edit->section, inidoc_why(doc));
                return false;
            }
            continue;
        }
        if (line < 0) {
            add_note(notes, CONFIG_NOTE_CHANGED_MEANWHILE, "'%s' changed meanwhile, so its change is written as a new line",
                     edit->original);
            if (!add_list_line(doc, edit, result))
                return false;
            continue;
        }
        // A numbered set keeps the line's own key: its number is already the line's
        const char *key = edit->key;
        char own[INIDOC_MAX_LINE + 1];
        if (edit->numbered) {
            if (!line_key(doc, edit->section, line, own, sizeof(own), result))
                return false;
            key = own;
        }
        if (!inidoc_list_set(doc, line, key, edit->value)) {
            snprintf(result->why, sizeof(result->why), "the %s value in [%s] cannot be written: %s", key,
                edit->section, inidoc_why(doc));
            return false;
        }
    }
    for (int i = 0; i < count; i++) {
        if (lists[i].op == CONFIG_LIST_ADD && !add_list_line(doc, &lists[i], result))
            return false;
    }
    return true;
}

// A function to save the settings screen's changes to single keys only (see config_save_all)
bool config_save(const char *loaded, const char *system_prefix, const char *user_config,
                 const ConfigEdit *edits, int count, ConfigSaveResult *result)
{
    return config_save_all(loaded, system_prefix, user_config, edits, count, NULL, 0, result);
}

// A function to save the settings screen's changes. `loaded` is the file the launcher read.
// When it cannot be written but lies under `system_prefix` (the packaged copy on Linux), the
// save goes to `user_config` instead, which the launcher searches first; pass NULL for both
// where there is no such fallback (Windows). The list edits follow the single-key edits; what they
// could not make as asked reaches `result->notes` only when the save succeeds, since a save that
// fails makes nothing.
bool config_save_all(const char *loaded, const char *system_prefix, const char *user_config,
                     const ConfigEdit *edits, int count, const ConfigListEdit *lists, int list_count,
                     ConfigSaveResult *result)
{
    memset(result, 0, sizeof(*result));
    SaveNotes notes;
    memset(&notes, 0, sizeof(notes));
    char source[CONFIG_SAVE_PATH_MAX];
    if (!fileio_real_path(loaded, source, sizeof(source))) {
        snprintf(result->path, sizeof(result->path), "%s", loaded);
        snprintf(result->why, sizeof(result->why), "%s", fileio_last_error());
        return false;
    }
    snprintf(result->path, sizeof(result->path), "%s", source);

    if (!fileio_is_writable(source)) {
        if (system_prefix == NULL || user_config == NULL || !starts_with(source, system_prefix)) {
            snprintf(result->why, sizeof(result->why), "%s", fileio_last_error());
            return false;
        }
        if (!use_user_config(user_config, source, sizeof(source), result))
            return false;
    }

    // Read the file as it is on disk now, so a change made meanwhile by hand survives
    size_t length = 0;
    char *text = fileio_read_all(source, &length);
    if (text == NULL) {
        snprintf(result->why, sizeof(result->why), "could not read %.300s: %s", source, fileio_last_error());
        return false;
    }
    IniDoc *doc = inidoc_parse(text, length);
    alloc_free(text);
    if (doc == NULL) {
        snprintf(result->why, sizeof(result->why), "out of memory");
        return false;
    }
    if (!apply_edits(doc, edits, count, result) ||
        !apply_list_edits(doc, lists, list_count, &notes, result)) {
        inidoc_free(doc);
        return false;
    }
    char *output = inidoc_serialize(doc, &length);
    inidoc_free(doc);
    if (output == NULL) {
        snprintf(result->why, sizeof(result->why), "out of memory");
        return false;
    }

    // Keep the file as it was, then write the new one beside it and swap it in whole. A file that
    // cannot be looked up (out of memory, say) is never taken for one that is not there, which
    // would replace it without a backup.
    bool present = fileio_present(result->path);
    bool ok = true;
    if (!present && strcmp(fileio_last_error(), "not found") != 0) {
        snprintf(result->why, sizeof(result->why), "could not look for %.300s: %s", result->path, fileio_last_error());
        ok = false;
    }
    ok = ok && (!present || keep_backup(result));
    char temporary[CONFIG_SAVE_PATH_MAX + 4];
    snprintf(temporary, sizeof(temporary), "%s.tmp", result->path);
    if (ok && !fileio_write_all(temporary, output, length)) {
        snprintf(result->why, sizeof(result->why), "could not write %.300s: %s", temporary, fileio_last_error());
        ok = false;
    }
    else if (ok && !fileio_replace(temporary, result->path)) {
        snprintf(result->why, sizeof(result->why), "could not replace the file: %s", fileio_last_error());
        ok = false;
    }
    if (!ok)
        fileio_remove(temporary);
    else {
        snprintf(result->warning, sizeof(result->warning), "%s", fileio_last_warning());
        snprintf(result->notes, sizeof(result->notes), "%s", notes.text);
        memcpy(result->note_kinds, notes.kinds, sizeof(result->note_kinds));
        result->note_count = notes.count;
    }
    alloc_free(output);
    return ok;
}
