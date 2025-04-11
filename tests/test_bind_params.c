/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "utils/test_setup.h"
#include "memory.h"

#define INPUT_ARRAY_SIZE 3

void test_bind_parameters(void **unused) {
    SF_UNUSED(unused);
    /* init */
    SF_STATUS status;
    SF_BIND_INPUT input_array[INPUT_ARRAY_SIZE];
    int64 input1;
    char input2[1000];
    float64 input3;
    char str[1000];
    unsigned int iter = 0;

    /* Connect with all parameters set */
    SF_CONNECT *sf = setup_snowflake_connection();
    status = snowflake_connect(sf);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* Create a statement once and reused */
    SF_STMT *stmt = snowflake_stmt(sf);
    /* NOTE: the numeric type here should fit into int64 otherwise
     * it is taken as a float */
    status = snowflake_query(
      stmt,
      "create or replace table t (c1 number(10,0) not null, c2 string, c3 double)",
      0
    );
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // Initialize bind inputs
    SF_BIND_INPUT string_input = {
      .idx= 2,
      .c_type = SF_C_TYPE_STRING,
      .value = &str,
      .len = sizeof(str)
    };

    for (iter = 0; iter < 3; iter++)
    {
        snowflake_bind_input_init(&input_array[iter]);
    }

    input_array[0].idx = 1;
    input_array[0].c_type = SF_C_TYPE_INT64;
    input_array[0].value = &input1;
    input_array[0].len = sizeof(input1);

    input_array[1].idx = 2;
    input_array[1].c_type = SF_C_TYPE_STRING;
    input_array[1].value = &input2;
    input_array[1].len = sizeof(input2);

    input_array[2].idx = 3;
    input_array[2].c_type = SF_C_TYPE_FLOAT64;
    input_array[2].value = &input3;
    input_array[2].len = sizeof(input3);

    status = snowflake_prepare(
      stmt,
      "insert into t values(?, ?, ?)",
      0
    );
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_bind_param_array(stmt, input_array, INPUT_ARRAY_SIZE);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // Set Data
    input1 = 1;
    strcpy(input2, "test1");
    input3 = 1.23;

    status = snowflake_execute(stmt);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // Bind new input value
    status = snowflake_bind_param(stmt, &string_input);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // Set new data to reuse the binding
    input1 = 2;
    strcpy(str, "test2");
    input3 = 2.34;

    status = snowflake_execute(stmt);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // test with parameters more than 8
    status = snowflake_prepare(
      stmt,
      "select ?,?,?,?,?,?,?,?,?",
      0
    );

    for (string_input.idx = 1; string_input.idx <= 9; string_input.idx++)
    {
      status = snowflake_bind_param(stmt, &string_input);
      assert_int_equal(status, SF_STATUS_SUCCESS);
    }

    status = snowflake_execute(stmt);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    snowflake_stmt_term(stmt);
    snowflake_term(sf);
}

