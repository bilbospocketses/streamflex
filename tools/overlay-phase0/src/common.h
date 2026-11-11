/* Shared helpers for the Phase 0 probe kit. */
#ifndef PROBE_COMMON_H
#define PROBE_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Wall-clock time in milliseconds since the Unix epoch. */
int64_t probe_now_ms(void);

/* Milliseconds on a monotonic clock, for intervals; the event lines' t stays wall time. */
int64_t probe_mono_ms(void);

/* fopen(path, "rb") for a UTF-8 path, on Windows too. */
FILE *probe_fopen_utf8(const char *path);

/* Print one JSON event line, {"t":<now>,<fields>}, to stdout and flush it. */
void probe_emit(const char *fields);

/* Write s into out as a quoted JSON string (a NULL s is ""). Stops early rather than overflow, so out is always a
   whole quoted string, cut between UTF-8 characters, never inside one; cap must be at least 3. */
void probe_json_string(char *out, size_t cap, const char *s);

#endif
