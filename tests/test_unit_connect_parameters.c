/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include "utils/test_setup.h"
#include "memory.h"

/**
 * Test default port
 */
void test_connection_parameters_default_port(void **unused) {
    SF_CONNECT *sf = (SF_CONNECT *) SF_CALLOC(1, sizeof(SF_CONNECT));
    sf->account = "testaccount";
    sf->host = "testaccount.snowflakecomputing.com";
    sf->user = "testuser";
    sf->password = "testpassword";
    assert_int_equal(
        _snowflake_check_connection_parameters(sf), SF_STATUS_SUCCESS);
    assert_string_equal(sf->port, "443");
    SF_FREE(sf);
}

/**
 * Test no host
 */
void test_connection_parameters_no_host(void **unused) {
    SF_CONNECT *sf = (SF_CONNECT *) SF_CALLOC(1, sizeof(SF_CONNECT));
    sf->account = "testaccount";
    sf->user = "testuser";
    sf->password = "testpassword";
    assert_int_equal(
        _snowflake_check_connection_parameters(sf), SF_STATUS_SUCCESS);
    assert_string_equal(sf->host, "testaccount.snowflakecomputing.com");

    SF_FREE(sf);
}

/**
 * Test with region
 */
void test_connection_parameters_with_region(void **unused) {
    SF_CONNECT *sf = (SF_CONNECT *) SF_CALLOC(1, sizeof(SF_CONNECT));
    sf->account = "testaccount";
    sf->user = "testuser";
    sf->password = "testpassword";
    sf->region = "somewhere";
    assert_int_equal(
        _snowflake_check_connection_parameters(sf), SF_STATUS_SUCCESS);
    assert_string_equal(sf->host,
                        "testaccount.somewhere.snowflakecomputing.com");

    SF_FREE(sf);
}

/**
 * Test account including region
 */
void test_connection_parameters_including_region(void **unused) {
    SF_CONNECT *sf = (SF_CONNECT *) SF_CALLOC(1, sizeof(SF_CONNECT));

    // allocate here, because it will be rewritten
    sf->account = (char *) SF_CALLOC(1, 128);
    strcpy(sf->account, "testaccount.somewhere");
    sf->user = "testuser";
    sf->password = "testpassword";
    assert_int_equal(
        _snowflake_check_connection_parameters(sf), SF_STATUS_SUCCESS);
    assert_string_equal(sf->host,
                        "testaccount.somewhere.snowflakecomputing.com");
    assert_string_equal(sf->account, "testaccount");
    assert_string_equal(sf->region, "somewhere");
    SF_FREE(sf->account);
    SF_FREE(sf);
}

/**
 * Test account including region including dots
 */
void test_connection_parameters_including_region_including_dot(void **unused) {
    SF_CONNECT *sf = (SF_CONNECT *) SF_CALLOC(1, sizeof(SF_CONNECT));

    // allocate here, because it will be rewritten
    sf->account = (char *) SF_CALLOC(1, 128);
    strcpy(sf->account, "testaccount.somewhere.here.there");
    sf->user = "testuser";
    sf->password = "testpassword";
    assert_int_equal(
        _snowflake_check_connection_parameters(sf), SF_STATUS_SUCCESS);
    assert_string_equal(sf->host,
                        "testaccount.somewhere.here.there.snowflakecomputing.com");
    assert_string_equal(sf->account, "testaccount");
    assert_string_equal(sf->region, "somewhere.here.there");
    SF_FREE(sf->account);
    SF_FREE(sf);
}

void test_connection_parameters_for_global_url_basic(void **unused) {
    SF_CONNECT *sf = (SF_CONNECT *) SF_CALLOC(1, sizeof(SF_CONNECT));

    // allocate here, because it will be rewritten
    sf->account = (char *) SF_CALLOC(1, 128);
    strcpy(sf->account, "testaccount-hfdw89q748ew9gqf48w9qgf.global");
    sf->user = "testuser";
    sf->password = "testpassword";
    assert_int_equal(
      _snowflake_check_connection_parameters(sf), SF_STATUS_SUCCESS);
    assert_string_equal(sf->host,
                        "testaccount-hfdw89q748ew9gqf48w9qgf.global.snowflakecomputing.com");
    assert_string_equal(sf->account, "testaccount");
    assert_string_equal(sf->region, "global");
    SF_FREE(sf->account);
    SF_FREE(sf);
}

void test_connection_parameters_for_global_url_full(void **unused) {
    SF_CONNECT *sf = (SF_CONNECT *) SF_CALLOC(1, sizeof(SF_CONNECT));

    // allocate here, because it will be rewritten
    sf->account = (char *) SF_CALLOC(1, 128);
    sf->host = (char *) SF_CALLOC(1, 128);
    strcpy(sf->account, "testaccount");
    strcpy(sf->host, "testaccount-hfdw89q748ew9gqf48w9qgf.global.snowflakecomputing.com");
    sf->user = "testuser";
    sf->password = "testpassword";
    assert_int_equal(
      _snowflake_check_connection_parameters(sf), SF_STATUS_SUCCESS);
    assert_string_equal(sf->host,
                        "testaccount-hfdw89q748ew9gqf48w9qgf.global.snowflakecomputing.com");
    assert_string_equal(sf->account, "testaccount");
    // No region should be extracted
    assert_int_equal(sf->region, NULL);
    SF_FREE(sf->account);
    SF_FREE(sf);
}

void test_connection_parameters_for_global_with_account_dashes(void **unused) {
    SF_CONNECT *sf = (SF_CONNECT *) SF_CALLOC(1, sizeof(SF_CONNECT));

    // allocate here, because it will be rewritten
    sf->account = (char *) SF_CALLOC(1, 128);
    strcpy(sf->account, "test-account-hfdw89q748ew9gqf48w9qgf.global");
    sf->user = "testuser";
    sf->password = "testpassword";
    assert_int_equal(
      _snowflake_check_connection_parameters(sf), SF_STATUS_SUCCESS);
    assert_string_equal(sf->host,
                        "test-account-hfdw89q748ew9gqf48w9qgf.global.snowflakecomputing.com");
    assert_string_equal(sf->account, "test-account");
    assert_string_equal(sf->region, "global");
    SF_FREE(sf->account);
    SF_FREE(sf);
}


int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_connection_parameters_default_port),
        cmocka_unit_test(test_connection_parameters_no_host),
        cmocka_unit_test(test_connection_parameters_with_region),
        cmocka_unit_test(test_connection_parameters_including_region),
        cmocka_unit_test(test_connection_parameters_including_region_including_dot),
        cmocka_unit_test(test_connection_parameters_for_global_url_basic),
        cmocka_unit_test(test_connection_parameters_for_global_url_full),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    return ret;
}