void test_array_binding_core(unsigned int array_size, sf_bool fallback, int64 stage_threshold, sf_bool stage_disable) {
    /* init */
    SF_STATUS status;
    int8* int8_array = NULL;
    int8 int8_value = -12;
    char int8_expected_result[] = "-12";
    uint8* uint8_array = NULL;
    uint8 uint8_value = 12;
    char uint8_expected_result[] = "12";
    int64* int64_array = NULL;
    int64 int64_value = 12345;
    char int64_expected_result[] = "12345";
    uint64* uint64_array = NULL;
    uint64 uint64_value = 12345;
    char uint64_expected_result[] = "12345";
    float64* float_array = NULL;
    float64 float_value = 1.23;
    char float_expected_result[] = "1.23";
    char* string_array = NULL;
    char string_value[] = "\"str\"";
    char string_expected_result[] = "\"str\"";
    int* null_ind_array = NULL;
    char* null_expected_result = NULL;
    unsigned char* binary_array = NULL;
    unsigned char binary_value[] = {0x12, 0x34, 0x56, 0x78};
    char binary_expected_result[] = "12345678";
    sf_bool* bool_array = NULL;
    sf_bool bool_value = SF_BOOLEAN_TRUE;
    char bool_expected_result[] = "1";
    SF_BIND_INPUT int8_input;
    SF_BIND_INPUT uint8_input;
    SF_BIND_INPUT int64_input;
    SF_BIND_INPUT uint64_input;
    SF_BIND_INPUT float_input;
    SF_BIND_INPUT string_input;
    SF_BIND_INPUT binary_input;
    SF_BIND_INPUT bool_input;
    SF_BIND_INPUT null_input;

    SF_BIND_INPUT input_array[9];
    char* expected_results[9];
    unsigned int i = 0, j = 0;

    // initialize bindings with argument
    int8_array = SF_CALLOC(array_size, sizeof(int8_value));
    uint8_array = SF_CALLOC(array_size, sizeof(uint8_value));
    int64_array = SF_CALLOC(array_size, sizeof(int64_value));
    uint64_array = SF_CALLOC(array_size, sizeof(uint64_value));
    float_array = SF_CALLOC(array_size, sizeof(float_value));
    string_array = SF_CALLOC(array_size, sizeof(string_value));
    binary_array = SF_CALLOC(array_size, sizeof(binary_value));
    bool_array = SF_CALLOC(array_size, sizeof(bool_value));
    null_ind_array = SF_CALLOC(array_size, sizeof(int));

    for (i = 0; i < array_size; i++)
    {
        int8_array[i] = int8_value;
        uint8_array[i] = uint8_value;
        int64_array[i] = int64_value;
        uint64_array[i] = uint64_value;
        float_array[i] = float_value;
        memcpy(string_array + sizeof(string_value) * i, string_value, sizeof(string_value));
        memcpy(binary_array + sizeof(binary_value) * i, binary_value, sizeof(binary_value));
        bool_array[i] = bool_value;
        null_ind_array[i] = SF_BIND_LEN_NULL;
    }

    snowflake_bind_input_init(&int8_input);
    snowflake_bind_input_init(&uint8_input);
    snowflake_bind_input_init(&int64_input);
    snowflake_bind_input_init(&uint64_input);
    snowflake_bind_input_init(&float_input);
    snowflake_bind_input_init(&string_input);
    snowflake_bind_input_init(&binary_input);
    snowflake_bind_input_init(&bool_input);
    snowflake_bind_input_init(&null_input);

    int8_input.idx = 1;
    int8_input.c_type = SF_C_TYPE_INT8;
    int8_input.value = int8_array;

    uint8_input.idx = 2;
    uint8_input.c_type = SF_C_TYPE_UINT8;
    uint8_input.value = uint8_array;
    
    int64_input.idx = 3;
    int64_input.c_type = SF_C_TYPE_INT64;
    int64_input.value = int64_array;

    uint64_input.idx = 4;
    uint64_input.c_type = SF_C_TYPE_UINT64;
    uint64_input.value = uint64_array;

    float_input.idx = 5;
    float_input.c_type = SF_C_TYPE_FLOAT64;
    float_input.value = float_array;

    string_input.idx = 6;
    string_input.c_type = SF_C_TYPE_STRING;
    string_input.value = string_array;
    string_input.len = sizeof(string_value);

    binary_input.idx = 7;
    binary_input.c_type = SF_C_TYPE_BINARY;
    binary_input.value = binary_array;
    binary_input.len = sizeof(binary_value);

    bool_input.idx = 8;
    bool_input.c_type = SF_C_TYPE_BOOLEAN;
    bool_input.value = bool_array;

    null_input.idx = 9;
    null_input.c_type = SF_C_TYPE_STRING;
    null_input.value = NULL;
    null_input.len_ind = null_ind_array;

    input_array[0] = int8_input;
    input_array[1] = uint8_input;
    input_array[2] = int64_input;
    input_array[3] = uint64_input;
    input_array[4] = float_input;
    input_array[5] = string_input;
    input_array[6] = binary_input;
    input_array[7] = bool_input;
    input_array[8] = null_input;

    expected_results[0] = int8_expected_result;
    expected_results[1] = uint8_expected_result;
    expected_results[2] = int64_expected_result;
    expected_results[3] = uint64_expected_result;
    expected_results[4] = float_expected_result;
    expected_results[5] = string_expected_result;
    expected_results[6] = binary_expected_result;
    expected_results[7] = bool_expected_result;
    expected_results[8] = null_expected_result;

    /* Connect with all parameters set */
    SF_CONNECT* sf = setup_snowflake_connection();
    // turn on FAIL_OPEN to around certificate issue with GCP
    sf_bool value = SF_BOOLEAN_TRUE;
    snowflake_set_attribute(sf, SF_CON_OCSP_FAIL_OPEN, &value);
    status = snowflake_connect(sf);
    assert_int_equal(status, SF_STATUS_SUCCESS);
    if (stage_threshold > 0)
    {
        int64* cur_threshold;
        snowflake_set_attribute(sf, SF_CON_STAGE_BIND_THRESHOLD, (void*)&stage_threshold);
        snowflake_get_attribute(sf, SF_CON_STAGE_BIND_THRESHOLD, (void**)&cur_threshold);
        assert_int_equal(*cur_threshold, stage_threshold);
    }
    sf_bool* cur_stage_disabled;
    if (stage_disable)
    {
        snowflake_set_attribute(sf, SF_CON_DISABLE_STAGE_BIND, (void*)&stage_disable);
        snowflake_get_attribute(sf, SF_CON_DISABLE_STAGE_BIND, (void**)&cur_stage_disabled);
        assert_int_equal(*cur_stage_disabled, SF_BOOLEAN_TRUE);
    }

    /* Create a statement once and reused */
    SF_STMT* stmt = snowflake_stmt(sf);
    status = snowflake_query(
      stmt,
      "create or replace temporary table t (c1 number, c2 number, c3 number, c4 number, c5 float, c6 string, c7 binary, c8 boolean, c9 string)",
      0
    );
    assert_int_equal(status, SF_STATUS_SUCCESS);

    int64 paramset_size = array_size;
    status = snowflake_stmt_set_attr(stmt, SF_STMT_PARAMSET_SIZE, &paramset_size);
    status = snowflake_prepare(
      stmt,
      "insert into t values(?, ?, ?, ?, ?, ?, ?, ?, ?)",
      0
    );
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_bind_param_array(stmt, input_array, sizeof(input_array) / sizeof(SF_BIND_INPUT));
    assert_int_equal(status, SF_STATUS_SUCCESS);

    if (fallback)
    {
      // set invalid proxy for storage endpoints to fail put command
      sf_setenv("https_proxy", "a.b.c");
      sf_setenv("no_proxy", ".snowflakecomputing.com");
    }
    status = snowflake_execute(stmt);
    if (status != SF_STATUS_SUCCESS)
    {
      dump_error(&stmt->error);
    }
    if (fallback)
    {
      // stage disabled after fallback
      snowflake_get_attribute(sf, SF_CON_DISABLE_STAGE_BIND, (void**)&cur_stage_disabled);
      assert_int_equal(*cur_stage_disabled, SF_BOOLEAN_TRUE);
      sf_unsetenv("https_proxy");
      sf_unsetenv("no_proxy");
      // in a low chance the insert query could take time on server side and
      // turn into async and need to access storage endpoint to get the result
      // Ignore the error in such case.
      if (status == SF_STATUS_ERROR_CURL)
      {
        fprintf(stderr, "test_array_binding_stage_fallback: insert query impacted by invalid proxy, skip.\n");
        return;
      }
    }

    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_affected_rows(stmt), array_size);

    status = snowflake_query(stmt, "select * from t", 0);
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(stmt), array_size);

    for (i = 0; i < array_size; i++)
    {
      status = snowflake_fetch(stmt);
      if (status != SF_STATUS_SUCCESS) {
        dump_error(&(stmt->error));
      }
      assert_int_equal(status, SF_STATUS_SUCCESS);
      const char* result = NULL;
      for (j = 0; j < 9; j++)
      {
        snowflake_column_as_const_str(stmt, j + 1, &result);
        if (expected_results[j])
        {
          assert_string_equal(result, expected_results[j]);
        }
        else
        {
          assert_ptr_equal(result, expected_results[j]);
        }
      }
    }
    snowflake_stmt_term(stmt);
    snowflake_term(sf);
}

