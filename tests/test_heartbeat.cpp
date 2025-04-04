/*
 * Copyright (c) 2025 Snowflake Computing, Inc. All rights reserved.
 */

#include <string>
#include <vector>
#include <thread>

#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"
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

int main(void) {
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_connect_with_client_session_keep_alive_disable),
    cmocka_unit_test(test_connect_with_client_session_keep_alive_current),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  return ret;
}
