#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/provider.h>
#include <cstring>
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

  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "yes");
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

  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "no");
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

  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "yes");
  test_simple_conn_helper(SF_BOOLEAN_FALSE);

  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "0");
  test_simple_conn_helper(SF_BOOLEAN_TRUE);

  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "");
  test_simple_conn_helper(SF_BOOLEAN_TRUE);

  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "false");
  test_simple_conn_helper(SF_BOOLEAN_TRUE);

  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "off");
  test_simple_conn_helper(SF_BOOLEAN_TRUE);

  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "no");
  test_simple_conn_helper(SF_BOOLEAN_TRUE);

  sf_unsetenv(SF_FIPS_ENABLED_ENV_VAR);
  EVP_default_properties_enable_fips(nullptr, fipsDefault);
}

static int fips_provider_already_loaded_cb(OSSL_PROVIDER *prov, void *cbdata) {
  const char *name = OSSL_PROVIDER_get0_name(prov);
  if (name != NULL && strcmp(name, "fips") == 0) {
    *static_cast<int *>(cbdata) = 1;
  }
  return 1;
}

static int fips_provider_already_loaded(void) {
  int loaded = 0;
  OSSL_PROVIDER_do_all(NULL, fips_provider_already_loaded_cb, &loaded);
  return loaded;
}

static int teardown_fips_env(void **unused) {
  SF_UNUSED(unused);
  sf_unsetenv(SF_FIPS_ENABLED_ENV_VAR);
  return 0;
}

void test_fips_not_loaded(void **unused) {
  SF_UNUSED(unused);
  // Unloading a config-activated FIPS provider is process-global and unsafe
  // for later tests. This case is only meaningful when FIPS was never loaded.
  if (fips_provider_already_loaded()) {
    skip();
  }

  sf_bool fips_enabled = SF_BOOLEAN_TRUE;
  snowflake_global_get_attribute(SF_GLOBAL_FIPS_ENABLED, &fips_enabled, 0);
  assert_int_equal(fips_enabled, SF_BOOLEAN_FALSE);

  sf_setenv(SF_FIPS_ENABLED_ENV_VAR, "1");
  test_simple_conn_helper(SF_BOOLEAN_FALSE);
}

int main(void) {
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
    cmocka_unit_test_teardown(test_fips_enabled, teardown_fips_env),
    cmocka_unit_test_teardown(test_fips_disabled, teardown_fips_env),
    cmocka_unit_test_teardown(test_fips_not_loaded, teardown_fips_env),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  snowflake_global_term();
  return ret;
}
