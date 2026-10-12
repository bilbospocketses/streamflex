// The VSync watch (spec section 3): a renderer that reports VSync but presents faster than the
// display is found by measurement. vsyncwatch.c is pure, so this links it alone, as test_winplan
// links winplan.c.
#include <stdbool.h>
#include <stdint.h>
#include "check.h"
#include "vsyncwatch.h"

#define FRAME 16666667ULL     // A 60 Hz display's frame, in nanoseconds, as its VSync gives presents
#define FRAME_240 4166667ULL  // A 240 Hz display's
#define FAST 1500000ULL       // A present 1.5 ms after the one before, as lavapipe gives them under Xvfb
#define START 5000000000ULL   // Five seconds after SDL started

// A function to feed the watch count presents, each step_ns after the one before (from *now, which
// it moves on), on a display of rate_hz; gives how many of them found VSync not delivered
static int presents_at(VsyncWatch *watch, uint64_t *now, int count, uint64_t step_ns, int rate_hz)
{
    int found = 0;
    for (int i = 0; i < count; i++) {
        *now += step_ns;
        if (vsyncwatch_present(watch, *now, rate_hz))
            found++;
    }
    return found;
}

// The same on a 60 Hz display
static int presents(VsyncWatch *watch, uint64_t *now, int count, uint64_t step_ns)
{
    return presents_at(watch, now, count, step_ns, 60);
}

// Presents a frame apart, as delivered VSync gives them, are never found however many come: at
// 60 Hz, and at 240 Hz, whose frame is a quarter of 60 Hz's
static void test_delivered(void)
{
    VsyncWatch watch;
    uint64_t now = START;
    vsyncwatch_reset(&watch);
    CHECK_INT(presents_at(&watch, &now, 1000, FRAME, 60), 0);
    CHECK(!watch.undelivered);
    vsyncwatch_reset(&watch);
    CHECK_INT(presents_at(&watch, &now, 1000, FRAME_240, 240), 0);
    CHECK(!watch.undelivered);
}

// Presents far under a frame apart are found at the run's last, which is the run's count of presents
// after the first (the first has none before it to be timed from); found once, and never again
static void test_undelivered(void)
{
    VsyncWatch watch;
    uint64_t now = START;
    vsyncwatch_reset(&watch);
    CHECK_INT(presents(&watch, &now, VSYNCWATCH_RUN, FAST), 0);
    CHECK(!watch.undelivered);
    CHECK_INT(presents(&watch, &now, 1, FAST), 1);
    CHECK(watch.undelivered);
    CHECK_INT(presents(&watch, &now, 500, FAST), 0);
    CHECK(watch.undelivered);
}

// A present that comes a frame after the one before starts the run again: one short of the run, then
// a frame, then one short again finds nothing; one more does
static void test_late_present_restarts(void)
{
    VsyncWatch watch;
    uint64_t now = START;
    vsyncwatch_reset(&watch);
    CHECK_INT(presents(&watch, &now, VSYNCWATCH_RUN, FAST), 0);
    CHECK_INT(presents(&watch, &now, 1, FRAME), 0);
    CHECK_INT(presents(&watch, &now, VSYNCWATCH_RUN - 1, FAST), 0);
    CHECK(!watch.undelivered);
    CHECK_INT(presents(&watch, &now, 1, FAST), 1);
}

// Half a frame exactly is not under half a frame, and the frame is the rate's: at 60 Hz (a frame of
// 16,666,666 ns) presents 8,333,333 ns apart are never found and 8,333,332 ns apart are; at 240 Hz
// (4,166,666 ns) the same holds at 2,083,333 and 2,083,332 ns. A frame worked out any other way
// moves one of these edges.
static void test_half_frame(void)
{
    VsyncWatch watch;
    uint64_t now = START;
    vsyncwatch_reset(&watch);
    CHECK_INT(presents_at(&watch, &now, 4 * VSYNCWATCH_RUN, 8333333, 60), 0);
    vsyncwatch_reset(&watch);
    CHECK_INT(presents_at(&watch, &now, VSYNCWATCH_RUN + 1, 8333332, 60), 1);
    vsyncwatch_reset(&watch);
    CHECK_INT(presents_at(&watch, &now, 4 * VSYNCWATCH_RUN, 2083333, 240), 0);
    vsyncwatch_reset(&watch);
    CHECK_INT(presents_at(&watch, &now, VSYNCWATCH_RUN + 1, 2083332, 240), 1);
}

// The first present after a reset has nothing before it, even when SDL's clock reads less than half
// a frame: it starts the run without counting in it
static void test_first_present(void)
{
    VsyncWatch watch;
    uint64_t now = 0;
    vsyncwatch_reset(&watch);
    CHECK_INT(presents(&watch, &now, VSYNCWATCH_RUN, FAST), 0);
    CHECK_INT(presents(&watch, &now, 1, FAST), 1);
}

// A reset forgets what was found: the watch finds it again, once, after a full run
static void test_reset(void)
{
    VsyncWatch watch;
    uint64_t now = START;
    vsyncwatch_reset(&watch);
    CHECK_INT(presents(&watch, &now, VSYNCWATCH_RUN + 1, FAST), 1);
    vsyncwatch_reset(&watch);
    CHECK(!watch.undelivered);
    CHECK_INT(presents(&watch, &now, VSYNCWATCH_RUN, FAST), 0);
    CHECK_INT(presents(&watch, &now, 1, FAST), 1);
    CHECK_INT(presents(&watch, &now, 100, FAST), 0);
}

int main(void)
{
    test_delivered();
    test_undelivered();
    test_late_present_restarts();
    test_half_frame();
    test_first_present();
    test_reset();
    return check_report();
}
