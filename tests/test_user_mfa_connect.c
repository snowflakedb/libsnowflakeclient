//
// Copyright (c) 2018-2024 Snowflake Computing, Inc. All rights reserved.
//

#include "utils/test_setup.h"

/**
 * Test connection with Duo Push
 */
void test_connect_with_duo_push(void **unused)
{
    SF_CONNECT *sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT,
                            getenv("SNOWFLAKE_TEST_ACCOUNT"));
    snowflake_set_attribute(sf, SF_CON_USER, getenv("SNOWFLAKE_TEST_USER"));
    snowflake_set_attribute(sf, SF_CON_PASSWORD,
                            getenv("SNOWFLAKE_TEST_PASSWORD"));
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_USR_PWD_MFA);
    char *host, *port, *protocol;
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

/**
 * Test connection with Duo Push
 */
void test_connect_with_duo_passcodeInPassword(void** unused)
{
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT,
        getenv("SNOWFLAKE_TEST_ACCOUNT"));
    snowflake_set_attribute(sf, SF_CON_USER, getenv("SNOWFLAKE_TEST_USER"));
    snowflake_set_attribute(sf, SF_CON_PASSWORD,
        getenv("SNOWFLAKE_TEST_PASSWORD"));
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_USR_PWD_MFA);
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

/**
 * Test connection with Duo Push
 */
void test_connect_with_duo_passcode(void **unused)
{
    SF_CONNECT *sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT,
                            getenv("SNOWFLAKE_TEST_ACCOUNT"));
    snowflake_set_attribute(sf, SF_CON_USER, getenv("SNOWFLAKE_TEST_USER"));
    snowflake_set_attribute(sf, SF_CON_PASSWORD,
                            getenv("SNOWFLAKE_TEST_PASSWORD"));
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_USR_PWD_MFA);
    char *host, *port, *protocol, *passcode;
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

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS)
    {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    snowflake_term(sf);
}

int main(void)
{
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
        //cmocka_unit_test(test_connect_with_duo_push),
        cmocka_unit_test(test_connect_with_duo_passcodeInPassword),
        //cmocka_unit_test(test_connect_with_duo_passcode),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
