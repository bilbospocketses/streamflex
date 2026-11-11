/* probe_spawn text: type a string as trusted keystrokes. */
#ifndef PROBE_TEXT_H
#define PROBE_TEXT_H

#include <stddef.h>
#include <stdint.h>

#define PROBE_TEXT_MAX 256 /* characters, as the spec's send_text allows a message */

/* Decodes n bytes of UTF-8; returns the count of code points, -1 for a malformed sequence (an overlong form, a
   surrogate, a value above U+10FFFF, a stray or cut byte), or -2 for more than cap code points. */
int probe_utf8_decode(const char *s, size_t n, uint32_t *out, int cap);

/* Writes cp as UTF-8 and a NUL into out (5 bytes); returns the length. */
int probe_utf8_encode(uint32_t cp, char *out);

/* A control character the command refuses: U+0000-U+001F and U+007F-U+009F. */
int probe_text_is_control(uint32_t cp);

/* What a platform gives probe_text_main.
   arg: argument i of the command (after "text") as UTF-8, or NULL when it is not valid text.
   prepare: learns the layout from layout (--layout's value, or NULL), printing a layout line; returns 0, or prints why
     not and returns the exit code.
   plan: for every character before anything is typed: returns 1 and writes the char line's platform fields (JSON,
     no leading comma) into detail, or returns 0 and writes the reason in words.
   open (may be NULL), type, close (may be NULL): only for a real send. open returns 0 or prints its own failure
     line; type returns 1 when every event went out, else prints send_failed and returns 0. */
typedef struct {
    const char *(*arg)(int argc, char **argv, int i);
    int (*prepare)(const char *layout);
    int (*plan)(uint32_t cp, char *detail, size_t cap);
    int (*open)(void);
    int (*type)(uint32_t cp);
    void (*sleep_ms)(long ms);
    void (*close)(void);
} probe_text_platform;

extern const probe_text_platform probe_text_platform_impl;

/* text [--dry-run] [--layout <layout>[(<variant>)]] <delay_ms> (<string> | --file <path>). Exit: 0 every character
   typed; 3 some named untyped and the rest typed; 2 refused before anything was typed; 1 a setup or send failure. */
int probe_text_main(int argc, char **argv, const probe_text_platform *p);

#endif
