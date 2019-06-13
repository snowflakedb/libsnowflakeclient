/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <string.h>
#include "utils/test_setup.h"

#define INPUT_ARRAY_SIZE 3

void test_bind_named_parameters(void **unused) {
    /* init */
    SF_STATUS status;
    SF_BIND_INPUT input_array[INPUT_ARRAY_SIZE];
    int64 input1;
    char input2[1000];
    float64 input3;
    char str[1000];
    int iter = 0;

    /* Connect with all parameters set */
    SF_CONNECT *sf = setup_snowflake_connection();
    status = snowflake_connect(sf);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    if (status != SF_STATUS_SUCCESS)
    {
        goto done;
    }

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

    // Initialize bad bind inputs to test
    // mixing of named and positional parameters

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

    input_array[0].name = "NUMBER";
    input_array[0].c_type = SF_C_TYPE_INT64;
    input_array[0].value = &input1;
    input_array[0].len = sizeof(input1);

    input_array[1].name = "STRING";
    input_array[1].c_type = SF_C_TYPE_STRING;
    input_array[1].value = &input2;
    input_array[1].len = sizeof(input2);

    input_array[2].name = "DOUBLE";
    input_array[2].c_type = SF_C_TYPE_FLOAT64;
    input_array[2].value = &input3;
    input_array[2].len = sizeof(input3);

    status = snowflake_prepare(
            stmt,
            "insert into t values(:NUMBER, :STRING, :DOUBLE)",
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

    // Try to mix named and positional
    status = snowflake_bind_param(stmt, &string_input);
    assert_int_equal(status, SF_STATUS_ERROR_OTHER);

    string_input.name = "STRING";
    string_input.idx = 0;
    status = snowflake_bind_param(stmt, &string_input);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // Set new data to reuse the binding
    input1 = 2;
    strcpy(str, "test2");
    input3 = 2.34;

    status = snowflake_execute(stmt);
    assert_int_equal(status, SF_STATUS_SUCCESS);

done:

    snowflake_stmt_term(stmt);
    snowflake_term(sf);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_bind_named_parameters),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
