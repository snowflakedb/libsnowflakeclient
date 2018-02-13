/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include "utils/test_setup.h"


void test_variant(void **unused) {
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
      "create or replace table t (c1 object, c2 array, c3 variant)",
      0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);


    /* insert data */
    status = snowflake_prepare(
      sfstmt,
      "insert into t select parse_json(?),parse_json(?),parse_json(?)",
      0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);


    SF_BIND_INPUT ic1;
    char ic1buf[1024];
    strcpy(ic1buf, "{\"test1\":1}");
    ic1.idx = 1;
    ic1.c_type = SF_C_TYPE_STRING;
    ic1.value = (void *) ic1buf;
    ic1.len = strlen(ic1buf);
    snowflake_bind_param(sfstmt, &ic1);

    SF_BIND_INPUT ic2;
    char ic2buf[1024];
    strcpy(ic2buf, "'[1,2,3]'");
    ic2.idx = 2;
    ic2.c_type = SF_C_TYPE_STRING;
    ic2.value = (void *) ic2buf;
    ic2.len = strlen(ic2buf);
    snowflake_bind_param(sfstmt, &ic2);

    SF_BIND_INPUT ic3;
    char ic3buf[1024];
    strcpy(ic3buf, "'[456,789]'");
    ic3.idx = 3;
    ic3.c_type = SF_C_TYPE_STRING;
    ic3.value = (void *) ic3buf;
    ic3.len = strlen(ic3buf);
    snowflake_bind_param(sfstmt, &ic3);

    status = snowflake_execute(sfstmt);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* query */
    status = snowflake_query(sfstmt, "select * from t", 0);
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
    snowflake_bind_result(sfstmt, &c1);

    SF_BIND_OUTPUT c2 = {0};
    char c2buf[1024];
    c2.idx = 2;
    c2.c_type = SF_C_TYPE_STRING;
    c2.value = (void *) c2buf;
    c2.len = sizeof(c2buf);
    c2.max_length = sizeof(c2buf);
    snowflake_bind_result(sfstmt, &c2);

    SF_BIND_OUTPUT c3 = {0};
    char c3buf[1024];
    c3.idx = 3;
    c3.c_type = SF_C_TYPE_STRING;
    c3.value = (void *) c3buf;
    c3.max_length = sizeof(c3buf);
    snowflake_bind_result(sfstmt, &c3);

    assert_int_equal(snowflake_num_rows(sfstmt), 1);

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        assert_string_equal(c1.value, "{\n  \"test1\": 1\n}");
        assert_string_equal(c2.value, "[\n  \"[1,2,3]\"\n]");
        assert_string_equal(c3.value, "\"[456,789]\"");
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
      cmocka_unit_test(test_variant),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
