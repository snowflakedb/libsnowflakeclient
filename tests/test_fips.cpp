#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/provider.h>
#include <functional>
#include <memory>
#include <boost/filesystem.hpp>
#include "snowflake/IBase64.hpp"
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"

void test_simple_conn_helper(sf_bool expect_success)
{
  SF_CONNECT *sf = setup_snowflake_connection();
  SF_STATUS status = snowflake_connect(sf);

  if (!expect_success)
  {
    assert_int_not_equal(status, SF_STATUS_SUCCESS);
    SF_ERROR_STRUCT *sferr = snowflake_error(sf);

    if (sferr->error_code != SF_STATUS_ERROR_FIPS_NOT_ENABLED) {
        dump_error(sferr);
    }
    assert_int_equal(sferr->error_code, SF_STATUS_ERROR_FIPS_NOT_ENABLED);
  }
  else
  {
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
  }

  snowflake_term(sf);
}

void test_fips_enabled(void **unused) {
  SF_UNUSED(unused);
  sf_bool fips_enabled = SF_BOOLEAN_FALSE;
  snowflake_global_get_attribute(SF_GLOBAL_FIPS_ENABLED, &fips_enabled, 0);
  assert_int_equal(fips_enabled, SF_BOOLEAN_TRUE);

  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "1");
  test_simple_conn_helper(SF_BOOLEAN_TRUE);

  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "true");
  test_simple_conn_helper(SF_BOOLEAN_TRUE);

  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "on");
  test_simple_conn_helper(SF_BOOLEAN_TRUE);

  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "");
  test_simple_conn_helper(SF_BOOLEAN_TRUE);

  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "0");
  test_simple_conn_helper(SF_BOOLEAN_TRUE);

  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "false");
  test_simple_conn_helper(SF_BOOLEAN_TRUE);

  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "off");
  test_simple_conn_helper(SF_BOOLEAN_TRUE);

  sf_unsetenv(SF_FIPS_ENABLED_ENV_VAR);
}

void test_fips_disabled(void **unused) {
  SF_UNUSED(unused);
  int fipsDefault = EVP_default_properties_is_fips_enabled(nullptr);
  EVP_default_properties_enable_fips(nullptr, 0);
  sf_bool fips_enabled = SF_BOOLEAN_TRUE;
  snowflake_global_get_attribute(SF_GLOBAL_FIPS_ENABLED, &fips_enabled, 0);
  assert_int_equal(fips_enabled, SF_BOOLEAN_FALSE);

  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "1");
  test_simple_conn_helper(SF_BOOLEAN_FALSE);

  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "true");
  test_simple_conn_helper(SF_BOOLEAN_FALSE);

  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "on");
  test_simple_conn_helper(SF_BOOLEAN_FALSE);

  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "0");
  test_simple_conn_helper(SF_BOOLEAN_TRUE);

  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "");
  test_simple_conn_helper(SF_BOOLEAN_TRUE);

  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "false");
  test_simple_conn_helper(SF_BOOLEAN_TRUE);

  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "off");
  test_simple_conn_helper(SF_BOOLEAN_TRUE);

  sf_unsetenv(SF_FIPS_ENABLED_ENV_VAR);
  EVP_default_properties_enable_fips(nullptr, fipsDefault);
}

void test_fips_not_loaded(void **unused) {
  SF_UNUSED(unused);
  OSSL_PROVIDER *prov = OSSL_PROVIDER_load(NULL, "fips");
  if (prov != NULL) {
    // unload twice to remove the initial ref count from configuration
    OSSL_PROVIDER_unload(prov);
    OSSL_PROVIDER_unload(prov);
  }

  sf_bool fips_enabled = SF_BOOLEAN_TRUE;
  snowflake_global_get_attribute(SF_GLOBAL_FIPS_ENABLED, &fips_enabled, 0);
  assert_int_equal(fips_enabled, SF_BOOLEAN_FALSE);

  if (prov != NULL) {
    OSSL_PROVIDER_load(NULL, "fips");
  }
}

int main(void) {
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_fips_enabled),
    cmocka_unit_test(test_fips_disabled),
    cmocka_unit_test(test_fips_not_loaded),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  snowflake_global_term();
  return ret;
}
