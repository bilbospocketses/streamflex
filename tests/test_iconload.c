// The job table behind parallel icon decode (spec section 8): the worker count, every job claimed
// exactly once, the batch returning only when every job has posted, and the main thread running the
// batch when no thread starts. It links SDL for its threads, and stands in for the launcher's log.
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "check.h"
#include "launcher.h"
#include "debug.h"
#include "iconload.h"

// The launcher's log, which iconload.c's log_debug and log_error (debug.h's macros) write through.
// Only the main thread logs; this keeps the count of each level and the last line of each.
static int debug_lines = 0, error_lines = 0;
static char last_debug[256], last_error[256];

void output_log(LogLevel log_level, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    if (log_level == LOGLEVEL_DEBUG) {
        debug_lines++;
        vsnprintf(last_debug, sizeof(last_debug), format, args);
    }
    else {
        error_lines++;
        vsnprintf(last_error, sizeof(last_error), format, args);
    }
    va_end(args);
}

static SDL_AtomicInt started;      // Worker states made
static SDL_AtomicInt ended;        // Worker states ended
static SDL_AtomicInt wrong_state;  // Jobs that were handed another state than their worker's own

static void *fake_start(void) { SDL_AddAtomicInt(&started, 1); return &started; }
static void fake_end(void *state) { (void) state; SDL_AddAtomicInt(&ended, 1); }

// A job that takes a little time, so the workers overlap and a double claim would show. It runs on
// the workers, so what it finds wrong is counted atomically, and checked once the batch is back.
static void fake_work(IconJob *job, void *state)
{
    if (state != &started)
        SDL_AddAtomicInt(&wrong_state, 1);
    job->claims++;
    SDL_Delay((Uint32) (1 + job->size % 3));
    job->done = true;
}

static void test_worker_count(void)
{
    static const int cores[]   = { -1, 0, 1, 2, 3, 4, 5, 6, 16 };
    static const int workers[] = {  1, 1, 1, 1, 2, 3, 4, 4,  4 };
    for (size_t i = 0; i < sizeof(cores) / sizeof(cores[0]); i++) {
        printf("worker count for %i cores: %i, wanted %i\n", cores[i], iconload_worker_count(cores[i]), workers[i]);
        CHECK_INT(iconload_worker_count(cores[i]), workers[i]);
    }
}

// The machine's own count is worked out and logged once, however often it is asked for
static void test_workers_logged_once(void)
{
    int before = debug_lines;
    int first = iconload_workers();
    int second = iconload_workers();
    char wanted[64];
    snprintf(wanted, sizeof(wanted), "Icons: decoding on %i threads" endline, first);
    printf("this machine: %i logical cores, %i decode threads\n", SDL_GetNumLogicalCPUCores(), first);
    CHECK_INT(first, iconload_worker_count(SDL_GetNumLogicalCPUCores()));
    CHECK_INT(second, first);
    CHECK_INT(debug_lines - before, 1);
    CHECK_STR(last_debug, wanted);
}

// A function to run a batch of COUNT fake jobs on THREADS workers, and check that each was claimed
// exactly once and had posted when the batch returned, that every worker state made was ended, and
// that the batch logged ERRORS errors
static void test_batch(int count, int threads, int errors)
{
    static IconJob jobs[200];
    memset(jobs, 0, sizeof(jobs));
    for (int i = 0; i < count; i++)
        jobs[i].size = i;
    SDL_SetAtomicInt(&started, 0);
    SDL_SetAtomicInt(&ended, 0);
    SDL_SetAtomicInt(&wrong_state, 0);
    int errors_before = error_lines;
    iconload_batch(jobs, count, threads, fake_work, fake_start, fake_end);
    int once = 0, posted = 0;
    for (int i = 0; i < count; i++) {
        once += jobs[i].claims == 1;
        posted += jobs[i].done;
    }
    printf("%i jobs on %i threads: %i claimed exactly once, %i posted, %i worker states made, %i ended\n",
           count, threads, once, posted, SDL_GetAtomicInt(&started), SDL_GetAtomicInt(&ended));
    CHECK_INT(once, count);
    CHECK_INT(posted, count);
    CHECK_INT(SDL_GetAtomicInt(&ended), SDL_GetAtomicInt(&started));
    CHECK_INT(SDL_GetAtomicInt(&wrong_state), 0);
    CHECK_INT(error_lines - errors_before, errors);
}

int main(void)
{
    if (!SDL_Init(0)) {
        printf("FAIL  SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    test_worker_count();
    test_workers_logged_once();
    test_batch(200, 4, 0);
    test_batch(3, 4, 0);     // Fewer jobs than threads
    test_batch(1, 1, 0);
    test_batch(0, 2, 0);     // An empty batch returns at once
    // No thread starts: the main thread runs the whole batch, and says so once
    SDL_SetHint("STREAMFLEX_TEST_ICONLOAD_NO_THREADS", "1");
    test_batch(50, 4, 1);
    CHECK_STR(last_error, "Icons: no decode thread started, decoding on the main thread\nTest hook: no thread starts" endline);
    SDL_ResetHint("STREAMFLEX_TEST_ICONLOAD_NO_THREADS");
    SDL_Quit();
    return check_report();
}
