/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include "utils/test_setup.h"


typedef struct test_case_to_string {
    const int64 c1in;
    const sf_bool *c2in;
    const char *c2out;
    const sf_bool c2_is_null;
} TEST_CASE_TO_STRING;

void test_bool(void **unused) {
    const sf_bool large_value = (sf_bool) 64;
    const sf_bool zero_value = (sf_bool) 0;
    const sf_bool negative_value = (sf_bool) -12;

    TEST_CASE_TO_STRING test_cases[] = {
      {.c1in = 1, .c2in = &SF_BOOLEAN_TRUE, .c2out = "1", .c2_is_null=SF_BOOLEAN_FALSE},
      {.c1in = 2, .c2in = &SF_BOOLEAN_FALSE, .c2out = "", .c2_is_null=SF_BOOLEAN_FALSE},
      {.c1in = 3, .c2in = &large_value, .c2out = "1", .c2_is_null=SF_BOOLEAN_FALSE},
      {.c1in = 4, .c2in = &zero_value, .c2out = "", .c2_is_null=SF_BOOLEAN_FALSE},
      {.c1in = 5, .c2in = NULL, .c2out = NULL, .c2_is_null=SF_BOOLEAN_TRUE},
      {.c1in = 6, .c2in = &negative_value, .c2out = "1", .c2_is_null=SF_BOOLEAN_FALSE}
    };

    SF_CONNECT *sf = setup_snowflake_connection();

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* Create a statement once and reused */
    SF_STMT *sfstmt = snowflake_stmt(sf);
    /* NOTE: the numeric type here should fit into int64 otherwise
     * it is taken as a float */
    status = snowflake_query(
      sfstmt,
      "create or replace table t (c1 int, c2 boolean)",
      0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* insert data */
    status = snowflake_prepare(
      sfstmt,
      "insert into t(c1,c2) values(?,?)",
      0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    size_t i;
    size_t len;
    for (i = 0, len = sizeof(test_cases) / sizeof(TEST_CASE_TO_STRING);
         i < len; i++) {
        TEST_CASE_TO_STRING v = test_cases[i];

        SF_BIND_INPUT ic1;
        int64 ic1buf = v.c1in;
        ic1.idx = 1;
        ic1.name = NULL;
        ic1.c_type = SF_C_TYPE_INT64;
        ic1.value = (void *) &ic1buf;
        ic1.len = sizeof(ic1buf);
        status = snowflake_bind_param(sfstmt, &ic1);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        SF_BIND_INPUT ic2;
        ic2.idx = 2;
        ic2.name = NULL;
        ic2.c_type = SF_C_TYPE_BOOLEAN;
        ic2.value = (void *) v.c2in;
        ic2.len = sizeof(sf_bool);
        status = snowflake_bind_param(sfstmt, &ic2);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        status = snowflake_execute(sfstmt);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);
    }

    /* query */
    status = snowflake_query(sfstmt, "select * from t order by 1", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    assert_int_equal(snowflake_num_rows(sfstmt),
                     sizeof(test_cases) / sizeof(TEST_CASE_TO_STRING));

    int64 c1 = 0;
    char *c2 = NULL;
    size_t c2_len = 0;
    size_t c2_max_size = 0;
    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_int64(sfstmt, 1, &c1);
        TEST_CASE_TO_STRING v = test_cases[c1 - 1];
        if (v.c2in != NULL) {
            snowflake_column_as_str(sfstmt, 2, &c2, &c2_len, &c2_max_size);
            assert_string_equal(v.c2out, c2);
        } else {
            sf_bool is_null;
            snowflake_column_is_null(sfstmt, 2, &is_null);
            assert_true(v.c2_is_null == is_null);
        }
    }
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
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_bool),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
