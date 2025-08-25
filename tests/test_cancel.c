#include <string.h>
#include "utils/test_setup.h"
#include "snowflake/platform.h"
#include "memory.h"
#include "snowflake_util.h"

/* In some bad timing we can't get expected query status,
 * such as cancel request sent before query getting executed,
 * limit the waiting time and skip the test in such case
 * to avoid infinite loop.
 */
const int STATUS_WAIT_RETRY_MAX = 10;

void test_basic_cancel() {
  SF_CONNECT *sf = setup_snowflake_connection();
  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  char unique_id[SF_UUID4_LEN];
  char query[256];
  generate_unique_id(unique_id);
  sprintf(query, "select '%s', count(*)%%1 from table(generator(timeLimit => 3600))", unique_id);
  /* query */
  SF_STMT *sfstmt = snowflake_stmt(sf);
  status = snowflake_prepare(sfstmt, query, 0);
  assert_int_equal(status, SF_STATUS_SUCCESS);

  SF_THREAD_HANDLE execute_thread;
  SF_THREAD_HANDLE cancel_thread;
  _thread_init(&execute_thread, (void *)snowflake_execute, (void *)sfstmt);
  // Give time for query to init
  sf_sleep_ms(1000);
  _thread_init(&cancel_thread, (void *)snowflake_cancel_query, (void *)sfstmt);

  _thread_join(execute_thread);
  _thread_join(cancel_thread);
  // Give time for query to cancel
  SF_QUERY_STATUS query_status = SF_QUERY_STATUS_RUNNING;
  int retry = 0;
  while (SF_QUERY_STATUS_RUNNING == query_status)
  {
    if (retry > STATUS_WAIT_RETRY_MAX)
    {
      snowflake_stmt_term(sfstmt);
      snowflake_term(sf);
      printf("test_basic_cancel failed to cancel query, ignore.\n");
      return;
    }
    sf_sleep_ms(1000);
    query_status = snowflake_get_query_status(sfstmt);
	retry++;
  }
  assert_int_equal(query_status, SF_QUERY_STATUS_FAILED_WITH_ERROR);
  assert_int_equal(sfstmt->error.error_code, SF_STATUS_ERROR_QUERY_CANCELLED);
  assert_string_equal(sfstmt->error.msg, "SQL execution canceled");

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
}

void test_no_stmt() {
  SF_STMT *sfstmt = NULL;
  SF_STATUS status = snowflake_cancel_query(sfstmt);
  assert_int_equal(status, SF_STATUS_ERROR_STATEMENT_NOT_EXIST);
}

void test_no_query() {
  SF_CONNECT *sf = setup_snowflake_connection();
  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  SF_STMT *sfstmt = snowflake_stmt(sf);
  status = snowflake_cancel_query(sfstmt);
  assert_int_equal(status, SF_STATUS_SUCCESS);
}

void test_prepared_query() {
  SF_CONNECT *sf = setup_snowflake_connection();
  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  SF_STMT *sfstmt = snowflake_stmt(sf);
  status = snowflake_prepare(sfstmt, "select 1;", 0);
  assert_int_equal(status, SF_STATUS_SUCCESS);

  status = snowflake_cancel_query(sfstmt);
  assert_int_equal(status, SF_STATUS_SUCCESS);

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
}

void test_async() {
  SF_CONNECT *sf = setup_snowflake_connection();
  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  char unique_id[SF_UUID4_LEN];
  char query[256];
  generate_unique_id(unique_id);
  sprintf(query, "select '%s', count(*)%%1 from table(generator(timeLimit => 3600))", unique_id);
  /* query */
  SF_STMT *sfstmt = snowflake_stmt(sf);
  status = snowflake_prepare(sfstmt, query, 0);
  assert_int_equal(status, SF_STATUS_SUCCESS);
  status = snowflake_async_execute(sfstmt);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);
  // Give time for query to init
  SF_QUERY_STATUS query_status = snowflake_get_query_status(sfstmt);
  int retry = 0;
  while (SF_QUERY_STATUS_RUNNING != query_status)
  {
    if (retry > STATUS_WAIT_RETRY_MAX)
    {
     /* Unlikely would happen, but avoid infinite loop just in case.
      */
      snowflake_stmt_term(sfstmt);
      snowflake_term(sf);
      printf("test_async failed to get query running\n");
      assert_true(0);
      return;
    }
    sf_sleep_ms(1000);
    query_status = snowflake_get_query_status(sfstmt);
	retry++;
  }
  status = snowflake_cancel_query(sfstmt);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
    assert_int_equal(status, SF_STATUS_ERROR_GENERAL);
    assert_int_equal(sfstmt->error.error_code, 605);
  }
  // Give time for query to cancel
  while (SF_QUERY_STATUS_RUNNING == query_status)
  {
    if (retry > STATUS_WAIT_RETRY_MAX)
    {
     /* Unlikely would happen, but avoid infinite loop just in case.
      */
      snowflake_stmt_term(sfstmt);
      snowflake_term(sf);
      printf("test_async failed to cancel query\n");
      assert_true(0);
      return;
    }
    sf_sleep_ms(1000);
    query_status = snowflake_get_query_status(sfstmt);
    retry++;
  }
  assert_int_equal(query_status, SF_QUERY_STATUS_FAILED_WITH_ERROR);
  assert_int_equal(sfstmt->error.error_code, SF_STATUS_ERROR_QUERY_CANCELLED);
  assert_string_equal(sfstmt->error.msg, "SQL execution canceled");

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
}

