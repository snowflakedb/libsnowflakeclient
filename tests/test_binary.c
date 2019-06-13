/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include "utils/test_setup.h"

typedef struct test_case_to_string {
    const int64 c1in;
    const char *c2in;
    const size_t c2inlen;
    const char *c2out;
    SF_STATUS error_code;
} TEST_CASE_TO_STRING;


void test_selectbin(void **unused) {
    TEST_CASE_TO_STRING test_cases[] = {
      {.c1in = 1, .c2in = "\xab\xcd\xef\x12\x34", .c2inlen=5, .c2out = "ABCDEF1234"},
      {.c1in = 2, .c2in = "\x56\x78\x9a\xbc\xde\xf0", .c2inlen=6, .c2out = "56789ABCDEF0"},
      {.c1in = 3, .c2in = "", .c2inlen=0, .c2out = ""},
      {.c1in = 4, .c2in = "\x00\x00\x00", .c2inlen=3, .c2out = "000000"},
      {.c1in = 5, .c2in = "\x56\x78\x00\xbc\xde\xf0", .c2inlen=6, .c2out = "567800BCDEF0"},
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
      "create or replace table t (c1 int, c2 binary)",
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
        ic2.c_type = SF_C_TYPE_BINARY;
        ic2.value = (void *) v.c2in;
        ic2.len = v.c2inlen;
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
    status = snowflake_query(sfstmt, "select * from t", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    assert_int_equal(snowflake_num_rows(sfstmt),
                     sizeof(test_cases) / sizeof(TEST_CASE_TO_STRING));

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        int64 c1 = 0;
        const char *c2 = NULL;
        snowflake_column_as_int64(sfstmt, 1, &c1);
        snowflake_column_as_const_str(sfstmt, 2, &c2);
        TEST_CASE_TO_STRING v = test_cases[c1 - 1];
        assert_string_equal(v.c2out, c2);
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

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}


int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_selectbin),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
