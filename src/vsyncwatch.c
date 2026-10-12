#include <string.h>
#include "vsyncwatch.h"

// A function to watch afresh: nothing seen, nothing found
void vsyncwatch_reset(VsyncWatch *watch)
{
    memset(watch, 0, sizeof(*watch));
}

// A function to time a present against the display's frame. With VSync delivered, presents come a
// frame apart; a renderer that reports VSync and does not deliver it (SDL3's Vulkan renderer on a
// swapchain that ignores FIFO, as lavapipe's does under Xvfb) presents as fast as it can draw. Only
// a present under half a frame after the one before is fast, so a late frame, or a display a little
// off the rate it reports, never is; and only VSYNCWATCH_RUN fast ones in a row find it, so the few
// a swapchain lets through early after a stall never do. A present that is not fast starts the run
// again.
bool vsyncwatch_present(VsyncWatch *watch, uint64_t now_ns, uint64_t period_ns)
{
    if (watch->undelivered)
        return false;
    bool fast = watch->started && (now_ns - watch->last_ns) * 2 < period_ns;
    watch->started = true;
    watch->last_ns = now_ns;
    watch->fast_run = fast ? watch->fast_run + 1 : 0;
    if (watch->fast_run < VSYNCWATCH_RUN)
        return false;
    watch->undelivered = true;
    return true;
}
