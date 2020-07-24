/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include "utils/test_setup.h"
#include "../lib/connection.h"


typedef struct test_case_to_string {
    const int64 c1in;
    const char *c2in;
    const char *c2out;
    SF_STATUS error_code;
} TEST_CASE_TO_STRING;

#define USER_TZ "America/New_York"

void test_timestamp_ltz_helper(sf_bool useZeroPrecision)
{

  TEST_CASE_TO_STRING test_cases[] = {
#ifndef _WIN32
          {.c1in = 1, .c2in = "2014-05-03 13:56:46.123 -04:00", .c2out = useZeroPrecision == SF_BOOLEAN_TRUE
                                                                         ? "2014-05-03 13:56:46"
                                                                         : "2014-05-03 13:56:46.12300"},
          {.c1in = 2, .c2in = "1969-11-21 05:17:23.0123 -05:00", .c2out = useZeroPrecision == SF_BOOLEAN_TRUE
                                                                          ? "1969-11-21 05:17:23"
                                                                          : "1969-11-21 05:17:23.01230"},
          {.c1in = 3, .c2in = "1960-01-01 00:00:00.0000", .c2out = useZeroPrecision == SF_BOOLEAN_TRUE
                                                                   ? "1960-01-01 00:00:00"
                                                                   : "1960-01-01 00:00:00.00000"},
#ifndef __APPLE__
          // Must run the tests High Sierra (10.13) or newer OS.
          {.c1in = 4, .c2in = "1500-01-01 00:00:00.0000", .c2out = useZeroPrecision == SF_BOOLEAN_TRUE
                                                                   ? "1500-01-01 00:00:00"
                                                                   : "1500-01-01 00:00:00.00000"},
          // High Sierra (10.13) fixed the calendar issue before 1600, yet the output is slightly different from Linux.
          // {.c1in = 5, .c2in = "0001-01-01 00:00:00.0000", .c2out = "0001-01-01 00:00:00.00000"},
          {.c1in = 5, .c2in = "0001-01-01 00:00:00.0000", .c2out = useZeroPrecision == SF_BOOLEAN_TRUE
                                                                   ? "1-01-01 00:00:00"
                                                                   : "1-01-01 00:00:00.00000"},
#endif // __APPLE__
          {.c1in = 6, .c2in = "9999-01-01 00:00:00.0000", .c2out = useZeroPrecision == SF_BOOLEAN_TRUE
                                                                   ? "9999-01-01 00:00:00"
                                                                   : "9999-01-01 00:00:00.00000"},
          {.c1in = 7, .c2in = "99999-12-31 23:59:59.9999", .c2out = "", .error_code=100035},
#endif // _WIN32
          {.c1in = 8, .c2in = NULL, .c2out = NULL},
          /* // none of the platform supports this
          {.c1in = 9, .c2in = "9999-12-31 23:59:59.9999", .c2out = "9999-12-31 23:59:59.99990 -05:00"},
           */
  };


  /* The client application must set the session parameter TIMEZONE
   * to the local timezone.
   * You could set the environment variable TZ but won't impact the result.
   */
  SF_CONNECT *sf = setup_snowflake_connection_with_autocommit(
          USER_TZ, SF_BOOLEAN_TRUE);

  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  /* Create a statement once and reused */
  SF_STMT *sfstmt = snowflake_stmt(sf);
  status = snowflake_query(
          sfstmt,
          useZeroPrecision == SF_BOOLEAN_TRUE
          ? "create or replace table t (c1 int, c2 timestamp_ltz(0))"
          : "create or replace table t (c1 int, c2 timestamp_ltz(5))",
          0
  );
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  /* insert data */
  status = snowflake_prepare(
          sfstmt,
          "insert into t(c1,c2) values(?,?)",
          0);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  size_t i;
  size_t len;
  int no_error_test_cases = 0;
  for (i = 0, len = sizeof(test_cases) / sizeof(TEST_CASE_TO_STRING);
       i < len; i++) {
    TEST_CASE_TO_STRING v = test_cases[i];
    SF_BIND_INPUT ic1;
    ic1.idx = 1;
    ic1.name = NULL;
    ic1.c_type = SF_C_TYPE_INT64;
    ic1.value = (void *) &v.c1in;
    ic1.len = sizeof(v.c1in);
    status = snowflake_bind_param(sfstmt, &ic1);
    if (status != SF_STATUS_SUCCESS) {
      dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    SF_BIND_INPUT ic2;
    ic2.idx = 2;
    ic2.name = NULL;
    ic2.c_type = SF_C_TYPE_STRING;
    ic2.value = (void *) v.c2in;
    ic2.len = v.c2in != NULL ? strlen(v.c2in) : 0;
    status = snowflake_bind_param(sfstmt, &ic2);
    if (status != SF_STATUS_SUCCESS) {
      dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_execute(sfstmt);
    if (v.error_code != SF_STATUS_SUCCESS) {
      // expecting failure
      SF_ERROR_STRUCT *error = snowflake_stmt_error(sfstmt);
      assert_int_equal(error->error_code, v.error_code);
    } else {
      // expecting success
      assert_int_equal(status, SF_STATUS_SUCCESS);
      ++no_error_test_cases;
    }
  }

  /* query */
  status = snowflake_query(sfstmt, "select * from t order by 1", 0);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  char *c2buf = NULL;
  size_t c2buf_len = 0;
  size_t c2buf_max_size = 0;
  sf_bool is_null;
  assert_int_equal(snowflake_num_rows(sfstmt), no_error_test_cases);

  int counter = 0;
  while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
    TEST_CASE_TO_STRING v;
    do {
      memcpy(&v, &test_cases[counter++], sizeof(TEST_CASE_TO_STRING));
    }
    while (v.error_code != (SF_STATUS)0);
    assert_int_equal(status, SF_STATUS_SUCCESS);
    if (v.c2out == NULL) {
      // expecting NULL
      snowflake_column_is_null(sfstmt, 2, &is_null);
      assert_true(is_null);
    } else {
      // expecting not null
      snowflake_column_as_str(sfstmt, 2, &c2buf, &c2buf_len, &c2buf_max_size);
      assert_string_equal(v.c2out, c2buf);
    }
  }
  if (status != SF_STATUS_EOF) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_EOF);

  status = snowflake_query(sfstmt, "drop table if exists t", 0);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  free(c2buf);
  c2buf = NULL;
  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
}

void test_timestamp_ltz(void** unused) {
  test_timestamp_ltz_helper(SF_BOOLEAN_TRUE);
  test_timestamp_ltz_helper(SF_BOOLEAN_FALSE);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_timestamp_ltz),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
