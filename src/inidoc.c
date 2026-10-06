#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "inidoc.h"
#include "alloc.h"

typedef enum {
    LINE_OTHER,          // Blank, a comment, or a line inih cannot read
    LINE_SECTION,
    LINE_KEY,
    LINE_CONTINUATION    // Indented after a key, even past blank, comment or unreadable lines: inih reads
                         // it as that key's value again, and the launcher keeps the last one read
} LineKind;

typedef struct {
    char *text;          // The line as written, without its line ending; it may hold a NUL
    size_t length;       // How many bytes `text` holds, so a NUL in it survives the round trip
    const char *eol;     // "\n", "\r\n", or "" for a last line with no ending
    LineKind form;       // What the line reads as on its own: SECTION, KEY or OTHER
    LineKind kind;       // What it reads as where it stands: `form`, or CONTINUATION after a key
    char *name;          // SECTION: the section's name, cut as inih cuts it; KEY: the key's name
    char *value;         // KEY: the value, as inih reads it
    size_t value_start;  // KEY: where the value starts in `text`; just after the separator when empty
    size_t value_length; // KEY: how many bytes of `text` the value spans
    int section;         // Index of the SECTION line this one sits under; -1 before the first
    int owner;           // CONTINUATION: the KEY line it follows, whose name inih gives it; -1 otherwise
} Line;

struct IniDoc {
    Line *lines;
    int count;
    int capacity;
    bool bom;            // The file started with a UTF-8 byte order mark
    const char *eol;     // The ending new lines get: the file's first one, else "\n"
    const char *why;     // Why the last set, add or list-mode remove failed; "" after one that succeeded
};

static const char *const EOL_LF = "\n";
static const char *const EOL_CRLF = "\r\n";
static const char *const EOL_NONE = "";

static const char *const OUT_OF_MEMORY = "out of memory";
static const char *const TOO_LONG_WITH_COMMENT =
    "it is too long for one line of config.ini (199 bytes at most) with its comment";
static const char *const TOO_LONG_WITH_SPACING =
    "it is too long for one line of config.ini (199 bytes at most) with the spacing on its line";
static const char *const SECTION_TOO_LONG =
    "the section's name is too long for one line of config.ini (199 bytes at most)";
static const char *const NO_SAFE_PLACE = "it would change how other lines read";
static const char *const NOT_A_KEY = "the line is not a key";

// A function to tell whitespace as inih does (isspace in the C locale)
static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

// A function to copy part of a string into a new one
static char *copy_span(const char *start, size_t length)
{
    char *copy = alloc_malloc(length + 1);
    if (copy != NULL) {
        memcpy(copy, start, length);
        copy[length] = '\0';
    }
    return copy;
}

// A function to find, as inih's find_chars_or_comment() does, the first of `chars` from `start`,
// or a ';' that follows whitespace (a trailing comment), or the end of the text
static size_t find_chars_or_comment(const char *text, size_t start, const char *chars)
{
    bool was_space = false;
    size_t i = start;
    while (text[i] != '\0' && (chars == NULL || strchr(chars, text[i]) == NULL) && !(was_space && text[i] == ';')) {
        was_space = is_space(text[i]);
        i++;
    }
    return i;
}

// A function to name the section a line sits under; lines before the first header are in ""
static const char *section_name(const IniDoc *doc, int section)
{
    return section < 0 ? "" : doc->lines[section].name;
}

// A function to tell whether a section's name is the one asked for. inih keeps only the first
// INIDOC_MAX_SECTION bytes of a name, so names that agree that far are one section to it.
static bool same_section(const char *name, const char *section)
{
    return strncmp(name, section, (size_t) INIDOC_MAX_SECTION) == 0;
}

// A function to free what a line holds
static void free_line(Line *line)
{
    alloc_free(line->text);
    alloc_free(line->name);
    alloc_free(line->value);
}

