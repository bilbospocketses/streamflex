// The VSync watch (spec section 3): a renderer that reports VSync but presents faster than the
// display is found by measurement. vsyncwatch.c is pure, so this links it alone, as test_winplan
// links winplan.c.
#include <stdbool.h>
#include <stdint.h>
#include "check.h"
#include "vsyncwatch.h"

#define FRAME 16666667ULL     // A 60 Hz display's frame, in nanoseconds
#define FAST 1500000ULL       // A present 1.5 ms after the one before, as lavapipe gives them under Xvfb
#define START 5000000000ULL   // Five seconds after SDL started

// A function to feed the watch count presents, each step_ns after the one before (from *now, which
// it moves on), on a 60 Hz display; gives how many of them found VSync not delivered
static int presents(VsyncWatch *watch, uint64_t *now, int count, uint64_t step_ns)
{
    int found = 0;
    for (int i = 0; i < count; i++) {
        *now += step_ns;
        if (vsyncwatch_present(watch, *now, FRAME))
            found++;
    }
    return found;
}

// Presents a frame apart, as delivered VSync gives them, are never found however many come
static void test_delivered(void)
{
    VsyncWatch watch;
    uint64_t now = START;
    vsyncwatch_reset(&watch);
    CHECK_INT(presents(&watch, &now, 1000, FRAME), 0);
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

// Half a frame exactly is not under half a frame: presents half a frame apart are never found, and
// a nanosecond less apart are
static void test_half_frame(void)
{
    VsyncWatch watch;
    uint64_t now = START;
    int found = 0;
    vsyncwatch_reset(&watch);
    for (int i = 0; i < 4 * VSYNCWATCH_RUN; i++) {
        now += 8000000;
        found += vsyncwatch_present(&watch, now, 16000000);
    }
    CHECK_INT(found, 0);
    vsyncwatch_reset(&watch);
    for (int i = 0; i <= VSYNCWATCH_RUN; i++) {
        now += 7999999;
        found += vsyncwatch_present(&watch, now, 16000000);
    }
    CHECK_INT(found, 1);
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
