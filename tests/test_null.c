/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include <assert.h>
#include "utils/test_setup.h"


typedef struct test_case_to_string {
    const int64 c1in;
    const char *c2in;
    const int64 *c3in;
    const sf_bool *c4in;
    const char *c5in;

    const char *c2out;
    const sf_bool c2_is_null;
    const char *c3out;
    const sf_bool c3_is_null;
    const char *c4out;
    const sf_bool c4_is_null;
    const char *c5out;
    const sf_bool c5_is_null;
} TEST_CASE_TO_STRING;


void test_null(void **unused) {
    TEST_CASE_TO_STRING test_cases[] = {
      {
          .c1in = 1, .c2in = NULL, .c3in = NULL, .c4in = NULL, .c5in = NULL, .c2out = "",
          .c2_is_null = SF_BOOLEAN_TRUE, .c3out="", .c3_is_null=SF_BOOLEAN_TRUE, .c4out= "",
          .c4_is_null = SF_BOOLEAN_TRUE, .c5out= "", .c5_is_null = SF_BOOLEAN_TRUE
      }
    };

    SF_CONNECT *sf = setup_snowflake_connection();

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* Create a statement once and reused */
    SF_STMT *sfstmt = snowflake_stmt(sf);
    status = snowflake_query(
      sfstmt,
      "create or replace table t (c1 int, c2 string, c3 number(18,0), c4 boolean)",
      0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* insert data */
    status = snowflake_prepare(
      sfstmt,
      "insert into t(c1,c2,c3,c4) values(?,?,?,?)",
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

        SF_BIND_INPUT ic2;
        ic2.idx = 2;
        ic2.name = NULL;
        ic2.c_type = SF_C_TYPE_STRING;
        ic2.value = NULL;
        ic2.len = (size_t) 0;
        status = snowflake_bind_param(sfstmt, &ic2);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        SF_BIND_INPUT ic3;
        ic3.idx = 3;
        ic3.name = NULL;
        ic3.c_type = SF_C_TYPE_INT64;
        ic3.value = NULL;
        ic3.len = (size_t) 0;
        status = snowflake_bind_param(sfstmt, &ic3);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        SF_BIND_INPUT ic4;
        ic4.idx = 4;
        ic4.name = NULL;
        ic4.c_type = SF_C_TYPE_BOOLEAN;
        ic4.value = NULL;
        ic4.len = (size_t) 0;
        status = snowflake_bind_param(sfstmt, &ic4);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        SF_BIND_INPUT ic5;
        ic5.idx = 5;
        ic5.name = NULL;
        ic5.c_type = SF_C_TYPE_NULL;
        ic5.value = NULL;
        ic5.len = (size_t) 0;
        status = snowflake_bind_param(sfstmt, &ic5);
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
    status = snowflake_query(sfstmt, "select * from t", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(sfstmt),
                     sizeof(test_cases) / sizeof(TEST_CASE_TO_STRING));

    sf_bool is_null = SF_BOOLEAN_FALSE;
    int64 c1 = 0;
    char *null_val = NULL;
    size_t null_val_len = 0;
    size_t max_null_val_len = 0;
    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_int64(sfstmt, 1, &c1);
        TEST_CASE_TO_STRING v = test_cases[c1 - 1];
        snowflake_column_is_null(sfstmt, 2, &is_null);
        snowflake_column_as_str(sfstmt, 2, &null_val, &null_val_len, &max_null_val_len);
        assert_true(v.c2_is_null == is_null);
        assert_string_equal(v.c2out, null_val);
        snowflake_column_is_null(sfstmt, 3, &is_null);
        snowflake_column_as_str(sfstmt, 3, &null_val, &null_val_len, &max_null_val_len);
        assert_true(v.c3_is_null == is_null);
        assert_string_equal(v.c3out, null_val);
        snowflake_column_is_null(sfstmt, 4, &is_null);
        snowflake_column_as_str(sfstmt, 4, &null_val, &null_val_len, &max_null_val_len);
        assert_true(v.c4_is_null == is_null);
        assert_string_equal(v.c4out, null_val);
        snowflake_column_is_null(sfstmt, 5, &is_null);
        snowflake_column_as_str(sfstmt, 5, &null_val, &null_val_len, &max_null_val_len);
        assert_true(v.c5_is_null == is_null);
        assert_string_equal(v.c5out, null_val);
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

    free(null_val);
    null_val = NULL;
    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_null),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
