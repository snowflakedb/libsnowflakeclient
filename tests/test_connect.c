//
// Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
//

#include "utils/test_setup.h"

/**
 * Test connection with null context
 */
void test_null_sf_connect(void **unused) {
    SF_CONNECT *sf = NULL;
    // Try connecting with a NULL connection struct, should fail
    SF_STATUS status = snowflake_connect(sf);
    assert_int_not_equal(status, SF_STATUS_SUCCESS);
}

/**
 * Test connection without parameters
 */
void test_no_connection_parameters(void **unused) {
    SF_CONNECT *sf = snowflake_init();
    SF_STATUS status = snowflake_connect(sf);
    assert_int_not_equal(status, SF_STATUS_SUCCESS);
    SF_ERROR_STRUCT *error = snowflake_error(sf);
    assert_int_equal(error->error_code, SF_STATUS_ERROR_BAD_CONNECTION_PARAMS);
    snowflake_term(sf);
}

/**
 * Test connection with minimum parameter set
 */
void test_connect_with_minimum_parameters(void **unused) {
    SF_CONNECT *sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT,
                            getenv("SNOWFLAKE_TEST_ACCOUNT"));
    snowflake_set_attribute(sf, SF_CON_USER, getenv("SNOWFLAKE_TEST_USER"));
    snowflake_set_attribute(sf, SF_CON_PASSWORD,
                            getenv("SNOWFLAKE_TEST_PASSWORD"));
    char *host, *port, *protocol;
    host = getenv("SNOWFLAKE_TEST_HOST");
    if (host) {
        snowflake_set_attribute(sf, SF_CON_HOST, host);
    }
    port = getenv("SNOWFLAKE_TEST_PORT");
    if (port) {
        snowflake_set_attribute(sf, SF_CON_PORT, port);
    }
    protocol = getenv("SNOWFLAKE_TEST_PROTOCOL");
    if (protocol) {
        snowflake_set_attribute(sf, SF_CON_PROTOCOL, protocol);
    }

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    snowflake_term(sf);
}

/**
 * Test connection with full parameter set
 */
void test_connect_with_full_parameters(void **unused) {
    SF_CONNECT *sf = setup_snowflake_connection();

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    snowflake_term(sf); // purge snowflake context
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_null_sf_connect),
      cmocka_unit_test(test_no_connection_parameters),
      cmocka_unit_test(test_connect_with_minimum_parameters),
      cmocka_unit_test(test_connect_with_full_parameters),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
