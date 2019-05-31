/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include "utils/test_setup.h"

void test_native_timestamp(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;

    // Setup connection, run query, and get results back
    setup_and_run_query(&sf, &sfstmt, "select to_time('12:34:56'),"
                                      "date_from_parts(2018, 09, 14), "
                                      "timestamp_ltz_from_parts(2014, 03, 20, 15, 30, 45, 493679329), "
                                      "timestamp_ntz_from_parts(2014, 03, 20, 15, 30, 45, 493679329), "
                                      "timestamp_tz_from_parts(2014, 03, 20, 15, 30, 45, 493679329, 'America/Los_Angeles')");

    // Stores the result from the fetch operation
    SF_TIMESTAMP ts;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        // Converting a TIME type into a SF_TIMESTAMP
        if (snowflake_column_as_timestamp(sfstmt, 1, &ts)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(12, snowflake_timestamp_get_hours(&ts));
        assert_int_equal(34, snowflake_timestamp_get_minutes(&ts));
        assert_int_equal(56, snowflake_timestamp_get_seconds(&ts));
        assert_int_equal(0, snowflake_timestamp_get_nanoseconds(&ts));

        // Converting a DATE type into a SF_TIMESTAMP
        if (snowflake_column_as_timestamp(sfstmt, 2, &ts)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(14, snowflake_timestamp_get_mday(&ts));
        assert_int_equal(9, snowflake_timestamp_get_month(&ts));
        assert_int_equal(2018, snowflake_timestamp_get_year(&ts));
        assert_int_equal(0, snowflake_timestamp_get_hours(&ts));
        assert_int_equal(0, snowflake_timestamp_get_minutes(&ts));
        assert_int_equal(0, snowflake_timestamp_get_seconds(&ts));
        assert_int_equal(0, snowflake_timestamp_get_nanoseconds(&ts));

        // Converting a TIMESTAMP_LTZ type into a SF_TIMESTAMP
        if (snowflake_column_as_timestamp(sfstmt, 3, &ts)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(20, snowflake_timestamp_get_mday(&ts));
        assert_int_equal(3, snowflake_timestamp_get_month(&ts));
        assert_int_equal(2014, snowflake_timestamp_get_year(&ts));
        assert_int_equal(15, snowflake_timestamp_get_hours(&ts));
        assert_int_equal(30, snowflake_timestamp_get_minutes(&ts));
        assert_int_equal(45, snowflake_timestamp_get_seconds(&ts));
        assert_int_equal(493679329, snowflake_timestamp_get_nanoseconds(&ts));

        // Converting a TIMESTAMP_NTZ type into a SF_TIMESTAMP
        if (snowflake_column_as_timestamp(sfstmt, 4, &ts)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(20, snowflake_timestamp_get_mday(&ts));
        assert_int_equal(3, snowflake_timestamp_get_month(&ts));
        assert_int_equal(2014, snowflake_timestamp_get_year(&ts));
        assert_int_equal(15, snowflake_timestamp_get_hours(&ts));
        assert_int_equal(30, snowflake_timestamp_get_minutes(&ts));
        assert_int_equal(45, snowflake_timestamp_get_seconds(&ts));
        assert_int_equal(493679329, snowflake_timestamp_get_nanoseconds(&ts));

        // Converting a TIMESTAMP_TZ type into a SF_TIMESTAMP
        if (snowflake_column_as_timestamp(sfstmt, 5, &ts)) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(20, snowflake_timestamp_get_mday(&ts));
        assert_int_equal(3, snowflake_timestamp_get_month(&ts));
        assert_int_equal(2014, snowflake_timestamp_get_year(&ts));
        assert_int_equal(15, snowflake_timestamp_get_hours(&ts));
        assert_int_equal(30, snowflake_timestamp_get_minutes(&ts));
        assert_int_equal(45, snowflake_timestamp_get_seconds(&ts));
        assert_int_equal(493679329, snowflake_timestamp_get_nanoseconds(&ts));
    }

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_native_timestamp),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
