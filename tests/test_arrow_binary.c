/*
 * Copyright (c) 2020 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include "utils/test_setup.h"

/**
 * Utility struct to help with building test cases.
 * c1 and c2 refer to the columns in the test table, t, used throughout this test set.
 *
 * - c1_in: The positional index of the test case.
 * - c2_in: A pointer to the input binary value to test.
 * - c2_in_len: The length of the input binary value in bytes.
 * - c2_out: The string representation of the binary value retrieved from the DB.
 * - c2_is_null: A flag representing whether c2 is NULL or not.
 */
typedef struct test_case_to_string {
    const int64 c1_in;
    const char *c2_in;
    const size_t c2_in_len;
    const char *c2_out;
    const sf_bool c2_is_null;
    SF_STATUS error_code;
} TEST_CASE_TO_STRING;

/**
 * Test set containing all individual test cases to run.
 */
void test_arrow_binary(void **unused) {
    // Test cases.
    const char * empty         = "";
    const char * hex_alpha     = "\xab\xcd\xef";
    const char * hex_alpha_out = "ABCDEF";
    const char * hex_digit     = "\x12\x34\x56\x78\x90";
    const char * hex_digit_out = "1234567890";
    const char * hex_mixed     = "\x56\x78\x00\xbc\xde\xf0";
    const char * hex_mixed_out = "567800BCDEF0";
    const char * hex_zeros     = "\x00\x00\x00";
    const char * hex_zeros_out = "000";

    // All cases with an expected error code go at the end to avoid
    // fragmentation of test cases indices.
    TEST_CASE_TO_STRING test_cases[] = {
        { .c1_in = 0, .c2_in = NULL,      .c2_in_len = 0,  .c2_out = NULL,          .c2_is_null = SF_BOOLEAN_TRUE },
        { .c1_in = 1, .c2_in = empty,     .c2_in_len = 0,  .c2_out = "",            .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1_in = 2, .c2_in = hex_alpha, .c2_in_len = 3,  .c2_out = hex_alpha_out, .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1_in = 3, .c2_in = hex_digit, .c2_in_len = 5,  .c2_out = hex_digit_out, .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1_in = 4, .c2_in = hex_mixed, .c2_in_len = 6,  .c2_out = hex_mixed_out, .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1_in = 5, .c2_in = hex_zeros, .c2_in_len = 3,  .c2_out = hex_zeros_out, .c2_is_null = SF_BOOLEAN_FALSE }
    };

    SF_CONNECT *sf = setup_snowflake_connection();

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // Create a statement object and reuse it for all test cases.
    SF_STMT *sfstmt = snowflake_stmt(sf);

    // Configure this session to use Arrow format.
    status = snowflake_query(
        sfstmt,
        "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE",
        0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // NOTE: The numeric type here should fit into int64.
    // Otherwise, it is taken as a float.
    status = snowflake_query(
        sfstmt,
        "create or replace table t (c1 int, c2 binary)",
        0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_prepare(
        sfstmt,
        "insert into t(c1,c2) values(?,?)",
        0);

    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // Loop over the test cases via index and insert them one by one.
    size_t i;
    size_t num_test_cases = sizeof(test_cases) / sizeof(TEST_CASE_TO_STRING);
    size_t num_successful_inserts = 0;

    for (i = 0; i < num_test_cases; i++) {
        TEST_CASE_TO_STRING tc = test_cases[i];

        // Bind input c1.
        SF_BIND_INPUT in_c1 = { 0 };
        in_c1.idx = 1;
        in_c1.name = NULL;
        in_c1.c_type = SF_C_TYPE_INT64;
        in_c1.value = (void*) &tc.c1_in;
        in_c1.len = sizeof(tc.c1_in);

        status = snowflake_bind_param(sfstmt, &in_c1);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        // Bind input c2.
        SF_BIND_INPUT in_c2 = { 0 };
        in_c2.idx = 2;
        in_c2.name = NULL;
        in_c2.c_type = SF_C_TYPE_BINARY;
        in_c2.value = (void*) tc.c2_in;
        in_c2.len = tc.c2_in_len;

        status = snowflake_bind_param(sfstmt, &in_c2);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        // Execute.
        // If the error_code member is non-zero, then we expect the query to fail.
        // In that case, ensure the error code matches with what is expected.
        // Otherwise, proceed as normal.
        status = snowflake_execute(sfstmt);
        if (tc.error_code == SF_STATUS_SUCCESS) {
            if (status != SF_STATUS_SUCCESS) {
                dump_error(&(sfstmt->error));
            }
            ++num_successful_inserts;
        } else {
            SF_ERROR_STRUCT * err = snowflake_stmt_error(sfstmt);
            assert_int_equal(tc.error_code, err->error_code);
        }
    }

    // Query the table and check for correctness.
    status = snowflake_query(sfstmt, "select * from t", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(sfstmt), num_test_cases);

    int64 c1 = 0;
    char *c2 = NULL;
    size_t c2_len = 0;
    size_t c2_max_size = 0;

    int64 curr_row;
    int64 last_read_row = 0;

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        // Skip past column 1 as it does not contain meaningful test data.
        for (curr_row = last_read_row; curr_row < num_successful_inserts; ++curr_row) {
            snowflake_next(sfstmt);
        }

        // Test column 2.
        for (curr_row = last_read_row; curr_row < num_successful_inserts; ++curr_row) {
            // Valid if value copied to c2 matches the value in tc.c2_out.
            // c2_len and c2_max_size are unused.
            TEST_CASE_TO_STRING tc = test_cases[curr_row];
            if (tc.c2_in != NULL) {
                snowflake_column_as_str(sfstmt, 2, &c2, &c2_len, &c2_max_size);
                snowflake_next(sfstmt);
                assert_string_equal(tc.c2_out, c2);
            } else {
                sf_bool c2_is_null;
                snowflake_column_is_null(sfstmt, 2, &c2_is_null);
                snowflake_next(sfstmt);
                assert_true(tc.c2_is_null == c2_is_null);
            }
        }

        last_read_row = curr_row;
    }

    // Clean-up.
    if (status != SF_STATUS_EOF) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_EOF);

    status = snowflake_query(sfstmt, "drop table if exists t", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    free(c2);
    c2 = NULL;
    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = { cmocka_unit_test(test_arrow_binary) };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
