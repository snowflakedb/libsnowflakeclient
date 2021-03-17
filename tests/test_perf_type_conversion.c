/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <string.h>
#include <snowflake/client.h>
#include "utils/test_setup.h"


void test_col_conv_int64_type(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;
    struct timespec begin, end;
    clockid_t clk_id = CLOCK_MONOTONIC;

    // Setup connection, run query, and get results back
    setup_and_run_query(&sf, &sfstmt, "select seq4() from table(generator(rowcount=>12000));");

    clock_gettime(clk_id, &begin);

    int64 out = 0;
    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_int64(sfstmt, 1, &out);
    }

    clock_gettime(clk_id, &end);

    process_results(begin, end, 12000, "test_col_conv_int64_type");

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_col_conv_float64_type(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;
    struct timespec begin, end;
    clockid_t clk_id = CLOCK_MONOTONIC;

    setup_and_run_query(&sf, &sfstmt, "select as_double(10.01) from table(generator(rowcount=>12000));");

    clock_gettime(clk_id, &begin);

    float64 out = 0;
    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_float64(sfstmt, 1, &out);
    }

    clock_gettime(clk_id, &end);

    process_results(begin, end, 12000, "test_col_conv_float64_type");

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_col_conv_str_type(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;
    struct timespec begin, end;
    clockid_t clk_id = CLOCK_MONOTONIC;

    setup_and_run_query(&sf, &sfstmt, "select randstr(999,random()) from table(generator(rowcount=>4000));");

    // Begin timing
    clock_gettime(clk_id, &begin);

    const char *out;
    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_const_str(sfstmt, 1, &out);
    }

    clock_gettime(clk_id, &end);

    process_results(begin, end, 4000, "test_col_conv_str_type");

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_col_conv_timestamp_type(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;
    struct timespec begin, end;
    clockid_t clk_id = CLOCK_MONOTONIC;

    setup_and_run_query(&sf, &sfstmt,
                        "select dateadd(day, uniform(1, 500, random()), current_timestamp) from table(generator(rowcount=>4000));");

    // Begin timing
    clock_gettime(clk_id, &begin);

    char *out = NULL;
    size_t out_len = 0;
    size_t out_max_size = 0;
    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_str(sfstmt, 1, &out, &out_len, &out_max_size);
    }

    clock_gettime(clk_id, &end);

    process_results(begin, end, 4000, "test_col_conv_timestamp_type");

    free(out);
    out = NULL;
    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_col_conv_multi_type(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;
    struct timespec begin, end;
    clockid_t clk_id = CLOCK_MONOTONIC;

    setup_and_run_query(&sf, &sfstmt, "select seq4() from table(generator(rowcount=>12000));");

    // Begin timing
    clock_gettime(clk_id, &begin);

    const char *out_str;
    int64 out_int;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        // Bind different result type every time to convert to multiple types
        switch(sfstmt->total_row_index % 2) {
            case 0:
                snowflake_column_as_const_str(sfstmt, 1, &out_str);
                break;
            case 1:
                snowflake_column_as_int64(sfstmt, 1, &out_int);
                break;
        }
    }

    clock_gettime(clk_id, &end);

    process_results(begin, end, 12000, "test_col_conv_multi_type");

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_col_conv_multi_types_per_row(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;
    struct timespec begin, end;
    clockid_t clk_id = CLOCK_MONOTONIC;

    setup_and_run_query(&sf, &sfstmt, "select seq4() from table(generator(rowcount=>12000));");

    clock_gettime(clk_id, &begin);

    const char *out_str;
    int64 out_int;
    float64 out_float;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_int64(sfstmt, 1, &out_int);
        snowflake_column_as_float64(sfstmt, 1, &out_float);
        snowflake_column_as_const_str(sfstmt, 1, &out_str);
    }

    clock_gettime(clk_id, &end);

    process_results(begin, end, 12000, "test_col_conv_multi_types_per_row");

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_col_conv_int64_type),
      cmocka_unit_test(test_col_conv_float64_type),
      cmocka_unit_test(test_col_conv_str_type),
      cmocka_unit_test(test_col_conv_timestamp_type),
      cmocka_unit_test(test_col_conv_multi_type),
      cmocka_unit_test(test_col_conv_multi_types_per_row),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}