// A function to read one line on its own, as inih's ini_parse_stream() does when the line is
// not a continuation: a section header and its name, or a key and its value. Like inih it stops
// at a NUL. It allocates the line's name and value, and is false only when out of memory, with
// nothing left allocated.
static bool read_line(Line *line)
{
    const char *text = line->text;
    line->form = LINE_OTHER;
    line->name = NULL;
    line->value = NULL;
    size_t start = 0;
    while (is_space(text[start]))
        start++;
    if (text[start] == '\0' || text[start] == ';' || text[start] == '#')
        return true;
    if (text[start] == '[') {
        size_t end = find_chars_or_comment(text, start + 1, "]");
        if (text[end] != ']')
            return true;
        size_t length = end - start - 1;
        if (length > (size_t) INIDOC_MAX_SECTION)
            length = (size_t) INIDOC_MAX_SECTION;
        line->name = copy_span(text + start + 1, length);
        if (line->name == NULL)
            return false;
        line->form = LINE_SECTION;
        return true;
    }
    size_t end = find_chars_or_comment(text, start, "=:");
    if (text[end] != '=' && text[end] != ':')
        return true;
    size_t name_end = end;
    while (name_end > start && is_space(text[name_end - 1]))
        name_end--;
    size_t value_begin = end + 1;
    size_t value_end = find_chars_or_comment(text, value_begin, NULL);
    size_t first = value_begin;
    while (first < value_end && is_space(text[first]))
        first++;
    size_t last = value_end;
    while (last > first && is_space(text[last - 1]))
        last--;
    line->name = copy_span(text + start, name_end - start);
    line->value = copy_span(text + first, last - first);
    if (line->name == NULL || line->value == NULL) {
        alloc_free(line->name);
        alloc_free(line->value);
        line->name = NULL;
        line->value = NULL;
        return false;
    }
    line->form = LINE_KEY;
    line->value_start = first == last ? value_begin : first;
    line->value_length = last - first;
    return true;
}

// A function to place every line the way inih's ini_parse_stream() does: which section it sits
// under, and whether an indented line continues the key before it. It runs again after each
// edit: config files are small, and placing them whole keeps every index honest. It allocates
// nothing, so it cannot fail.
static void classify_all(IniDoc *doc)
{
    int section = -1;
    int previous = -1;   // inih's prev_name: the key read last since the header. A key with an
                         // empty name empties prev_name, so nothing continues it.
    for (int i = 0; i < doc->count; i++) {
        Line *line = &doc->lines[i];
        line->kind = line->form;
        line->section = section;
        line->owner = -1;
        const char *text = line->text;
        size_t start = 0;
        while (is_space(text[start]))
            start++;
        if (text[start] == '\0' || text[start] == ';' || text[start] == '#')
            continue;
        if (previous >= 0 && start > 0) {
            line->kind = LINE_CONTINUATION;
            line->owner = previous;
        }
        else if (line->form == LINE_SECTION) {
            line->section = i;
            section = i;
            previous = -1;
        }
        else if (line->form == LINE_KEY) {
            previous = line->name[0] != '\0' ? i : -1;
        }
    }
}

// A function to tell whether a continuation line sets a key. inih gives it the name of the key it
// follows as that key's prev_name holds it: cut to INIDOC_MAX_NAME bytes. So a continuation after
// a key of 50 bytes or more sets the key named by its first 49 bytes, not the key it follows.
static bool continues(const IniDoc *doc, const Line *line, const char *key)
{
    const char *name = doc->lines[line->owner].name;
    size_t length = strlen(name);
    if (length > (size_t) INIDOC_MAX_NAME)
        length = (size_t) INIDOC_MAX_NAME;
    return strlen(key) == length && strncmp(name, key, length) == 0;
}

// A function to tell whether a line sets a key in a section: as the key's own line, or as a
// continuation that inih reads as that key's value
static bool sets(const IniDoc *doc, const Line *line, const char *section, const char *key)
{
    if (!same_section(section_name(doc, line->section), section))
        return false;
    if (line->kind == LINE_KEY)
        return strcmp(line->name, key) == 0;
    return line->kind == LINE_CONTINUATION && continues(doc, line, key);
}

// A function to find the last line that sets a key in a section, of either kind: the value the
// launcher ends up with
static int last_setter(const IniDoc *doc, const char *section, const char *key)
{
    for (int i = doc->count - 1; i >= 0; i--) {
        if (sets(doc, &doc->lines[i], section, key))
            return i;
    }
    return -1;
}

