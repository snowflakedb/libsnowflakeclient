#include <string.h>
#include "../utils/test_setup.h"
#include "../utils/mock_setup.h"

void test_session_gone_during_query(void **unused) {
    setup_mock_login_standard();

    SF_CONNECT *sf = setup_snowflake_connection();
    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* query */
    setup_mock_query_session_gone();
    SF_STMT *sfstmt = snowflake_stmt(sf);
    status = snowflake_query(sfstmt, "select 1;", 0);
    if (status != SF_STATUS_ERROR_CONNECTION_NOT_EXIST) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_ERROR_CONNECTION_NOT_EXIST);
    snowflake_stmt_term(sfstmt);

    setup_mock_delete_connection_session_gone();
    status = snowflake_term(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
}

void test_session_gone_during_delete(void **unused) {
    setup_mock_login_standard();

    SF_CONNECT *sf = setup_snowflake_connection();
    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* query */
    setup_mock_query_standard();
    SF_STMT *sfstmt = snowflake_stmt(sf);
    status = snowflake_query(sfstmt, "select 1;", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    int64 out = 0;
    assert_int_equal(snowflake_num_rows(sfstmt), 1);

    int counter = 0;
    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_int64(sfstmt, 1, &out);
        assert_int_equal(out, 1);
        ++counter;
    }
    if (status != SF_STATUS_EOF) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_EOF);
    snowflake_stmt_term(sfstmt);

    setup_mock_delete_connection_session_gone();
    status = snowflake_term(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
}

int test_setup(void **unused) {
    putenv("SNOWFLAKE_TEST_HOST=standard.snowflakecomputing.com");
    putenv("SNOWFLAKE_TEST_USER=standarduser");
    putenv("SNOWFLAKE_TEST_ACCOUNT=standard");
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_session_gone_during_query),
      cmocka_unit_test(test_session_gone_during_delete),
    };
    int ret = cmocka_run_group_tests(tests, test_setup, NULL);
    snowflake_global_term();
    return ret;
}