void test_array_binding_normal(void** unused) {
    SF_UNUSED(unused);
    test_array_binding_core(1000, SF_BOOLEAN_FALSE, 0, SF_BOOLEAN_FALSE);
}

void test_array_binding_stage(void** unused) {
    SF_UNUSED(unused);
    test_array_binding_core(100000, SF_BOOLEAN_FALSE, 0, SF_BOOLEAN_FALSE);
}

void test_array_binding_stage_fallback(void** unused) {
    SF_UNUSED(unused);
    // Azure SDK take too long (14min) to fail with invalid proxy
    char* cenv = getenv("CLOUD_PROVIDER");
    if (cenv && !strncmp(cenv, "AZURE", 5))
    {
      printf("Skipping - fallback test take too long on Azure\n");
      return;
    }
    test_array_binding_core(100000, SF_BOOLEAN_TRUE, 0, SF_BOOLEAN_FALSE);
}

void test_array_binding_threshold(void** unused) {
  SF_UNUSED(unused);
  test_array_binding_core(1000, SF_BOOLEAN_FALSE, 500, SF_BOOLEAN_FALSE);
}

// test threshold with fallback so we can ensure stage binding
// is actually used with lower threshold and disabled after fallback
void test_array_binding_threshold_fallback(void** unused) {
  SF_UNUSED(unused);
  // Azure SDK take too long (14min) to fail with invalid proxy
  char* cenv = getenv("CLOUD_PROVIDER");
  if (cenv && !strncmp(cenv, "AZURE", 5))
  {
      printf("Skipping - fallback test take too long on Azure\n");
      return;
  }
  test_array_binding_core(1000, SF_BOOLEAN_TRUE, 500, SF_BOOLEAN_FALSE);
}

void test_array_binding_disable(void** unused) {
  SF_UNUSED(unused);
  test_array_binding_core(1000, SF_BOOLEAN_FALSE, 500, SF_BOOLEAN_TRUE);
}

