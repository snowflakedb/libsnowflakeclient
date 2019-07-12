/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */


#include "utils/test_setup.h"

/**
 * Tests converting a string representation of log level to the log level enum
 */
void test_log_str_to_level(void **unused) {
    assert_int_equal(log_from_str_to_level("TRACE"), SF_LOG_TRACE);
    assert_int_equal(log_from_str_to_level("DEBUG"), SF_LOG_DEBUG);
    assert_int_equal(log_from_str_to_level("INFO"), SF_LOG_INFO);
    assert_int_equal(log_from_str_to_level("wArN"), SF_LOG_WARN);
    assert_int_equal(log_from_str_to_level("erroR"), SF_LOG_ERROR);
    assert_int_equal(log_from_str_to_level("fatal"), SF_LOG_FATAL);

    /* negative */
    assert_int_equal(log_from_str_to_level("hahahaha"), SF_LOG_FATAL);
    assert_int_equal(log_from_str_to_level(NULL), SF_LOG_FATAL);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_log_str_to_level),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
