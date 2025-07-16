#include <string.h>
#include "utils/test_setup.h"
#include "../lib/connection.h"


typedef struct test_case_to_string {
    const int64 c1in;
    const char *c2in;
    const char *c2out;
    const char* c3out;
    const char* c4out;
    SF_STATUS error_code;
} TEST_CASE_TO_STRING;

#ifdef _WIN32
// On Windows TZ variable has different syntax.
// https://docs.microsoft.com/en-us/previous-versions/90s5c885(v=vs.140)#remarks
// timezone in connection property like America/New_York won't change local
// timezone setting as expected, while server doesn't accept Windows syntax like
// UTC-5:00. Setting session timezone to the actual timezone of the client would
// work so it won't causing any issue on customer side. But since we need to make
// the test case work on any timezone, "UTC" is the only timezone setting can be
// accepted by both server and client side.
#define USER_TZ "UTC"
#else
#define USER_TZ "America/New_York"
#endif // _WIN32

#define WARSAW "Europe/Warsaw"
#define TOKYO "Asia/Tokyo"

void test_timestamp_ltz_helper(sf_bool use_arrow, sf_bool useZeroPrecision)
{

  TEST_CASE_TO_STRING test_cases[] = {
          {.c1in = 1, .c2in = "2014-05-03 13:56:46.123", .c2out = useZeroPrecision == SF_BOOLEAN_TRUE
                                                                         ? "2014-05-03 13:56:46"
                                                                         : "2014-05-03 13:56:46.12300"},
          {.c1in = 2, .c2in = "1969-11-21 05:17:23.0123", .c2out = useZeroPrecision == SF_BOOLEAN_TRUE
                                                                          ? "1969-11-21 05:17:23"
                                                                          : "1969-11-21 05:17:23.01230"},
          {.c1in = 3, .c2in = "1960-01-01 00:00:00.0000", .c2out = useZeroPrecision == SF_BOOLEAN_TRUE
                                                                   ? "1960-01-01 00:00:00"
                                                                   : "1960-01-01 00:00:00.00000"},
#ifdef __linux__
          // Must run the tests High Sierra (10.13) or newer OS.
          // Windows get inaccurate result as well.
          {.c1in = 4, .c2in = "1500-01-01 00:00:00.0000", .c2out = useZeroPrecision == SF_BOOLEAN_TRUE
                                                                   ? "1500-01-01 00:00:00"
                                                                   : "1500-01-01 00:00:00.00000"},
          // High Sierra (10.13) fixed the calendar issue before 1600, yet the output is slightly different from Linux.
          // Windows get the same output as MacOS
          // {.c1in = 5, .c2in = "0001-01-01 00:00:00.0000", .c2out = "0001-01-01 00:00:00.00000"},
          {.c1in = 5, .c2in = "0001-01-01 00:00:00.0000", .c2out = useZeroPrecision == SF_BOOLEAN_TRUE
                                                                   ? "1-01-01 00:00:00"
                                                                   : "1-01-01 00:00:00.00000"},
#endif // __linux__
          {.c1in = 6, .c2in = "9999-01-01 00:00:00.0000", .c2out = useZeroPrecision == SF_BOOLEAN_TRUE
                                                                   ? "9999-01-01 00:00:00"
                                                                   : "9999-01-01 00:00:00.00000"},
          {.c1in = 7, .c2in = "99999-12-31 23:59:59.9999", .c2out = "", .error_code=100035},
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

  /* Set query result format to Arrow if necessary */
  status = snowflake_query(
      sfstmt,
      use_arrow == SF_BOOLEAN_TRUE
      ? "alter session set C_API_QUERY_RESULT_FORMAT=ARROW_FORCE"
      : "alter session set C_API_QUERY_RESULT_FORMAT=JSON",
      0
  );
  if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

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

void test_timestamp_ltz_arrow(void **unused) {
  SF_UNUSED(unused);
  test_timestamp_ltz_helper(SF_BOOLEAN_TRUE, SF_BOOLEAN_TRUE);
  test_timestamp_ltz_helper(SF_BOOLEAN_TRUE, SF_BOOLEAN_FALSE);
}

void test_timestamp_ltz_json(void **unused) {
  SF_UNUSED(unused);
  test_timestamp_ltz_helper(SF_BOOLEAN_FALSE, SF_BOOLEAN_TRUE);
  test_timestamp_ltz_helper(SF_BOOLEAN_FALSE, SF_BOOLEAN_FALSE);
}

void test_timestamp_ltz_verifying_binding_value_helper(const char* timezone, sf_bool isStageBinding) {

    TEST_CASE_TO_STRING test_cases[] = {
            {.c1in = 1, .c2in = "2014-05-03 13:56:46.123", .c2out = "2014-05-03 13:56:46", .c3out = "2014-05-03 13:56:46.12300", .c4out ="2014-05-03 13:56:46.123000000"},
            {.c1in = 2, .c2in = "1969-11-21 05:17:23.0123", .c2out = "1969-11-21 05:17:23", .c3out = "1969-11-21 05:17:23.01230", .c4out = "1969-11-21 05:17:23.012300000"},
            {.c1in = 3, .c2in = "1960-01-01 00:00:00.0000", .c2out = "1960-01-01 00:00:00", .c3out = "1960-01-01 00:00:00.00000", .c4out = "1960-01-01 00:00:00.000000000"},
  #ifdef __linux__
        // Must run the tests High Sierra (10.13) or newer OS.
        // Windows get inaccurate result as well.
        {.c1in = 4, .c2in = "1500-01-01 00:00:00.0000", .c2out = "1500-01-01 00:00:00",.c3out = "1500-01-01 00:00:00.00000", .c4out = "1500-01-01 00:00:00.000000000"},
        // High Sierra (10.13) fixed the calendar issue before 1600, yet the output is slightly different from Linux.
        // Windows get the same output as MacOS
        // {.c1in = 5, .c2in = "0001-01-01 00:00:00.0000", .c2out = "0001-01-01 00:00:00.00000"},
        {.c1in = 5, .c2in = "0001-01-01 00:00:00.0000", .c2out = "1-01-01 00:00:00",.c3out = "1-01-01 00:00:00.00000", .c4out = "1-01-01 00:00:00.000000000"},
#endif // __linux__
          {.c1in = 6, .c2in = "9999-01-01 00:00:00.0000", .c2out = "9999-01-01 00:00:00", .c3out = "9999-01-01 00:00:00.00000", .c4out = "9999-01-01 00:00:00.000000000"},
    };
    SF_CONNECT* sf = setup_snowflake_connection_with_autocommit(
        timezone, SF_BOOLEAN_TRUE); // set the session timezone
    sf_bool disable_stage_binding = SF_BOOLEAN_FALSE;
    snowflake_set_attribute(sf, SF_CON_DISABLE_STAGE_BIND, &disable_stage_binding);

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* Create a statement once and reused */
    SF_STMT* sfstmt = snowflake_stmt(sf);

    if (isStageBinding) {
        int64 stage_treshold = 1;
        int64* cur_threshold;
        snowflake_set_attribute(sfstmt->connection, SF_CON_STAGE_BIND_THRESHOLD, (void*)&stage_treshold);
        snowflake_get_attribute(sfstmt->connection, SF_CON_STAGE_BIND_THRESHOLD, (void**)&cur_threshold);
        assert_int_equal(*cur_threshold, stage_treshold);
    }

    status = snowflake_query(
        sfstmt,
        "alter session set C_API_QUERY_RESULT_FORMAT=JSON",
        0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_query(
        sfstmt,
        "create or replace table t (c1 int, c2 timestamp_ltz(0), c3 timestamp_ltz(5), c4 timestamp_ltz(9))",
        0
    );
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* insert data */
    status = snowflake_prepare(
        sfstmt,
        "insert into t(c1,c2,c3,c4) values(?,?,?,?)",
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
        SF_BIND_INPUT ic1 = { 0 };
        ic1.idx = 1;
        ic1.name = NULL;
        ic1.c_type = SF_C_TYPE_INT64;
        ic1.value = (void*)&v.c1in;
        ic1.len = sizeof(v.c1in);
        status = snowflake_bind_param(sfstmt, &ic1);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        SF_BIND_INPUT ic2 = { 0 };
        ic2.idx = 2;
        ic2.name = NULL;
        ic2.c_type = SF_C_TYPE_STRING;
        ic2.value = (void*)v.c2in;
        ic2.len = v.c2in != NULL ? strlen(v.c2in) : 0;
        status = snowflake_bind_param(sfstmt, &ic2);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        SF_BIND_INPUT ic3 = { 0 };
        ic3.idx = 3;
        ic3.name = NULL;
        ic3.c_type = SF_C_TYPE_STRING;
        ic3.value = (void*)v.c2in;
        ic3.len = v.c2in != NULL ? strlen(v.c2in) : 0;
        status = snowflake_bind_param(sfstmt, &ic3);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        SF_BIND_INPUT ic4 = { 0 };
        ic4.idx = 4;
        ic4.name = NULL;
        ic4.c_type = SF_C_TYPE_STRING;
        ic4.value = (void*)v.c2in;
        ic4.len = v.c2in != NULL ? strlen(v.c2in) : 0;
        status = snowflake_bind_param(sfstmt, &ic4);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(sfstmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);

        status = snowflake_execute(sfstmt);
        if (v.error_code != SF_STATUS_SUCCESS) {
            // expecting failure
            SF_ERROR_STRUCT* error = snowflake_stmt_error(sfstmt);
            assert_int_equal(error->error_code, v.error_code);
        }
        else {
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

    char* c2buf = NULL;
    size_t c2buf_len = 0;
    size_t c2buf_max_size = 0;
    char* c3buf = NULL;
    size_t c3buf_len = 0;
    size_t c3buf_max_size = 0;
    char* c4buf = NULL;
    size_t c4buf_len = 0;
    size_t c4buf_max_size = 0;

    assert_int_equal(snowflake_num_rows(sfstmt), no_error_test_cases);

    int counter = 0;
    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        TEST_CASE_TO_STRING v;
        do {
            memcpy(&v, &test_cases[counter++], sizeof(TEST_CASE_TO_STRING));
        } while (v.error_code != (SF_STATUS)0);
        assert_int_equal(status, SF_STATUS_SUCCESS);
            // expecting not null
            snowflake_column_as_str(sfstmt, 2, &c2buf, &c2buf_len, &c2buf_max_size);
            assert_string_equal(v.c2out, c2buf);
            snowflake_column_as_str(sfstmt, 3, &c3buf, &c3buf_len, &c3buf_max_size);
            assert_string_equal(v.c3out, c3buf);
            snowflake_column_as_str(sfstmt, 4, &c4buf, &c4buf_len, &c4buf_max_size);
            assert_string_equal(v.c4out, c4buf);
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
    free(c3buf);
    free(c4buf);

    c2buf = NULL;
    c3buf = NULL;
    c4buf = NULL;

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_verify_data_types_with_two_different_binding_UTC(void** unused) {
    SF_UNUSED(unused);
    test_timestamp_ltz_verifying_binding_value_helper("UTC", SF_BOOLEAN_FALSE);
    test_timestamp_ltz_verifying_binding_value_helper("UTC", SF_BOOLEAN_TRUE);
}
//On Windows, the LTZ colum has a different behavior. Please have a look at #17.
#ifndef _WIN32
void test_verify_data_types_with_two_different_binding_WARSAW(void** unused) {
    SF_UNUSED(unused);
    test_timestamp_ltz_verifying_binding_value_helper(WARSAW, SF_BOOLEAN_FALSE);
    test_timestamp_ltz_verifying_binding_value_helper(WARSAW, SF_BOOLEAN_TRUE);
}
void test_verify_data_types_with_two_different_binding_TOKYO(void** unused) {
    SF_UNUSED(unused);
    test_timestamp_ltz_verifying_binding_value_helper(TOKYO, SF_BOOLEAN_FALSE);
    test_timestamp_ltz_verifying_binding_value_helper(TOKYO, SF_BOOLEAN_TRUE);
}

#endif // !_WIN32


int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_timestamp_ltz_arrow),
      cmocka_unit_test(test_timestamp_ltz_json),
      cmocka_unit_test(test_verify_data_types_with_two_different_binding_UTC),
#ifndef _WIN32
      cmocka_unit_test(test_verify_data_types_with_two_different_binding_WARSAW),
      cmocka_unit_test(test_verify_data_types_with_two_different_binding_TOKYO),
#endif // !_WIN32
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
