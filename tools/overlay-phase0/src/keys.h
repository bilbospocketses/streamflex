/* The keys the probe can send: one name, its Windows virtual key and its Linux evdev code. */
#ifndef PROBE_KEYS_H
#define PROBE_KEYS_H

#include <stddef.h>

#define PROBE_MAX_CHORD 4
#define PROBE_KEYLOG_MAX_S 2147483L /* SetTimer's longest wait (USER_TIMER_MAXIMUM), in whole seconds */

typedef struct {
    const char *name;
    unsigned short vk;
    unsigned short evdev;
    int extended; /* Windows: send with KEYEVENTF_EXTENDEDKEY */
} probe_key;

/* What a platform gives probe_keys_main. open runs only for a real send, after every chord has been checked; it
   returns 0, or prints its own failure line and returns nonzero, and keys then exits 1 with no key lines. send
   prints a send_failed line for a key that did not go out, and the platform's probe_keys then exits 1 once every
   chord is done (its key-ups still go out). open and close may be NULL. */
typedef struct {
    int (*open)(void);
    void (*send)(const probe_key *k, int up);
    void (*sleep_ms)(long ms);
    void (*close)(void);
} probe_key_sender;

extern const probe_key probe_keys_table[];
extern const unsigned probe_keys_count;

const probe_key *probe_key_find(const char *name);

/* Splits "Alt+F4" into keys; returns the count, or -1 with bad set to the unknown (or empty, or fifth) part. */
int probe_parse_chord(const char *arg, const probe_key **out, char *bad, size_t bad_cap);

/* Parses s as a whole decimal number from 0 to max; returns 0 with *out set, or -1 for an empty value, trailing junk,
   a negative value or one above max. */
int probe_parse_count(const char *s, long max, long *out);

/* The keys command, [--dry-run] <delay_ms> <chord>...: refuses the whole command before sending anything when any
   chord has an unknown part. */
int probe_keys_main(int argc, char **argv, const probe_key_sender *sender);

#endif