void test_array_binding_supported_false_update(void** unused) {
    SF_UNUSED(unused);
    SF_STATUS status;
    char bind_data_b[2][4] = { "2.3", "3.4" };
    char bind_data_c[2][7] = { "bind", "insert" };
    char bind_data_d[2][11] = { "2001-10-12",
                                "2001-10-12" };
    char bind_data_a[2][2] = { "2", "3" };

    SF_BIND_INPUT input_a;
    SF_BIND_INPUT input_b;
    SF_BIND_INPUT input_c;
    SF_BIND_INPUT input_d;

    SF_BIND_INPUT input_array[4];

    snowflake_bind_input_init(&input_a);
    snowflake_bind_input_init(&input_b);
    snowflake_bind_input_init(&input_c);
    snowflake_bind_input_init(&input_d);

    input_b.idx = 1;
    input_b.c_type = SF_C_TYPE_STRING;
    input_b.value = bind_data_b;
    input_b.len = 4;

    input_c.idx = 2;
    input_c.c_type = SF_C_TYPE_STRING;
    input_c.value = bind_data_c;
    input_c.len = 7;

    input_d.idx = 3;
    input_d.c_type = SF_C_TYPE_STRING;
    input_d.value = bind_data_d;
    input_d.len = 11;

    input_a.idx = 4;
    input_a.c_type = SF_C_TYPE_STRING;
    input_a.value = bind_data_a;
    input_a.len = 2;

    input_array[0] = input_b;
    input_array[1] = input_c;
    input_array[2] = input_d;
    input_array[3] = input_a;

    /* Connect with all parameters set */
    SF_CONNECT* sf = setup_snowflake_connection();
	// turn on FAIL_OPEN to around certificate issue with GCP
    sf_bool value = SF_BOOLEAN_TRUE;
    snowflake_set_attribute(sf, SF_CON_OCSP_FAIL_OPEN, &value);
    status = snowflake_connect(sf);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* Create a statement once and reused */
    SF_STMT* stmt = snowflake_stmt(sf);
    status = snowflake_query(
        stmt,
        "create or replace temporary table foo1(a int, b double, c string, d date)",
        0
    );
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_query(
      stmt,
      "insert into foo1 values (2, NULL, NULL, NULL), (3, NULL, NULL, NULL)",
      0
    );
    assert_int_equal(status, SF_STATUS_SUCCESS);

    int64 paramset_size = 2;
    status = snowflake_stmt_set_attr(stmt, SF_STMT_PARAMSET_SIZE, &paramset_size);
    status = snowflake_prepare(
      stmt,
      "update foo1 set b = ?, c = ?, d = ? where a = ?",
      0
    );
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_bind_param_array(stmt, input_array, sizeof(input_array) / sizeof(SF_BIND_INPUT));
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_execute(stmt);
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_affected_rows(stmt), 2);

    status = snowflake_query(stmt, "select * from foo1", 0);
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(stmt), 2);

    for (int i = 0; i < 2; i++)
    {
      status = snowflake_fetch(stmt);
      if (status != SF_STATUS_SUCCESS) {
        dump_error(&(stmt->error));
      }
      assert_int_equal(status, SF_STATUS_SUCCESS);
      char* result = NULL;
      size_t value_len = 0;
      size_t max_value_size = 0;

      snowflake_column_as_str(stmt, 1, &result, &value_len, &max_value_size);
      assert_string_equal(result, bind_data_a[i]);
      snowflake_column_as_str(stmt, 2, &result, &value_len, &max_value_size);
      assert_string_equal(result, bind_data_b[i]);
      snowflake_column_as_str(stmt, 3, &result, &value_len, &max_value_size);
      assert_string_equal(result, bind_data_c[i]);
      snowflake_column_as_str(stmt, 4, &result, &value_len, &max_value_size);
      assert_string_equal(result, bind_data_d[i]);

      free(result);
    }

    snowflake_stmt_term(stmt);
    snowflake_term(sf);
}

void test_array_binding_supported_false_insert(void** unused) {
    SF_UNUSED(unused);

// SNOW-1878297 TODO: disable for now due to server issue.
// Sever returns arrayBindSupported=true while it's not really supported.
    return;

    SF_STATUS status;
    char bind_data[2][2] = { "1", "" };

    SF_BIND_INPUT bind_input;

    snowflake_bind_input_init(&bind_input);

    bind_input.idx = 1;
    bind_input.c_type = SF_C_TYPE_STRING;
    bind_input.value = bind_data;
    bind_input.len = 2;

    /* Connect with all parameters set */
    SF_CONNECT* sf = setup_snowflake_connection();
    status = snowflake_connect(sf);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* Create a statement once and reused */
    SF_STMT* stmt = snowflake_stmt(sf);
    status = snowflake_query(
        stmt,
        "create or replace temporary table foo1(a string)",
        0
    );
    assert_int_equal(status, SF_STATUS_SUCCESS);

    int64 paramset_size = 2;
    status = snowflake_stmt_set_attr(stmt, SF_STMT_PARAMSET_SIZE, &paramset_size);
    status = snowflake_prepare(
        stmt,
        "INSERT INTO foo1 (a) VALUES (NULLIF(?, ''))",
        0
    );
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_bind_param(stmt, &bind_input);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_execute(stmt);
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_affected_rows(stmt), 2);

    status = snowflake_query(stmt, "select * from foo1", 0);
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(stmt), 2);

    snowflake_stmt_term(stmt);
    snowflake_term(sf);
}

void test_array_binding_supported_false_select(void** unused) {
    SF_UNUSED(unused);
    SF_STATUS status;
    char bind_data[2][2] = { "1", "2" };

    SF_BIND_INPUT bind_input;

    snowflake_bind_input_init(&bind_input);

    bind_input.idx = 1;
    bind_input.c_type = SF_C_TYPE_STRING;
    bind_input.value = bind_data;
    bind_input.len = 2;

    /* Connect with all parameters set */
    SF_CONNECT* sf = setup_snowflake_connection();
    status = snowflake_connect(sf);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* Create a statement once and reused */
    SF_STMT* stmt = snowflake_stmt(sf);

    int64 paramset_size = 2;
    status = snowflake_stmt_set_attr(stmt, SF_STMT_PARAMSET_SIZE, &paramset_size);
    status = snowflake_prepare(stmt, "select ?", 0);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_bind_param(stmt, &bind_input);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // expected failure for non-dml query with array binding unsupported.
    status = snowflake_execute(stmt);
    assert_int_equal(status, SF_STATUS_ERROR_GENERAL);

    snowflake_stmt_term(stmt);
    snowflake_term(sf);
}

