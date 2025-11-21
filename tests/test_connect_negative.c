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

void test_connect_login_timeout(void** unused) {
  SF_CONNECT* sf = setup_snowflake_connection();
  // set to use invalid proxy so the connection will fail
  snowflake_set_attribute(sf, SF_CON_PROXY, "172.23.19.112");
  int64 timeout = 10;
  int64 delta = 2;
  snowflake_set_attribute(sf, SF_CON_LOGIN_TIMEOUT, &timeout);
  unsigned long start_time = (unsigned long)time(NULL);
  SF_STATUS ret = snowflake_connect(sf);
  unsigned long end_time = (unsigned long)time(NULL);
  assert_int_not_equal(ret, SF_STATUS_SUCCESS); // must fail
  SF_ERROR_STRUCT* sferr = snowflake_error(sf);
  if (sferr->error_code != SF_STATUS_ERROR_CURL) {
    dump_error(sferr);
  }
  assert_int_equal(sferr->error_code, SF_STATUS_ERROR_RETRY);
  assert_in_range(end_time - start_time, timeout - delta, timeout + delta);
  snowflake_term(sf);
}

void test_chunk_downloading_timeout(void** unused) {
  SF_CONNECT* sf = setup_snowflake_connection();
  // set to use invalid proxy but exclude snowflake host so
  // only chunk downloading requests would fail
  snowflake_set_attribute(sf, SF_CON_PROXY, "172.23.19.112");
  snowflake_set_attribute(sf, SF_CON_NO_PROXY, "snowflakecomputing.com");
  int64 timeout = 20;
  int64 delta = 3;

  SF_STMT* sfstmt = NULL;
  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  int rows = 100000; // total number of rows
  char sql_buf[1024];
  sprintf(
    sql_buf,
    "select seq4(),randstr(1000,random()) from table(generator(rowcount=>%d));",
    rows);

  /* query */
  sfstmt = snowflake_stmt(sf);

  snowflake_set_attribute(sf, SF_CON_NETWORK_TIMEOUT, &timeout);
  // sometime this could get timeout
  for (int i = 0; i < 3; i++)
  {
    status = snowflake_query(sfstmt, sql_buf, 0);
    if (status == SF_STATUS_SUCCESS) {
      break;
    }
  }
  if (status != SF_STATUS_SUCCESS)
  {
    fprintf(stderr, "test_chunk_downloading_timeout: query timeout after retry, skip.\n");
    return;
  }

  unsigned long start_time = (unsigned long)time(NULL);
  // we could have some rows returned in query response so fetch
  // might succeed at first
  while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
  }
  unsigned long end_time = (unsigned long)time(NULL);
  assert_int_not_equal(status, SF_STATUS_SUCCESS);
  assert_in_range(end_time - start_time, timeout - delta, timeout + delta);

  snowflake_stmt_term(sfstmt);
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
      cmocka_unit_test(test_connect_login_timeout),
      cmocka_unit_test(test_chunk_downloading_timeout),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
