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
 & - c2_out: A pointer to the expected output value retrieved from the server.
 * - c2_is_null: A flag indicating whether the input number value is NULL or not.
 * - error_code: The error code a test case is expected to fail with.
 *     - 100038 for "Numeric value '___' is not recognized"
 */
typedef struct test_case_to_string {
    const int64 c1;
    const char * c2;
    const size_t c2_len;
    const char * c2_out;
    const sf_bool c2_is_null;
    SF_STATUS error_code;
} TEST_CASE_TO_STRING;

/**
 * This test set targets the FLOAT data type on Snowflake.
 * The FLOAT type is synonymous with FLOAT4 and FLOAT8, which exist for compatibility.
 *
 * Snowflake uses double-precision (64 bit) IEEE 754 floating-point numbers.
 *
 * Integers within [-2^53 to +2^53] can be exactly represented.
 * We will treat this as our range of values as the true max is too long.
 * This is a precision of approximately 15 digits.
 *
 * Beyond this, integers that are exactly representable are spaced apart
 * by increasing powers of two.
 *
 * For example, some representable integers are:
 *
 * - Every 2nd integer between [2^53 + 2, 2^54],
 * - Every 4th integer between [2^54 + 4, 2^55],
 * - Every 8th integer between [2^55 + 8, 2^56],
 *
 * and so on.
 *
 * Floating-points can span approximately [1e-308, 1e308], but for testing
 * purposes, will be limited to [1e-15, 1e15] instead.
 *
 * Numeric literals follow the syntax: [+-][digits][.digits][e[+-]digits]
 * 
 * For example, the following are valid numeric literals:
 *
 * - 15
 * - +1.23
 * - 0.2
 * - 15e-03
 * - 1.23E4
 * - 1.23E+4
 * - -1
 *
 * Finally, Snowflake supports the following special values:
 *
 * - 'NaN' for 'not a number',
 * - 'inf' for positive infinity, and
 * - '-inf' for negative infinity.
 */
void test_arrow_float(void** unused) {
    // Test cases.
    // NOTE: For some reason, Snowflake caps precision to 15 digits.
    // NOTE: There is some kind of error when trying to send payloads for
    //       NaN, inf, and -inf. These cases are not working as expected.
    const char * nan        = "NaN";
    const char * inf_neg    = "-inf";
    const char * inf_pos    = "inf";
    const char * trash      = "ABC";
    const char * empty      = "";
    // Scale 0.
    const char * s0_max_neg_2     = "-18014398509481984"; // -2^54
    const char * s0_max_neg_2_out = "-18014398509481984.";
    const char * s0_max_neg       = "-9007199254740992"; // -2^53
    const char * s0_max_neg_out   = "-9007199254740992.";
    const char * s0_reg_neg       = "-1234567890";
    const char * s0_reg_neg_out   = "-1234567890.";
    const char * s0_zero          = "0";
    const char * s0_zero_out      = "0.";
    const char * s0_one           = "1";
    const char * s0_one_out       = "1.";
    const char * s0_reg_pos       = "1234567890";
    const char * s0_reg_pos_out   = "1234567890.";
    const char * s0_max_pos       = "9007199254740992"; // 2^53
    const char * s0_max_pos_out   = "9007199254740992.";
    const char * s0_max_pos_2     = "18014398509481984"; // 2^54
    const char * s0_max_pos_2_out = "18014398509481984.";
    // Scale 8
    const char * s8_max_neg  = "-1234567.123456";
    const char * s8_reg_neg  = "-0.123456";
    const char * s8_zero     = "0.0";
    const char * s8_zero_out = "0.";
    const char * s8_one      = "1.0";
    const char * s8_one_out  = "1.";
    const char * s8_reg_pos  = "0.123456";
    const char * s8_max_pos  = "1234567.123456";

    // All cases with an expected error code go at the end to avoid
    // fragmentation of test cases indices.
    TEST_CASE_TO_STRING test_cases[] = {
        { .c1 =  0, .c2 = NULL,         .c2_len = 0,  .c2_out = "",               .c2_is_null = SF_BOOLEAN_TRUE },
        { .c1 =  1, .c2 = s0_max_neg_2, .c2_len = 19, .c2_out = s0_max_neg_2_out, .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 =  2, .c2 = s0_max_neg,   .c2_len = 18, .c2_out = s0_max_neg_out,   .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 =  3, .c2 = s0_reg_neg,   .c2_len = 12, .c2_out = s0_reg_neg_out,   .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 =  4, .c2 = s0_zero,      .c2_len = 2,  .c2_out = s0_zero_out,      .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 =  5, .c2 = s0_one,       .c2_len = 2,  .c2_out = s0_one_out,       .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 =  6, .c2 = s0_reg_pos,   .c2_len = 11, .c2_out = s0_reg_pos_out,   .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 =  7, .c2 = s0_max_pos,   .c2_len = 17, .c2_out = s0_max_pos_out,   .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 =  8, .c2 = s0_max_pos_2, .c2_len = 18, .c2_out = s0_max_pos_2_out, .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 =  9, .c2 = s8_max_neg,   .c2_len = 16, .c2_out = s8_max_neg,       .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 10, .c2 = s8_reg_neg,   .c2_len = 10, .c2_out = s8_reg_neg,       .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 11, .c2 = s8_zero,      .c2_len = 4,  .c2_out = s8_zero_out,      .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 12, .c2 = s8_one,       .c2_len = 4,  .c2_out = s8_one_out,       .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 13, .c2 = s8_reg_pos,   .c2_len = 9,  .c2_out = s8_reg_pos,       .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 14, .c2 = s8_max_pos,   .c2_len = 15, .c2_out = s8_max_pos,       .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1 = 15, .c2 = trash,        .c2_len = 4,  .c2_out = "",               .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100038 },
        { .c1 = 16, .c2 = empty,        .c2_len = 0,  .c2_out = "",               .c2_is_null = SF_BOOLEAN_FALSE, .error_code = 100038 }
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

    // The test table created holds several columns of type NUMBER with varying
    // precisions and scales. The TEST_CASE_TO_STRING struct should map a given
    // test case to the correct column index by using the .c2_idx member.
    status = snowflake_query(
        sfstmt,
        "create or replace table t (c1 int, c2 float)",
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
        in_c2.value = (void*) tc.c2;
        in_c2.len = tc.c2_len;

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
    assert_int_equal(snowflake_num_rows(sfstmt), num_successful_inserts);

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
            sf_bool c2_is_null;
            snowflake_column_is_null(sfstmt, 2, &c2_is_null);
            assert_true(tc.c2_is_null == c2_is_null);
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

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = { cmocka_unit_test(test_arrow_float) };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
