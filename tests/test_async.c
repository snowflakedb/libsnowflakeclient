/*
 * Copyright (c) 2025 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include "memory.h"
#include "utils/test_setup.h"
#include "util.h"

void test_normal_async_select_query() {
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
    assert_int_equal(snowflake_num_fields(sfstmt), 1);
    assert_int_equal(snowflake_num_rows(sfstmt), 1);

    int count = 0;
    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
      count++;
      snowflake_column_as_int64(sfstmt, 1, &out);
      assert_int_equal(out, 1);
    }
    assert_int_equal(count, 1);

    if (status != SF_STATUS_EOF) {
      dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_EOF);
    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_long_query_get_query_status() {
  SF_CONNECT* sf = setup_snowflake_connection();
  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  /* query */
  SF_STMT* sfstmt = snowflake_stmt(sf);
  status = snowflake_prepare(sfstmt, "select system$wait(5);", 0);
  assert_int_equal(status, SF_STATUS_SUCCESS);
  status = snowflake_async_execute(sfstmt);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  SF_QUERY_STATUS query_status = snowflake_get_query_status(sfstmt);
  assert_int_equal(query_status, SF_QUERY_STATUS_RUNNING);

  int retries = 0;
  while (query_status != SF_QUERY_STATUS_SUCCESS || retries > 5) {
    query_status = snowflake_get_query_status(sfstmt);
    sf_sleep_ms(2000);
    retries++;
  }

  /* get results */
  char *out = NULL;
  size_t value_len = 0;
  size_t max_value_size = 0;
  assert_int_equal(snowflake_num_rows(sfstmt), 1);

  int count = 0;
  while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
    count++;
    snowflake_column_as_str(sfstmt, 1, &out, &value_len, &max_value_size);
    assert_string_equal(out, "waited 5 seconds");
  }
  assert_int_equal(count, 1);

  if (status != SF_STATUS_EOF) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_EOF);
  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
  SF_FREE(out);
}

void test_premature_fetch() {
  SF_CONNECT* sf = setup_snowflake_connection();
  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  /* query */
  SF_STMT* sfstmt = snowflake_stmt(sf);
  status = snowflake_prepare(sfstmt, "select system$wait(50);", 0);
  assert_int_equal(status, SF_STATUS_SUCCESS);
  status = snowflake_async_execute(sfstmt);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  /* get results */
  char* out = NULL;
  size_t value_len = 0;
  size_t max_value_size = 0;

  /* Premature fetch */
  status = snowflake_fetch(sfstmt);
  assert_int_equal(status, SF_STATUS_ERROR_GENERAL);
  SF_ERROR_STRUCT* error = snowflake_stmt_error(sfstmt);
  assert_string_equal(error->msg,
    "Cannot retrieve data on the status of this query. Max retries hit. Query status: 7");
  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
  SF_FREE(out);
}

