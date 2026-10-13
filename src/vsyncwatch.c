#include <string.h>
#include "vsyncwatch.h"

// A function to watch afresh: nothing seen, nothing found
void vsyncwatch_reset(VsyncWatch *watch)
{
    memset(watch, 0, sizeof(*watch));
}

// A function to time a present against the display's frame. With VSync delivered, presents come a
// frame apart; a renderer that reports VSync and does not deliver it (SDL3's Vulkan renderer on a
// swapchain that ignores FIFO, as lavapipe's does under Xvfb) presents as fast as it can draw. A
// present is early when it comes under VSYNCWATCH_EARLY_NUM / VSYNCWATCH_EARLY_DEN (three quarters)
// of a frame after the one before, and only VSYNCWATCH_RUN early ones in a row find it. A present that
// is not early starts the run again.
//
// Why three quarters. The fraction F must lie above 1/2 and below 1:
// - Under 1/2 only, as it was, a renderer presenting at one to two times the display's rate was never
//   found: lavapipe at 1920 x 1080 draws at 60-120 fps on a 60 Hz display, so it went unpaced, and
//   everything then still timed in frames ran fast. At 3/4 anything above 4/3 of the display's rate is found.
// - Real VSync never gives a run. Its presents come a frame apart, and late, never early, apart from
//   two cases. A present that returns late makes the next look early, but the one after is a frame on
//   again, so a run never forms. After a stall, the 2-3 frames a FIFO swapchain queued come back at
//   once, and then the presents are a frame apart again: 2-3 is far short of the run of 60. Sixty
//   presents in a row under F of a frame take under 60 F frames, which delivered VSync cannot give for
//   any F below about 0.95; 3/4 leaves room below that for a timer's jitter.
// - A display running above the rate it reports (one that says 60 Hz, or reports nothing and gets the
//   default, and runs at 85-120 Hz) is found above 4/3 of the reported rate and paced to it: it shows
//   about the reported rate, 60 fps, with some judder where its true rate is not a multiple of that.
//   Nothing is timed in frames (the pad's repeat, the slideshow's fade and the screensaver's dim run in
//   wall time), so this costs smoothness only, never timing; a fraction nearer 1 would pace even a
//   display a few hertz above its report, and 3/4 leaves those to their own VSync.
bool vsyncwatch_present(VsyncWatch *watch, uint64_t now_ns, int rate_hz)
{
    if (watch->undelivered)
        return false;
    uint64_t period_ns = UINT64_C(1000000000) / (uint64_t) rate_hz;   // The display's frame
    uint64_t gap_ns = now_ns - watch->last_ns;
    bool early = watch->started && gap_ns * VSYNCWATCH_EARLY_DEN < period_ns * VSYNCWATCH_EARLY_NUM;

    // The run is measured from the present before its first, with its shortest and longest gap
    if (early && watch->early_run == 0) {
        watch->run_first_ns = watch->last_ns;
        watch->run_min_ns = gap_ns;
        watch->run_max_ns = gap_ns;
    }
    else if (early) {
        watch->run_min_ns = gap_ns < watch->run_min_ns ? gap_ns : watch->run_min_ns;
        watch->run_max_ns = gap_ns > watch->run_max_ns ? gap_ns : watch->run_max_ns;
    }
    watch->started = true;
    watch->last_ns = now_ns;
    watch->early_run = early ? watch->early_run + 1 : 0;
    if (watch->early_run < VSYNCWATCH_RUN)
        return false;
    watch->undelivered = true;
    return true;
}
