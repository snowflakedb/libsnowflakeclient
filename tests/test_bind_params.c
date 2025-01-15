/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <string.h>
#include "utils/test_setup.h"
#include "memory.h"

#define INPUT_ARRAY_SIZE 3

void test_bind_parameters(void **unused) {
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

void test_array_binding_core(unsigned int array_size, sf_bool fallback) {
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
    char string_value[] = "str";
    char string_expected_result[] = "str";
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

    SF_BIND_INPUT input_array[8];
    char* expected_results[8];
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
    }

    snowflake_bind_input_init(&int8_input);
    snowflake_bind_input_init(&uint8_input);
    snowflake_bind_input_init(&int64_input);
    snowflake_bind_input_init(&uint64_input);
    snowflake_bind_input_init(&float_input);
    snowflake_bind_input_init(&string_input);
    snowflake_bind_input_init(&binary_input);
    snowflake_bind_input_init(&bool_input);

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

    input_array[0] = int8_input;
    input_array[1] = uint8_input;
    input_array[2] = int64_input;
    input_array[3] = uint64_input;
    input_array[4] = float_input;
    input_array[5] = string_input;
    input_array[6] = binary_input;
    input_array[7] = bool_input;

    expected_results[0] = int8_expected_result;
    expected_results[1] = uint8_expected_result;
    expected_results[2] = int64_expected_result;
    expected_results[3] = uint64_expected_result;
    expected_results[4] = float_expected_result;
    expected_results[5] = string_expected_result;
    expected_results[6] = binary_expected_result;
    expected_results[7] = bool_expected_result;

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
      "create or replace temporary table t (c1 number, c2 number, c3 number, c4 number, c5 float, c6 string, c7 binary, c8 boolean)",
      0
    );
    assert_int_equal(status, SF_STATUS_SUCCESS);

    int64 paramset_size = array_size;
    status = snowflake_stmt_set_attr(stmt, SF_STMT_PARAMSET_SIZE, &paramset_size);
    status = snowflake_prepare(
      stmt,
      "insert into t values(?, ?, ?, ?, ?, ?, ?, ?)",
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
      for (j = 0; j < 8; j++)
      {
        snowflake_column_as_const_str(stmt, j + 1, &result);
        assert_string_equal(result, expected_results[j]);
      }
    }
    snowflake_stmt_term(stmt);
    snowflake_term(sf);
}

void test_array_binding_normal(void** unused) {
    test_array_binding_core(1000, SF_BOOLEAN_FALSE);
}

void test_array_binding_stage(void** unused) {
    test_array_binding_core(100000, SF_BOOLEAN_FALSE);
}

void test_array_binding_stage_fallback(void** unused) {
    test_array_binding_core(100000, SF_BOOLEAN_TRUE);
}

void test_array_binding_supported_false_update(void** unused) {
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

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_bind_parameters),
      cmocka_unit_test(test_array_binding_normal),
      cmocka_unit_test(test_array_binding_stage),
      cmocka_unit_test(test_array_binding_supported_false_update),
      cmocka_unit_test(test_array_binding_supported_false_insert),
      cmocka_unit_test(test_array_binding_supported_false_select),
      cmocka_unit_test(test_array_binding_stage_fallback),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