// A function to make room for one more line
static bool grow(IniDoc *doc)
{
    if (doc->count < doc->capacity)
        return true;
    int capacity = doc->capacity ? doc->capacity * 2 : 16;
    Line *lines = alloc_realloc(doc->lines, (size_t) capacity * sizeof(Line));
    if (lines == NULL)
        return false;
    doc->lines = lines;
    doc->capacity = capacity;
    return true;
}

// A function to add a line read from the file
static bool append_raw(IniDoc *doc, const char *text, size_t length, const char *eol)
{
    if (!grow(doc))
        return false;
    Line *line = &doc->lines[doc->count];
    memset(line, 0, sizeof(*line));
    line->text = copy_span(text, length);
    line->length = length;
    line->eol = eol;
    if (line->text == NULL || !read_line(line)) {
        free_line(line);
        return false;
    }
    doc->count++;
    return true;
}

// A function to insert a new line at an index. A new last line takes over "no line ending" from
// the old last line, so a file that did not end with a newline still does not. When out of
// memory the document is left as it was.
static bool insert_line(IniDoc *doc, int at, const char *text)
{
    Line line;
    memset(&line, 0, sizeof(line));
    line.length = strlen(text);
    line.text = copy_span(text, line.length);
    line.eol = doc->eol;
    if (line.text == NULL || !read_line(&line) || !grow(doc)) {
        free_line(&line);
        return false;
    }
    memmove(&doc->lines[at + 1], &doc->lines[at], (size_t) (doc->count - at) * sizeof(Line));
    doc->lines[at] = line;
    doc->count++;
    if (at == doc->count - 1 && at > 0 && doc->lines[at - 1].eol[0] == '\0') {
        doc->lines[at - 1].eol = doc->eol;
        doc->lines[at].eol = EOL_NONE;
    }
    classify_all(doc);
    return true;
}

// A function to delete a line. When the deleted line was the last and had no line ending, the
// new last line loses its ending too.
static void remove_line(IniDoc *doc, int at)
{
    bool was_open_end = at == doc->count - 1 && doc->lines[at].eol[0] == '\0';
    free_line(&doc->lines[at]);
    memmove(&doc->lines[at], &doc->lines[at + 1], (size_t) (doc->count - at - 1) * sizeof(Line));
    doc->count--;
    if (was_open_end && doc->count > 0)
        doc->lines[doc->count - 1].eol = EOL_NONE;
    classify_all(doc);
}

// A function to find the last line setting a key in a section: the one the parser ends up with
static int find_key(const IniDoc *doc, const char *section, const char *key)
{
    for (int i = doc->count - 1; i >= 0; i--) {
        const Line *line = &doc->lines[i];
        if (line->kind == LINE_KEY && strcmp(line->name, key) == 0 && same_section(section_name(doc, line->section), section))
            return i;
    }
    return -1;
}

// A function to find the last header of a section
static int find_header(const IniDoc *doc, const char *section)
{
    for (int i = doc->count - 1; i >= 0; i--) {
        if (doc->lines[i].kind == LINE_SECTION && same_section(doc->lines[i].name, section))
            return i;
    }
    return -1;
}

// A function to find a key's last line: the key itself, or its last continuation line. inih
// keeps reading indented lines as the key's value past blank, comment and unreadable lines, up
// to the next key or section header.
static int end_of_key(const IniDoc *doc, int i)
{
    int end = i;
    for (int k = i + 1; k < doc->count && doc->lines[k].kind != LINE_KEY && doc->lines[k].kind != LINE_SECTION; k++) {
        if (doc->lines[k].kind == LINE_CONTINUATION)
            end = k;
    }
    return end;
}

// A function to remove a key's continuation lines, leaving any blank, comment or unreadable lines
// among them. It works from the last one up: a line is read from the lines before it, so the ones
// still to go keep their places and their kinds.
static void remove_continuations(IniDoc *doc, int i)
{
    for (int k = end_of_key(doc, i); k > i; k--) {
        if (doc->lines[k].kind == LINE_CONTINUATION)
            remove_line(doc, k);
    }
}

