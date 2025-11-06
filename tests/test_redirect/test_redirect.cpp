#include "snowflake/client.h"
#include "../utils/test_setup.h"
#include "../utils/TestSetup.hpp"
#include "../wiremock/wiremock.hpp"

using namespace Snowflake::Client;

WiremockRunner* wiremock;

void test_redirect_307(void** unused) {
  SF_UNUSED(unused);

  wiremock = new WiremockRunner();
  wiremock->initMappingFromFile("http_307_retry.json", std::stoi(wiremockPort));

  SF_CONNECT *sf = setup_snowflake_connection();
  snowflake_set_attribute(sf, SF_CON_HOST, wiremockHost);
  snowflake_set_attribute(sf, SF_CON_PORT, wiremockPort);
  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  /* query */
  SF_STMT *sfstmt = snowflake_stmt(sf);
  status = snowflake_query(sfstmt, "select 1;", 0);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  int64 out = 0;
  assert_int_equal(snowflake_num_rows(sfstmt), 1);

  int counter = 0;
  while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
    snowflake_column_as_int64(sfstmt, 1, &out);
    assert_int_equal(out, 1);
    ++counter;
  }
  if (status != SF_STATUS_EOF) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_EOF);
  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
}

int main(void) {
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_redirect_307),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}