void test_finished_query() {
  SF_CONNECT *sf = setup_snowflake_connection();
  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  /* query */
  SF_STMT *sfstmt = snowflake_stmt(sf);
  status = snowflake_prepare(sfstmt, "select 1;", 0);
  assert_int_equal(status, SF_STATUS_SUCCESS);
  status = snowflake_execute(sfstmt);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  status = snowflake_cancel_query(sfstmt);
  assert_int_equal(status, SF_STATUS_SUCCESS);

  SF_QUERY_STATUS query_status = snowflake_get_query_status(sfstmt);
  assert_int_equal(query_status, SF_QUERY_STATUS_SUCCESS);
  assert_int_equal(sfstmt->error.error_code, SF_STATUS_SUCCESS);

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
}

void test_multiple_statements() {
  SF_CONNECT *sf = setup_snowflake_connection();
  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  /* query */
  SF_STMT *sfstmt = snowflake_stmt(sf);
  int64 multi_stmt_count = 3;
  status = snowflake_stmt_set_attr(sfstmt, SF_STMT_MULTI_STMT_COUNT, &multi_stmt_count);
  assert_int_equal(status, SF_STATUS_SUCCESS);

  char unique_id[SF_UUID4_LEN];
  char query[256];
  generate_unique_id(unique_id);
  sprintf(query, "select 1; select '%s', count(*)%%1 from table(generator(timeLimit => 3600)); select 3", unique_id);
  status = snowflake_prepare(sfstmt, query, 0);
  assert_int_equal(status, SF_STATUS_SUCCESS);

  SF_THREAD_HANDLE execute_thread;
  SF_THREAD_HANDLE cancel_thread;
  _thread_init(&execute_thread, (void *)snowflake_execute, (void *)sfstmt);
  // Give time for query to init
  sf_sleep_ms(1000);
  _thread_init(&cancel_thread, (void *)snowflake_cancel_query, (void *)sfstmt);

  _thread_join(execute_thread);
  _thread_join(cancel_thread);
  // Give time for query to cancel
  SF_QUERY_STATUS query_status = SF_QUERY_STATUS_RUNNING;
  int retry = 0;
  while (SF_QUERY_STATUS_RUNNING == query_status)
  {
    if (retry > STATUS_WAIT_RETRY_MAX)
    {
      snowflake_stmt_term(sfstmt);
      snowflake_term(sf);
      printf("test_multiple_statements failed to get query running, ignore\n");
      return;
    }
    sf_sleep_ms(1000);
    query_status = snowflake_get_query_status(sfstmt);
	retry++;
  }
  while (SF_QUERY_STATUS_RUNNING == query_status)
  {
    if (retry > STATUS_WAIT_RETRY_MAX)
    {
      snowflake_stmt_term(sfstmt);
      snowflake_term(sf);
      printf("test_multiple_statements failed to cancel query, ignore\n");
      return;
    }
    sf_sleep_ms(1000);
    query_status = snowflake_get_query_status(sfstmt);
    retry++;
  }
  assert_int_equal(query_status, SF_QUERY_STATUS_FAILED_WITH_ERROR);
  assert_int_equal(sfstmt->error.error_code, SF_STATUS_ERROR_QUERY_CANCELLED);
  assert_string_equal(sfstmt->error.msg, "SQL execution canceled");

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
}