// A function to remove the continuation lines after line `from` that inih reads as a key's value:
// the key's own, and those after a longer key whose name inih cuts to this one. A longer key's
// continuations that set another name stay.
static void remove_later_continuations(IniDoc *doc, int from, const char *section, const char *key)
{
    for (int k = doc->count - 1; k > from; k--) {
        if (doc->lines[k].kind == LINE_CONTINUATION && sets(doc, &doc->lines[k], section, key))
            remove_line(doc, k);
    }
}

// A function to count the keys, to prove an insert made exactly one more
static int count_keys(const IniDoc *doc)
{
    int keys = 0;
    for (int i = 0; i < doc->count; i++) {
        if (doc->lines[i].kind == LINE_KEY)
            keys++;
    }
    return keys;
}

// A function to read a file's text into lines
IniDoc *inidoc_parse(const char *text, size_t length)
{
    IniDoc *doc = alloc_calloc(1, sizeof(IniDoc));
    if (doc == NULL)
        return NULL;
    doc->eol = EOL_LF;
    doc->why = "";
    bool eol_found = false;
    size_t i = 0;
    if (length >= 3 && (unsigned char) text[0] == 0xEF && (unsigned char) text[1] == 0xBB && (unsigned char) text[2] == 0xBF) {
        doc->bom = true;
        i = 3;
    }
    while (i < length) {
        size_t end = i;
        while (end < length && text[end] != '\n')
            end++;
        const char *eol = EOL_NONE;
        size_t text_end = end;
        if (end < length) {
            eol = EOL_LF;
            if (end > i && text[end - 1] == '\r') {
                eol = EOL_CRLF;
                text_end = end - 1;
            }
            if (!eol_found) {
                doc->eol = eol;
                eol_found = true;
            }
        }
        if (!append_raw(doc, text + i, text_end - i, eol)) {
            inidoc_free(doc);
            return NULL;
        }
        i = end < length ? end + 1 : end;
    }
    classify_all(doc);
    return doc;
}

// A function to write the lines back out as one text, byte for byte, a NUL in a line included
char *inidoc_serialize(const IniDoc *doc, size_t *length)
{
    size_t total = doc->bom ? 3 : 0;
    for (int i = 0; i < doc->count; i++)
        total += doc->lines[i].length + strlen(doc->lines[i].eol);
    char *out = alloc_malloc(total + 1);
    if (out == NULL)
        return NULL;
    size_t used = 0;
    if (doc->bom) {
        memcpy(out, "\xEF\xBB\xBF", 3);
        used = 3;
    }
    for (int i = 0; i < doc->count; i++) {
        size_t eol_length = strlen(doc->lines[i].eol);
        memcpy(out + used, doc->lines[i].text, doc->lines[i].length);
        used += doc->lines[i].length;
        memcpy(out + used, doc->lines[i].eol, eol_length);
        used += eol_length;
    }
    out[used] = '\0';
    if (length != NULL)
        *length = used;
    return out;
}

// A function to read the value on a key's own line, as inih reads it. inih would then read any
// continuation lines after the key as its value instead; inidoc's edits leave none after a key
// they set.
const char *inidoc_get(const IniDoc *doc, const char *section, const char *key)
{
    int i = find_key(doc, section, key);
    return i < 0 ? NULL : doc->lines[i].value;
}

// A function to say why `key=value` cannot be written so that inih reads it back unchanged,
// or NULL when it can. A line break in either would split the line in two when written out.
const char *inidoc_check(const char *key, const char *value)
{
    size_t length = strlen(value);
    if (strchr(value, '\n') != NULL || strchr(value, '\r') != NULL || strchr(key, '\n') != NULL || strchr(key, '\r') != NULL)
        return "it contains a line break";
    if (length > 0 && (is_space(value[0]) || is_space(value[length - 1])))
        return "it starts or ends with a space, which config.ini would drop";
    if (value[0] == ';')
        return "it starts with a semicolon, which config.ini would read as a comment";
    for (size_t i = 1; i < length; i++) {
        if (value[i] == ';' && is_space(value[i - 1]))
            return "it has a semicolon after a space, which config.ini would read as a comment";
    }
    if (strlen(key) + 1 + length > INIDOC_MAX_LINE)
        return "it is too long for one line of config.ini (199 bytes at most)";
    return NULL;
}

