// Failures the headless harness asks for, where no real run can be made to fail. Only a build that
// defines STREAMFLEX_TEST_HOOKS has them, and only tests/headless/run.sh defines it: everywhere else
// test_fail() is compiled to nothing, and test_hooks.c to no code at all.
#ifndef TEST_HOOKS_H
#define TEST_HOOKS_H

#include <stdbool.h>

#ifdef STREAMFLEX_TEST_HOOKS
void test_fail(const char *step, bool on);   // Starts (true) or ends (false) the failure STREAMFLEX_TEST_FAIL names
bool test_failing(const char *step);         // Whether STREAMFLEX_TEST_FAIL names a step its own code fails
#else
#define test_fail(step, on)
#define test_failing(step) false
#endif

#endif
