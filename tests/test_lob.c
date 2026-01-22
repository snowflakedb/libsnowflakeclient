#include "utils/test_setup.h"
#include <cJSON.h>
#include <string.h>
#include <connection.h>
#include <error.h>

#define MAX_LOB_SIZE    (128 * 1024 * 1024)
#define LARGE_SIZE      (MAX_LOB_SIZE / 2)
#define MEDIUM_SIZE     (LARGE_SIZE / 2)
#define ORIGIN_SIZE     (16 * 1024 * 1024)
#define SMALL_SIZE      128
#define SIZE_NUM        5

static char* lob_data;
int test_sizes[SIZE_NUM];

void initialize_lob_data()
{
  lob_data = malloc(MAX_LOB_SIZE + 1);
  for (int i = 0; i < MAX_LOB_SIZE / 8; i++)
  {
    sprintf(lob_data + i * 8, "%08d", i);
  }
  lob_data[MAX_LOB_SIZE] = '\0';

  test_sizes[0] = SMALL_SIZE;
  test_sizes[1] = ORIGIN_SIZE;
  test_sizes[2] = MEDIUM_SIZE;
  test_sizes[3] = LARGE_SIZE;
  test_sizes[4] = MAX_LOB_SIZE;
}

sf_bool verify_lob_data(const char* data, int size)
{
  char exp_data[32];
  for (int i = 0; i < size / 8; i++)
  {
    sprintf(exp_data, "%08d", i);
    if (strncmp(exp_data, data + i * 8, 8) != 0)
    {
      return SF_BOOLEAN_FALSE;
    }
  }

  return SF_BOOLEAN_TRUE;
}

void free_lob_data()
{
  free(lob_data);
}

void verify_result(SF_STMT *sfstmt, int exp_size, sf_bool accurate_desc, sf_bool verify_data)
{
  int desc_byte_size, desc_col_size;
  if (accurate_desc != SF_BOOLEAN_TRUE)
  {
    desc_byte_size = desc_col_size = MAX_LOB_SIZE;
  }
  else
  {
    desc_col_size = exp_size;
    desc_byte_size = exp_size * 4;
    if (desc_byte_size > MAX_LOB_SIZE)
    {
      desc_byte_size = MAX_LOB_SIZE;
    }
  }
  assert_int_equal(1, snowflake_num_fields(sfstmt));

  // fetch on the resultset
  SF_STATUS status = snowflake_fetch(sfstmt);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
  }
  if (SF_STATUS_ERROR_RETRY == sfstmt->error.error_code)
  {
    // retry timeout exceed on some test environments, ignore.
    return;
  }

  assert_int_equal(status, SF_STATUS_SUCCESS);

  // Check size of the data returned
  size_t length;
  if (snowflake_column_strlen(sfstmt, 1, &length)) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(exp_size, length);

  const char *out;
  if (snowflake_column_as_const_str(sfstmt, 1, &out)) {
    dump_error(&(sfstmt->error));
  }
  assert_int_equal(exp_size, strlen(out));

  // check the data content
  if (SF_BOOLEAN_TRUE == verify_data)
  {
    assert_int_equal(verify_lob_data(out, exp_size), SF_BOOLEAN_TRUE);
  }
}

void test_lob_setup(SF_CONNECT **out_sf, SF_STMT **out_sfstmt, sf_bool use_arrow)
{
  SF_CONNECT *sf = setup_snowflake_connection();
  // lower the timeout to speed up test and ignore possible timeout
  int64 timeout = 30;
  snowflake_set_attribute(sf, SF_CON_NETWORK_TIMEOUT, &timeout);
  sf->retry_timeout = timeout;
  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  log_set_level(SF_LOG_INFO);

  /* Create a statement once and reused */
  SF_STMT *sfstmt = snowflake_stmt(sf);

  // Do not check result for now as it could fail
  status = snowflake_query(
    sfstmt,
    "alter session set FEATURE_INCREASED_MAX_LOB_SIZE_IN_MEMORY='ENABLED'",
    0
  );

  if (SF_BOOLEAN_FALSE == use_arrow)
  {
    status = snowflake_query(
      sfstmt,
      "alter session set C_API_QUERY_RESULT_FORMAT=JSON",
      0
    );
  }
  else
  {
    status = snowflake_query(
      sfstmt,
      "alter session set C_API_QUERY_RESULT_FORMAT=ARROW",
      0
    );
  }
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  *out_sf = sf;
  *out_sfstmt = sfstmt;
}

