/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */
#include "utils/test_setup.h"


void test_large_result_set_helper(sf_bool use_arrow) {

    int rows = 100000; // total number of rows

    SF_STMT *sfstmt = NULL;
    SF_CONNECT *sf = setup_snowflake_connection();
#ifdef __APPLE__
    sf_bool insecure_mode = SF_BOOLEAN_TRUE;
    snowflake_set_attribute(sf, SF_CON_INSECURE_MODE, &insecure_mode);
#endif
    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    char sql_buf[1024];
    sprintf(
      sql_buf,
      "select seq4(),randstr(1000,random()) from table(generator(rowcount=>%d));",
      rows);

    /* query */
    sfstmt = snowflake_stmt(sf);

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

    status = snowflake_query(sfstmt, sql_buf, 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    uint64 counter = 0;
    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        // printf("output: %lld, %s\n", out, c2buf);
        counter++;
    }
    assert_int_equal(counter, rows);
    assert_int_equal(snowflake_num_rows(sfstmt), rows);
    if (status != SF_STATUS_EOF) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_EOF);

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_large_result_set_arrow(void **unused) {
    test_large_result_set_helper(SF_BOOLEAN_TRUE);
}

void test_large_result_set_json(void **unused) {
    test_large_result_set_helper(SF_BOOLEAN_FALSE);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_large_result_set_arrow),
      cmocka_unit_test(test_large_result_set_json),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