void test_bind_params() {
  int array_size = 100000;
  int64 *int64_array = NULL;
  int64 int64_value = 12345;
  sf_bool *bool_array = NULL;
  sf_bool bool_value = SF_BOOLEAN_TRUE;
  char *string_array = NULL;
  char string_value[] = "\"str\"";

  SF_BIND_INPUT input_array[3];
  SF_BIND_INPUT int64_input;
  SF_BIND_INPUT bool_input;
  SF_BIND_INPUT string_input;
  int64_array = SF_CALLOC(array_size, sizeof(int64_value));
  bool_array = SF_CALLOC(array_size, sizeof(bool_value));
  string_array = SF_CALLOC(array_size, sizeof(string_value));

  for (int i = 0; i < array_size; i++)
  {
    int64_array[i] = int64_value;
    bool_array[i] = bool_value;
    memcpy(string_array + sizeof(string_value) * i, string_value, sizeof(string_value));
  }

  snowflake_bind_input_init(&int64_input);
  snowflake_bind_input_init(&bool_input);
  snowflake_bind_input_init(&string_input);
  int64_input.idx = 1;
  int64_input.c_type = SF_C_TYPE_INT64;
  int64_input.value = int64_array;
  int64_input.len = 0;
  bool_input.idx = 2;
  bool_input.c_type = SF_C_TYPE_BOOLEAN;
  bool_input.value = bool_array;
  bool_input.len = 0;
  string_input.idx = 3;
  string_input.c_type = SF_C_TYPE_STRING;
  string_input.value = string_array;
  string_input.len = sizeof(string_value);

  input_array[0] = int64_input;
  input_array[1] = bool_input;
  input_array[2] = string_input;

  SF_CONNECT *sf = setup_snowflake_connection();
  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  char unique_id[SF_UUID4_LEN];
  char query[256];
  generate_unique_id(unique_id);
  sprintf(query, "create or replace temporary table t%s (c1 number, c2 boolean, c3 string)", unique_id);
  SF_STMT *sfstmt = snowflake_stmt(sf);
  status = snowflake_query(
    sfstmt,
    query,
    0
  );
  assert_int_equal(status, SF_STATUS_SUCCESS);

  int64 paramset_size = (int64)array_size;
  status = snowflake_stmt_set_attr(sfstmt, SF_STMT_PARAMSET_SIZE, &paramset_size);
  sprintf(query, "insert into t%s values(?, ?, ?)", unique_id);
  status = snowflake_prepare(
    sfstmt,
    query,
    0
  );
  assert_int_equal(status, SF_STATUS_SUCCESS);

  status = snowflake_bind_param_array(sfstmt, input_array, sizeof(input_array) / sizeof(SF_BIND_INPUT));
  assert_int_equal(status, SF_STATUS_SUCCESS);
  status = snowflake_async_execute(sfstmt);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);
  status = snowflake_cancel_query(sfstmt);
  assert_int_equal(status, SF_STATUS_SUCCESS);
  // Give time for query to cancel
  SF_QUERY_STATUS query_status = SF_QUERY_STATUS_RUNNING;
  while (SF_QUERY_STATUS_RUNNING == query_status)
  {
    sf_sleep_ms(1000);
    query_status = snowflake_get_query_status(sfstmt);
  }
  if (query_status == SF_QUERY_STATUS_SUCCESS)
  {
    printf("test_bind_params query succeeded before cancel, ignore\n");
    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
    return;
  }
  assert_int_equal(query_status, SF_QUERY_STATUS_FAILED_WITH_ERROR);
  assert_int_equal(sfstmt->error.error_code, SF_STATUS_ERROR_QUERY_CANCELLED);
  assert_string_equal(sfstmt->error.msg, "SQL execution canceled");

  sprintf(query, "select count(*) from t%s", unique_id);
  status = snowflake_query(sfstmt, query, 0);
  assert_int_equal(status, SF_STATUS_SUCCESS);
  status = snowflake_fetch(sfstmt);
  assert_int_equal(status, SF_STATUS_SUCCESS);
  int64 value;
  snowflake_column_as_int64(sfstmt, 1, &value);
  assert_int_equal(value, 0);

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
}