// A function to remember why a set failed, and fail
static bool refuse(IniDoc *doc, const char *why)
{
    doc->why = why;
    return false;
}

// A function to say why a key's line would be too long with a new value in place of its old one,
// or NULL when it would fit. `head` is how many bytes the line will have before the value: its own
// (line->value_start) when the key stays, or "key=" when list mode writes another key. When the key
// and value fit (inidoc_check), what the line keeps around them is what does not: its comment, or
// its spacing.
static const char *too_long_in_line(const Line *line, size_t head, size_t value_length)
{
    size_t tail = line->value_start + line->value_length;
    if (head + value_length + (line->length - tail) <= INIDOC_MAX_LINE)
        return NULL;
    return memchr(line->text + tail, ';', line->length - tail) != NULL ? TOO_LONG_WITH_COMMENT : TOO_LONG_WITH_SPACING;
}

// A function to put a new value into an existing key's line, keeping everything around it, and to
// remove the continuation lines inih would read as the key's value instead. Only the new line can
// fail, and it does before anything changes; removing lines cannot.
static bool replace_value(IniDoc *doc, int i, const char *section, const char *key, const char *value)
{
    Line *line = &doc->lines[i];
    size_t head = line->value_start;
    size_t tail = line->value_start + line->value_length;
    size_t value_length = strlen(value);
    size_t new_length = head + value_length + (line->length - tail);
    const char *too_long = too_long_in_line(line, head, value_length);
    if (too_long != NULL)
        return refuse(doc, too_long);
    Line updated = *line;   // Keeps the line ending; read_line() replaces the name and value
    updated.text = alloc_malloc(new_length + 1);
    if (updated.text == NULL)
        return refuse(doc, OUT_OF_MEMORY);
    memcpy(updated.text, line->text, head);
    memcpy(updated.text + head, value, value_length);
    memcpy(updated.text + head + value_length, line->text + tail, line->length - tail);
    updated.text[new_length] = '\0';
    updated.length = new_length;
    if (!read_line(&updated)) {
        alloc_free(updated.text);
        return refuse(doc, OUT_OF_MEMORY);
    }
    Line old = *line;
    *line = updated;
    classify_all(doc);

    // Prove the line reads back as intended; if not, put the old line back
    if (find_key(doc, section, key) != i || strcmp(doc->lines[i].value, value) != 0) {
        free_line(&doc->lines[i]);
        doc->lines[i] = old;
        classify_all(doc);
        return refuse(doc, NO_SAFE_PLACE);
    }
    free_line(&old);
    remove_later_continuations(doc, i, section, key);
    return true;
}

// A function to prove that a new key's line, just inserted at `at`, did what was meant: the file
// holds exactly one more key, reading as intended, with no line after it that inih would read as
// its value instead
static bool proven(const IniDoc *doc, int at, const char *section, const char *key, const char *value, int keys_before)
{
    const char *read_back = inidoc_get(doc, section, key);
    return count_keys(doc) == keys_before + 1 && end_of_key(doc, at) == at && last_setter(doc, section, key) == at &&
           read_back != NULL && strcmp(read_back, value) == 0;
}

// A function to try a new key's line at one place, keeping it only when it is proven
static bool try_place(IniDoc *doc, int at, const char *text, const char *section, const char *key, const char *value)
{
    int keys_before = count_keys(doc);
    if (!insert_line(doc, at, text))
        return refuse(doc, OUT_OF_MEMORY);
    if (proven(doc, at, section, key, value, keys_before))
        return true;
    remove_line(doc, at);
    return refuse(doc, NO_SAFE_PLACE);
}

