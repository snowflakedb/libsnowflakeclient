#include "utils/test_setup.h"
#include "snowflake/Stopwatch.h"
#include "snowflake_util.h"

void test_is_started() {
  Stopwatch stopwatch;
  stopwatch_start(&stopwatch);
  assert_true(stopwatch_isStarted(&stopwatch));
  stopwatch_stop(&stopwatch);
}

void test_is_stopped() {
  Stopwatch stopwatch;
  stopwatch_start(&stopwatch);
  stopwatch_stop(&stopwatch);
  assert_false(stopwatch_isStarted(&stopwatch));
}

void test_reset() {
  Stopwatch stopwatch;
  stopwatch_start(&stopwatch);
  stopwatch_reset(&stopwatch);
  assert_false(stopwatch_isStarted(&stopwatch));
  assert_int_equal(stopwatch_elapsedMillis(&stopwatch), 0);
}

void test_restart() {
  Stopwatch stopwatch;
  stopwatch_start(&stopwatch);
  sf_sleep_ms(500);
  stopwatch_stop(&stopwatch);
  assert_false(stopwatch_isStarted(&stopwatch));
  stopwatch_restart(&stopwatch);
  assert_true(stopwatch_isStarted(&stopwatch));
  assert_true(stopwatch_elapsedMillis(&stopwatch) < 500);
  stopwatch_stop(&stopwatch);
}

void test_stop_already_stopped() {
  Stopwatch stopwatch;
  stopwatch_reset(&stopwatch);
  stopwatch_start(&stopwatch);
  sf_sleep_ms(100);
  stopwatch_stop(&stopwatch);
  assert_false(stopwatch_isStarted(&stopwatch));
  stopwatch_stop(&stopwatch); // should not change state
  assert_false(stopwatch_isStarted(&stopwatch));
  assert_true(stopwatch_elapsedMillis(&stopwatch) > 0);
}

void test_start_already_started() {
  Stopwatch stopwatch;
  stopwatch_reset(&stopwatch);
  stopwatch_start(&stopwatch);
  sf_sleep_ms(500);
  stopwatch_start(&stopwatch); // should not change state
  assert_true(stopwatch_isStarted(&stopwatch));
  assert_true(stopwatch_elapsedMillis(&stopwatch) >= 500);
}

void test_get_millis_stopped() {
  Stopwatch stopwatch;
  stopwatch_reset(&stopwatch);
  stopwatch_start(&stopwatch);
  sf_sleep_ms(100);
  stopwatch_stop(&stopwatch);
  long elapsed = stopwatch_elapsedMillis(&stopwatch);
  assert_true(elapsed > 0);
  assert_false(stopwatch_isStarted(&stopwatch));
}

void test_get_millis_running() {
  Stopwatch stopwatch;
  stopwatch_reset(&stopwatch);
  stopwatch_start(&stopwatch);
  // wait for a short duration
  sf_sleep_ms(500);
  long elapsed = stopwatch_elapsedMillis(&stopwatch);
  assert_true(elapsed >= 500); // should be at least 500 milliseconds
  assert_true(stopwatch_isStarted(&stopwatch));
  sf_sleep_ms(100);
  assert_true(stopwatch_elapsedMillis(&stopwatch) > elapsed);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_is_started),
      cmocka_unit_test(test_is_stopped),
      cmocka_unit_test(test_reset),
      cmocka_unit_test(test_restart),
      cmocka_unit_test(test_stop_already_stopped),
      cmocka_unit_test(test_start_already_started),
      cmocka_unit_test(test_get_millis_stopped),
      cmocka_unit_test(test_get_millis_running)
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    return ret;
}
