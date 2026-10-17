// Matching an installed app to a library icon by its name (spec "Icons", choice 1). Pure: no SDL, no
// allocation, so tests/test_iconmatch.c builds it with the library alone.
#ifndef ICONMATCH_H
#define ICONMATCH_H

#include <stddef.h>
#include "library.h"

#define ICONMATCH_KEY_MAX 256   // The most of a name compared, normalized; both sides are cut alike

void iconmatch_normalize(const char *name, char *out, size_t size);   // Lower case, no punctuation, vendor words dropped
int iconmatch_find(const char *app_name, const LibraryInfo *icons, int count);   // Index, or -1

#endif