const int8 array_size = 5;
void execute_insert_query_with_two_different_bindings(SF_STMT* stmt, char* timezone){
    /* init */
    SF_STATUS status;
    int8 id_array[5] = { 1,2,3,4,5 };
    char timestamp_array[5][31] = { "0001-01-01 23:24:25.987000000", "9999-12-30 23:24:25.9870000000","2025-04-08 11:37:25.3260000000","2024-06-22 23:37:25.5200000000", "2000-01-01 00:00:00.000000000" };

    SF_BIND_INPUT id_input;
    SF_BIND_INPUT NTZ_input;
    SF_BIND_INPUT TZ_input;
    SF_BIND_INPUT LTZ_input;

    SF_BIND_INPUT input_array[4];

    snowflake_bind_input_init(&id_input);
    snowflake_bind_input_init(&NTZ_input);
    snowflake_bind_input_init(&TZ_input);
    snowflake_bind_input_init(&LTZ_input);

    id_input.idx = 1;
    id_input.c_type = SF_C_TYPE_UINT8;
    id_input.value = id_array;
    id_input.len = sizeof(int8);

    NTZ_input.idx = 2;
    NTZ_input.c_type = SF_C_TYPE_STRING;
    NTZ_input.value = timestamp_array;
    NTZ_input.len = 31;

    TZ_input.idx = 3;
    TZ_input.c_type = SF_C_TYPE_STRING;
    TZ_input.value = timestamp_array;
    TZ_input.len = 31;

    LTZ_input.idx = 4;
    LTZ_input.c_type = SF_C_TYPE_STRING;
    LTZ_input.value = timestamp_array;
    LTZ_input.len = 31;


    input_array[0] = id_input;
    input_array[1] = NTZ_input;
    input_array[2] = TZ_input;
    input_array[3] = LTZ_input;


    /* Create a statement once and reused */

    char* alter_command = "alter session set TIMEZONE = '";
    char* timezone_query = (char*)SF_CALLOC(1,strlen(alter_command) + strlen(timezone) + 2);
    strcpy(timezone_query, alter_command);
    strcat(timezone_query, timezone);
    strcat(timezone_query, "'");

    status = snowflake_query(
        stmt,
        timezone_query,
        0
    );
    assert_int_equal(status, SF_STATUS_SUCCESS);

     status = snowflake_query(
        stmt,
         "create or replace table ARRAYBINDINSERT (c1 INT, c2 TIMESTAMP_LTZ, c3 TIMESTAMP_LTZ, c4 TIMESTAMP_LTZ)",

        //"create or replace table ARRAYBINDINSERT (c1 INT, c2 TIMESTAMP_NTZ, c3 TIMESTAMP_TZ, c4 TIMESTAMP_LTZ)",
        0
    );
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_query(
        stmt,
        //"create or replace table STAGEBINDINSERT (c1 INT, c2 TIMESTAMP_NTZ, c3 TIMESTAMP_TZ, c4 TIMESTAMP_LTZ)",
        "create or replace table STAGEBINDINSERT (c1 INT, c2 TIMESTAMP_LTZ, c3 TIMESTAMP_LTZ, c4 TIMESTAMP_LTZ)",

        0
    );
    assert_int_equal(status, SF_STATUS_SUCCESS);

    int64 paramset_size = array_size;
    status = snowflake_stmt_set_attr(stmt, SF_STMT_PARAMSET_SIZE, &paramset_size);
    status = snowflake_prepare(
        stmt,
        "insert into ARRAYBINDINSERT values(?, ?, ?, ?)",
        0
    );
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_bind_param_array(stmt, input_array, sizeof(input_array) / sizeof(SF_BIND_INPUT));
    assert_int_equal(status, SF_STATUS_SUCCESS);
    status = snowflake_execute(stmt);
    if (status != SF_STATUS_SUCCESS)
    {
        dump_error(&stmt->error);
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_affected_rows(stmt), array_size);

    int64 stage_treshold = 1;
    int64* cur_threshold;
    snowflake_set_attribute(stmt->connection, SF_CON_STAGE_BIND_THRESHOLD, (void*)&stage_treshold);
    snowflake_get_attribute(stmt->connection, SF_CON_STAGE_BIND_THRESHOLD, (void**)&cur_threshold);
    assert_int_equal(*cur_threshold, stage_treshold);
    status = snowflake_prepare(
        stmt,
        "insert into STAGEBINDINSERT values(?, ?, ?, ?)",
        0
    );
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_bind_param_array(stmt, input_array, sizeof(input_array) / sizeof(SF_BIND_INPUT));
    assert_int_equal(status, SF_STATUS_SUCCESS);
    status = snowflake_execute(stmt);
    if (status != SF_STATUS_SUCCESS)
    {
        dump_error(&stmt->error);
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_affected_rows(stmt), array_size);

    SF_FREE(timezone_query);
}

void get_time_difference_from_timezone(int* offset_hours, int* offset_minutes) {
time_t now = time(NULL);

struct tm local_tm = *localtime(&now);
struct tm utc_tm = *gmtime(&now);

time_t local_time = mktime(&local_tm);
time_t utc_time = mktime(&utc_tm);

double offset_seconds = difftime(local_time, utc_time);

*offset_hours = (int)(offset_seconds / 3600);
*offset_minutes = (int)(offset_seconds / 60) % 60;
}