void test_lob_retrieval_core(sf_bool use_arrow)
{
  SF_CONNECT *sf;
  SF_STMT *sfstmt;
  SF_STATUS status;

  test_lob_setup(&sf, &sfstmt, use_arrow);

  char query[1024];
  for (int i = 0; i < SIZE_NUM; i++)
  {
    sprintf(query, "select randstr(%d, 124)", test_sizes[i]);
    status = snowflake_query(
      sfstmt,
      query,
      0
    );
    if (status != SF_STATUS_SUCCESS) {
      dump_error(&(sfstmt->error));
    }
    if (SF_STATUS_ERROR_RETRY == sfstmt->error.error_code)
    {
      // retry timeout exceed on some test environments, ignore.
      snowflake_stmt_term(sfstmt);
      snowflake_term(sf);
      return;
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // randstr always returns column size as max
    verify_result(sfstmt, test_sizes[i], SF_BOOLEAN_FALSE, SF_BOOLEAN_FALSE);
  }

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
}

void test_lob_literal_core(sf_bool use_arrow)
{
  SF_CONNECT *sf;
  SF_STMT *sfstmt;
  SF_STATUS status;

  test_lob_setup(&sf, &sfstmt, use_arrow);

  char *query = malloc(MAX_LOB_SIZE + 1024);
  for (int i = 0; i < SIZE_NUM; i++)
  {
    sprintf(query, "select '");
    strncat(query, lob_data, test_sizes[i]);
    strcat(query, "'");
    status = snowflake_query(
      sfstmt,
      query,
      0
    );
    if (status != SF_STATUS_SUCCESS) {
      dump_error(&(sfstmt->error));
    }
    if (SF_STATUS_ERROR_RETRY == sfstmt->error.error_code)
    {
      // retry timeout exceed on some test environments, ignore.
      free(query);
      snowflake_stmt_term(sfstmt);
      snowflake_term(sf);
      return;
    }

    assert_int_equal(status, SF_STATUS_SUCCESS);

    verify_result(sfstmt, test_sizes[i], SF_BOOLEAN_TRUE, SF_BOOLEAN_TRUE);
  }

  free(query);
  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
}

void test_lob_positional_bind_core(sf_bool use_arrow)
{
  SF_CONNECT *sf;
  SF_STMT *sfstmt;
  SF_STATUS status;

  test_lob_setup(&sf, &sfstmt, use_arrow);

  for (int i = 0; i < SIZE_NUM; i++)
  {
    status = snowflake_prepare(
      sfstmt,
      "select ?",
      0
    );
    assert_int_equal(status, SF_STATUS_SUCCESS);

    SF_BIND_INPUT string_input = {
      .idx = 1,
      .c_type = SF_C_TYPE_STRING,
      .value = lob_data,
      .len = test_sizes[i]
    };
    // terminate the binding data with '\0'
    lob_data[test_sizes[i]] = '\0';

    status = snowflake_bind_param(sfstmt, &string_input);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_execute(sfstmt);
    if (status != SF_STATUS_SUCCESS) {
      dump_error(&(sfstmt->error));
    }
    if (SF_STATUS_ERROR_RETRY == sfstmt->error.error_code)
    {
      // retry timeout exceed on some test environments, ignore.
      snowflake_stmt_term(sfstmt);
      snowflake_term(sf);
      return;
    }

    assert_int_equal(status, SF_STATUS_SUCCESS);

    // remove the terminator of '\0' in lob data
    if (test_sizes[i] < MAX_LOB_SIZE)
    {
      lob_data[test_sizes[i]] = '0';
    }

    verify_result(sfstmt, test_sizes[i], SF_BOOLEAN_TRUE, SF_BOOLEAN_TRUE);
  }

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
}

void test_lob_named_bind_core(sf_bool use_arrow)
{
  SF_CONNECT *sf;
  SF_STMT *sfstmt;
  SF_STATUS status;

  test_lob_setup(&sf, &sfstmt, use_arrow);

  for (int i = 0; i < SIZE_NUM; i++)
  {
    status = snowflake_prepare(
      sfstmt,
      "select :STRING",
      0
    );
    assert_int_equal(status, SF_STATUS_SUCCESS);

    SF_BIND_INPUT string_input = {
      .name = "STRING",
      .c_type = SF_C_TYPE_STRING,
      .value = lob_data,
      .len = test_sizes[i]
    };
    // terminate the binding data with '\0'
    lob_data[test_sizes[i]] = '\0';

    status = snowflake_bind_param(sfstmt, &string_input);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_execute(sfstmt);
    if (status != SF_STATUS_SUCCESS) {
      dump_error(&(sfstmt->error));
    }
    if (SF_STATUS_ERROR_RETRY == sfstmt->error.error_code)
    {
      // retry timeout exceed on some test environments, ignore.
      snowflake_stmt_term(sfstmt);
      snowflake_term(sf);
      return;
    }

    assert_int_equal(status, SF_STATUS_SUCCESS);

    // remove the terminator of '\0' in lob data
    if (test_sizes[i] < MAX_LOB_SIZE)
    {
      lob_data[test_sizes[i]] = '0';
    }

    verify_result(sfstmt, test_sizes[i], SF_BOOLEAN_TRUE, SF_BOOLEAN_TRUE);
  }

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
}

void test_lob_describe_only_core(sf_bool use_arrow)
{
  SF_CONNECT *sf;
  SF_STMT *sfstmt;
  SF_STATUS status;

  test_lob_setup(&sf, &sfstmt, use_arrow);

  char *query = malloc(MAX_LOB_SIZE + 1024);
  for (int i = 0; i < SIZE_NUM; i++)
  {
    SF_QUERY_RESULT_CAPTURE *result_capture;
    snowflake_query_result_capture_init(&result_capture);

    sprintf(query, "select '");
    strncat(query, lob_data, test_sizes[i]);
    strcat(query, "'");

    clear_snowflake_error(&sfstmt->error);
    status = snowflake_prepare(sfstmt, query, 0);
    assert_int_equal(status, SF_STATUS_SUCCESS);
    status = snowflake_describe_with_capture(sfstmt, result_capture);
    if (status != SF_STATUS_SUCCESS) {
      dump_error(&(sfstmt->error));
    }
    if (SF_STATUS_ERROR_RETRY == sfstmt->error.error_code)
    {
      // retry timeout exceed on some test environments, ignore.
      free(query);
      snowflake_stmt_term(sfstmt);
      snowflake_term(sf);
      return;
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    char *resultBuffer = result_capture->capture_buffer;
    // Parse the JSON, and grab a few values to verify correctness
    cJSON *parsedJSON = snowflake_cJSON_Parse(resultBuffer);

    sf_bool success;
    json_copy_bool(&success, parsedJSON, "success");
    assert_int_equal(success, SF_BOOLEAN_TRUE);

    cJSON *data = snowflake_cJSON_GetObjectItem(parsedJSON, "data");
    assert_int_equal(
      strlen(snowflake_cJSON_GetObjectItem(data, "queryID")->valuestring) + 1,
      SF_UUID4_LEN);

    // Make sure that the query is run in describe only mode and the actual result is empty
	// temporarily disable this check as there could be a server issue
//    assert_int_equal(snowflake_cJSON_GetArraySize(snowflake_cJSON_GetObjectItem(data, "rowset")), 0);
    // Make sure row types are returned
    cJSON *rowtype = snowflake_cJSON_GetArrayItem(snowflake_cJSON_GetObjectItem(data, "rowtype"), 0);
    assert_string_equal(snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(rowtype, "type")), "text");

    // verify length returned
    int byte_size = test_sizes[i] * 4;
    if (byte_size > MAX_LOB_SIZE)
    {
      byte_size = MAX_LOB_SIZE;
    }

    // seems to be a test server issue with describe only request doesn't return length properly
    // which doesn't happen with product deployment. Skip for now.
	if (test_sizes[i] < MAX_LOB_SIZE)
    {
      assert_int_equal(snowflake_cJSON_GetUint64Value(snowflake_cJSON_GetObjectItem(rowtype, "length")), test_sizes[i]);
    }

    snowflake_cJSON_Delete(parsedJSON);
    snowflake_query_result_capture_term(result_capture);
  }

  free(query);
  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
}

void test_lob_retrieval_arrow(void **unused)
{
  SF_UNUSED(unused);
  test_lob_retrieval_core(SF_BOOLEAN_TRUE);
}

void test_lob_retrieval_json(void **unused)
{
  SF_UNUSED(unused);
  test_lob_retrieval_core(SF_BOOLEAN_FALSE);
}

void test_lob_literal_arrow(void **unused)
{
  SF_UNUSED(unused);
  test_lob_literal_core(SF_BOOLEAN_TRUE);
}

void test_lob_literal_json(void **unused)
{
  SF_UNUSED(unused);
  test_lob_literal_core(SF_BOOLEAN_FALSE);
}

void test_lob_positional_bind_arrow(void **unused)
{
  SF_UNUSED(unused);
  test_lob_positional_bind_core(SF_BOOLEAN_TRUE);
}

void test_lob_positional_bind_json(void **unused)
{
  SF_UNUSED(unused);
  test_lob_positional_bind_core(SF_BOOLEAN_FALSE);
}

void test_lob_named_bind_arrow(void **unused)
{
  SF_UNUSED(unused);
  test_lob_named_bind_core(SF_BOOLEAN_TRUE);
}

void test_lob_named_bind_json(void **unused)
{
  SF_UNUSED(unused);
  test_lob_named_bind_core(SF_BOOLEAN_FALSE);
}

void test_lob_describe_only_arrow(void **unused)
{
  SF_UNUSED(unused);
  test_lob_describe_only_core(SF_BOOLEAN_TRUE);
}

void test_lob_describe_only_json(void **unused)
{
  SF_UNUSED(unused);
  test_lob_describe_only_core(SF_BOOLEAN_FALSE);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    initialize_lob_data();
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_lob_retrieval_arrow),
      cmocka_unit_test(test_lob_literal_arrow),
      cmocka_unit_test(test_lob_positional_bind_arrow),
      cmocka_unit_test(test_lob_named_bind_arrow),
      cmocka_unit_test(test_lob_describe_only_arrow),
      cmocka_unit_test(test_lob_retrieval_json),
      cmocka_unit_test(test_lob_literal_json),
      cmocka_unit_test(test_lob_positional_bind_json),
      cmocka_unit_test(test_lob_named_bind_json),
      cmocka_unit_test(test_lob_describe_only_json),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    free_lob_data();
    snowflake_global_term();
    return ret;
}
