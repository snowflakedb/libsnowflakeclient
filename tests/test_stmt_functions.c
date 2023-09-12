/*
 * Copyright (c) 2023 Snowflake Computing, Inc. All rights reserved.
 * Test case for snowflake_sfqid() for now but we could add more test cases for
 * other functions on SF_STMT if anything missing.
 */
#include <string.h>
#include "utils/test_setup.h"

void test_sfqid(void **unused) {
    char qid[SF_UUID4_LEN] = { '\0' };
    SF_CONNECT *sf = setup_snowflake_connection();
    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* query */
    SF_STMT *sfstmt = snowflake_stmt(sf);
    status = snowflake_query(sfstmt, "select 1;", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // returns valid query in success case
    sb_strncpy(qid, SF_UUID4_LEN, snowflake_sfqid(sfstmt), SF_UUID4_LEN);
    assert_int_equal(strlen(qid), SF_UUID4_LEN - 1);

    sfstmt = snowflake_stmt(sf);
    status = snowflake_query(sfstmt, "select * from table_not_exists;", 0);
    assert_int_equal(status, SF_STATUS_ERROR_GENERAL);

    // ensure the failed query overwrites query id
    assert_string_not_equal(qid, snowflake_sfqid(sfstmt));

    // returns valid query in fail case
    sb_strncpy(qid, SF_UUID4_LEN, snowflake_sfqid(sfstmt), SF_UUID4_LEN);
    assert_int_equal(strlen(qid), SF_UUID4_LEN - 1);

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_sfqid),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
