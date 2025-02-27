/*
* Copyright (c) 2018-2025 Snowflake Computing, Inc. All rights reserved.
*/

#include "utils/test_setup.h"

/**
 * Test connection with OAuth authentication.
 */
void test_oauth_connect(void **unused)
{
    SF_UNUSED(unused);
    const char* manual_test = getenv("SNOWFLAKE_MANUAL_TEST_TYPE");
    if (manual_test == NULL || strcmp(manual_test, "test_oauth_connect") != 0) 
    {
        printf("This test was skipped.\n");
        return;
    }

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
    SF_UNUSED(unused);
    const char* manual_test = getenv("SNOWFLAKE_MANUAL_TEST_TYPE");
    if (manual_test == NULL || strcmp(manual_test, "test_mfa_connect_with_duo_push") != 0)
    {
        printf("This test was skipped.\n");
        return;
    }

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
    SF_UNUSED(unused);
    const char* manual_test = getenv("SNOWFLAKE_MANUAL_TEST_TYPE");
    if (manual_test == NULL || strcmp(manual_test, "test_mfa_connect_with_duo_passcode") != 0)
    {
        printf("This test was skipped.\n");
        return;
    }

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
    SF_UNUSED(unused);
    const char* manual_test = getenv("SNOWFLAKE_MANUAL_TEST_TYPE");
    if (manual_test == NULL || strcmp(manual_test, "test_mfa_connect_with_duo_passcodeInPassword") != 0)
    {
        printf("This test was skipped.\n");
        return;
    }

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

void test_mfa_connect_with_mfa_cache(void** unused)
{
  SF_UNUSED(unused);
  /*
   * Should trigger mfa push notification at most once.
   * Make sure ALLOW_CLIENT_MFA_CACHING is set to true
   * For more details refer to: https://docs.snowflake.com/en/user-guide/security-mfa#using-mfa-token-caching-to-minimize-the-number-of-prompts-during-authentication-optional
   */
  for (int i = 0; i < 2; i++) {
    SF_CONNECT *sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_APPLICATION_NAME, "ODBC");
    snowflake_set_attribute(sf, SF_CON_APPLICATION_VERSION, "2.30.0");
    snowflake_set_attribute(sf, SF_CON_ACCOUNT,
                            getenv("SNOWFLAKE_TEST_ACCOUNT"));
    snowflake_set_attribute(sf, SF_CON_USER, getenv("SNOWFLAKE_TEST_USER"));
    snowflake_set_attribute(sf, SF_CON_PASSWORD,
                            getenv("SNOWFLAKE_TEST_PASSWORD"));
    char *host, *port, *protocol, *passcode;
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
    passcode = getenv("SNOWFLAKE_TEST_PASSCODE");
    if (passcode) {
      snowflake_set_attribute(sf, SF_CON_PASSCODE, passcode);
    } else {
      dump_error(&(sf->error));
    }

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
      dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    snowflake_term(sf);
  }
}

void test_okta_connect(void** unused)
{
    SF_UNUSED(unused);
    const char* manual_test = getenv("SNOWFLAKE_MANUAL_TEST_TYPE");
    if (manual_test == NULL || strcmp(manual_test, "test_okta_connect") != 0)
    {
        printf("This test was skipped.\n");
        return;
    }

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
  
void test_external_browser(void** unused)
{
    SF_UNUSED(unused);
    const char* manual_test = getenv("SNOWFLAKE_MANUAL_TEST_TYPE");
    if (manual_test == NULL || strcmp(manual_test, "test_external_browser") != 0)
    {
        printf("This test was skipped.\n");
        return;
    }

    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT,
        getenv("SNOWFLAKE_TEST_ACCOUNT"));
    snowflake_set_attribute(sf, SF_CON_USER, getenv("SNOWFLAKE_TEST_EXTERNAL_BROWSER_USERNAME"));
    snowflake_set_attribute(sf, SF_CON_PASSWORD,
        getenv("SNOWFLAKE_TEST_EXTERNAL_BROWSER_PASSWORD"));
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR,
        SF_AUTHENTICATOR_EXTERNAL_BROWSER);
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