int days_in_month(int year, int month) {
    // month: 1~12
    int dim[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if (month == 2) {
        if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
            return 29;
    }
    return dim[month - 1];
}

void subtract_offset_custom(int* year, int* month, int* day,
    int* hour, int* minute, int* second,
    int offset_hours, int offset_minutes) {
    int total_minutes = (*hour) * 60 + (*minute) - (offset_hours * 60 + offset_minutes);

    while (total_minutes < 0) {
        total_minutes += 1440;
        (*day)--;

        if (*day < 1) {
            (*month)--;
            if (*month < 1) {
                (*month) = 12;
                (*year)--;
            }
            *day = days_in_month(*year, *month);
        }
    }

    *hour = total_minutes / 60;
    *minute = total_minutes % 60;
}

void convert_to_local_timezone(char* input) {
    int year, month, day, hour, min, sec, micro;
    sscanf(input,
        "%d-%d-%d %d:%d:%d.%3d",
        &year, &month, &day, &hour, &min, &sec, &micro);

    subtract_offset_custom(&year, &month, &day, &hour, &min, &sec, 9, 0);

#ifdef __linux__
    char* format = "%d-%02d-%02d %02d:%02d:%02d.%03d000000";
#else
    char* format = "%04d-%02d-%02d %02d:%02d:%02d.%03d000000";
#endif
    snprintf(input, 31, format,
        year, month, day, hour, min, sec, micro);

}


void test_verify_data_types_with_two_different_binding_UTC(void** unused) {
    sf_setenv("TZ", "UTC");
    sf_tzset();
    char expected_id_restuls[5][2] = { "1", "2","3","4", "5" };
#ifdef __linux__
    char expected_ntz_results[5][31] = { "1-01-01 23:24:25.987000000", "9999-12-30 23:24:25.987000000","2025-04-08 11:37:25.326000000","2024-06-22 23:37:25.520000000", "2000-01-01 00:00:00.000000000" };
    char expected_tz_results[5][38] = { "1-01-01 23:24:25.987000000 -00:00", "9999-12-30 23:24:25.987000000 -00:00","2025-04-08 11:37:25.326000000 -00:00","2024-06-22 23:37:25.520000000 -00:00", "2000-01-01 00:00:00.000000000 -00:00" };
    char expected_ltz_results[5][31] = { "1-01-01 23:24:25.987000000", "9999-12-30 23:24:25.987000000","2025-04-08 11:37:25.326000000","2024-06-22 23:37:25.520000000", "2000-01-01 00:00:00.000000000" };
#else
    char expected_ntz_results[5][31] = { "0001-01-01 23:24:25.987000000", "9999-12-30 23:24:25.987000000","2025-04-08 11:37:25.326000000","2024-06-22 23:37:25.520000000", "2000-01-01 00:00:00.000000000" };
    char expected_tz_results[5][38] = { "0001-01-01 23:24:25.987000000 -00:00", "9999-12-30 23:24:25.987000000 -00:00","2025-04-08 11:37:25.326000000 -00:00","2024-06-22 23:37:25.520000000 -00:00", "2000-01-01 00:00:00.000000000 -00:00"};
    char expected_ltz_results[5][31] = { "0001-01-01 23:24:25.987000000", "9999-12-30 23:24:25.987000000","2025-04-08 11:37:25.326000000","2024-06-22 23:37:25.520000000", "2000-01-01 00:00:00.000000000" };
#endif
    /* Connect with all parameters set */
    SF_STATUS status;
    SF_CONNECT* sf = setup_snowflake_connection();
    // turn on FAIL_OPEN to around certificate issue with GCP
    sf_bool value = SF_BOOLEAN_TRUE;
    snowflake_set_attribute(sf, SF_CON_OCSP_FAIL_OPEN, &value);
    if (snowflake_connect(sf) != SF_STATUS_SUCCESS)
    {
        dump_error(&sf->error);
    }
    SF_STMT* stmt = snowflake_stmt(sf);
    execute_insert_query_with_two_different_bindings(stmt, "UTC");

    status = snowflake_query(stmt, "select * from ARRAYBINDINSERT order by c1", 0);
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(stmt), array_size);

    for (int8 i = 0; i < array_size; i++)
    {
        status = snowflake_fetch(stmt);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(stmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);
        char* result = NULL;
        size_t value_len = 0;
        size_t max_value_size = 0;

        snowflake_column_as_str(stmt, 1, &result, &value_len, &max_value_size);
        assert_string_equal(result, expected_id_restuls[i]);
        snowflake_column_as_str(stmt, 2, &result, &value_len, &max_value_size);
        assert_string_equal(result, expected_ntz_results[i]);
        snowflake_column_as_str(stmt, 3, &result, &value_len, &max_value_size);
        assert_string_equal(result, expected_tz_results[i]);
        snowflake_column_as_str(stmt, 4, &result, &value_len, &max_value_size);
        assert_string_equal(result, expected_ltz_results[i]);
        free(result);
    }

    status = snowflake_query(stmt, "select * from ARRAYBINDINSERT order by c1", 0);
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(stmt), array_size);

    for (int8 i = 0; i < array_size; i++)
    {
        status = snowflake_fetch(stmt);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(stmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);
        char* result = NULL;
        size_t value_len = 0;
        size_t max_value_size = 0;

        snowflake_column_as_str(stmt, 1, &result, &value_len, &max_value_size);
        assert_string_equal(result, expected_id_restuls[i]);
        snowflake_column_as_str(stmt, 2, &result, &value_len, &max_value_size);
        assert_string_equal(result, expected_ntz_results[i]);
        snowflake_column_as_str(stmt, 3, &result, &value_len, &max_value_size);
        assert_string_equal(result, expected_tz_results[i]);
        snowflake_column_as_str(stmt, 4, &result, &value_len, &max_value_size);
        assert_string_equal(result, expected_ltz_results[i]);
        free(result);
    }

    snowflake_stmt_term(stmt);
    snowflake_term(sf);
}

