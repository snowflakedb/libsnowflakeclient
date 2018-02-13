/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include "utils/test_setup.h"


typedef struct test_case_to_string {
    const int64 c1in;
    const float64 c2in;
    const int64 c3in;
    const float64 c4in;

    const char *c2out;
    const char *c3out;
    const char *c4out;
} TEST_CASE_TO_STRING;

void test_number(void **unused) {
    TEST_CASE_TO_STRING test_cases[] = {
      {.c1in = 1, .c2in = 123.456, .c3in = 98765, .c4in = 234.5678, .c2out="123.456000", .c3out="98765", .c4out="234.5678"},
      {.c1in = 2, .c2in = 12345678.987, .c3in = -12345678901234567, .c4in = -0.000123, .c2out="12345678.987000", .c3out="-12345678901234567", .c4out="-0.000123"}
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
    status = snowflake_query(
      sfstmt,
      "create or replace table t (c1 int, c2 number(38,6), c3 number(18,0), c4 float)",
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

        SF_BIND_INPUT ic1 = {0};
        ic1.idx = 1;
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
        ic2.c_type = SF_C_TYPE_FLOAT64;
        ic2.value = (void *) &v.c2in;
        ic2.len = sizeof(v.c2in);
        status = snowflake_bind_param(sfstmt, &ic2);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        SF_BIND_INPUT ic3 = {0};
        ic3.idx = 3;
        ic3.c_type = SF_C_TYPE_INT64;
        ic3.value = (void *) &v.c3in;
        ic3.len = sizeof(v.c3in);
        status = snowflake_bind_param(sfstmt, &ic3);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        SF_BIND_INPUT ic4 = {0};
        ic4.idx = 4;
        ic4.c_type = SF_C_TYPE_FLOAT64;
        ic4.value = (void *) &v.c4in;
        ic4.len = sizeof(v.c4in);
        status = snowflake_bind_param(sfstmt, &ic4);
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

    SF_BIND_OUTPUT c1 = {0};
    char c1buf[1024];
    c1.idx = 1;
    c1.c_type = SF_C_TYPE_STRING;
    c1.value = (void *) c1buf;
    c1.len = sizeof(c1buf);
    c1.max_length = sizeof(c1buf);
    status = snowflake_bind_result(sfstmt, &c1);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    SF_BIND_OUTPUT c2 = {0};
    char c2buf[1024];
    c2.idx = 2;
    c2.c_type = SF_C_TYPE_STRING;
    c2.value = (void *) c2buf;
    c2.len = sizeof(c2buf);
    c2.max_length = sizeof(c2buf);
    status = snowflake_bind_result(sfstmt, &c2);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    SF_BIND_OUTPUT c3 = {0};
    char c3buf[1024];
    c3.idx = 3;
    c3.c_type = SF_C_TYPE_STRING;
    c3.value = (void *) c3buf;
    c3.max_length = sizeof(c3buf);
    status = snowflake_bind_result(sfstmt, &c3);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    SF_BIND_OUTPUT c4 = {0};
    char c4buf[1024];
    c4.idx = 4;
    c4.c_type = SF_C_TYPE_STRING;
    c4.value = (void *) c4buf;
    c4.max_length = sizeof(c4buf);
    status = snowflake_bind_result(sfstmt, &c4);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(sfstmt),
                     sizeof(test_cases) / sizeof(TEST_CASE_TO_STRING));

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        TEST_CASE_TO_STRING v = test_cases[atoll(c1.value) - 1];
        assert_string_equal(v.c2out, c2.value);
        assert_string_equal(v.c3out, c3.value);
        assert_string_equal(v.c4out, c4.value);
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
      cmocka_unit_test(test_number),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
