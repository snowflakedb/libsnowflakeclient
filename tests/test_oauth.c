//
// Copyright (c) 2018-2024 Snowflake Computing, Inc. All rights reserved.
//

#include "utils/test_setup.h"

/**
 * Test connection with OAuth authentication.
 */
void test_connect_with_oauth(void **unused) {
    SF_CONNECT *sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT,
                            getenv("SNOWFLAKE_TEST_ACCOUNT"));
    //snowflake_set_attribute(sf, SF_CON_USER, getenv("SNOWFLAKE_TEST_USER"));
    snowflake_set_attribute(sf, SF_CON_USER, getenv("SNOWFLAKE_TEST_USER"));

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
    char* token = "ver:1-hint:92019684051950-ETMsDgAAAZHjCazwABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwEAABAAEDLFbJVGI/VDGAIG0Jm0AQsAAABQPbcQJdbDk+Q0Zpk/0x9WWvPtfvqRPrRXnRqpRTvo4doyr2GRi3szvTSB0ARuep3UxDnkvtFyYVPL8KrDxocJG7PqGqdOzghtGIh3XEs2sR0AFPax6y9x7O+Kn8I2WE4PjRx+Ynjm";
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "oauth");
    snowflake_set_attribute(sf, SF_CON_OAUTH_TOKEN, token);

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    snowflake_term(sf);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_connect_with_oauth),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
