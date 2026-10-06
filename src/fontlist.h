// The font picker's list: font faces, as a scan found them, grouped into families by name (ignoring
// case), each family writing its Regular face, else the first face seen. The bundled fonts sort
// first, then every family by name. Pure: no SDL, no globals; memory comes from alloc.h.
#ifndef FONTLIST_H
#define FONTLIST_H

#include <stdbool.h>

typedef struct FontList FontList;

FontList *fontlist_create(void);
void fontlist_free(FontList *list);
bool fontlist_add(FontList *list, const char *path, int face, const char *family, const char *style, bool bundled);
void fontlist_finish(FontList *list);                 // Sorts the families: bundled first, then by name
int fontlist_count(const FontList *list);
const char *fontlist_family(const FontList *list, int index);
const char *fontlist_path(const FontList *list, int index);   // The face a family writes: Regular, else its first
int fontlist_face(const FontList *list, int index);
int fontlist_find(const FontList *list, const char *path, int face);   // The family holding a face; -1

#endif
