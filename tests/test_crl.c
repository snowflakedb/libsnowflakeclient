#include "utils/test_setup.h"

void test_succees_with_crl_check(void **unused) {
  SF_UNUSED(unused);

  // disable OCSP check
  sf_bool value = SF_BOOLEAN_FALSE;
  snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &value);

  SF_CONNECT *sf = setup_snowflake_connection();

  // enable CLR check
  sf_bool clr_check = SF_BOOLEAN_TRUE;
  snowflake_set_attribute(sf, SF_CON_CLR_CHECK, &clr_check);

  SF_STATUS ret = snowflake_connect(sf);

  // must succeed with CURL error
  assert_int_equal(ret, SF_STATUS_SUCCESS);

  snowflake_term(sf);
}

void test_fail_with_no_crl(void **unused) {
  SF_UNUSED(unused);

  // disable OCSP check
  sf_bool value = SF_BOOLEAN_FALSE;
  snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &value);

  // set env variables for test
  sf_setenv("SF_TEST_CRL_NO_CRL", "true");

  SF_CONNECT *sf = setup_snowflake_connection();

  // enable CLR check
  sf_bool clr_check = SF_BOOLEAN_TRUE;
  snowflake_set_attribute(sf, SF_CON_CLR_CHECK, &clr_check);

  SF_STATUS ret = snowflake_connect(sf);

  // must fail with CURL error
  assert_int_not_equal(ret, SF_STATUS_SUCCESS);
  SF_ERROR_STRUCT *sferr = snowflake_error(sf);
  if (sferr->error_code != SF_STATUS_ERROR_CURL) {
    dump_error(sferr);
  }
  assert_int_equal(sferr->error_code, SF_STATUS_ERROR_CURL);

  snowflake_term(sf);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_succees_with_crl_check),
        cmocka_unit_test(test_fail_with_no_crl),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
