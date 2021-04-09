/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include "utils/test_setup.h"


void test_variant_helper(sf_bool use_arrow) {
    SF_CONNECT *sf = setup_snowflake_connection();

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* Create a statement once and reused */
    SF_STMT *sfstmt = snowflake_stmt(sf);

    /* Set query result format to Arrow if necessary */
    status = snowflake_query(
        sfstmt,
        use_arrow == SF_BOOLEAN_TRUE
        ? "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE"
        : "alter session set C_API_QUERY_RESULT_FORMAT=JSON",
        0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

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
    ic1.name = NULL;
    ic1.c_type = SF_C_TYPE_STRING;
    ic1.value = (void *) ic1buf;
    ic1.len = strlen(ic1buf);
    snowflake_bind_param(sfstmt, &ic1);

    SF_BIND_INPUT ic2;
    char ic2buf[1024];
    strcpy(ic2buf, "'[1,2,3]'");
    ic2.idx = 2;
    ic2.name = NULL;
    ic2.c_type = SF_C_TYPE_STRING;
    ic2.value = (void *) ic2buf;
    ic2.len = strlen(ic2buf);
    snowflake_bind_param(sfstmt, &ic2);

    SF_BIND_INPUT ic3;
    char ic3buf[1024];
    strcpy(ic3buf, "'[456,789]'");
    ic3.idx = 3;
    ic3.name = NULL;
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

    const char *c1buf = NULL;
    const char *c2buf = NULL;
    const char *c3buf = NULL;
    assert_int_equal(snowflake_num_rows(sfstmt), 1);

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_const_str(sfstmt, 1, &c1buf);
        snowflake_column_as_const_str(sfstmt, 2, &c2buf);
        snowflake_column_as_const_str(sfstmt, 3, &c3buf);
        assert_string_equal(c1buf, "{\n  \"test1\": 1\n}");
        assert_string_equal(c2buf, "[\n  \"[1,2,3]\"\n]");
        assert_string_equal(c3buf, "\"[456,789]\"");
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

void test_variant_arrow(void **unused) {
    test_variant_helper(SF_BOOLEAN_TRUE);
}

void test_variant_json(void **unused) {
    test_variant_helper(SF_BOOLEAN_FALSE);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_variant_arrow),
      cmocka_unit_test(test_variant_json),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
