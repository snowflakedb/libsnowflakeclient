/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <string.h>
#include <snowflake/client.h>
#include "utils/test_setup.h"


void test_int64(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;
    struct timespec begin, end;
    clockid_t clk_id = CLOCK_MONOTONIC;

    setup_and_run_query(&sf, &sfstmt, "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE;");
    snowflake_query(sfstmt, "select seq4() from table(generator(rowcount=>400000));", 0);

    clock_gettime(clk_id, &begin);

    int64 out;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_int64(sfstmt, 1, &out);
    }

    clock_gettime(clk_id, &end);

    process_results(begin, end, 400000, "test_int64");

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_uint64(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;
    struct timespec begin, end;
    clockid_t clk_id = CLOCK_MONOTONIC;

    setup_and_run_query(&sf, &sfstmt, "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE;");
    snowflake_query(sfstmt, "select seq4() from table(generator(rowcount=>400000));", 0);

    clock_gettime(clk_id, &begin);

    uint64 out;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_uint64(sfstmt, 1, &out);
    }

    clock_gettime(clk_id, &end);

    process_results(begin, end, 400000, "test_uint64");

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_float32(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;
    struct timespec begin, end;
    clockid_t clk_id = CLOCK_MONOTONIC;

    setup_and_run_query(&sf, &sfstmt, "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE;");
    snowflake_query(sfstmt, "select as_double(10.01) from table(generator(rowcount=>400000));", 0);

    clock_gettime(clk_id, &begin);

    float32 out;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_float32(sfstmt, 1, &out);
    }

    clock_gettime(clk_id, &end);

    process_results(begin, end, 400000, "test_float32");

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_float64(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;
    struct timespec begin, end;
    clockid_t clk_id = CLOCK_MONOTONIC;

    setup_and_run_query(&sf, &sfstmt, "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE;");
    snowflake_query(sfstmt, "select as_double(10.01) from table(generator(rowcount=>400000));", 0);

    clock_gettime(clk_id, &begin);

    float64 out;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_float64(sfstmt, 1, &out);
    }

    clock_gettime(clk_id, &end);

    process_results(begin, end, 400000, "test_float64");

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_const_string(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;
    struct timespec begin, end;
    clockid_t clk_id = CLOCK_MONOTONIC;

    setup_and_run_query(&sf, &sfstmt, "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE;");
    snowflake_query(sfstmt, "select randstr(999,random()) from table(generator(rowcount=>400000));", 0);

    clock_gettime(clk_id, &begin);

    const char * out;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_const_str(sfstmt, 1, &out);
    }

    clock_gettime(clk_id, &end);

    process_results(begin, end, 400000, "test_const_string");

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_timestamp(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;
    struct timespec begin, end;
    clockid_t clk_id = CLOCK_MONOTONIC;

    setup_and_run_query(&sf, &sfstmt, "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE;");
    snowflake_query(sfstmt, "select dateadd(day, uniform(1, 500, random()), current_timestamp) from table(generator(rowcount=>400000));", 0);

    clock_gettime(clk_id, &begin);

    SF_TIMESTAMP out;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_timestamp(sfstmt, 1, &out);
    }

    clock_gettime(clk_id, &end);

    process_results(begin, end, 400000, "test_timestamp");

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_int64),
      cmocka_unit_test(test_uint64),
      cmocka_unit_test(test_float32),
      cmocka_unit_test(test_float64),
      cmocka_unit_test(test_const_string),
      cmocka_unit_test(test_timestamp),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
