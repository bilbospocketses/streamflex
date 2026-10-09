// Memory for the pure modules. They allocate and free through these, which use the C library's own
// unless a unit test puts its functions in place to make an allocation fail on purpose. With the C
// library's in place, memory from these may be freed with free(), and memory from malloc() with
// alloc_free(). Pure: no SDL.
#ifndef ALLOC_H
#define ALLOC_H

#include <stddef.h>

typedef struct {
    void *(*reallocate)(void *memory, size_t size);   // As realloc(): NULL `memory` allocates anew
    void (*release)(void *memory);                    // As free(); never given NULL
} AllocHooks;

void *alloc_malloc(size_t size);
void *alloc_calloc(size_t count, size_t size);
void *alloc_realloc(void *memory, size_t size);
char *alloc_strdup(const char *text);
void alloc_free(void *memory);
void alloc_set_hooks(const AllocHooks *hooks);   // Unit tests only; NULL puts the C library's back

#endif
