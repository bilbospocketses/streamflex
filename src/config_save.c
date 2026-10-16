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
// and beside the key it is removed, so the file never holds two lines for one setting. A menu's own
// setting (placed under its header) whose section a hand edit renamed or removed meanwhile is skipped,
// with a note: setting it would make the section again, which the launcher shows as an empty menu.
static bool apply_edits(IniDoc *doc, const ConfigEdit *edits, int count, ConfigSaveResult *result)
{
    for (int i = 0; i < count; i++) {
        const ConfigEdit *edit = &edits[i];
        const char *key = edit->key;
        if (edit->value != NULL && edit->placement == INIDOC_UNDER_HEADER && !inidoc_has_section(doc, edit->section)) {
            char note[CONFIG_SAVE_NOTES_SIZE];
            snprintf(note, sizeof(note), "[%s] is not there any more, so its %s change is skipped", edit->section, key);
            config_save_note(result, CONFIG_NOTE_SECTION_GONE, note);
            continue;
        }
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

// A function to add a note of a kind to a save's notes, one per line; one that does not fit is cut
// short, and one with no room left at all adds no line, and so no kind
static void add_note(ConfigSaveResult *result, ConfigNoteKind kind, const char *format, const char *line)
{
    size_t size = sizeof(result->notes);
    size_t used = strlen(result->notes);
    if (used > 0 && used + 1 < size)
        result->notes[used++] = '\n';
    if (used + 1 < size)
        result->note_kinds[result->note_count++] = kind;
    snprintf(result->notes + used, size - used, format, line);
}

// A function to add one note line of a kind, in the words a document callback chooses. An empty one
// adds nothing: each line then takes two bytes at least with its break, as `note_kinds` is sized for.
void config_save_note(ConfigSaveResult *result, ConfigNoteKind kind, const char *text)
{
    if (text[0] == '\0')
        return;
    add_note(result, kind, "%s", text);
}

// A function to take back a failed save's notes: it made nothing, so nothing was skipped
static void drop_notes(ConfigSaveResult *result)
{
    result->notes[0] = '\0';
    memset(result->note_kinds, 0, sizeof(result->note_kinds));
    result->note_count = 0;
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

// A function to number a key one above the highest <stem>N the section holds (Hotkey1 and Hotkey4 give
// Hotkey5; menuedit.c's Entry1 and Entry4 give Entry5), into `out`; false (with the reason) when out of
// memory, or when the highest number is too large to go one above (a hand edit's
// Hotkey99999999999999999999), which would wrap round
bool config_save_next_key(const IniDoc *doc, const char *section, const char *stem, char *out, size_t size,
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
    else if (!config_save_next_key(doc, edit->section, edit->key, key, sizeof(key), result))
        return false;
    if (inidoc_list_add(doc, edit->section, key, edit->value))
        return true;
    snprintf(result->why, sizeof(result->why), "the %s value in [%s] cannot be written: %s", key, edit->section, inidoc_why(doc));
    return false;
}

// A function to apply the list edits to the fresh file: changes and removals first, found by the line
// each had when settings opened, then the additions. A line a hand edit changed or removed meanwhile
// takes its change as a new line, and its removal is skipped; both say so in the result's notes. A
// removal inidoc refuses (a key with an empty name) fails the save with its reason, as a line that cannot
// be written does.
static bool apply_list_edits(IniDoc *doc, const ConfigListEdit *lists, int count, ConfigSaveResult *result)
{
    for (int i = 0; i < count; i++) {
        const ConfigListEdit *edit = &lists[i];
        if (edit->op == CONFIG_LIST_ADD)
            continue;
        int line = inidoc_find_line(doc, edit->section, edit->original);
        if (edit->op == CONFIG_LIST_REMOVE) {
            if (line < 0)
                add_note(result, CONFIG_NOTE_REMOVAL_SKIPPED, "'%s' is not there any more, so its removal is skipped",
                         edit->original);
            else if (!inidoc_list_remove(doc, line)) {
                snprintf(result->why, sizeof(result->why), "the line '%s' in [%s] cannot be removed: %s", edit->original,
                    edit->section, inidoc_why(doc));
                return false;
            }
            continue;
        }
        if (line < 0) {
            add_note(result, CONFIG_NOTE_CHANGED_MEANWHILE, "'%s' changed meanwhile, so its change is written as a new line",
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

// A function to save the settings screen's changes and list edits, with no document callback
bool config_save_all(const char *loaded, const char *system_prefix, const char *user_config,
                     const ConfigEdit *edits, int count, const ConfigListEdit *lists, int list_count,
                     ConfigSaveResult *result)
{
    return config_save_with(loaded, system_prefix, user_config, edits, count, lists, list_count, NULL, NULL, result);
}

// A function to save the settings screen's changes. `loaded` is the file the launcher read.
// When it cannot be written but lies under `system_prefix` (the packaged copy on Linux), the
// save goes to `user_config` instead, which the launcher searches first; pass NULL for both
// where there is no such fallback (Windows). On the fresh read, `doc_edit` (NULL for none) makes its
// changes first (menuedit_apply() writes the menus so, and a menu's settings then find its section by
// its new name), then the single-key edits, then the list edits. What they could not make as asked
// reaches `result->notes`, which a save that fails leaves empty, since it makes nothing.
bool config_save_with(const char *loaded, const char *system_prefix, const char *user_config,
                      const ConfigEdit *edits, int count, const ConfigListEdit *lists, int list_count,
                      ConfigDocEdit doc_edit, void *context, ConfigSaveResult *result)
{
    memset(result, 0, sizeof(*result));
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
    if ((doc_edit != NULL && !doc_edit(doc, context, result)) || !apply_edits(doc, edits, count, result) ||
        !apply_list_edits(doc, lists, list_count, result)) {
        inidoc_free(doc);
        drop_notes(result);
        return false;
    }
    char *output = inidoc_serialize(doc, &length);
    inidoc_free(doc);
    if (output == NULL) {
        snprintf(result->why, sizeof(result->why), "out of memory");
        drop_notes(result);
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
    if (!ok) {
        fileio_remove(temporary);
        drop_notes(result);
    }
    else
        snprintf(result->warning, sizeof(result->warning), "%s", fileio_last_warning());
    alloc_free(output);
    return ok;
}
