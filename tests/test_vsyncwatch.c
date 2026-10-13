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

// Three quarters of a frame exactly is not under three quarters of a frame, and the frame is the rate's:
// at 60 Hz (a frame of 16,666,666 ns, three quarters of it 12,499,999.5) presents 12,500,000 ns apart are
// never found and 12,499,999 ns apart are; at 240 Hz (4,166,666 ns, 3,124,999.5) the same holds at
// 3,125,000 and 3,124,999 ns. A frame or a fraction worked out any other way moves one of these edges.
static void test_early_fraction(void)
{
    VsyncWatch watch;
    uint64_t now = START;
    vsyncwatch_reset(&watch);
    CHECK_INT(presents_at(&watch, &now, 4 * VSYNCWATCH_RUN, 12500000, 60), 0);
    vsyncwatch_reset(&watch);
    CHECK_INT(presents_at(&watch, &now, VSYNCWATCH_RUN + 1, 12499999, 60), 1);
    vsyncwatch_reset(&watch);
    CHECK_INT(presents_at(&watch, &now, 4 * VSYNCWATCH_RUN, 3125000, 240), 0);
    vsyncwatch_reset(&watch);
    CHECK_INT(presents_at(&watch, &now, VSYNCWATCH_RUN + 1, 3124999, 240), 1);
}

// The band the watch once missed: presents between one and two times the display's rate, as lavapipe
// gives them at 1920 x 1080. At 90 and 120 presents a second on a 60 Hz display, both found at the run's
// last; under half a frame, the old rule, neither ever was.
static void test_blind_band(void)
{
    VsyncWatch watch;
    uint64_t now = START;
    vsyncwatch_reset(&watch);
    CHECK_INT(presents(&watch, &now, VSYNCWATCH_RUN, 11111111), 0);
    CHECK_INT(presents(&watch, &now, 1, 11111111), 1);
    vsyncwatch_reset(&watch);
    CHECK_INT(presents(&watch, &now, VSYNCWATCH_RUN, 8333333), 0);
    CHECK_INT(presents(&watch, &now, 1, 8333333), 1);
}

// Delivered VSync that looks early now and then is never found: a present that returns late makes the
// next one look early, but the one after is a frame on, which starts the run again; and after a stall the
// few frames a swapchain queues come back at once, then the frames are a frame apart again
static void test_jitter_and_stalls(void)
{
    VsyncWatch watch;
    uint64_t now = START;
    int found = 0;
    vsyncwatch_reset(&watch);
    for (int i = 0; i < 500; i++) {
        found += presents(&watch, &now, 1, 22000000);
        found += presents(&watch, &now, 1, 11333333);
    }
    CHECK_INT(found, 0);
    vsyncwatch_reset(&watch);
    for (int i = 0; i < 20; i++) {
        found += presents(&watch, &now, 1, 100000000);
        found += presents(&watch, &now, 3, 500000);
        found += presents(&watch, &now, VSYNCWATCH_RUN - 4, FRAME);
    }
    CHECK_INT(found, 0);
}

// The run that finds VSync undelivered is measured: when the present before its first returned, and its
// shortest and longest gap, so the harness can say how far apart its presents came. Early presents before
// a late one do not count in it, neither their 1.5 ms gaps nor a 12 ms one, shorter and longer than any of
// its own: at 9 and 11 ms in turn after the late one, its gaps are 9 to 11 ms, 10 on average.
static void test_run_measured(void)
{
    VsyncWatch watch;
    uint64_t now = START;
    int found = 0;
    vsyncwatch_reset(&watch);
    CHECK_INT(presents(&watch, &now, 4, FAST), 0);
    CHECK_INT(presents(&watch, &now, 1, 12000000), 0);
    CHECK_INT(presents(&watch, &now, 1, FRAME), 0);
    uint64_t before = now;
    for (int i = 0; i < VSYNCWATCH_RUN / 2; i++) {
        found += presents(&watch, &now, 1, 9000000);
        found += presents(&watch, &now, 1, 11000000);
    }
    CHECK_INT(found, 1);
    CHECK(watch.run_first_ns == before);
    CHECK(watch.run_min_ns == 9000000);
    CHECK(watch.run_max_ns == 11000000);
    CHECK((watch.last_ns - watch.run_first_ns) / VSYNCWATCH_RUN == 10000000);
}

// The first present after a reset has nothing before it, even when SDL's clock reads less than three
// quarters of a frame: it starts the run without counting in it
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
    test_early_fraction();
    test_blind_band();
    test_jitter_and_stalls();
    test_run_measured();
    test_first_present();
    test_reset();
    return check_report();
}
