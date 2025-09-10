#include <utility>
#include <thread>

#include "snowflake/CurlDescPool.hpp"
#include "utils/test_setup.h"

using namespace ::Snowflake::Client;

void test_succees_with_crl_check(void **unused) {
  SF_UNUSED(unused);

  // disable OCSP check
  sf_bool value = SF_BOOLEAN_FALSE;
  snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &value);

  SF_CONNECT *sf = setup_snowflake_connection();

  // enable CLR check
  sf_bool crl_check = SF_BOOLEAN_TRUE;
  snowflake_set_attribute(sf, SF_CON_CRL_CHECK, &crl_check);

  SF_STATUS ret = snowflake_connect(sf);

  // must succeed with success
  assert_int_equal(ret, SF_STATUS_SUCCESS);

  snowflake_term(sf);
}

void test_fail_with_no_crl(void **unused) {
  SF_UNUSED(unused);

  // we need to remove curl cache
  ClientCurlDescPool::getInstance().init();
  std::this_thread::sleep_for(std::chrono::milliseconds(std::chrono::milliseconds(6000)));

  // disable OCSP check
  sf_bool value = SF_BOOLEAN_FALSE;
  snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &value);

  // set env variables for test
  sf_setenv("SF_TEST_CRL_NO_CRL", "true");

  SF_CONNECT *sf = setup_snowflake_connection();

  // enable CLR check
  sf_bool crl_check = SF_BOOLEAN_TRUE;
  snowflake_set_attribute(sf, SF_CON_CRL_CHECK, &crl_check);

  // disable advisory, disable allow no crl
  sf_bool crl_enabled = SF_BOOLEAN_FALSE;
  snowflake_set_attribute(sf, SF_CON_CRL_ADVISORY, &crl_enabled);
  snowflake_set_attribute(sf, SF_CON_CRL_ALLOW_NO_CRL, &crl_enabled);

  SF_STATUS ret = snowflake_connect(sf);

  // must fail with CURL error
  assert_int_not_equal(ret, SF_STATUS_SUCCESS);
  SF_ERROR_STRUCT *sferr = snowflake_error(sf);
  if (sferr->error_code != SF_STATUS_ERROR_CURL) {
    dump_error(sferr);
  }
  assert_int_equal(sferr->error_code, SF_STATUS_ERROR_CURL);
  assert_string_equal(sferr->msg, "curl_easy_perform() failed: SSL peer certificate or SSH remote key was not OK");

  snowflake_term(sf);
  sf_unsetenv("SF_TEST_CRL_NO_CRL");
}

void test_success_with_no_crl_if_allow_no_crl(void **unused) {
  SF_UNUSED(unused);

  // we need to remove curl cache
  ClientCurlDescPool::getInstance().init();
  std::this_thread::sleep_for(std::chrono::milliseconds(std::chrono::milliseconds(6000)));

  // disable OCSP check
  sf_bool value = SF_BOOLEAN_FALSE;
  snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &value);

  // set env variables for test
  sf_setenv("SF_TEST_CRL_NO_CRL", "true");

  SF_CONNECT *sf = setup_snowflake_connection();

  // enable CLR check and allow no crl
  sf_bool crl_check = SF_BOOLEAN_TRUE;
  sf_bool crl_advisory = SF_BOOLEAN_FALSE;
  sf_bool crl_allow_no_crl = SF_BOOLEAN_TRUE;
  snowflake_set_attribute(sf, SF_CON_CRL_CHECK, &crl_check);
  snowflake_set_attribute(sf, SF_CON_CRL_ADVISORY, &crl_advisory);
  snowflake_set_attribute(sf, SF_CON_CRL_ALLOW_NO_CRL, &crl_allow_no_crl);

  SF_STATUS ret = snowflake_connect(sf);

  assert_int_equal(ret, SF_STATUS_SUCCESS);

  snowflake_term(sf);
  sf_unsetenv("SF_TEST_CRL_NO_CRL");
}

void test_success_with_no_crl_in_advisory_mode(void **unused) {
  SF_UNUSED(unused);

  // we need to remove curl cache
  ClientCurlDescPool::getInstance().init();
  std::this_thread::sleep_for(std::chrono::milliseconds(std::chrono::milliseconds(6000)));

  // disable OCSP check
  sf_bool value = SF_BOOLEAN_FALSE;
  snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &value);

  // set env variables for test
  sf_setenv("SF_TEST_CRL_NO_CRL", "true");

  SF_CONNECT *sf = setup_snowflake_connection();

  // enable CLR check and advisory mode
  sf_bool crl_check = SF_BOOLEAN_TRUE;
  sf_bool crl_advisory = SF_BOOLEAN_TRUE;
  sf_bool crl_allow_no_crl = SF_BOOLEAN_FALSE;
  snowflake_set_attribute(sf, SF_CON_CRL_CHECK, &crl_check);
  snowflake_set_attribute(sf, SF_CON_CRL_ADVISORY, &crl_advisory);
  snowflake_set_attribute(sf, SF_CON_CRL_ALLOW_NO_CRL, &crl_allow_no_crl);

  SF_STATUS ret = snowflake_connect(sf);

  assert_int_equal(ret, SF_STATUS_SUCCESS);

  snowflake_term(sf);
  sf_unsetenv("SF_TEST_CRL_NO_CRL");
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_succees_with_crl_check),
        cmocka_unit_test(test_fail_with_no_crl),
        cmocka_unit_test(test_success_with_no_crl_if_allow_no_crl),
        cmocka_unit_test(test_success_with_no_crl_in_advisory_mode)
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