void test_array_binding() {
  SF_STATUS status;
  char bind_data_b[5][4] = { "2.3", "3.4", "4.5", "5.6", "6.7"};
  char bind_data_a[5][2] = { "2", "3", "4", "5", "6"};

  SF_BIND_INPUT input_a;
  SF_BIND_INPUT input_b;

  SF_BIND_INPUT input_array[2];

  snowflake_bind_input_init(&input_a);
  snowflake_bind_input_init(&input_b);

  input_b.idx = 1;
  input_b.c_type = SF_C_TYPE_STRING;
  input_b.value = bind_data_b;
  input_b.len = 4;
  input_a.idx = 2;
  input_a.c_type = SF_C_TYPE_STRING;
  input_a.value = bind_data_a;
  input_a.len = 2;

  input_array[0] = input_b;
  input_array[1] = input_a;

  /* Connect with all parameters set */
  SF_CONNECT *sf = setup_snowflake_connection();
  // turn on FAIL_OPEN to around certificate issue with GCP
  sf_bool value = SF_BOOLEAN_TRUE;
  snowflake_set_attribute(sf, SF_CON_OCSP_FAIL_OPEN, &value);
  status = snowflake_connect(sf);
  assert_int_equal(status, SF_STATUS_SUCCESS);

  /* Create a statement once and reused */
  SF_STMT *sfstmt = snowflake_stmt(sf);
  char unique_id[SF_UUID4_LEN];
  char query[256];
  generate_unique_id(unique_id);
  sprintf(query, "create or replace temporary table foo1%s(a int, b double)", unique_id);
  status = snowflake_query(
    sfstmt,
    query,
    0
  );
  assert_int_equal(status, SF_STATUS_SUCCESS);

  sprintf(query, "insert into foo1%s values (2, NULL), (3, NULL), (4, NULL), (5, NULL), (6, NULL)", unique_id);
  status = snowflake_query(
    sfstmt,
    query,
    0
  );
  assert_int_equal(status, SF_STATUS_SUCCESS);

  int64 paramset_size = 5;
  status = snowflake_stmt_set_attr(sfstmt, SF_STMT_PARAMSET_SIZE, &paramset_size);
  sprintf(query, "update foo1%s set b = ? where a = ?", unique_id);
  status = snowflake_prepare(
    sfstmt,
    query,
    0
  );
  assert_int_equal(status, SF_STATUS_SUCCESS);

  status = snowflake_bind_param_array(sfstmt, input_array, sizeof(input_array) / sizeof(SF_BIND_INPUT));
  assert_int_equal(status, SF_STATUS_SUCCESS);

  SF_THREAD_HANDLE execute_thread;
  _thread_init(&execute_thread, (void *)snowflake_execute, (void *)sfstmt);
  sf_sleep_ms(100);
  status = snowflake_cancel_query(sfstmt);
  bool isCancelSucceed = true;
  if (status == SF_STATUS_ERROR_GENERAL)
  {
    assert_int_equal(sfstmt->error.error_code, 605);
    isCancelSucceed = false;
  }
  _thread_join(execute_thread);

  sprintf(query, "select * from foo1%s", unique_id);
  status = snowflake_query(sfstmt, query, 0);
  assert_int_equal(status, SF_STATUS_SUCCESS);
  assert_int_equal(snowflake_num_rows(sfstmt), 5);

  for (int i = 0; i < 5; i++)
  {
    status = snowflake_fetch(sfstmt);
    if (status != SF_STATUS_SUCCESS) {
      dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    char *result = NULL;
    size_t value_len = 0;
    size_t max_value_size = 0;

    snowflake_column_as_str(sfstmt, 1, &result, &value_len, &max_value_size);
    assert_string_equal(result, bind_data_a[i]);
    snowflake_column_as_str(sfstmt, 2, &result, &value_len, &max_value_size);
    /*
     * Check the last row value only when cancel succeeded.
     * The query doesn't support array binding so the driver fallback with batch execution.
     * When cancel failed not sure whether the query is being executed then canceled,
     * or the cancel is before executing the query.
     * When cancel succeeded which row being canceled. The only thing for sure is there is
     * something being canceled so the last row must not updated.
     */
    if ((isCancelSucceed) && (i == 4))
    {
      assert_string_equal(result, "");
    }

    free(result);
  }

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_basic_cancel),
      cmocka_unit_test(test_no_stmt),
      cmocka_unit_test(test_no_query),
      cmocka_unit_test(test_prepared_query),
      cmocka_unit_test(test_async),
      cmocka_unit_test(test_finished_query),
      cmocka_unit_test(test_multiple_statements),
      cmocka_unit_test(test_bind_params),
      cmocka_unit_test(test_array_binding),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