void test_verify_data_types_with_two_different_binding_EUROPE_WARSAW(void** unused) {
    sf_setenv("TZ", "UTC");
    sf_tzset();
    char expected_id_restuls[5][2] = { "1", "2","3","4", "5" };

#ifdef __linux__
    char expected_ntz_results[5][31] = { "1-01-01 23:24:25.987000000", "9999-12-30 23:24:25.987000000","2025-04-08 11:37:25.326000000","2024-06-22 23:37:25.520000000", "2000-01-01 00:00:00.000000000" };
    char expected_tz_results[5][38] = { "1-01-01 23:24:25.987000000 +01:24", "9999-12-30 23:24:25.987000000 +01:00","2025-04-08 11:37:25.326000000 +02:00","2024-06-22 23:37:25.520000000 +02:00", "2000-01-01 00:00:00.000000000 +01:00" };
    char expected_ltz_results[5][31] = { "1-01-01 23:24:25.987000000", "9999-12-30 23:24:25.987000000","2025-04-08 11:37:25.326000000","2024-06-22 23:37:25.520000000", "2000-01-01 00:00:00.000000000" };
#else
    char expected_ntz_results[5][31] = { "0001-01-01 23:24:25.987000000", "9999-12-30 23:24:25.987000000","2025-04-08 11:37:25.326000000","2024-06-22 23:37:25.520000000", "2000-01-01 00:00:00.000000000" };
    char expected_tz_results[5][38] = { "0001-01-01 23:24:25.987000000 +01:24", "9999-12-30 23:24:25.987000000 +01:00","2025-04-08 11:37:25.326000000 +02:00","2024-06-22 23:37:25.520000000 +02:00", "2000-01-01 00:00:00.000000000 +01:00" };
    char expected_ltz_results[5][31] = { "0001-01-02 00:24:25.987000000", "9999-12-30 23:24:25.987000000","2025-04-08 11:37:25.326000000","2024-06-22 23:37:25.520000000", "2000-01-01 00:00:00.000000000" };
#endif

    for (int i = 0; i < 5; i++) {
        convert_to_local_timezone(expected_ltz_results[i]);
    }

    /* Connect with all parameters set */
    SF_STATUS status;
    SF_CONNECT* sf = setup_snowflake_connection();
    // turn on FAIL_OPEN to around certificate issue with GCP
    sf_bool value = SF_BOOLEAN_TRUE;
    snowflake_set_attribute(sf, SF_CON_OCSP_FAIL_OPEN, &value);
    if (snowflake_connect(sf) != SF_STATUS_SUCCESS)
    {
        dump_error(&sf->error);
    }
    SF_STMT* stmt = snowflake_stmt(sf);
    execute_insert_query_with_two_different_bindings(stmt, "Europe/Warsaw");

    status = snowflake_query(stmt, "select * from ARRAYBINDINSERT order by c1", 0);
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(stmt), array_size);

    for (int8 i = 0; i < array_size; i++)
    {
        status = snowflake_fetch(stmt);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(stmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);
        char* result = NULL;
        int8 id;
        size_t value_len = 0;
        size_t max_value_size = 0;

        snowflake_column_as_str(stmt, 1, &result, &value_len, &max_value_size);
        assert_string_equal(result, expected_id_restuls[i]);
        snowflake_column_as_str(stmt, 2, &result, &value_len, &max_value_size);
        assert_string_equal(result, expected_ntz_results[i]);
        snowflake_column_as_str(stmt, 3, &result, &value_len, &max_value_size);
        assert_string_equal(result, expected_tz_results[i]);
        free(result);
    }

    status = snowflake_query(stmt, "select * from ARRAYBINDINSERT order by c1", 0);
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(stmt), array_size);

    for (int8 i = 0; i < array_size; i++)
    {
        status = snowflake_fetch(stmt);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(stmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);
        char* result = NULL;
        size_t value_len = 0;
        size_t max_value_size = 0;

        snowflake_column_as_str(stmt, 1, &result, &value_len, &max_value_size);
        assert_string_equal(result, expected_id_restuls[i]);
        snowflake_column_as_str(stmt, 2, &result, &value_len, &max_value_size);
        assert_string_equal(result, expected_ntz_results[i]);
        snowflake_column_as_str(stmt, 3, &result, &value_len, &max_value_size);
        assert_string_equal(result, expected_tz_results[i]);
        snowflake_column_as_str(stmt, 4, &result, &value_len, &max_value_size);
        assert_string_equal(result, expected_ltz_results[i]);
        free(result);
    }

    snowflake_stmt_term(stmt);
    snowflake_term(sf);
}

