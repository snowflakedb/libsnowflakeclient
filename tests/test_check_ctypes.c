/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */
#include "utils/test_setup.h"

void test_check_c_types(void **unused) {
    SF_STATUS status;
    SF_CONNECT *sf = NULL;
    SF_STMT *sfstmt = NULL;
    sf = setup_snowflake_connection();
    status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    sfstmt = snowflake_stmt(sf);
    status = snowflake_query(
      sfstmt,
      "select seq4(), randstr(1000,random()), as_double(10.01) from table(generator(rowcount=>1));",
      0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    SF_COLUMN_DESC *desc = snowflake_desc(sfstmt);
    // Check seq4 type
    if (desc[0].c_type != SF_C_TYPE_INT64) {
        fprintf(
          stderr,
          "Error, seq4 column is not type SF_C_TYPE_INT64. Actual type is %s\n",
          snowflake_c_type_to_string(desc[0].c_type));
    }
    assert_int_equal(desc[0].c_type, SF_C_TYPE_INT64);

    // Check randstr type
    if (desc[1].c_type != SF_C_TYPE_STRING) {
        fprintf(
          stderr,
          "Error, randstr column is not type SF_C_TYPE_STRING. Actual type is %s\n",
          snowflake_c_type_to_string(desc[1].c_type));
    }
    assert_int_equal(desc[1].c_type, SF_C_TYPE_STRING);

    // Check as_double type
    if (desc[2].c_type != SF_C_TYPE_FLOAT64) {
        fprintf(
          stderr,
          "Error, as_double('10.01') column is not type SF_C_TYPE_FLOAT64. Actual type is %s\n",
          snowflake_c_type_to_string(desc[2].c_type));
    }
    assert_int_equal(desc[2].c_type, SF_C_TYPE_FLOAT64);

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf); // purge snowflake context
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_check_c_types),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
