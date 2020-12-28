/*
 * Copyright (c) 2020 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include "test_setup.h"

/**
 * Utility struct to help with building test cases.
 * c1 and c2 refer to the columns in the test table, t, used throughout this test set.
 *
 * - c1_in: The positional index of the test case.
 * - c2_in: A pointer to the input Boolean value to test.
 * - c2_out: The Boolean value retrieved from the DB as a string.
 *          False is represented as an empty string.
 * - c2_is_null: A flag indicating whether the input is NULL or not.
 */
typedef struct test_case_to_string {
    const int64 c1_in;
    const sf_bool *c2_in;
    const char *c2_out;
    const sf_bool c2_is_null;
} TEST_CASE_TO_STRING;

/**
 * Test set containing all individual test cases to run.
 */
void test_arrow_bool(void **unused) {
    // Test cases.
    const sf_bool oob_neg_value = (sf_bool) -129;
    const sf_bool max_neg_value = (sf_bool) -128;
    const sf_bool reg_neg_value = (sf_bool) -32;
    const sf_bool zero_value    = (sf_bool) 0;
    const sf_bool one_value     = (sf_bool) 1;
    const sf_bool reg_pos_value = (sf_bool) 32;
    const sf_bool max_pos_value = (sf_bool) 127;
    const sf_bool oob_pos_value = (sf_bool) 128;

    TEST_CASE_TO_STRING test_cases[] = {
        { .c1_in = 0,  .c2_in = NULL,              .c2_out = NULL, .c2_is_null = SF_BOOLEAN_TRUE },
        { .c1_in = 1,  .c2_in = &SF_BOOLEAN_TRUE,  .c2_out = "1",  .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1_in = 2,  .c2_in = &SF_BOOLEAN_FALSE, .c2_out = "",   .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1_in = 3,  .c2_in = &oob_neg_value,    .c2_out = "",   .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1_in = 4,  .c2_in = &max_neg_value,    .c2_out = "1",  .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1_in = 5,  .c2_in = &reg_neg_value,    .c2_out = "1",  .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1_in = 6,  .c2_in = &zero_value,       .c2_out = "",   .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1_in = 7,  .c2_in = &one_value,        .c2_out = "1",  .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1_in = 8,  .c2_in = &reg_pos_value,    .c2_out = "1",  .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1_in = 9,  .c2_in = &max_pos_value,    .c2_out = "1",  .c2_is_null = SF_BOOLEAN_FALSE },
        { .c1_in = 10, .c2_in = &oob_pos_value,    .c2_out = "",   .c2_is_null = SF_BOOLEAN_FALSE }
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

    // NOTE: The numeric type here should fit into int64.
    // Otherwise, it is taken as a float.
    status = snowflake_query(
        sfstmt,
        "create or replace table t (c1 int, c2 boolean)",
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

    for (i = 0; i < num_test_cases; i++) {
        TEST_CASE_TO_STRING tc = test_cases[i];

        // Bind input c1.
        SF_BIND_INPUT in_c1;
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
        SF_BIND_INPUT in_c2;
        in_c2.idx = 2;
        in_c2.name = NULL;
        in_c2.c_type = SF_C_TYPE_BOOLEAN;
        in_c2.value = (void*) &tc.c2_in;
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
        if (tc.c2_in != NULL) {
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

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = { cmocka_unit_test(test_arrow_bool) };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