void test_new_connection() {
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

  SF_STMT* async_sfstmt = snowflake_init_async_query_result(sf, sfqid);
  SF_FREE(sfqid);

  /* get results */
  int64 out = 0;
  assert_int_equal(snowflake_num_rows(async_sfstmt), 1);

  int count = 0;
  while ((status = snowflake_fetch(async_sfstmt)) == SF_STATUS_SUCCESS) {
    count++;
    snowflake_column_as_int64(async_sfstmt, 1, &out);
    assert_int_equal(out, 1);
  }
  assert_int_equal(count, 1);
  if (status != SF_STATUS_EOF) {
    dump_error(&(async_sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_EOF);
}

void test_fake_table() {
  SF_CONNECT* sf = setup_snowflake_connection();
  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  /* query */
  SF_STMT* sfstmt = snowflake_stmt(sf);
  status = snowflake_prepare(sfstmt, "select * from my_fake_table;", 0);
  assert_int_equal(status, SF_STATUS_SUCCESS);
  status = snowflake_async_execute(sfstmt);
  assert_int_equal(status, SF_STATUS_SUCCESS);

  /* get results */
  status = snowflake_fetch(sfstmt);
  assert_int_equal(status, SF_STATUS_ERROR_GENERAL);
  SF_ERROR_STRUCT* error = snowflake_error(sf);
  assert_string_equal(error->msg, "SQL compilation error:\nObject 'MY_FAKE_TABLE' does not exist or not authorized.");

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
}

void test_invalid_query_id() {
  SF_CONNECT* sf = setup_snowflake_connection();
  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  char* fake_sfqid = "fake-query-id";
  SF_STMT* async_sfstmt = snowflake_init_async_query_result(sf, fake_sfqid);

  assert_non_null(async_sfstmt);
  assert_non_null(async_sfstmt->connection);
  assert_string_equal(async_sfstmt->sfqid, fake_sfqid);
  assert_null(async_sfstmt->result_set);

  SF_QUERY_STATUS query_status = snowflake_get_query_status(async_sfstmt);
  assert_int_equal(query_status, SF_QUERY_STATUS_UNKNOWN);

  snowflake_stmt_term(async_sfstmt);
  snowflake_term(sf);
}

void test_multiple_chunk() {
  SF_CONNECT* sf = setup_snowflake_connection();
  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  /* query */
  SF_STMT* sfstmt = snowflake_stmt(sf);
  status = snowflake_prepare(sfstmt, "select randstr(100,random()) from table(generator(rowcount=>2000))", 0);
  assert_int_equal(status, SF_STATUS_SUCCESS);
  status = snowflake_async_execute(sfstmt);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  /* get results */
  char* value = NULL;
  size_t value_len = 0;
  size_t max_value_size = 0;
  assert_int_equal(snowflake_num_rows(sfstmt), 2000);

  int count = 0;
  while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
    count++;
    snowflake_column_as_str(sfstmt, 1, &value, &value_len, &max_value_size);
    assert_int_equal(strlen(value), 100);
  }
  assert_int_equal(count, 2000);

  SF_FREE(value);
  if (status != (SF_STATUS_EOF | SF_STATUS_SUCCESS)) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(status, (SF_STATUS_EOF | SF_STATUS_SUCCESS));
  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
}

void test_sleep_max_retries() {
    SF_CONNECT* sf = setup_snowflake_connection();
    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* query */
    SF_STMT* sfstmt = snowflake_stmt(sf);
    status = snowflake_prepare(sfstmt, "select system$wait(120);", 0);
    assert_int_equal(status, SF_STATUS_SUCCESS);
    status = snowflake_async_execute(sfstmt);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* get results */
    assert_int_equal(snowflake_num_rows(sfstmt), -1);
    assert_int_equal(snowflake_num_fields(sfstmt), -1);
    assert_int_equal(snowflake_num_params(sfstmt), 0);

    status = snowflake_fetch(sfstmt);
    assert_int_equal(status, SF_STATUS_ERROR_GENERAL);
    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_ordered_results() {
    SF_CONNECT* sf = setup_snowflake_connection();
    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* query */
    SF_STMT* sfstmt = snowflake_stmt(sf);
    status = snowflake_prepare(
      sfstmt,
      "with recursive test as (select 1 as num union all select num + 1 from test where num < 10) select num from test;",
      0);
    assert_int_equal(status, SF_STATUS_SUCCESS);
    status = snowflake_async_execute(sfstmt);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* get results */
    int64 out = 0;
    assert_int_equal(snowflake_num_rows(sfstmt), 10);
    assert_int_equal(snowflake_num_fields(sfstmt), 1);

    int count = 0;
    int i = 1;
    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
      count++;
      snowflake_column_as_int64(sfstmt, 1, &out);
      assert_int_equal(out, i);
      i++;
    }
    assert_int_equal(count, 10);
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
      //cmocka_unit_test(test_normal_async_select_query),
      //cmocka_unit_test(test_long_query_get_query_status),
      //cmocka_unit_test(test_premature_fetch),
      //cmocka_unit_test(test_new_connection),
      //cmocka_unit_test(test_fake_table),
      cmocka_unit_test(test_invalid_query_id),
      //cmocka_unit_test(test_multiple_chunk),
      //cmocka_unit_test(test_sleep_max_retries),
      //cmocka_unit_test(test_ordered_results),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
