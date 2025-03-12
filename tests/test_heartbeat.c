//
// Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
//

#include "utils/test_setup.h"
#include "../lib/snowflake_util.h"

void test_connect_with_client_session_keep_alive_disable(void** unused) {
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

void test_connect_with_client_session_keep_alive(void** unused) {
    log_set_quiet(0);
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
    sf->is_heart_beat_debug_mode = SF_BOOLEAN_TRUE;

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }

    sf_sleep_ms(15000);

    assert_int_equal(status, SF_STATUS_SUCCESS);
    snowflake_term(sf);
}


int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_connect_with_client_session_keep_alive_disable),
      cmocka_unit_test(test_connect_with_client_session_keep_alive),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
