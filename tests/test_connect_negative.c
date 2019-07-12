/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include "utils/test_setup.h"

void test_connect_account_missing(void **unused) {

    // account parameter is missing
    SF_CONNECT *sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_USER, "abc");
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "abc");

    SF_STATUS ret = snowflake_connect(sf);
    assert_int_not_equal(ret, SF_STATUS_SUCCESS); // must fail
    SF_ERROR_STRUCT *sferr = snowflake_error(sf);
    if (sferr->error_code != SF_STATUS_ERROR_BAD_CONNECTION_PARAMS) {
        dump_error(sferr);
    }
    assert_int_equal(sferr->error_code, SF_STATUS_ERROR_BAD_CONNECTION_PARAMS);
    snowflake_term(sf);
}

void test_connect_user_missing(void **unused) {
    SF_CONNECT *sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "abc");
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "abc");

    SF_STATUS ret = snowflake_connect(sf);
    assert_int_not_equal(ret, SF_STATUS_SUCCESS); // must fail
    SF_ERROR_STRUCT *sferr = snowflake_error(sf);

    if (sferr->error_code != SF_STATUS_ERROR_BAD_CONNECTION_PARAMS) {
        dump_error(sferr);
    }
    assert_int_equal(sferr->error_code, SF_STATUS_ERROR_BAD_CONNECTION_PARAMS);
    snowflake_term(sf);

}

void test_connect_password_missing(void **unused) {
    SF_CONNECT *sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "abc");
    snowflake_set_attribute(sf, SF_CON_USER, "abc");

    SF_STATUS ret = snowflake_connect(sf);
    assert_int_not_equal(ret, SF_STATUS_SUCCESS); // must fail
    SF_ERROR_STRUCT *sferr = snowflake_error(sf);

    if (sferr->error_code != SF_STATUS_ERROR_BAD_CONNECTION_PARAMS) {
        dump_error(sferr);
    }
    assert_int_equal(sferr->error_code, SF_STATUS_ERROR_BAD_CONNECTION_PARAMS);
    snowflake_term(sf);

}

void test_connect_invalid_database(void **unused) {
    SF_CONNECT *sf = setup_snowflake_connection();
    snowflake_set_attribute(sf, SF_CON_DATABASE, "NEVER_EXISTS");

    SF_STATUS ret = snowflake_connect(sf);
    assert_int_not_equal(ret, SF_STATUS_SUCCESS); // must fail
    SF_ERROR_STRUCT *sferr = snowflake_error(sf);
    if (sferr->error_code != SF_STATUS_ERROR_APPLICATION_ERROR) {
        dump_error(sferr);
    }
    assert_int_equal(sferr->error_code, SF_STATUS_ERROR_APPLICATION_ERROR);
    snowflake_term(sf);

}

void test_connect_invalid_schema(void **unused) {
    SF_CONNECT *sf = setup_snowflake_connection();
    snowflake_set_attribute(sf, SF_CON_SCHEMA, "NEVER_EXISTS");

    SF_STATUS ret = snowflake_connect(sf);
    assert_int_not_equal(ret, SF_STATUS_SUCCESS); // must fail
    SF_ERROR_STRUCT *sferr = snowflake_error(sf);
    if (sferr->error_code != SF_STATUS_ERROR_APPLICATION_ERROR) {
        dump_error(sferr);
    }
    assert_int_equal(sferr->error_code, SF_STATUS_ERROR_APPLICATION_ERROR);
    snowflake_term(sf);

}

void test_connect_invalid_warehouse(void **unused) {
    SF_CONNECT *sf = setup_snowflake_connection();
    snowflake_set_attribute(sf, SF_CON_WAREHOUSE, "NEVER_EXISTS");

    SF_STATUS ret = snowflake_connect(sf);
    assert_int_not_equal(ret, SF_STATUS_SUCCESS); // must fail
    SF_ERROR_STRUCT *sferr = snowflake_error(sf);
    if (sferr->error_code != SF_STATUS_ERROR_APPLICATION_ERROR) {
        dump_error(sferr);
    }
    assert_int_equal(sferr->error_code, SF_STATUS_ERROR_APPLICATION_ERROR);
    snowflake_term(sf);

}

void test_connect_invalid_role(void **unused) {
    SF_CONNECT *sf = setup_snowflake_connection();
    snowflake_set_attribute(sf, SF_CON_ROLE, "NEVER_EXISTS");

    SF_STATUS ret = snowflake_connect(sf);
    assert_int_not_equal(ret, SF_STATUS_SUCCESS); // must fail
    SF_ERROR_STRUCT *sferr = snowflake_error(sf);
    if (sferr->error_code != (SF_STATUS)390189) {
        dump_error(sferr);
    }
    assert_int_equal(sferr->error_code, (SF_STATUS)390189);
    snowflake_term(sf);

}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_connect_account_missing),
      cmocka_unit_test(test_connect_user_missing),
      cmocka_unit_test(test_connect_password_missing),
      //cmocka_unit_test(test_connect_invalid_database),
      //cmocka_unit_test(test_connect_invalid_schema),
      //cmocka_unit_test(test_connect_invalid_warehouse),
      cmocka_unit_test(test_connect_invalid_role),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}