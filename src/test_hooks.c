#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
// The hooks use no SDL: SDL.h and launcher.h are here only because util.h (UNUSED) and debug.h
// (log_debug) declare functions with their types. A hook for a pure module belongs in that module.
#include <SDL.h>
#include "launcher.h"
#include "test_hooks.h"
#include "alloc.h"
#include "fileio.h"
#include "util.h"
#include "debug.h"

#ifdef STREAMFLEX_TEST_HOOKS
// Only the headless harness builds these. STREAMFLEX_TEST_FAIL names one of the settings screen's
// steps (places, browser, command or keep; the pickers' list, rows, select or pads; the font
// picker's fontlist, fontscan or faces), which then runs
// as if memory had run out, or for keep, as if the saved file's permissions could not be kept:
// failures no real run can be made to give.

// A function standing in for realloc that always fails
static void *failing_reallocate(void *memory, size_t size)
{
    UNUSED(memory);
    UNUSED(size);
    return NULL;
}

// A function standing in for free
static void releasing(void *memory)
{
    free(memory);
}

// A function to start (true) or end (false) the failure the harness asked for, when it names `step`
void test_fail(const char *step, bool on)
{
    static const AllocHooks failing = { failing_reallocate, releasing };
    const char *asked = getenv("STREAMFLEX_TEST_FAIL");
    if (asked == NULL || strcmp(asked, step) != 0)
        return;
    if (strcmp(step, "keep") == 0)
        fileio_set_fault(on ? FILEIO_FAULT_KEEP : FILEIO_FAULT_NONE, 0, 0);
    else
        alloc_set_hooks(on ? &failing : NULL);
    if (on)
        log_debug("Test hook: %s fails", step);
}
#endif
