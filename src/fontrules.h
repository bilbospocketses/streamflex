// Pure rules for fallback fonts (spec section 4), with no SDL so tests/test_fontrules.c links them
// alone: the code points a font set never looks up, the spaces and the blank Braille pattern a
// fallback draws as nothing on purpose, where a provider goes in a set's chain, the face as the log
// writes it, and Windows' fallback families: their order and their faces in the Fonts registry key
#ifndef FONTRULES_H
#define FONTRULES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool fontrules_skip(uint32_t code_point);
bool fontrules_blank(uint32_t code_point);
int fontrules_insert_at(const int *ranks, int count, int rank);
void fontrules_face_text(int index, char *out, size_t size);

#define FONTRULES_WINDOWS_FAMILIES 17

const char *fontrules_cjk_first(const char *language, const char *country);
int fontrules_windows_families(const char *language, const char *country, const char **out);
int fontrules_registry_face(const char *value_name, const char *family);

#endif