void test_verify_data_types_with_two_different_binding_TOKYO(void** unused) {
    sf_setenv("TZ", "UTC");
    sf_tzset();
    char expected_id_restuls[5][2] = { "1", "2","3","4", "5" };

#ifdef __linux__
    char expected_ntz_results[5][31] = { "1-01-01 23:24:25.987000000", "9999-12-30 23:24:25.987000000","2025-04-08 11:37:25.326000000","2024-06-22 23:37:25.520000000", "2000-01-01 00:00:00.000000000" };
    char expected_tz_results[5][38] = { "1-01-01 23:23:26.987000000 +09:18", "9999-12-30 23:24:25.987000000 +09:00","2025-04-08 11:37:25.326000000 +09:00","2024-06-22 23:37:25.520000000 +09:00", "2000-01-01 00:00:00.000000000 +09:00" };
    char expected_ltz_results[5][31] = { "1-01-01 23:24:25.987000000", "9999-12-30 23:24:25.987000000","2025-04-08 11:37:25.326000000","2024-06-22 23:37:25.520000000", "2024-06-22 23:37:25.520000000" };
#else
    char expected_ntz_results[5][31] = { "0001-01-01 23:24:25.987000000", "9999-12-30 23:24:25.987000000","2025-04-08 11:37:25.326000000","2024-06-22 23:37:25.520000000", "2000-01-01 00:00:00.000000000" };
    char expected_tz_results[5][38] = { "0001-01-01 23:23:26.987000000 +09:18", "9999-12-30 23:24:25.987000000 +09:00","2025-04-08 11:37:25.326000000 +09:00","2024-06-22 23:37:25.520000000 +09:00", "2000-01-01 00:00:00.000000000 +09:00" };
    char expected_ltz_results[5][31] = { "0001-01-01 23:05:26.987000000", "9999-12-30 23:24:25.987000000","2025-04-08 11:37:25.326000000","2024-06-22 23:37:25.520000000", "2000-01-01 00:00:00.000000000" };
#endif
    for (int i = 0; i < 5; i++) {
        convert_to_local_timezone(expected_ltz_results[i]);
    }

    /* Connect with all parameters set */
    SF_STATUS status;
    SF_CONNECT* sf = setup_snowflake_connection();
    // turn on FAIL_OPEN to around certificate issue with GCP
    sf_bool value = SF_BOOLEAN_TRUE;
    snowflake_set_attribute(sf, SF_CON_OCSP_FAIL_OPEN, &value);
    if (snowflake_connect(sf) != SF_STATUS_SUCCESS)
    {
        dump_error(&sf->error);
    }
    SF_STMT* stmt = snowflake_stmt(sf);
    execute_insert_query_with_two_different_bindings(stmt, "Asia/Tokyo");

    status = snowflake_query(stmt, "select * from ARRAYBINDINSERT order by c1", 0);
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(stmt), array_size);

    for (int8 i = 0; i < array_size; i++)
    {
        status = snowflake_fetch(stmt);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(stmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);
        char* result = NULL;
        size_t value_len = 0;
        size_t max_value_size = 0;

        snowflake_column_as_str(stmt, 1, &result, &value_len, &max_value_size);
        assert_string_equal(result, expected_id_restuls[i]);
        snowflake_column_as_str(stmt, 2, &result, &value_len, &max_value_size);
        assert_string_equal(result, expected_ntz_results[i]);
        snowflake_column_as_str(stmt, 3, &result, &value_len, &max_value_size);
        assert_string_equal(result, expected_tz_results[i]);
        snowflake_column_as_str(stmt, 4, &result, &value_len, &max_value_size);
        assert_string_equal(result, expected_ltz_results[i]);
        free(result);
    }

    status = snowflake_query(stmt, "select * from ARRAYBINDINSERT order by c1", 0);
    assert_int_equal(status, SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(stmt), array_size);

    for (int8 i = 0; i < array_size; i++)
    {
        status = snowflake_fetch(stmt);
        if (status != SF_STATUS_SUCCESS) {
            dump_error(&(stmt->error));
        }
        assert_int_equal(status, SF_STATUS_SUCCESS);
        char* result = NULL;
        size_t value_len = 0;
        size_t max_value_size = 0;

        snowflake_column_as_str(stmt, 1, &result, &value_len, &max_value_size);
        assert_string_equal(result, expected_id_restuls[i]);
        snowflake_column_as_str(stmt, 2, &result, &value_len, &max_value_size);
        assert_string_equal(result, expected_ntz_results[i]);
        snowflake_column_as_str(stmt, 3, &result, &value_len, &max_value_size);
        assert_string_equal(result, expected_tz_results[i]);
        snowflake_column_as_str(stmt, 4, &result, &value_len, &max_value_size);
        assert_string_equal(result, expected_ltz_results[i]);
        free(result);
    }

    snowflake_stmt_term(stmt);
    snowflake_term(sf);
}

void testing(void** unused)
{
    char expected_ltz_results[5][31] = { "0001-01-01 23:24:25.987000000", "9999-12-30 23:24:25.987000000","2025-04-08 11:37:25.326000000","2024-06-22 23:37:25.520000000", "2024-06-22 23:37:25.520000000" };

    for (int i = 0; i < 5; i++)
    {
        convert_to_local_timezone(expected_ltz_results[i]);
        sf_printf("%s", expected_ltz_results[i]);
    }

}


int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      //cmocka_unit_test(test_bind_parameters),
      //cmocka_unit_test(test_array_binding_normal),
      //cmocka_unit_test(test_array_binding_stage),
      //cmocka_unit_test(test_array_binding_supported_false_update),
      //cmocka_unit_test(test_array_binding_supported_false_insert),
      //cmocka_unit_test(test_array_binding_supported_false_select),
      //cmocka_unit_test(test_array_binding_stage_fallback),
      //cmocka_unit_test(test_array_binding_threshold),
      //cmocka_unit_test(test_array_binding_threshold_fallback),
      //cmocka_unit_test(test_array_binding_disable),
      //cmocka_unit_test(test_verify_data_types_with_two_different_binding_UTC),
      cmocka_unit_test(test_verify_data_types_with_two_different_binding_EUROPE_WARSAW),
      //cmocka_unit_test(test_verify_data_types_with_two_different_binding_TOKYO),
      //cmocka_unit_test(testing),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
