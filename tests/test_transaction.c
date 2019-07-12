/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include "utils/test_setup.h"

void test_transaction(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = setup_snowflake_connection_with_autocommit(
      "UTC",
      SF_BOOLEAN_FALSE);
    status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    SF_STMT *sfstmt = NULL;

    /* execute a DML */
    sfstmt = snowflake_stmt(sf);
    status = snowflake_query(
      sfstmt,
      "create or replace table t(c1 number(10,0), c2 string)",
      0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* begin transaction */
    status = snowflake_trans_begin(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_prepare(sfstmt, "INSERT INTO t(c1,c2) values(?,?)", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    SF_BIND_INPUT p1, p2;

    /* insert one row */
    int64 v = 3;
    p1.idx = 1;
    p1.name = NULL;
    p1.c_type = SF_C_TYPE_INT64;
    p1.value = (void *) &v;
    status = snowflake_bind_param(sfstmt, &p1);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    p2.idx = 2;
    p2.name = NULL;
    p2.c_type = SF_C_TYPE_STRING;
    p2.value = (void *) "test2";
    p2.len = strlen(p2.value);
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

    status = snowflake_trans_commit(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }

    /* insert additional row */
    v = 5;
    p1.idx = 1;
    p1.c_type = SF_C_TYPE_INT64;
    p1.value = (void *) &v;
    status = snowflake_bind_param(sfstmt, &p1);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }

    p2.idx = 2;
    p2.c_type = SF_C_TYPE_STRING;
    p2.value = (void *) "test4";
    p2.len = strlen(p2.value);
    status = snowflake_bind_param(sfstmt, &p2);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }

    status = snowflake_execute(sfstmt);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_affected_rows(sfstmt), 1);

    /* fetch result */
    status = snowflake_query(sfstmt, "select c1, c2 from t order by 1 desc", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    
    status = snowflake_fetch(sfstmt);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(v, 5);

    status = snowflake_trans_rollback(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* fetch result (2nd) */
    status = snowflake_query(sfstmt, "select c1, c2 from t order by 1 desc", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_fetch(sfstmt);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    snowflake_column_as_int64(sfstmt, 1, &v);
    assert_int_equal(v, 3);

    status = snowflake_query(
      sfstmt,
      "drop table if exists t",
      0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_trans_rollback(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf); // purge snowflake context
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_transaction),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
