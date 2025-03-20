#include "utils/test_setup.h"
#include "../lib/snowflake_util.h"
#include "../include/snowflake/client.h"
#include "connection.h"
#include "memory.h"

void test_connect_with_client_session_keep_alive_disable(void** unused) 
{
    SF_UNUSED(unused);
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT,
        getenv("SNOWFLAKE_TEST_ACCOUNT"));
    snowflake_set_attribute(sf, SF_CON_USER, getenv("SNOWFLAKE_TEST_USER"));
    snowflake_set_attribute(sf, SF_CON_PASSWORD,
        getenv("SNOWFLAKE_TEST_PASSWORD"));
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
    sf_bool client_session_keep_alive = SF_BOOLEAN_FALSE;
    snowflake_set_attribute(sf, SF_CON_CLIENT_SESSION_KEEP_ALIVE, &client_session_keep_alive);

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    snowflake_term(sf);
}

void test_connect_with_token_request(void** unused)
{
    //log_set_quiet(0);
    SF_UNUSED(unused);
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT,
        getenv("SNOWFLAKE_TEST_ACCOUNT"));
    snowflake_set_attribute(sf, SF_CON_USER, getenv("SNOWFLAKE_TEST_USER"));
    snowflake_set_attribute(sf, SF_CON_PASSWORD,
        getenv("SNOWFLAKE_TEST_PASSWORD"));
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
    sf_bool client_session_keep_alive = SF_BOOLEAN_FALSE;
    snowflake_set_attribute(sf, SF_CON_CLIENT_SESSION_KEEP_ALIVE, &client_session_keep_alive);

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    char* previous_sessiontoken = SF_MALLOC(strlen(sf->token)+1);
    char* previous_masterToken = SF_MALLOC(strlen(sf->master_token) + 1);;
    strcpy(previous_sessiontoken, sf->token);
    strcpy(previous_masterToken, sf->master_token);
    
    token_request(sf, 0);

    assert_false(strcmp(previous_sessiontoken, sf->token) == 0);
    assert_false(strcmp(previous_masterToken, sf->master_token) == 0);

    SF_FREE(previous_sessiontoken);
    SF_FREE(previous_masterToken);

    snowflake_term(sf);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_connect_with_client_session_keep_alive_disable),
      cmocka_unit_test(test_connect_with_token_request),

    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
