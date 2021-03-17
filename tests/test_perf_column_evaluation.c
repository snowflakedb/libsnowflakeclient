/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <string.h>
#include "utils/test_setup.h"

const char *COL_EVAL_QUERY = "select seq4(), seq4(), seq4(), seq4(), seq4(), seq4() from table(generator(rowcount=>4000));";
const int NUM_COLS = 6;
const int NUM_ROWS = 4000;

void test_eval_all_cols(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;
    struct timespec begin, end;
    clockid_t clk_id = CLOCK_MONOTONIC;
    FILE *dev_null = fopen("/dev/null", "w");

    // Setup connection, run query, and get results back
    setup_and_run_query(&sf, &sfstmt, COL_EVAL_QUERY);

    clock_gettime(clk_id, &begin);

    // Configure result binding and bind
    int64 out[NUM_COLS];

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        for (int i = 0; i < NUM_COLS; i++) {
            snowflake_column_as_int64(sfstmt, i + 1, &out[i]);
        }
        fprintf(dev_null, "%lli, %lli, %lli, %lli, %lli, %lli", out[0], out[1], out[2], out[3], out[4], out[5]);
    }

    clock_gettime(clk_id, &end);

    process_results(begin, end, NUM_ROWS, "test_eval_all_cols");

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
    fclose(dev_null);
}

void test_eval_half_cols(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;
    struct timespec begin, end;
    clockid_t clk_id = CLOCK_MONOTONIC;
    FILE *dev_null = fopen("/dev/null", "w");

    // Setup connection, run query, and get results back
    setup_and_run_query(&sf, &sfstmt, COL_EVAL_QUERY);

    clock_gettime(clk_id, &begin);

    // Configure result binding and bind
    int64 out[NUM_COLS];

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        if (sfstmt->total_row_index % 2) {
            for (int i = 0; i < 3; i++) {
                snowflake_column_as_int64(sfstmt, i + 1, &out[i]);
            }
            fprintf(dev_null, "%lli, %lli, %lli", out[0], out[1], out[2]);
        } else {
            for (int i = 3; i < NUM_COLS; i++) {
                snowflake_column_as_int64(sfstmt, i + 1, &out[i]);
            }
            fprintf(dev_null, "%lli, %lli, %lli", out[3], out[4], out[5]);
        }
    }

    clock_gettime(clk_id, &end);

    process_results(begin, end, NUM_ROWS, "test_eval_half_cols");

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
    fclose(dev_null);
}

void test_skip_rows_half(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;
    struct timespec begin, end;
    clockid_t clk_id = CLOCK_MONOTONIC;
    FILE *dev_null = fopen("/dev/null", "w");

    // Setup connection, run query, and get results back
    setup_and_run_query(&sf, &sfstmt, COL_EVAL_QUERY);

    clock_gettime(clk_id, &begin);

    // Configure result binding and bind
    int64 out[NUM_COLS];

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        if (sfstmt->total_row_index % 2) {
            for (int i = 0; i < NUM_COLS; i++) {
                snowflake_column_as_int64(sfstmt, i + 1, &out[i]);
            }
            fprintf(dev_null, "%lli, %lli, %lli, %lli, %lli, %lli", out[0], out[1], out[2], out[3], out[4], out[5]);
        }
    }

    clock_gettime(clk_id, &end);

    process_results(begin, end, NUM_ROWS, "test_skip_rows_half");

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
    fclose(dev_null);
}

void test_skip_all_rows(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;
    struct timespec begin, end;
    clockid_t clk_id = CLOCK_MONOTONIC;
    FILE *dev_null = fopen("/dev/null", "w");

    // Setup connection, run query, and get results back
    setup_and_run_query(&sf, &sfstmt, COL_EVAL_QUERY);

    clock_gettime(clk_id, &begin);

    // Configure result binding and bind
    int64 out[NUM_COLS];

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        for (int i = 0; i < NUM_COLS; i++) {
            snowflake_column_as_int64(sfstmt, i + 1, &out[i]);
        }
    }

    clock_gettime(clk_id, &end);

    process_results(begin, end, NUM_ROWS, "test_skip_all_rows");

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
    fclose(dev_null);
}

void test_skip_all_rows_no_bind(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;
    struct timespec begin, end;
    clockid_t clk_id = CLOCK_MONOTONIC;
    FILE *dev_null = fopen("/dev/null", "w");

    // Setup connection, run query, and get results back
    setup_and_run_query(&sf, &sfstmt, COL_EVAL_QUERY);

    clock_gettime(clk_id, &begin);

    // Skip configuring and bind of result bindings

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {}

    clock_gettime(clk_id, &end);

    process_results(begin, end, NUM_ROWS, "test_skip_all_rows_no_bind");

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
    fclose(dev_null);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_eval_all_cols),
      cmocka_unit_test(test_eval_half_cols),
      cmocka_unit_test(test_skip_rows_half),
      cmocka_unit_test(test_skip_all_rows),
      cmocka_unit_test(test_skip_all_rows_no_bind),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}

