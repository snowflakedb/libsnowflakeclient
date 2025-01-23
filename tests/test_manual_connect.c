/*
* Copyright (c) 2018-2025 Snowflake Computing, Inc. All rights reserved.
*/

#include "utils/test_setup.h"

/**
 * Test connection with OAuth authentication.
 */
void test_oauth_connect(void **unused) 
{
    SF_CONNECT *sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT,
                            getenv("SNOWFLAKE_TEST_ACCOUNT"));
    snowflake_set_attribute(sf, SF_CON_USER, getenv("SNOWFLAKE_TEST_USER"));

    char *host, *port, *protocol, *role;
    host = getenv("SNOWFLAKE_TEST_HOST");
    if (host) 
    {
        snowflake_set_attribute(sf, SF_CON_HOST, host);
    }
    port = getenv("SNOWFLAKE_TEST_PORT");
    if (port) 
    {
        snowflake_set_attribute(sf, SF_CON_PORT, port);
    }
    protocol = getenv("SNOWFLAKE_TEST_PROTOCOL");
    if (protocol) 
    {
        snowflake_set_attribute(sf, SF_CON_PROTOCOL, protocol);
    }
    role = getenv("SNOWFLAKE_TEST_ROLE");
    if (role)
    {
        snowflake_set_attribute(sf, SF_CON_ROLE, role);
    }
    char* token = "<Pass your token here>";
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "oauth");
    snowflake_set_attribute(sf, SF_CON_OAUTH_TOKEN, token);

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    snowflake_term(sf);
}

void test_mfa_connect_with_duo_push(void** unused)
{
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT,
        getenv("SNOWFLAKE_TEST_ACCOUNT"));
    snowflake_set_attribute(sf, SF_CON_USER, getenv("SNOWFLAKE_TEST_USER"));
    snowflake_set_attribute(sf, SF_CON_PASSWORD,
        getenv("SNOWFLAKE_TEST_PASSWORD"));
    char* host, * port, * protocol;
    host = getenv("SNOWFLAKE_TEST_HOST");
    if (host)
    {
        snowflake_set_attribute(sf, SF_CON_HOST, host);
    }
    port = getenv("SNOWFLAKE_TEST_PORT");
    if (port)
    {
        snowflake_set_attribute(sf, SF_CON_PORT, port);
    }
    protocol = getenv("SNOWFLAKE_TEST_PROTOCOL");
    if (protocol)
    {
        snowflake_set_attribute(sf, SF_CON_PROTOCOL, protocol);
    }

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS)
    {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    snowflake_term(sf);
}

void test_mfa_connect_with_duo_passcode(void** unused)
{
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT,
        getenv("SNOWFLAKE_TEST_ACCOUNT"));
    snowflake_set_attribute(sf, SF_CON_USER, getenv("SNOWFLAKE_TEST_USER"));
    snowflake_set_attribute(sf, SF_CON_PASSWORD,
        getenv("SNOWFLAKE_TEST_PASSWORD"));
    char* host, * port, * protocol, * passcode;
    host = getenv("SNOWFLAKE_TEST_HOST");
    if (host)
    {
        snowflake_set_attribute(sf, SF_CON_HOST, host);
    }
    port = getenv("SNOWFLAKE_TEST_PORT");
    if (port)
    {
        snowflake_set_attribute(sf, SF_CON_PORT, port);
    }
    protocol = getenv("SNOWFLAKE_TEST_PROTOCOL");
    if (protocol)
    {
        snowflake_set_attribute(sf, SF_CON_PROTOCOL, protocol);
    }
    passcode = getenv("SNOWFLAKE_TEST_PASSCODE");
    if (passcode)
    {
        snowflake_set_attribute(sf, SF_CON_PASSCODE, passcode);
    }
    else {
        dump_error(&(sf->error));
    }

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS)
    {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    snowflake_term(sf);
}

void test_mfa_connect_with_duo_passcodeInPassword(void** unused)
{
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT,
        getenv("SNOWFLAKE_TEST_ACCOUNT"));
    snowflake_set_attribute(sf, SF_CON_USER, getenv("SNOWFLAKE_TEST_USER"));
    snowflake_set_attribute(sf, SF_CON_PASSWORD,
        getenv("SNOWFLAKE_TEST_PASSWORD"));
    sf_bool passcode_in_password = SF_BOOLEAN_TRUE;
    snowflake_set_attribute(sf, SF_CON_PASSCODE_IN_PASSWORD, &passcode_in_password);

    char* host, * port, * protocol;
    host = getenv("SNOWFLAKE_TEST_HOST");
    if (host)
    {
        snowflake_set_attribute(sf, SF_CON_HOST, host);
    }
    port = getenv("SNOWFLAKE_TEST_PORT");
    if (port)
    {
        snowflake_set_attribute(sf, SF_CON_PORT, port);
    }
    protocol = getenv("SNOWFLAKE_TEST_PROTOCOL");
    if (protocol)
    {
        snowflake_set_attribute(sf, SF_CON_PROTOCOL, protocol);
    }

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS)
    {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    snowflake_term(sf);
}

void test_okta_connect(void** unused)
{
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT,
        getenv("SNOWFLAKE_TEST_ACCOUNT"));
    snowflake_set_attribute(sf, SF_CON_USER, getenv("SNOWFLAKE_TEST_OKTA_USERNAME"));
    snowflake_set_attribute(sf, SF_CON_PASSWORD,
        getenv("SNOWFLAKE_TEST_OKTA_PASSWORD"));
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR,
        getenv("SNOWFLAKE_TEST_AUTHENTICATOR"));
    char* host, * port, * protocol;
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

void test_none(void** unused) {}


int main(void)
{
    initialize_test(SF_BOOLEAN_FALSE);
    struct CMUnitTest tests[1] = {
        cmocka_unit_test(test_none)
    };
    const char* manual_test = getenv("SNOWFLAKE_MANUAL_TEST_TYPE");
    if (manual_test)
    {
        if (strcmp(manual_test, "test_oauth_connect") == 0) {
            tests[0].name = "test_oauth_connect";
            tests[0].test_func = test_oauth_connect;
        }
        else if (strcmp(manual_test, "test_mfa_connect_with_duo_push") == 0) {
            tests[0].name = "test_mfa_connect_with_duo_push";
            tests[0].test_func = test_mfa_connect_with_duo_push;
        }
        else if (strcmp(manual_test, "test_mfa_connect_with_duo_passcode") == 0) {
            tests[0].name = "test_mfa_connect_with_duo_passcode";
            tests[0].test_func = test_mfa_connect_with_duo_passcode;
        }
        else  if (strcmp(manual_test, "test_mfa_connect_with_duo_passcodeInPassword") == 0) {
            tests[0].name = "test_mfa_connect_with_duo_passcodeInPassword";
            tests[0].test_func = test_mfa_connect_with_duo_passcodeInPassword;
        }
        else  if (strcmp(manual_test, "test_okta_connect") == 0) {
            tests[0].name = "test_okta_connect";
            tests[0].test_func = test_okta_connect;
        }
        else {
            printf("No matching test found for: %s\n", manual_test);
        }
    }
    else {
        printf("No value in SNOWFLAKE_MANUAL_TEST_TYPE. Skip the test\n");
    }
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}

