/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
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
    col_conv_setup(&sf, &sfstmt, "select seq4() from table(generator(rowcount=>12000));");

    clock_gettime(clk_id, &begin);

    // Configure result binding and bind
    SF_BIND_OUTPUT c1 = {0};
    int64 out = 0;
    c1.idx = 1;
    c1.c_type = SF_C_TYPE_INT64;
    c1.value = (void *) &out;
    c1.max_length = sizeof(out);
    snowflake_bind_result(sfstmt, &c1);

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {}

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

    col_conv_setup(&sf, &sfstmt, "select as_double(10.01) from table(generator(rowcount=>12000));");

    clock_gettime(clk_id, &begin);

    // Configure result binding and bind
    SF_BIND_OUTPUT c1 = {0};
    float64 out = 0;
    c1.idx = 1;
    c1.c_type = SF_C_TYPE_FLOAT64;
    c1.value = (void *) &out;
    c1.max_length = sizeof(out);
    snowflake_bind_result(sfstmt, &c1);

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {}

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

    col_conv_setup(&sf, &sfstmt, "select randstr(999,random()) from table(generator(rowcount=>4000));");

    // Begin timing
    clock_gettime(clk_id, &begin);

    // Configure result binding and bind
    SF_BIND_OUTPUT c1 = {0};
    char out[1000];
    c1.idx = 1;
    c1.c_type = SF_C_TYPE_STRING;
    c1.value = (void *) &out;
    c1.max_length = sizeof(out);
    snowflake_bind_result(sfstmt, &c1);

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {}

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

    col_conv_setup(&sf, &sfstmt, "select dateadd(day, uniform(1, 500, random()), current_timestamp) from table(generator(rowcount=>4000));");

    // Begin timing
    clock_gettime(clk_id, &begin);

    // Configure result binding and bind
    SF_BIND_OUTPUT c1 = {0};
    char out[1000];
    c1.idx = 1;
    c1.c_type = SF_C_TYPE_STRING;
    c1.value = (void *) &out;
    c1.max_length = sizeof(out);
    snowflake_bind_result(sfstmt, &c1);

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {}

    clock_gettime(clk_id, &end);

    process_results(begin, end, 4000, "test_col_conv_timestamp_type");

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_col_conv_multi_type(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;
    struct timespec begin, end;
    clockid_t clk_id = CLOCK_MONOTONIC;

    col_conv_setup(&sf, &sfstmt, "select seq4() from table(generator(rowcount=>12000));");

    // Begin timing
    clock_gettime(clk_id, &begin);

    // Configure result binding and bind
    SF_BIND_OUTPUT c1;
    char out_str[1000];
    c1.idx = 1;
    c1.c_type = SF_C_TYPE_STRING;
    c1.value = (void *) &out_str;
    c1.max_length = sizeof(out_str);

    int64 out_int;

    snowflake_bind_result(sfstmt, &c1);

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        // Bind different result type every time to convert to multiple types
        switch(sfstmt->total_row_index % 2) {
            case 0:
                c1.c_type = SF_C_TYPE_STRING;
                c1.value = (void *) &out_str;
                c1.max_length = sizeof(out_str);
                break;
            case 1:
                c1.c_type = SF_C_TYPE_INT64;
                c1.value = (void *) &out_int;
                c1.max_length = sizeof(out_int);
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

    col_conv_setup(&sf, &sfstmt, "select seq4() from table(generator(rowcount=>12000));");

    clock_gettime(clk_id, &begin);

    // Configure result binding and bind
    SF_BIND_OUTPUT c1 = {0};
    char out_str[1000];
    c1.idx = 1;
    c1.c_type = SF_C_TYPE_STRING;
    c1.value = (void *) &out_str;
    c1.max_length = sizeof(out_str);
    snowflake_bind_result(sfstmt, &c1);

    int64 out_int;
    float64 out_float;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        out_int = strtol(out_str, NULL, 0);
        out_float = strtod(out_str, NULL);
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

