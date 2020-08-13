/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */
#include <assert.h>
#include "utils/test_setup.h"

void test_stats(void **unused) {
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
        "create or replace table t (c1 number, c2 number)",
        0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert(sfstmt->stats == NULL);

    status = snowflake_query(
        sfstmt,
        "insert into t values (1, 2), (3, 4)",
        0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    assert(sfstmt->is_dml);
    assert(sfstmt->stats);
    assert_int_equal(sfstmt->stats->num_rows_inserted, 2);
    assert_int_equal(sfstmt->stats->num_rows_updated, 0);
    assert_int_equal(sfstmt->stats->num_rows_deleted, 0);
    assert_int_equal(sfstmt->stats->num_duplicate_rows_updated, 0);

    status = snowflake_query(
        sfstmt,
        "update t set c2 = src.c2\n"
        "  from (select $1 as c1, $2 as c2 from values (1, 10), (1, 5), (2, 6)) src\n"
        "  where t.c1 = src.c1",
        0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    assert(sfstmt->is_dml);
    assert(sfstmt->stats);
    assert_int_equal(sfstmt->stats->num_rows_inserted, 0);
    assert_int_equal(sfstmt->stats->num_rows_updated, 1);
    assert_int_equal(sfstmt->stats->num_rows_deleted, 0);
    assert_int_equal(sfstmt->stats->num_duplicate_rows_updated, 1);

    status = snowflake_query(
        sfstmt,
        "delete from t",
        0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    assert(sfstmt->is_dml);
    assert(sfstmt->stats);
    assert_int_equal(sfstmt->stats->num_rows_inserted, 0);
    assert_int_equal(sfstmt->stats->num_rows_updated, 0);
    assert_int_equal(sfstmt->stats->num_rows_deleted, 2);
    assert_int_equal(sfstmt->stats->num_duplicate_rows_updated, 0);

    status = snowflake_query(sfstmt, "drop table if exists t", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert(sfstmt->stats == NULL);

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_stats),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