// A function to add a missing section at the end, after a blank line, with the key's line in it
static bool add_section(IniDoc *doc, const char *text, const char *section, const char *key, const char *value)
{
    size_t header_size = strlen(section) + 3;
    if (header_size - 1 > INIDOC_MAX_LINE)
        return refuse(doc, SECTION_TOO_LONG);
    char *header = alloc_malloc(header_size);
    if (header == NULL)
        return refuse(doc, OUT_OF_MEMORY);
    snprintf(header, header_size, "[%s]", section);
    int lines_before = doc->count;
    bool ok = true;
    if (doc->count > 0 && doc->lines[doc->count - 1].text[0] != '\0')
        ok = insert_line(doc, doc->count, "");
    ok = ok && insert_line(doc, doc->count, header);
    alloc_free(header);
    if (!ok)
        refuse(doc, OUT_OF_MEMORY);
    else
        ok = try_place(doc, doc->count, text, section, key, value);

    // On failure take back every line added: the header and the blank line before it
    while (!ok && doc->count > lines_before)
        remove_line(doc, doc->count - 1);
    return ok;
}

// A function to set a key's value: in its line when it exists, else on a new line placed as asked.
// A new line goes directly under the header or after the section's last key and its continuation
// lines; where that would change how any line reads (an indented line under the header would become
// its continuation, say), it goes after the last key instead, and failing that at the end of the
// section. It is refused only when no place is safe. inidoc_why() says why a set failed.
bool inidoc_set(IniDoc *doc, const char *section, const char *key, const char *value, IniDocPlacement placement)
{
    doc->why = "";
    const char *reason = inidoc_check(key, value);
    if (reason != NULL)
        return refuse(doc, reason);
    int existing = find_key(doc, section, key);
    if (existing >= 0)
        return replace_value(doc, existing, section, key, value);

    size_t size = strlen(key) + strlen(value) + 2;
    char *text = alloc_malloc(size);
    if (text == NULL)
        return refuse(doc, OUT_OF_MEMORY);
    snprintf(text, size, "%s=%s", key, value);
    int header = find_header(doc, section);
    bool ok = false;
    if (header < 0)
        ok = add_section(doc, text, section, key, value);
    else {
        // The places to try, in order; the lines do not move between tries, since a failed try is
        // taken back whole
        int after_last_key = -1;
        for (int i = 0; i < doc->count; i++) {
            const Line *line = &doc->lines[i];
            if (line->kind == LINE_KEY && same_section(section_name(doc, line->section), section))
                after_last_key = end_of_key(doc, i) + 1;
        }
        int section_end = header + 1;
        while (section_end < doc->count && doc->lines[section_end].kind != LINE_SECTION)
            section_end++;
        int places[3];
        int count = 0;
        if (placement == INIDOC_UNDER_HEADER || after_last_key < 0)
            places[count++] = header + 1;
        if (after_last_key >= 0)
            places[count++] = after_last_key;
        places[count++] = section_end;
        for (int i = 0; i < count && !ok && doc->why != OUT_OF_MEMORY; i++) {
            if (i == 0 || places[i] != places[i - 1])
                ok = try_place(doc, places[i], text, section, key, value);
        }
    }
    alloc_free(text);
    if (ok)
        doc->why = "";
    return ok;
}

// A function to say why the last set, add or list-mode remove failed; "" when it succeeded
const char *inidoc_why(const IniDoc *doc)
{
    return doc->why;
}

// A function to say, before anything is set, why inidoc_set() would refuse `key=value` in a
// section of this document, or NULL when it would not: inidoc_check()'s reasons, then the file's
// own line for the key, which may be too long with its comment, or a new section's header. It
// changes nothing, so the folder browser can refuse a path with the save's own reason.
const char *inidoc_check_in(const IniDoc *doc, const char *section, const char *key, const char *value)
{
    const char *reason = inidoc_check(key, value);
    if (reason != NULL)
        return reason;
    int i = find_key(doc, section, key);
    if (i >= 0)
        return too_long_in_line(&doc->lines[i], doc->lines[i].value_start, strlen(value));
    if (find_header(doc, section) < 0 && strlen(section) + 2 > INIDOC_MAX_LINE)
        return SECTION_TOO_LONG;
    return NULL;
}

