/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include "utils/test_setup.h"


typedef struct test_case_to_string {
    const int64 c1in;
    const char *c2in;
    const char *c2out;
    SF_STATUS error_code;
} TEST_CASE_TO_STRING;

void test_time(void **unused) {
    TEST_CASE_TO_STRING test_cases[] = {
      {.c1in = 1, .c2in = "13:56:46.123", .c2out = "13:56:46.123000000"},
      {.c1in = 2, .c2in = "05:08:12.0001234", .c2out = "05:08:12.000123400"},
      {.c1in = 3, .c2in = "GARBAGE", .c2out = "", .error_code=100108},
      {.c1in = 4, .c2in = "98:98:12", .c2out = "", .error_code=100108},
      {.c1in = 5, .c2in = "08:34:23.9876543211", .c2out = "08:34:23.987654321"},
      {.c1in = 6, .c2in = "08:34:23.987654321", .c2out = "08:34:23.987654321"},
    };

    SF_STATUS status;
    SF_CONNECT *sf = setup_snowflake_connection();

    status = snowflake_connect(sf);
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
      "create or replace table t (c1 int, c2 time)",
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
    int no_error_test_cases = 0;
    for (i = 0, len = sizeof(test_cases) / sizeof(TEST_CASE_TO_STRING);
         i < len; i++) {
        TEST_CASE_TO_STRING v = test_cases[i];

        SF_BIND_INPUT ic1 = {0};
        ic1.idx = 1;
        ic1.name = NULL;
        ic1.c_type = SF_C_TYPE_INT64;
        ic1.value = (void *) &v.c1in;
        ic1.len = sizeof(v.c1in);
        status = snowflake_bind_param(sfstmt, &ic1);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        SF_BIND_INPUT ic2 = {0};
        ic2.idx = 2;
        ic2.name = NULL;
        ic2.c_type = SF_C_TYPE_STRING;
        ic2.value = (void *) v.c2in;
        ic2.len = strlen(v.c2in);
        status = snowflake_bind_param(sfstmt, &ic2);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        status = snowflake_execute(sfstmt);
        if (v.error_code != SF_STATUS_SUCCESS) {
            // expecting failure
            SF_ERROR_STRUCT *error = snowflake_stmt_error(sfstmt);
            assert_int_equal(v.error_code, error->error_code);
        } else {
            // expecting success
            assert_int_equal(status, SF_STATUS_SUCCESS);
            ++no_error_test_cases;
        }
    }

    /* query */
    status = snowflake_query(sfstmt, "select * from t order by 1", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    int64 c1_out;
    char *c2_out = NULL;
    size_t c2_out_len = 0;
    size_t c2_out_max_size = 0;
    assert_int_equal(snowflake_num_rows(sfstmt), no_error_test_cases);

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_int64(sfstmt, 1, &c1_out);
        snowflake_column_as_str(sfstmt, 2, &c2_out, &c2_out_len, &c2_out_max_size);
        TEST_CASE_TO_STRING v = test_cases[c1_out - 1];
        assert_int_equal(status, SF_STATUS_SUCCESS);
        assert_string_equal(v.c2out, c2_out);
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

    free(c2_out);
    c2_out = NULL;
    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_time),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
