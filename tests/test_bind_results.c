/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include <string.h>
#include "utils/test_setup.h"

void test_bind_results(void **unused) {
    /* init */
    SF_BIND_OUTPUT results[2];
    int64 result1;
    char result2[1000];

    /* Connect with all parameters set */
    SF_CONNECT *sf = setup_snowflake_connection();
    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    SF_STMT *sfstmt = snowflake_stmt(sf);
    /* NOTE: the numeric type here should fit into int64 otherwise
     * it is taken as a float */
    status = snowflake_query(
      sfstmt,
      "select seq4(),randstr(999,random()) from table(generator(rowcount=>10))",
      0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // Initialize bind outputs
    results[0].idx = 1;
    results[0].max_length = sizeof(result1);
    results[0].c_type = SF_C_TYPE_INT64;
    results[0].value = &result1;

    results[1].idx = 2;
    results[1].max_length = sizeof(result2);
    results[1].c_type = SF_C_TYPE_STRING;
    results[1].value = &result2;

    // Bind array of outputs
    status = snowflake_bind_result_array(sfstmt, results, 2);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    int counter = 0;
    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        assert_int_equal(result1, counter);
        assert_int_equal(strlen(result2), 999);
        // printf("column 1: %lld, column 2: %s\n", result1, result2);
        ++counter;
    }
    assert_int_equal(counter, 10);
    if (status != SF_STATUS_EOF) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_EOF);
    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);

}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_bind_results),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
