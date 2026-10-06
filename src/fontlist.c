#include <stdlib.h>
#include <string.h>
#include "fontlist.h"
#include "alloc.h"

typedef struct {
    char *name;
    char *path;       // The face it writes
    int face;
    bool regular;     // That face is its Regular
    bool bundled;     // One of its faces is a bundled font
} Family;

typedef struct {
    char *path;
    int face;
    int family;       // Its family's index, before the sort
} Face;

typedef struct {
    int family;       // A family's index, before the sort...
    const char *name; // ...and what it sorts by, so the comparison needs nothing else
    bool bundled;
} Place;

struct FontList {
    Family *families;
    int family_count;
    int family_capacity;
    Face *faces;
    int face_count;
    int face_capacity;
    Place *order;     // After fontlist_finish(): the families in sorted order; NULL, the order found
};

// A function to lower-case an ASCII letter
static int lower(char c)
{
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : (unsigned char) c;
}

// A function to compare two names without regard to ASCII case
static int compare_names(const char *a, const char *b)
{
    while (*a != '\0' && lower(*a) == lower(*b)) {
        a++;
        b++;
    }
    return lower(*a) - lower(*b);
}

// A function to make an empty list; NULL when out of memory
FontList *fontlist_create(void)
{
    return alloc_calloc(1, sizeof(FontList));
}

// A function to free the list
void fontlist_free(FontList *list)
{
    if (list == NULL)
        return;
    for (int i = 0; i < list->family_count; i++) {
        alloc_free(list->families[i].name);
        alloc_free(list->families[i].path);
    }
    for (int i = 0; i < list->face_count; i++)
        alloc_free(list->faces[i].path);
    alloc_free(list->families);
    alloc_free(list->faces);
    alloc_free(list->order);
    alloc_free(list);
}

// A function to make room for one more face; false when out of memory, with the faces as they were
static bool grow_faces(FontList *list)
{
    if (list->face_count < list->face_capacity)
        return true;
    int bigger = list->face_capacity ? list->face_capacity * 2 : 32;
    Face *more = alloc_realloc(list->faces, (size_t) bigger * sizeof(Face));
    if (more == NULL)
        return false;
    list->faces = more;
    list->face_capacity = bigger;
    return true;
}

// A function to make room for one more family; false when out of memory, with the families as they were
static bool grow_families(FontList *list)
{
    if (list->family_count < list->family_capacity)
        return true;
    int bigger = list->family_capacity ? list->family_capacity * 2 : 32;
    Family *more = alloc_realloc(list->families, (size_t) bigger * sizeof(Family));
    if (more == NULL)
        return false;
    list->families = more;
    list->family_capacity = bigger;
    return true;
}

// A function to add a face: to its family, found by name, or to a new one. A Regular face becomes
// the one its family writes, in place of any other. False when out of memory, with nothing added.
bool fontlist_add(FontList *list, const char *path, int face, const char *family, const char *style, bool bundled)
{
    bool regular = compare_names(style, "Regular") == 0;
    int index = -1;
    for (int i = 0; i < list->family_count && index < 0; i++) {
        if (compare_names(list->families[i].name, family) == 0)
            index = i;
    }
    if (!grow_faces(list))
        return false;
    char *face_path = alloc_strdup(path);
    char *family_path = alloc_strdup(path);
    char *name = index < 0 ? alloc_strdup(family) : NULL;
    bool ok = face_path != NULL && family_path != NULL && (index >= 0 || name != NULL);
    if (ok && index < 0)
        ok = grow_families(list);
    if (!ok) {
        alloc_free(face_path);
        alloc_free(family_path);
        alloc_free(name);
        return false;
    }
    if (index < 0) {
        index = list->family_count++;
        list->families[index] = (Family) { .name = name, .path = family_path, .face = face, .regular = regular,
                                           .bundled = bundled };
    }
    else {
        Family *f = &list->families[index];
        f->bundled = f->bundled || bundled;
        if (regular && !f->regular) {
            alloc_free(f->path);
            f->path = family_path;
            f->face = face;
            f->regular = true;
        }
        else
            alloc_free(family_path);
    }
    list->faces[list->face_count++] = (Face) { .path = face_path, .face = face, .family = index };
    return true;
}

// A function to order two families: bundled first, then by name (two families never share a name)
static int compare_places(const void *a, const void *b)
{
    const Place *x = a;
    const Place *y = b;
    if (x->bundled != y->bundled)
        return x->bundled ? -1 : 1;
    return compare_names(x->name, y->name);
}

// A function to put the families in order once every face is added. Out of memory, they keep the
// order they were found in.
void fontlist_finish(FontList *list)
{
    alloc_free(list->order);
    list->order = alloc_calloc((size_t) (list->family_count > 0 ? list->family_count : 1), sizeof(Place));
    if (list->order == NULL)
        return;
    for (int i = 0; i < list->family_count; i++)
        list->order[i] = (Place) { .family = i, .name = list->families[i].name, .bundled = list->families[i].bundled };
    qsort(list->order, (size_t) list->family_count, sizeof(Place), compare_places);
}

// A function to count the families
int fontlist_count(const FontList *list)
{
    return list->family_count;
}

// A function to find a family by its place in the sorted list
static const Family *family_at(const FontList *list, int index)
{
    if (index < 0 || index >= list->family_count)
        return NULL;
    return &list->families[list->order != NULL ? list->order[index].family : index];
}

// A function to get a family's name
const char *fontlist_family(const FontList *list, int index)
{
    const Family *f = family_at(list, index);
    return f != NULL ? f->name : NULL;
}

// A function to get the file a family writes
const char *fontlist_path(const FontList *list, int index)
{
    const Family *f = family_at(list, index);
    return f != NULL ? f->path : NULL;
}

// A function to get the face within that file
int fontlist_face(const FontList *list, int index)
{
    const Family *f = family_at(list, index);
    return f != NULL ? f->face : 0;
}

// A function to find the family that holds a face, by its place in the sorted list; -1 for none
int fontlist_find(const FontList *list, const char *path, int face)
{
    int family = -1;
    for (int i = 0; i < list->face_count && family < 0; i++) {
        if (list->faces[i].face == face && strcmp(list->faces[i].path, path) == 0)
            family = list->faces[i].family;
    }
    for (int i = 0; family >= 0 && i < list->family_count; i++) {
        if ((list->order != NULL ? list->order[i].family : i) == family)
            return i;
    }
    return -1;
}
