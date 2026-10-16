// The on-screen keyboard's model (spec "The on-screen keyboard"): its layouts as data tables, which the
// overlay's keyboard can reuse; the highlight's moves between keys; Shift and caps lock; and the text it
// edits, UTF-8 with the cursor on character boundaries, held to a byte limit and to a validator that says
// why Done is disabled. Also the rule a real keyboard's key types by. settings_pickers.c draws it.
// Pure: no SDL, no globals; memory comes from alloc.h.
#ifndef OSK_H
#define OSK_H

#include <stdbool.h>

// SDL3's SDLK_SCANCODE_MASK: a keycode with it set names a key that types nothing (an arrow, F1)
#define OSK_SCANCODE_MASK (1u << 30)

typedef enum { OSK_QWERTY, OSK_ABC } OskLayout;   // The remembered choice; ?123 and àé are pages over either
typedef enum { OSK_PAGE_LETTERS, OSK_PAGE_SYMBOLS, OSK_PAGE_ACCENTS } OskPage;
typedef enum {
    OSK_KEY_CHAR, OSK_KEY_LAYOUT, OSK_KEY_SYMBOLS, OSK_KEY_ACCENTS, OSK_KEY_SHIFT, OSK_KEY_SPACE,
    OSK_KEY_BACKSPACE, OSK_KEY_CURSOR_LEFT, OSK_KEY_CURSOR_RIGHT, OSK_KEY_DONE
} OskKeyKind;
// A key `width` units wide. A row's keys sit end to end, and every row is centered on its page's widest:
// Up and Down go to the key whose center is nearest, the left one of two as near, and the keys are drawn
// where that puts them. `text` is what a character key types, `shifted` what it types with Shift or caps
// lock (NULL: the same); `label` is what the key shows.
typedef struct { OskKeyKind kind; const char *label; const char *text; const char *shifted; float width; } OskKey;
typedef enum { OSK_UP, OSK_DOWN, OSK_LEFT, OSK_RIGHT, OSK_OK, OSK_BACK, OSK_BACKSPACE, OSK_SPACE, OSK_DONE } OskCommand;
typedef enum { OSK_NONE, OSK_MOVED, OSK_CHANGED, OSK_FINISHED, OSK_CANCELED, OSK_ASKED } OskResult;   // ASKED: "Back again to discard"
typedef const char *(*OskCheck)(const char *text, void *context);   // Why Done is disabled, or NULL

typedef struct Osk Osk;
Osk *osk_create(const char *label, const char *initial, int max_bytes, OskCheck check, void *context, OskLayout layout);
void osk_free(Osk *osk);
OskResult osk_command(Osk *osk, OskCommand command);
OskResult osk_insert(Osk *osk, const char *utf8);
bool osk_key_char(unsigned int keycode, char out[5]);   // The real-keyboard rule: false for masked, control, U+007F, U+FFFD
const char *osk_text(const Osk *osk);
int osk_cursor(const Osk *osk);                          // A byte offset on a character boundary
const char *osk_label(const Osk *osk);
const char *osk_why(const Osk *osk);                     // NULL when Done is enabled
OskLayout osk_layout(const Osk *osk);
OskPage osk_page(const Osk *osk);
bool osk_shift(const Osk *osk);
bool osk_caps(const Osk *osk);
int osk_rows(const Osk *osk);
int osk_row_length(const Osk *osk, int row);
const OskKey *osk_key(const Osk *osk, int row, int column);   // NULL off the page; good until the next command
void osk_highlight(const Osk *osk, int *row, int *column);
bool osk_asking(const Osk *osk);                         // "Back again to discard" is showing

#endif
