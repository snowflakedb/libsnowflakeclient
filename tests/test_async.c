/*
 * Copyright (c) 2024 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include "memory.h"
#include "utils/test_setup.h"

 /**
  * Test normal query flow with async
  */
void test_select(void **unused) {
    SF_CONNECT* sf = setup_snowflake_connection();
    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* query */
    SF_STMT* sfstmt = snowflake_stmt(sf);
    status = snowflake_prepare(sfstmt, "select 1;", 0);
    assert_int_equal(status, SF_STATUS_SUCCESS);
    status = snowflake_async_execute(sfstmt);
    if (status != SF_STATUS_SUCCESS) {
      dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* get results */
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
    snowflake_term(sf);
}

/**
 * Test async with new connection
 */
void test_new_connection(void** unused) {
  SF_CONNECT* sf = setup_snowflake_connection();
  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  /* query */
  SF_STMT* sfstmt = snowflake_stmt(sf);
  status = snowflake_prepare(sfstmt, "select 1;", 0);
  assert_int_equal(status, SF_STATUS_SUCCESS);
  status = snowflake_async_execute(sfstmt);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  char* sfqid = (char*)SF_CALLOC(1, SF_UUID4_LEN);
  sf_strcpy(sfqid, SF_UUID4_LEN, sfstmt->sfqid);

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);

  /* new connection */
  sf = setup_snowflake_connection();
  status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  SF_STMT* async_sfstmt = snowflake_async_stmt(sf, sfqid);

  /* get results */
  int64 out = 0;
  assert_int_equal(snowflake_num_rows(async_sfstmt), 1);

  int counter = 0;
  while ((status = snowflake_fetch(async_sfstmt)) == SF_STATUS_SUCCESS) {
    snowflake_column_as_int64(async_sfstmt, 1, &out);
    assert_int_equal(out, 1);
    ++counter;
  }
  if (status != SF_STATUS_EOF) {
    dump_error(&(async_sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_EOF);
}

/**
 * Test async query with fake table
 */
void test_fake_table(void** unused) {
  SF_CONNECT* sf = setup_snowflake_connection();
  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  /* query */
  SF_STMT* sfstmt = snowflake_stmt(sf);
  status = snowflake_prepare(sfstmt, "select * from fake_table;", 0);
  assert_int_equal(status, SF_STATUS_SUCCESS);
  status = snowflake_async_execute(sfstmt);
  assert_int_equal(status, SF_STATUS_ERROR_GENERAL);
}

/**
 * Test async query with fake table
 */
void test_invalid_query_id(void** unused) {
  SF_CONNECT* sf = setup_snowflake_connection();
  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  char* fake_sfqid = "fake-query-id";
  SF_STMT* async_sfstmt = snowflake_async_stmt(sf, fake_sfqid);

  assert_non_null(async_sfstmt);
  assert_non_null(async_sfstmt->connection);
  assert_string_equal(async_sfstmt->sfqid, fake_sfqid);
  assert_null(async_sfstmt->result_set);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE); 
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_select),
      cmocka_unit_test(test_new_connection),
      cmocka_unit_test(test_fake_table),
      cmocka_unit_test(test_invalid_query_id),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
