/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include "utils/test_setup.h"

void _fetch_data(SF_STMT *sfstmt, int64 expected_sum) {
    SF_STATUS status = snowflake_query(sfstmt, "select * from t", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    int64 c1 = 0;
    int64 num_fields = snowflake_num_fields(sfstmt);
    assert_int_equal(num_fields, 2);

    SF_COLUMN_DESC *descs = snowflake_desc(sfstmt);
    int i;
    for (i = 0; i < num_fields; ++i) {
        switch (i) {
            case 0:
                assert_string_equal(descs[i].name, "C1");
                assert_int_equal(descs[i].idx, 1);
                assert_int_equal(descs[i].type, SF_DB_TYPE_FIXED);
                assert_int_equal(descs[i].c_type, SF_C_TYPE_INT64);
                assert_int_equal(descs[i].byte_size, 0);
                assert_int_equal(descs[i].internal_size, 0);
                assert_int_equal(descs[i].precision, 10);
                assert_int_equal(descs[i].scale, 0);
                assert_int_equal(descs[i].null_ok, 0);
                break;
            case 1:
                assert_string_equal(descs[i].name, "C2");
                assert_int_equal(descs[i].idx, 2);
                assert_int_equal(descs[i].type, SF_DB_TYPE_TEXT);
                assert_int_equal(descs[i].c_type, SF_C_TYPE_STRING);
                assert_int_equal(descs[i].byte_size, 16777216);
                assert_int_equal(descs[i].internal_size, 16777216);
                assert_int_equal(descs[i].precision, 0);
                assert_int_equal(descs[i].scale, 0);
                assert_int_equal(descs[i].null_ok, 1);
                break;
            default:
                assert_in_range(i, 0, 1);
        }
    }
    int64 total = 0;
    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        // printf("c1: %lld, c2: %s\n", c1v, c2v);
        snowflake_column_as_int64(sfstmt, 1, &c1);
        total += c1;
    }
    assert_int_equal(total, expected_sum);
    if (status != SF_STATUS_EOF) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_EOF);
}

void test_crud(void **unused) {
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
      "create or replace table t (c1 number(10,0) not null, c2 string)",
      0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_query(
      sfstmt,
      "insert into t values(1, 'test1'),(2, 'test2')",
      0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_affected_rows(sfstmt), 2);

    _fetch_data(sfstmt, 3);

    status = snowflake_prepare(sfstmt, "update t set c1=? where c2=?", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    int64 p1v = 102;
    SF_BIND_INPUT p1;
    p1.idx = 1;
    p1.name = NULL;
    p1.c_type = SF_C_TYPE_INT64;
    p1.value = &p1v;
    p1.len = sizeof(p1v);
    status = snowflake_bind_param(sfstmt, &p1);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    char p2v[1000];
    strcpy(p2v, "test2");
    SF_BIND_INPUT p2;
    p2.idx = 2;
    p2.name = NULL;
    p2.c_type = SF_C_TYPE_STRING;
    p2.value = &p2v;
    p2.len = sizeof(p2v);

    status = snowflake_bind_param(sfstmt, &p2);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_execute(sfstmt);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_affected_rows(sfstmt), 1);

    /* update the second row */
    p1v = 101;
    p1.idx = 1;
    p1.c_type = SF_C_TYPE_INT64;
    p1.value = &p1v;
    p1.len = sizeof(p1v);
    status = snowflake_bind_param(sfstmt, &p1);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    strcpy(p2v, "test1");
    p2.idx = 2;
    p2.c_type = SF_C_TYPE_STRING;
    p2.value = &p2v;
    p2.len = sizeof(p2v);

    status = snowflake_bind_param(sfstmt, &p2);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_execute(sfstmt);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_affected_rows(sfstmt), 1);

    _fetch_data(sfstmt, 203);

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
      cmocka_unit_test(test_crud),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