// A function to remove every line that sets a key in a section, with its continuation lines, so
// none is left for inih to read as the value of the key before it. Continuation lines that inih
// reads as this key's value after a longer key (see continues()) go too. A key with an empty name
// is refused: nothing continues it (inih empties prev_name), so the indented lines after it are
// lines of their own, and with it gone they would join the key before it.
bool inidoc_remove(IniDoc *doc, const char *section, const char *key)
{
    bool removed = false;
    int i;
    if (key[0] == '\0')
        return false;
    while ((i = find_key(doc, section, key)) >= 0) {
        remove_continuations(doc, i);
        remove_line(doc, i);
        removed = true;
    }
    while ((i = last_setter(doc, section, key)) >= 0) {
        remove_line(doc, i);
        removed = true;
    }
    return removed;
}

// A function to tell whether a key is one a list leaves out (the settings its section also holds)
static bool skipped(const char *key, const char *const *skip)
{
    for (int i = 0; skip != NULL && skip[i] != NULL; i++) {
        if (strcmp(skip[i], key) == 0)
            return true;
    }
    return false;
}

// A function to list a section's key lines in order, every one (a key may repeat), leaving out the
// keys in `skip` (NULL-terminated; NULL for none). It fills at most `max` items and returns how many
// there are. Continuation lines are not listed: they move with their key.
int inidoc_list(const IniDoc *doc, const char *section, const char *const *skip, IniDocItem *items, int max)
{
    int count = 0;
    for (int i = 0; i < doc->count; i++) {
        const Line *line = &doc->lines[i];
        if (line->kind != LINE_KEY || !same_section(section_name(doc, line->section), section) || skipped(line->name, skip))
            continue;
        if (count < max)
            items[count] = (IniDocItem) { .line = i, .key = line->name, .value = line->value, .text = line->text };
        count++;
    }
    return count;
}

// A function to find a section's key line by its whole text; -1 when no line reads so
int inidoc_find_line(const IniDoc *doc, const char *section, const char *text)
{
    size_t length = strlen(text);
    for (int i = 0; i < doc->count; i++) {
        const Line *line = &doc->lines[i];
        if (line->kind == LINE_KEY && line->length == length && memcmp(line->text, text, length) == 0 &&
            same_section(section_name(doc, line->section), section))
            return i;
    }
    return -1;
}

// A function to tell whether a key line has continuation lines after it
static bool has_continuations(const IniDoc *doc, int i)
{
    return end_of_key(doc, i) != i;
}

// A function to set one line of a list section. The same key keeps the line's spacing and trailing
// comment; another key is written as key=value with the comment kept. The line must read back as
// written and stay a key of its own; another key over continuation lines is refused, since they
// would start setting it. So is another key that would take continuation lines it did not have:
// after a key with an empty name, indented lines are keys of their own (inih empties prev_name).
bool inidoc_list_set(IniDoc *doc, int line, const char *key, const char *value)
{
    doc->why = "";
    if (line < 0 || line >= doc->count || doc->lines[line].kind != LINE_KEY)
        return refuse(doc, NOT_A_KEY);
    const char *reason = inidoc_check(key, value);
    if (reason != NULL)
        return refuse(doc, reason);
    Line *old = &doc->lines[line];
    bool same_key = strcmp(old->name, key) == 0;
    // Needed beside the read-back below, which cannot see this case: renamed to an empty name, the
    // key ends its continuation lines (inih empties prev_name), and each becomes a key of its own
    if (!same_key && has_continuations(doc, line))
        return refuse(doc, NO_SAFE_PLACE);
    size_t tail = old->value_start + old->value_length;
    size_t head = same_key ? old->value_start : strlen(key) + 1;
    size_t value_length = strlen(value);
    size_t new_length = head + value_length + (old->length - tail);
    const char *too_long = too_long_in_line(old, head, value_length);
    if (too_long != NULL)
        return refuse(doc, too_long);
    Line updated = *old;   // Keeps the line ending; read_line() replaces the name and value
    updated.text = alloc_malloc(new_length + 1);
    if (updated.text == NULL)
        return refuse(doc, OUT_OF_MEMORY);
    if (same_key)
        memcpy(updated.text, old->text, head);
    else {
        memcpy(updated.text, key, head - 1);
        updated.text[head - 1] = '=';
    }
    memcpy(updated.text + head, value, value_length);
    memcpy(updated.text + head + value_length, old->text + tail, old->length - tail);
    updated.text[new_length] = '\0';
    updated.length = new_length;
    if (!read_line(&updated)) {
        alloc_free(updated.text);
        return refuse(doc, OUT_OF_MEMORY);
    }
    Line before = *old;
    *old = updated;
    classify_all(doc);
    const Line *now = &doc->lines[line];
    if (now->kind != LINE_KEY || strcmp(now->name, key) != 0 || strcmp(now->value, value) != 0 ||
        (!same_key && has_continuations(doc, line))) {
        free_line(&doc->lines[line]);
        doc->lines[line] = before;
        classify_all(doc);
        return refuse(doc, NO_SAFE_PLACE);
    }
    free_line(&before);
    return true;
}

