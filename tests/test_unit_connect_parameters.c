/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include <curl/curl.h>
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
 * Test with cn region
 */
void test_connection_parameters_with_cn_region(void **unused) {
    SF_CONNECT *sf = (SF_CONNECT *) SF_CALLOC(1, sizeof(SF_CONNECT));
    sf->account = "testaccount";
    sf->user = "testuser";
    sf->password = "testpassword";
    sf->region = "cn-somewhere";
    assert_int_equal(
        _snowflake_check_connection_parameters(sf), SF_STATUS_SUCCESS);
    assert_string_equal(sf->host,
                        "testaccount.cn-somewhere.snowflakecomputing.cn");

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

/**
 * Test no host
 */
void test_connection_parameters_application(void **unused) {
    SF_CONNECT *sf = (SF_CONNECT *) SF_CALLOC(1, sizeof(SF_CONNECT));
    memset(sf, 0, sizeof(SF_CONNECT));
    sf->account = "testaccount";
    sf->user = "testuser";
    sf->password = "testpassword";

    // application is null
    assert_int_equal(
        _snowflake_check_connection_parameters(sf), SF_STATUS_SUCCESS);

    // application is empty
    sf->application = "";
    assert_int_equal(
        _snowflake_check_connection_parameters(sf), SF_STATUS_SUCCESS);

    // valid application names
    sf->application = "test1234";
    assert_int_equal(
        _snowflake_check_connection_parameters(sf), SF_STATUS_SUCCESS);

    sf->application = "test_1234";
    assert_int_equal(
        _snowflake_check_connection_parameters(sf), SF_STATUS_SUCCESS);

    sf->application = "test-1234";
    assert_int_equal(
        _snowflake_check_connection_parameters(sf), SF_STATUS_SUCCESS);

    sf->application = "test.1234";
    assert_int_equal(
        _snowflake_check_connection_parameters(sf), SF_STATUS_SUCCESS);

    // invalid application names
    sf->application = "1234test";
    assert_int_equal(
        _snowflake_check_connection_parameters(sf), SF_STATUS_ERROR_GENERAL);

    sf->application = "test$A";
    assert_int_equal(
        _snowflake_check_connection_parameters(sf), SF_STATUS_ERROR_GENERAL);

    sf->application = "test<script>";
    assert_int_equal(
        _snowflake_check_connection_parameters(sf), SF_STATUS_ERROR_GENERAL);

    SF_FREE(sf);
}

/**
* Test connection with session token renew
*/

extern sf_bool STDCALL renew_session(CURL* curl, SF_CONNECT* sf, SF_ERROR_STRUCT* error);
void test_connect_with_renew(void** unused) {
    UNUSED(unused);

    SF_CONNECT* sf = setup_snowflake_connection();

    SF_STATUS status = snowflake_connect(sf);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    SF_STMT* sfstmt = snowflake_stmt(sf);

    // renew session
    CURL* curl = curl_easy_init();
    sf_bool renew_result = renew_session(curl, sf, &sf->error);
    curl_easy_cleanup(curl);
    if (!renew_result)
    {
        dump_error(&sf->error);
    }
    assert_int_equal(renew_result, SF_BOOLEAN_TRUE);

    // The query will be completed after sessin renew.
    status = snowflake_query(sfstmt, "select 1", 0);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_connection_parameters_default_port),
        cmocka_unit_test(test_connection_parameters_no_host),
        cmocka_unit_test(test_connection_parameters_with_region),
        cmocka_unit_test(test_connection_parameters_with_cn_region),
        cmocka_unit_test(test_connection_parameters_including_region),
        cmocka_unit_test(test_connection_parameters_including_region_including_dot),
        cmocka_unit_test(test_connection_parameters_for_global_url_basic),
        cmocka_unit_test(test_connection_parameters_for_global_url_full),
        cmocka_unit_test(test_connection_parameters_application),
        cmocka_unit_test(test_connect_with_renew),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    return ret;
}
