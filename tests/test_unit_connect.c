#include <string.h>
#include "utils/test_setup.h"
#include "connection.h"
#include "./authenticator.h"
#include "memory.h"

/*
 * Test authenticator workflow.
 */
void test_unit_connect(void **unused)
{
  SF_UNUSED(unused);
  SF_CONNECT *sf = snowflake_init();
  snowflake_set_attribute(sf, SF_CON_ACCOUNT, "test_account");
  snowflake_set_attribute(sf, SF_CON_USER, "test_user");
  snowflake_set_attribute(sf, SF_CON_HOST, "host");
  snowflake_set_attribute(sf, SF_CON_PORT, "443");
  snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
  snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "test");

  auth_initialize(sf);
  auth_authenticate(sf);
  cJSON *body = snowflake_cJSON_CreateObject();

  auth_update_json_body(sf, body);
  cJSON *data = snowflake_cJSON_GetObjectItem(body, "data");

  assert_int_equal(snowflake_cJSON_GetNumberValue(snowflake_cJSON_GetObjectItem(data, "test")), 0);

  auth_renew_json_body(sf, body);
  data = snowflake_cJSON_GetObjectItem(body, "data");
  assert_int_equal(snowflake_cJSON_GetNumberValue(snowflake_cJSON_GetObjectItem(data, "test")), 1);

  snowflake_cJSON_Delete(body);
  snowflake_term(sf);
}

int main(void)
{
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_unit_connect),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  snowflake_global_term();
  return ret;
}
