/*
 * Copyright (c) 2025 Snowflake Computing, Inc. All rights reserved.
 */

#include <string>
#include <vector>
#include <thread>

#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"
#include "memory.h"
#include "snowflake_util.h"
#include "../lib/heart_beat_background.h"

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

void connection_thread(sf_bool* result)
{
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
    sf_bool client_session_keep_alive = SF_BOOLEAN_TRUE;
    snowflake_set_attribute(sf, SF_CON_CLIENT_SESSION_KEEP_ALIVE, &client_session_keep_alive);
    snowflake_connect(sf);
    std::this_thread::sleep_for(std::chrono::milliseconds(20000));
    //Make sure renew_session_sync work well
    *result = renew_session_sync(sf);

    snowflake_term(sf);
}

// enable keep alive and execute a query takes 1 second
// this could help on testing heartbeat when the driver built with
// HEARTBEAT_DEBUG turned on from Cmake when you build libsf.
// https://github.com/snowflakedb/snowflake-sdks-drivers-issues-teamwork/issues/368
void test_connect_with_client_session_keep_alive_current(void** unused)
{
    const int THREADS = 10;
    SF_UNUSED(unused);
    sf_bool results[THREADS];

    std::vector<std::thread> threads;
    for (int i = 0; i < THREADS; i++)
    {
        threads.push_back(std::thread(connection_thread, &(results[i])));
    }

    for (int i = 0; i < THREADS; i++)
    {
        threads[i].join();
    }

    for (int i = 0; i < THREADS; i++)
    {
        assert_true(results[i]);
    }
}

void test_heartbeat_manually(void** unused)
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

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    start_heart_beat_for_this_session(sf);
    start_heart_beat_for_this_session(sf);
    assert_true(sf->is_heart_beat_on);

    stop_heart_beat_for_this_session(sf);
    stop_heart_beat_for_this_session(sf);
    assert_false(sf->is_heart_beat_on);

    //Turn on to enable session.
    start_heart_beat_for_this_session(sf);
    assert_true(sf->is_heart_beat_on);

    char* previous_sessiontoken = (char*)SF_MALLOC(strlen(sf->token) + 1);
    char* previous_masterToken = (char*)SF_MALLOC(strlen(sf->master_token) + 1);;
    strcpy(previous_sessiontoken, sf->token);
    strcpy(previous_masterToken, sf->master_token);
    test_heartbeat(sf);

    assert_false(sf_strncasecmp(previous_sessiontoken, sf->token, strlen(sf->token)) == 0);
    assert_false(sf_strncasecmp(previous_masterToken, sf->master_token, strlen(sf->token)) == 0);
    SF_FREE(previous_sessiontoken);
    SF_FREE(previous_masterToken);

    snowflake_term(sf);

}

//HEARTBEAT_DEBUG should be enabled
void test_heartbeat(void** unused)
{
#ifndef HEARTBEAT_DEBUG
    return;
#endif // 

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
    sf_bool client_session_keep_alive = SF_BOOLEAN_TRUE;
    snowflake_set_attribute(sf, SF_CON_CLIENT_SESSION_KEEP_ALIVE, &client_session_keep_alive);

    uint64 client_heartbeat_frequency = 30;
    snowflake_set_attribute(sf, SF_CON_CLIENT_SESSION_KEEP_ALIVE_HEARTBEAT_FREQUENCY, &client_heartbeat_frequency);

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    char* previous_sessiontoken = (char*)SF_MALLOC(strlen(sf->token) + 1);
    char* previous_masterToken = (char*)SF_MALLOC(strlen(sf->master_token) + 1);
    strcpy(previous_sessiontoken, sf->token);
    strcpy(previous_masterToken, sf->master_token);

    sf_sleep_ms(30 * 1000);
    assert_false(sf_strncasecmp(previous_sessiontoken, sf->token, strlen(sf->token)) == 0);
    assert_false(sf_strncasecmp(previous_masterToken, sf->master_token, strlen(sf->token)) == 0);

    snowflake_term(sf);
}


int main(void) {
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
    //cmocka_unit_test(test_connect_with_client_session_keep_alive_disable),
    //cmocka_unit_test(test_connect_with_client_session_keep_alive_current),
    //cmocka_unit_test(test_token_renew),
    cmocka_unit_test(test_heartbeat_manually),
    //cmocka_unit_test(test_heartbeat),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  return ret;

}
