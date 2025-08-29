/*
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

    /* query with null stmt*/
    SF_STMT *sfstmt = NULL;
    status = snowflake_query(sfstmt, "select 1;", 0);
    assert_int_equal(status, SF_STATUS_ERROR_STATEMENT_NOT_EXIST);

    /* query */
    sfstmt = snowflake_stmt(sf);
    status = snowflake_query(sfstmt, "select 1;", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // returns valid query in success case
    sf_strncpy(qid, SF_UUID4_LEN, snowflake_sfqid(sfstmt), SF_UUID4_LEN);
    assert_int_equal(strlen(qid), SF_UUID4_LEN - 1);

    sfstmt = snowflake_stmt(sf);
    status = snowflake_query(sfstmt, "select * from table_not_exists;", 0);
    assert_int_equal(status, SF_STATUS_ERROR_GENERAL);

    // ensure the failed query overwrites query id
    assert_string_not_equal(qid, snowflake_sfqid(sfstmt));

    // returns valid query in fail case
    sf_strncpy(qid, SF_UUID4_LEN, snowflake_sfqid(sfstmt), SF_UUID4_LEN);
    assert_int_equal(strlen(qid), SF_UUID4_LEN - 1);

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_failed_request() {
  SF_CONNECT *sf = snowflake_init();

  // Set up a valid session token so we don't hit the earlier error path
  sf->token = "dummy_token";
  sf->master_token = "dummy_master_token";

  // Set up a host that will fail to connect (invalid host)
  sf->host = "invalid.snowflakecomputing.com";
  sf->user = "dummy_user";
  sf->account = "dummy_account";

  SF_STMT *sfstmt = snowflake_stmt(sf);

  snowflake_prepare(sfstmt, "select 1;", 0);

  // Call the function directly; request will fail
  SF_STATUS status = snowflake_execute(sfstmt);

  assert_int_not_equal(status, SF_STATUS_SUCCESS);

  snowflake_stmt_term(sfstmt);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_sfqid),
      cmocka_unit_test(test_failed_request),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
