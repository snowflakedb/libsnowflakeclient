#include <string.h>
#include "utils/test_setup.h"
#include "connection.h"
#include "memory.h"

/*
 * Test auth connection when the token was not provided.
 */
void test_sf_get_callers_executable_path(void **unused) {
  SF_UNUSED(unused);

  char path[MAX_PATH];

  sf_get_callers_executable_path(path, sizeof(path));
  assert_true(strstr(path, "test_unit_platform") != NULL);
}

int main(void) {
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_sf_get_callers_executable_path),
};
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  snowflake_global_term();
  return ret;
}
