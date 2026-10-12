// Whether a renderer that reports VSync presents at the display's pace (spec section 3): pure, no
// SDL, so tests/test_vsyncwatch.c builds it on its own, as winplan.c is built
#ifndef VSYNCWATCH_H
#define VSYNCWATCH_H

#include <stdbool.h>
#include <stdint.h>

// The presents in a row, each under half the display's frame after the one before, that show VSync is
// not delivered: a second's worth at 60 Hz. With VSync delivered, only the few frames a swapchain
// queues can come early, after a stall; never this many.
#define VSYNCWATCH_RUN 60

typedef struct {
    bool started;       // A present has been seen since the reset, at last_ns
    uint64_t last_ns;   // When the last present returned
    int fast_run;       // The presents in a row that came under half the display's frame
    bool undelivered;   // Found: the renderer does not deliver the VSync it reports
} VsyncWatch;

// Watch afresh, as after a new renderer or a new VSync wish
void vsyncwatch_reset(VsyncWatch *watch);
// A present that returned at now_ns, on a display of rate_hz (at least 1, as derive_refresh_rate()
// gives it): true once, at the present that finds VSync not delivered; false before it, and after
bool vsyncwatch_present(VsyncWatch *watch, uint64_t now_ns, int rate_hz);

#endif
