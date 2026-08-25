#include <chrono>
#include <thread>

#include "utils/test_setup.h"
#include "snowflake/Stopwatch.h"

/*
 * Regression test for SNOW-4007740.
 *
 * The C `Stopwatch` struct is routinely declared as a bare `Stopwatch s;`
 * local and then started without an explicit stopwatch_reset(). Before the
 * fix, an uninitialized (garbage-truthy) `isStarted` byte turned
 * stopwatch_start()'s idempotent-start guard (`!s->isStarted`) into a no-op,
 * leaving startTime at stack garbage; stopwatch_elapsedMillis() then returned
 * (steady_clock_now - garbage), i.e. a CLOCK_MONOTONIC-since-boot value that
 * grows with host uptime.
 *
 * Part A of the fix adds C++ default member initializers to the struct, so a
 * default-declared Stopwatch in any C++ translation unit is guaranteed to
 * start zeroed. These tests exercise that guarantee. A C test cannot cover it
 * (the initializers are guarded by `#ifdef __cplusplus`), so this test lives
 * in a .cpp. The existing C-side semantics are covered by test_stopwatch.c,
 * which is intentionally left unchanged.
 */

// A default-declared Stopwatch (no explicit reset) must be stopped and read
// exactly 0 -- never an uptime-scale value.
void test_default_initialized_is_zero() {
  Stopwatch stopwatch;  // relies on Part A default member initializers
  assert_false(stopwatch_isStarted(&stopwatch));
  assert_int_equal(stopwatch_elapsedMillis(&stopwatch), 0);
}

// Start/stop on a default-declared Stopwatch (again, no explicit reset) must
// measure a small, sane interval -- not host uptime.
void test_default_initialized_start_is_sane() {
  Stopwatch stopwatch;  // no stopwatch_reset() on purpose
  stopwatch_start(&stopwatch);
  assert_true(stopwatch_isStarted(&stopwatch));
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  stopwatch_stop(&stopwatch);
  assert_false(stopwatch_isStarted(&stopwatch));
  long elapsed = stopwatch_elapsedMillis(&stopwatch);
  assert_true(elapsed >= 100);   // at least the time we slept (generous margin)
  assert_true(elapsed < 60000);  // uptime-scale garbage would blow past this
}

int main(void) {
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_default_initialized_is_zero),
      cmocka_unit_test(test_default_initialized_start_is_sane),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  return ret;
}
