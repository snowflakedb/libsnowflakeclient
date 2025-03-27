#include "auth_utils.h"

void test_okta_successful_connection(void **unused) {
    SF_UNUSED(unused);

    SF_CONNECT * sf = snowflake_init();
    set_all_snowflake_attributes(sf);

    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, getenv("SNOWFLAKE_AUTH_TEST_OKTA_AUTH"));
    snowflake_set_attribute(sf, SF_CON_PASSWORD, getenv("SNOWFLAKE_AUTH_TEST_OKTA_PASS"));
    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    snowflake_term(sf);
}

void test_okta_wrong_credentials(void **unused) {
    SF_UNUSED(unused);

    SF_CONNECT * sf = snowflake_init();
    set_all_snowflake_attributes(sf);

    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, getenv("SNOWFLAKE_AUTH_TEST_OKTA_AUTH"));
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "invalidPassword");
    SF_STATUS status = snowflake_connect(sf);
    assert_int_equal(status, SF_STATUS_ERROR_GENERAL);
    SF_ERROR_STRUCT* error = snowflake_error(sf);
    assert_string_equal(error->msg, "Incorrect username or password was specified.");
    snowflake_term(sf);
}

//todo SNOW-2004133 return more descriptive error message
void test_okta_invalid_url(void **unused) {
    SF_UNUSED(unused);

    SF_CONNECT * sf = snowflake_init();
    set_all_snowflake_attributes(sf);

    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "https://invalid.okta.com");
    snowflake_set_attribute(sf, SF_CON_PASSWORD, getenv("SNOWFLAKE_AUTH_TEST_OKTA_PASS"));
    SF_STATUS status = snowflake_connect(sf);
    assert_int_equal(status, SF_STATUS_ERROR_GENERAL);
    SF_ERROR_STRUCT* error = snowflake_error(sf);
    assert_string_equal(error->msg, "authentication failed");
    snowflake_term(sf);
}

//todo SNOW-2004133 return more descriptive error message
void test_okta_invalid_url_no_okta_path(void **unused) {
    SF_UNUSED(unused);

    SF_CONNECT * sf = snowflake_init();
    set_all_snowflake_attributes(sf);

    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "https://invalid.abc.com");
    snowflake_set_attribute(sf, SF_CON_PASSWORD, getenv("SNOWFLAKE_AUTH_TEST_OKTA_PASS"));
    SF_STATUS status = snowflake_connect(sf);
    assert_int_equal(status, SF_STATUS_ERROR_GENERAL);
    SF_ERROR_STRUCT* error = snowflake_error(sf);
    assert_string_equal(error->msg, "authentication failed");
    snowflake_term(sf);
}
