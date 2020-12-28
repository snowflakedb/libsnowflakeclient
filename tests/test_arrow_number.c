/*
 * Copyright (c) 2020 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include "utils/test_setup.h"

/**
 * Utility struct to help with building test cases.
 * c1, c2 and c3 refer to the columns in the test table, t, used throughout this test set.
 *
 * - c1: The positional index of the test case.
 * - c2: A pointer to an input number value to test.
 *       Note: the number is represented as a string to use arbitrary precision and scale.
 * - c2_len: The length of the input number value.
 * - c2_is_null: A flag indicating whether the input number value is NULL or not.
 * - error_code: The error code a test case is expected to fail with.
 *     - ?????? for "Numeric value '___' is not recognized"
 *     - ?????? for "Numeric value '___' is out of range"
 */
typedef struct test_case_to_string {
    const int64 c1;
    const char *c2;
    const size_t c2_len;
    const sf_bool c2_is_null;
    const SF_STATUS error_code;
} TEST_CASE_TO_STRING;

/**
 * This test set targets the NUMBER data type on Snowflake.
 * The NUMBER type is synonymous with NUMERIC and DECIMAL.
 *
 * Integral types (INT, INTEGER, BIGINT, TINYINT, BYTEINT) are equivalent to NUMBER(38,0).
 * That is, these types have a fixed precision of 38 and scale of 0.
 *
 */
void test_arrow_number_helper(sf_bool use_zero_scale) {
    // Test cases.
    // The range of valid values for NUMBER is the interval [-1e39,1e39].
    const char * trash      = "ABC";
    const char * empty      = "";
    // Scale 0.
    const char * s0_oob_neg = "-100000000000000000000000000000000000000";  // -1e39
    const char * s0_max_neg = "-99999999999999999999999999999999999999";
    const char * s0_reg_neg = "-1234567890";
    const char * s0_zero    = "0";
    const char * s0_one     = "1";
    const char * s0_reg_pos = "1234567890";
    const char * s0_max_pos = "99999999999999999999999999999999999999";
    const char * s0_oob_pos = "100000000000000000000000000000000000000";   // 1e39
    // Scale 37.
    const char * s37_oob_neg = "-1.00000000000000000000000000000000000000";
    const char * s37_max_neg = "-9.9999999999999999999999999999999999999";
    const char * s37_reg_neg = "-1234567890.0987654321";
    const char * s37_zero    = "0.0";
    const char * s37_one     = "1.0";
    const char * s37_reg_pos = "1234567890.0987654321";
    const char * s37_max_pos = "9.9999999999999999999999999999999999999";
    const char * s37_oob_pos = "1.00000000000000000000000000000000000000";

    TEST_CASE_TO_STRING s0_test_cases[] = {
        { .c1 = 0,  .c2 = NULL,        .c2_len = 0,  .c2_is_null = SF_BOOLEAN_TRUE },
        { .c1 = 1,  .c2 = &trash,      .c2_len = 0,  .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 2,  .c2 = &empty,      .c2_len = 0,  .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 3,  .c2 = &s0_oob_neg, .c2_len = 40, .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 4,  .c2 = &s0_max_neg, .c2_len = 39, .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 5,  .c2 = &s0_reg_neg, .c2_len = 11, .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 6,  .c2 = &s0_zero,    .c2_len = 1,  .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 7,  .c2 = &s0_one,     .c2_len = 1,  .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 8,  .c2 = &s0_reg_pos, .c2_len = 21, .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 9,  .c2 = &s0_max_pos, .c2_len = 38, .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 10, .c2 = &s0_oob_pos, .c2_len = 39, .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
    };

    TEST_CASE_TO_STRING s37_test_cases[] = {
        { .c1 = 0,  .c2 = NULL,         .c2_len = 0,  .c2_is_null = SF_BOOLEAN_TRUE },
        { .c1 = 1,  .c2 = &trash,       .c2_len = 0,  .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 2,  .c2 = &empty,       .c2_len = 0,  .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 3,  .c2 = &s37_oob_neg, .c2_len = 41, .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 },
        { .c1 = 4,  .c2 = &s37_max_neg, .c2_len = 40, .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 5,  .c2 = &s37_reg_neg, .c2_len = 22, .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 6,  .c2 = &s37_zero,    .c2_len = 3,  .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 7,  .c2 = &s37_one,     .c2_len = 3,  .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 8,  .c2 = &s37_reg_pos, .c2_len = 10, .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 9,  .c2 = &s37_max_pos, .c2_len = 39, .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 10, .c2 = &s37_oob_pos, .c2_len = 40, .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100108 }
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
        "alter session set ODBC_QUERY_RESULT_FORMAT=ARROW_FORCE",
        0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // The test table created holds several columns of type NUMBER with varying
    // precisions and scales. The TEST_CASE_TO_STRING struct should map a given
    // test case to the correct column index by using the .c2_idx member.
    status = snowflake_query(
        sfstmt,
        use_zero_scale ?
            "create or replace table t (c1 int, c2 number(38,0))" :
            "create or replace table t (c1 int, c2 number(38,37))",
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
    TEST_CASE_TO_STRING test_cases[] =
        use_zero_scale ? s0_test_cases : s37_test_cases;
    size_t i;
    size_t num_test_cases = sizeof(test_cases) / sizeof(TEST_CASE_TO_STRING);

    for (i = 0; i < num_test_cases; i++) {
        TEST_CASE_TO_STRING tc = test_cases[i];

        // Bind input c1.
        SF_BIND_INPUT in_c1;
        in_c1.idx = 1;
        in_c1.name = NULL;
        in_c1.c_type = SF_C_TYPE_INT64;
        in_c1.value = (void*) &tc.c1;
        in_c1.len = sizeof(tc.c1);

        status = snowflake_bind_param(sfstmt, &in_c1);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        // Bind input c2.
        SF_BIND_INPUT in_c2;
        in_c2.idx = 2;
        in_c2.name = NULL;
        in_c2.c_type = SF_C_TYPE_STRING;
        in_c2.value = (void*) &tc.c2;
        in_c2.len = sizeof(sf_bool);

        status = snowflake_bind_param(sfstmt, &in_c1);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        // Execute.
        status = snowflake_execute(sfstmt);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);
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

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_int64(sfstmt, 1, &c1);
        TEST_CASE_TO_STRING tc = test_cases[c1];

        // Valid if the value copied to c2 matches the value in tc.c2_out.
        // c2_len and c2_max_size are unused.
        if (tc.c2 != NULL) {
            snowflake_column_as_str(sfstmt, 2, &c2, &c2_len, &c2_max_size);
            assert_string_equal(tc.c2_out, c2);
        } else {
            sf_bool is_null;
            snowflake_column_is_null(sfstmt, 2, &is_null);
            assert_true(tc.c2_is_null == is_null);
        }
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

/**
 * Test set containing all individual test cases to run.
 */
void test_arrow_number(void **unused) {
    test_arrow_number_helper(SF_BOOLEAN_TRUE);
    test_arrow_number_helper(SF_BOOLEAN_FALSE);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = { cmocka_unit_test(test_arrow_number) };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
