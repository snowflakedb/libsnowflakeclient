/*
 * Copyright (c) 2018-2020 Snowflake Computing, Inc. All rights reserved.
 */
#include "utils/test_setup.h"
#include <cJSON.h>
#include <string.h>
#include <connection.h>
#include <memory.h>

/**
 *
 * This test issues a queries that generates large results, and then ensures that fetching
 * results from the ResultSet fails gracefully.
 *
 * @param unused
 */
void test_arrow_force(void **unused) {
    SF_CONNECT *sf = setup_snowflake_connection();
    snowflake_connect(sf);
    SF_STATUS status = enable_arrow_force(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    int rows = 100; // total number of rows

    char sql_buf[1024];
    sprintf(
            sql_buf,
            "select seq4(),randstr(1000,random()) from table(generator(rowcount=>%d));",
            rows);

    /* query */
    SF_STMT *sfstmt = snowflake_stmt(sf);
    status = snowflake_query(sfstmt, sql_buf, 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // This should fail with the appropriate error code
    status = snowflake_fetch(sfstmt);
    assert_int_equal(status, SF_STATUS_ERROR_ATTEMPT_TO_RETRIEVE_FORCE_ARROW);

    // Same as trying to get a result value
    int64 c1;
    status = snowflake_column_as_int64(sfstmt, 1, &c1);
    assert_int_equal(status, SF_STATUS_ERROR_ATTEMPT_TO_RETRIEVE_FORCE_ARROW);

    char* c2;
    status = snowflake_column_as_str(sfstmt, 2, &c2, 0, 0);
    assert_int_equal(status, SF_STATUS_ERROR_ATTEMPT_TO_RETRIEVE_FORCE_ARROW);

    // numRows should return -1
    assert_int_equal(snowflake_num_rows(sfstmt), -1);

    // But running another query should clear the status
    status = snowflake_query(sfstmt, "select 1", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // Reset it back to default, and verify that queries still work.
    status = snowflake_query(
            sfstmt, "alter session set c_api_query_result_format = default",
            0);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_query(sfstmt, sql_buf, 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // This should fail with the appropriate error code
    status = snowflake_fetch(sfstmt);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // Same as trying to get a result value
    status = snowflake_column_as_int64(sfstmt, 1, &c1);
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(c1, 0);

    status = snowflake_column_as_str(sfstmt, 2, &c2, 0, 0);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    assert_int_equal(snowflake_num_rows(sfstmt), rows);

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_arrow_force),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
