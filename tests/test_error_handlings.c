/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */
#include "utils/test_setup.h"

void test_syntax_error(void **unused) {
    SF_CONNECT *sf = setup_snowflake_connection();
    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    SF_STMT *sfstmt = snowflake_stmt(sf);
    status = snowflake_query(sfstmt, "select 1 frooom dual", 0);
    assert_int_not_equal(status, SF_STATUS_SUCCESS); // must fail
    SF_ERROR_STRUCT *error = snowflake_stmt_error(sfstmt);
    if (error->error_code != 1003) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(error->error_code, 1003);

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_incorrect_password(void **unused) {
    SF_CONNECT *sf = setup_snowflake_connection();
    snowflake_set_attribute(sf, SF_CON_USER, "HIHIHI");
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "HAHAHA");
    SF_STATUS status = snowflake_connect(sf);
    assert_int_not_equal(status, SF_STATUS_SUCCESS); // must fail

    SF_ERROR_STRUCT *error = snowflake_error(sf);
    if (error->error_code != (SF_STATUS)390100) {
        dump_error(&(sf->error));
    }
    assert_int_equal(error->error_code, (SF_STATUS)390100);
    snowflake_term(sf); // purge snowflake context
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_syntax_error),
      cmocka_unit_test(test_incorrect_password),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