void test_sso_token_auth(void** unused)
{

    SF_UNUSED(unused);
    const char* manual_test = getenv("SNOWFLAKE_MANUAL_TEST_TYPE");
    if (manual_test == NULL || strcmp(manual_test, "test_sso_token_auth") != 0)
    {
        printf("This test was skipped.\n");
        return;
    }
    /*
 * Should trigger external browser auth at the first time
 * Make sure ALLOW_ID_TOKEN is set to true
 * For more details refer to: https://docs.snowflake.com/en/user-guide/admin-security-fed-auth-overview
 */
    for (int i = 0; i < 2; i++) {
        SF_CONNECT* sf = snowflake_init();
        snowflake_set_attribute(sf, SF_CON_ACCOUNT,
            getenv("SNOWFLAKE_TEST_ACCOUNT"));
        snowflake_set_attribute(sf, SF_CON_USER, getenv("SNOWFLAKE_TEST_EXTERNAL_BROWSER_USERNAME"));
        snowflake_set_attribute(sf, SF_CON_PASSWORD,
            getenv("SNOWFLAKE_TEST_EXTERNAL_BROWSER_PASSWORD"));
        snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR,
            SF_AUTHENTICATOR_EXTERNAL_BROWSER);
        sf_bool client_store_temporary_credential = SF_BOOLEAN_TRUE;
        snowflake_set_attribute(sf, SF_CON_CLIENT_STORE_TEMPORARY_CREDENTIAL, &client_store_temporary_credential);
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

        if (i != 1) {
            sf->token_cache = secure_storage_init();
            secure_storage_remove_credential(sf->token_cache, sf->host, sf->user, SSO_TOKEN);
        }
       
        SF_STATUS status = snowflake_connect(sf);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sf->error));
        }

        assert_int_equal(status, SF_STATUS_SUCCESS);
        snowflake_term(sf);
    }
}

void test_sso_token_auth_renew(void** unused)
{
    SF_UNUSED(unused);
    const char* manual_test = getenv("SNOWFLAKE_MANUAL_TEST_TYPE");
    if (manual_test == NULL || strcmp(manual_test, "test_sso_token_auth_renew") != 0)
    {
        printf("This test was skipped.\n");
        return;
    }

    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT,
        getenv("SNOWFLAKE_TEST_ACCOUNT"));
    snowflake_set_attribute(sf, SF_CON_USER, getenv("SNOWFLAKE_TEST_EXTERNAL_BROWSER_USERNAME"));
    snowflake_set_attribute(sf, SF_CON_PASSWORD,
        getenv("SNOWFLAKE_TEST_EXTERNAL_BROWSER_PASSWORD"));
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR,
        SF_AUTHENTICATOR_EXTERNAL_BROWSER);
    sf_bool client_store_temporary_credential = SF_BOOLEAN_TRUE;
    snowflake_set_attribute(sf, SF_CON_CLIENT_STORE_TEMPORARY_CREDENTIAL, &client_store_temporary_credential);
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

    sf->token_cache = secure_storage_init();
    secure_storage_remove_credential(sf->token_cache, sf->host, sf->user, SSO_TOKEN);
    secure_storage_save_credential(sf->token_cache, sf->host, sf->user, SSO_TOKEN, "wrong token");


    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }

    assert_int_equal(status, SF_STATUS_SUCCESS);
    snowflake_term(sf);
}

int main(void)
{
    initialize_test(SF_BOOLEAN_FALSE);
    struct CMUnitTest tests[] = {
        cmocka_unit_test(test_oauth_connect),
        cmocka_unit_test(test_mfa_connect_with_duo_passcode),
        cmocka_unit_test(test_mfa_connect_with_duo_push),
        cmocka_unit_test(test_mfa_connect_with_duo_passcodeInPassword),
        cmocka_unit_test(test_external_browser),
        cmocka_unit_test(test_okta_connect),
        cmocka_unit_test(test_sso_token_auth),
        cmocka_unit_test(test_sso_token_auth_renew),

     };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}