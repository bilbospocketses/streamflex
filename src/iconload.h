// Parallel icon decode (spec section 8): the job table and the worker pool. SDL threads, no image
// code: what a job does is the caller's (image.c's decode_icon_job()).
#ifndef ICONLOAD_H
#define ICONLOAD_H

#include <stdbool.h>
#include <SDL3/SDL.h>

typedef enum {
    ICON_KEPT_OFF,       // The chroma pass ran, or had nothing to do (Windows, an SVG, a failed decode)
    ICON_NOT_CONVERTED,  // Linux: the surface could not be converted to RGBA32, so it is drawn as it decoded
    ICON_NOT_LOCKED      // Linux: the surface could not be locked, so its pixels were not read
} IconKeptOff;

typedef struct {
    const char *path;      // In: the icon file, already past the main thread's fileio_not_a_file() guard
    int size;              // In: the button size, which an SVG is rasterized to
    bool svg;              // In: rasterize instead of decode
    SDL_Surface *surface;  // Out: NULL when it failed
    char error[256];       // Out: why it failed, or why the chroma pass did not run, copied on the worker (SDL's error is per thread)
    int moved;             // Out: pixels the chroma pass moved off the key (Linux); logged by the main thread
    IconKeptOff kept_off;  // Out: whether the chroma pass ran; the main thread logs why not
    Uint64 read, decode;   // Out: performance-counter counts
    int claims;            // Out: how many workers claimed it; exactly 1 (the job table's tests read it)
    bool done;             // Out: posted
} IconJob;

typedef void (*IconWork)(IconJob *job, void *worker_state);
typedef void *(*IconWorkerStart)(void);           // A worker's own state (its SVG rasterizer); may return NULL
typedef void (*IconWorkerEnd)(void *worker_state);

int iconload_worker_count(int logical_cores);      // clamp(cores - 1, 1, 4); a core count below 1 gives 1
int iconload_workers(void);                        // iconload_worker_count(SDL_GetNumLogicalCPUCores()), worked out once
// Runs every job once on `workers` SDL threads, each claiming the next unclaimed job, and returns only
// when every job has posted. A thread that cannot start leaves its share to the others, and when none
// starts the main thread runs every job itself, logged once.
void iconload_batch(IconJob *jobs, int count, int workers, IconWork work, IconWorkerStart start, IconWorkerEnd end);

#endif