// A function to prove a list line just inserted at `at`: exactly one more key, reading as written,
// with no line after it that inih would read as its value
static bool list_line_proven(const IniDoc *doc, int at, const char *key, const char *value, int keys_before)
{
    const Line *line = &doc->lines[at];
    return count_keys(doc) == keys_before + 1 && line->kind == LINE_KEY && strcmp(line->name, key) == 0 &&
           strcmp(line->value, value) == 0 && end_of_key(doc, at) == at;
}

// A function to add a line to a list section: after its last key and that key's continuation lines,
// else (no key yet) under its header, else at the end of the section; a missing section is added at
// the end of the file. Refused only when no place is safe.
bool inidoc_list_add(IniDoc *doc, const char *section, const char *key, const char *value)
{
    doc->why = "";
    const char *reason = inidoc_check(key, value);
    if (reason != NULL)
        return refuse(doc, reason);
    size_t size = strlen(key) + strlen(value) + 2;
    char *text = alloc_malloc(size);
    if (text == NULL)
        return refuse(doc, OUT_OF_MEMORY);
    snprintf(text, size, "%s=%s", key, value);
    int header = find_header(doc, section);
    bool ok = false;
    if (header < 0)
        ok = add_section(doc, text, section, key, value);
    else {
        int after_last_key = -1;
        for (int i = 0; i < doc->count; i++) {
            const Line *line = &doc->lines[i];
            if (line->kind == LINE_KEY && same_section(section_name(doc, line->section), section))
                after_last_key = end_of_key(doc, i) + 1;
        }
        int section_end = header + 1;
        while (section_end < doc->count && doc->lines[section_end].kind != LINE_SECTION)
            section_end++;
        int places[2] = { after_last_key >= 0 ? after_last_key : header + 1, section_end };
        for (int i = 0; i < 2 && !ok && doc->why != OUT_OF_MEMORY; i++) {
            if (i > 0 && places[i] == places[0])
                break;
            int keys_before = count_keys(doc);
            if (!insert_line(doc, places[i], text))
                refuse(doc, OUT_OF_MEMORY);
            else if (list_line_proven(doc, places[i], key, value, keys_before))
                ok = true;
            else {
                remove_line(doc, places[i]);
                refuse(doc, NO_SAFE_PLACE);
            }
        }
    }
    alloc_free(text);
    if (ok)
        doc->why = "";
    return ok;
}

// A function to remove one line of a list section, with its continuation lines, so none is left for
// inih to read as the value of the key before it. A key with an empty name is refused, as
// inidoc_remove() refuses it: the indented lines after it are keys of their own, and with it gone
// they would become the key before it's continuations. inidoc_why() says why a remove failed.
bool inidoc_list_remove(IniDoc *doc, int line)
{
    doc->why = "";
    if (line < 0 || line >= doc->count || doc->lines[line].kind != LINE_KEY)
        return refuse(doc, NOT_A_KEY);
    if (doc->lines[line].name[0] == '\0')
        return refuse(doc, NO_SAFE_PLACE);
    remove_continuations(doc, line);
    remove_line(doc, line);
    return true;
}

// A function to free a document
void inidoc_free(IniDoc *doc)
{
    if (doc == NULL)
        return;
    for (int i = 0; i < doc->count; i++)
        free_line(&doc->lines[i]);
    alloc_free(doc->lines);
    alloc_free(doc);
}
