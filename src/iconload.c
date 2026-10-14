// Parallel icon decode (spec section 8): a batch of jobs run on a few SDL threads, each claiming the
// next unclaimed job, and the batch returns only when every job has posted. The jobs touch nothing
// but themselves; the caller makes the textures afterward, on the main thread.
#include <stdio.h>
#include <SDL3/SDL.h>
#include "launcher.h"
#include "debug.h"
#include "iconload.h"

#define ICONLOAD_MAX_WORKERS 4

typedef struct {
    IconJob *jobs;
    int count;
    SDL_AtomicInt next;   // The next unclaimed job
    IconWork work;
    IconWorkerStart start;
    IconWorkerEnd end;
} Batch;

// A function to work out how many decode threads a machine with this many logical cores gets: one
// fewer than its cores, so the main thread keeps one, and at least 1 and at most 4
int iconload_worker_count(int logical_cores)
{
    int workers = logical_cores - 1;
    if (workers < 1)
        workers = 1;
    if (workers > ICONLOAD_MAX_WORKERS)
        workers = ICONLOAD_MAX_WORKERS;
    return workers;
}

// A function to say how many decode threads this machine gets, worked out and logged once
int iconload_workers(void)
{
    static int workers = 0;
    if (workers == 0) {
        workers = iconload_worker_count(SDL_GetNumLogicalCPUCores());
        log_debug("Icons: decoding on %i threads", workers);
    }
    return workers;
}

// A function to run jobs until none is left unclaimed, with one worker state for the run
static void run_jobs(Batch *batch)
{
    void *state = batch->start != NULL ? batch->start() : NULL;
    for (;;) {
        int i = SDL_AddAtomicInt(&batch->next, 1);   // Returns the value before the add
        if (i >= batch->count)
            break;
        batch->work(&batch->jobs[i], state);
        batch->jobs[i].done = true;
    }
    if (batch->end != NULL)
        batch->end(state);
}

static int worker_main(void *data)
{
    run_jobs((Batch *) data);
    return 0;
}

void iconload_batch(IconJob *jobs, int count, int workers, IconWork work, IconWorkerStart start, IconWorkerEnd end)
{
    if (count <= 0)
        return;
    Batch batch = { .jobs = jobs, .count = count, .work = work, .start = start, .end = end };
    SDL_SetAtomicInt(&batch.next, 0);
    if (workers > ICONLOAD_MAX_WORKERS)
        workers = ICONLOAD_MAX_WORKERS;
    if (workers > count)
        workers = count;
    SDL_Thread *threads[ICONLOAD_MAX_WORKERS] = { NULL };
    int running = 0;
    bool no_threads = false;
#ifdef STREAMFLEX_TEST_HOOKS
    // Only test builds have this: no decode thread starts, as when the system refuses one
    no_threads = SDL_GetHintBoolean("STREAMFLEX_TEST_ICONLOAD_NO_THREADS", false);
#endif
    for (int i = 0; i < workers && !no_threads; i++) {
        threads[i] = SDL_CreateThread(worker_main, "Icon decode", &batch);
        if (threads[i] != NULL)
            running++;
    }
    if (running == 0) {
        log_error("Icons: no decode thread started, decoding on the main thread\n%s",
                  no_threads ? "Test hook: no thread starts" : SDL_GetError());
        run_jobs(&batch);
    }
    for (int i = 0; i < workers; i++) {
        if (threads[i] != NULL)
            SDL_WaitThread(threads[i], NULL);
    }
}
