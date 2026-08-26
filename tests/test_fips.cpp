#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <functional>
#include <memory>
#include <boost/filesystem.hpp>
#include "snowflake/IBase64.hpp"
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"

void test_fips_enabled(void **unused) {
  SF_UNUSED(unused);
  sf_bool fips_enabled = SF_BOOLEAN_FALSE;
  snowflake_global_get_attribute(SF_GLOBAL_FIPS_ENABLED, &fips_enabled, 0);
  assert_int_equal(fips_enabled, SF_BOOLEAN_TRUE);
  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "1");

  SF_CONNECT *sf = setup_snowflake_connection();

  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  /* query */
  SF_STMT* sfstmt = snowflake_stmt(sf);

  /* Set query result format to Arrow if necessary */
  status = snowflake_query(sfstmt, "select 1", 0);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
  sf_unsetenv(SF_FIPS_ENABLED_ENV_VAR);
}

void test_fips_disabled(void **unused) {
  SF_UNUSED(unused);
  EVP_default_properties_enable_fips(nullptr, 0);
  sf_bool fips_enabled = SF_BOOLEAN_TRUE;
  snowflake_global_get_attribute(SF_GLOBAL_FIPS_ENABLED, &fips_enabled, 0);
  assert_int_equal(fips_enabled, SF_BOOLEAN_FALSE);

  // connection/query succeed with env turned off
  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "0");

  SF_CONNECT *sf = setup_snowflake_connection();

  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  /* query */
  SF_STMT* sfstmt = snowflake_stmt(sf);

  /* Set query result format to Arrow if necessary */
  status = snowflake_query(sfstmt, "select 1", 0);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);

  // connection fails with env turned on
  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "1");
  sf = setup_snowflake_connection();
  status = snowflake_connect(sf);
  assert_int_not_equal(status, SF_STATUS_SUCCESS);
  SF_ERROR_STRUCT *sferr = snowflake_error(sf);

  if (sferr->error_code != SF_STATUS_ERROR_FIPS_NOT_ENABLED) {
      dump_error(sferr);
  }
  assert_int_equal(sferr->error_code, SF_STATUS_ERROR_FIPS_NOT_ENABLED);

  sf_unsetenv(SF_FIPS_ENABLED_ENV_VAR);
  snowflake_term(sf);
}

int main(void) {
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_fips_enabled),
    cmocka_unit_test(test_fips_disabled),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  snowflake_global_term();
  return ret;
}
