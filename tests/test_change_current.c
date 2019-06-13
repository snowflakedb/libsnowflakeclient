/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include "utils/test_setup.h"

void test_change_current_schema(void **unused) {
    /* init */
    SF_STATUS status;
    SF_CONNECT *sf = setup_snowflake_connection();
    status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* create a schema */
    SF_STMT *sfstmt = snowflake_stmt(sf);
    status = snowflake_query(sfstmt, "create or replace schema phpschema", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_string_equal(sf->schema, "PHPSCHEMA");

    status = snowflake_query(
      sfstmt, "drop schema if exists phpschema", 0);
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
      cmocka_unit_test(test_change_current_schema),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
