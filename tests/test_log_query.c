#include "utils/test_setup.h"
#include <snowflake/client_config_parser.h>
#include "log_file_util.h"
#include <stdio.h>
#include <stdlib.h>

SF_CONNECT* validate_connection(sf_bool log_query_text, sf_bool log_query_parameters)
{
    SF_CONNECT* sf = setup_snowflake_connection();
    sf->log_query_text = log_query_text;
    sf->log_query_parameters = log_query_parameters;
    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    return sf;
}

void validate_log_file(FILE* fp, const char* query_text, sf_bool log_query_text, sf_bool log_query_parameters)
{
    sf_bool found_query_text = SF_BOOLEAN_FALSE;
    sf_bool masked_sql_found = SF_BOOLEAN_FALSE;
    sf_bool found_query_parameters = SF_BOOLEAN_FALSE;
    sf_bool masked_query_param_found = SF_BOOLEAN_FALSE;

    char line[1024];

    fseek(fp, 0, SEEK_SET);
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, query_text) != NULL)
        {
            found_query_text = SF_BOOLEAN_TRUE;
        }

        if (strstr(line, "sqlText\":	\"****\"") != NULL)
        {
            masked_sql_found = SF_BOOLEAN_TRUE;
        }

        if (strstr(line, "bindings\":	\"****\"") != NULL)
        {
            masked_query_param_found = SF_BOOLEAN_TRUE;
        }

        if (strstr(line, "value\":	\"log query testing\"") != NULL)
        {
            found_query_parameters = SF_BOOLEAN_TRUE;
        }
    }


    if (log_query_text) 
    {
        assert_true(found_query_text);
        assert_false(masked_sql_found);
    }
    else
    {
        assert_false(found_query_text);
        assert_true(masked_sql_found);
    }

    if (log_query_parameters) 
    {
        assert_false(masked_query_param_found);
        assert_true(found_query_parameters);
    }
    else
    {
        assert_false(found_query_parameters);
        assert_true(masked_query_param_found);
    }
}

void test_normal_query_helper(sf_bool log_query_text)
{
    SF_CONNECT* sf = validate_connection(log_query_text, SF_BOOLEAN_FALSE);

    /* query */
    SF_STMT* sfstmt = snowflake_stmt(sf);

    char* log_fp = "sql.log";
    remove(log_fp);

    FILE* fp = fopen(log_fp, "w+");
    assert_non_null(fp);
    log_set_lock(NULL);
    log_set_level(SF_LOG_DEBUG);
    log_set_quiet(1);
    log_set_fp(fp);

    const char* query_text = "select 1";

    SF_STATUS status = snowflake_query(sfstmt, query_text, 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    sf_bool found_query_text = SF_BOOLEAN_FALSE;
    sf_bool masked_sql_found = SF_BOOLEAN_FALSE;

    char line[1024];

    fseek(fp, 0, SEEK_SET);
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, query_text) != NULL)
        {
            found_query_text = SF_BOOLEAN_TRUE;
        }

        if (strstr(line, "sqlText\":	\"****\"") != NULL)
        {
            masked_sql_found = SF_BOOLEAN_TRUE;
        }
    }

    if (log_query_text) 
    {
        assert_true(found_query_text);
        assert_false(masked_sql_found);
    }
    else
    {
        assert_false(found_query_text);
        assert_true(masked_sql_found);

    }
    log_close();
    remove(log_fp);

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}


void test_log_query_params_array_binding_helper(sf_bool log_query_parameters)
{
    /* init */
    SF_STATUS status;
    SF_BIND_INPUT input_array[3];
    int64 input1;
    char input2[1000];
    float64 input3;
    unsigned int iter = 0;

    /* Connect with all parameters set */
    SF_CONNECT* sf = validate_connection(SF_BOOLEAN_FALSE, log_query_parameters);

    /* Create a statement once and reused */
    SF_STMT* stmt = snowflake_stmt(sf);
    /* NOTE: the numeric type here should fit into int64 otherwise
     * it is taken as a float */
    status = snowflake_query(
        stmt,
        "create or replace table t (c1 number(10,0) not null, c2 string, c3 double)",
        0
    );
    assert_int_equal(status, SF_STATUS_SUCCESS);

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

    char* log_fp = "sql.log";
    remove(log_fp);

    FILE* fp = fopen(log_fp, "w+");
    assert_non_null(fp);
    log_set_lock(NULL);
    log_set_level(SF_LOG_DEBUG);
    log_set_quiet(1);
    log_set_fp(fp);

    char* query_text = "insert into t values(?, ?, ?)";

    status = snowflake_prepare(
        stmt,
        query_text,
        0
    );
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_bind_param_array(stmt, input_array, 3);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    input1 = 1;
    strcpy(input2, "log query testing");
    input3 = 1.23;

    status = snowflake_execute(stmt);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    validate_log_file(fp, query_text, SF_BOOLEAN_FALSE, log_query_parameters);

    log_close();
    remove(log_fp);

    snowflake_stmt_term(stmt);
    snowflake_term(sf);
}

void test_log_query_params_single_binding_helper(sf_bool log_query_text, sf_bool log_query_parameters)
{
    SF_CONNECT* sf = validate_connection(log_query_text, log_query_parameters);

    char str[20];
    // Initialize bind inputs
    SF_BIND_INPUT string_input = {
      .idx = 2,
      .c_type = SF_C_TYPE_STRING,
      .value = "log query testing",
      .len = sizeof(str)
    };

    /* Create a statement once and reused */
    SF_STMT* stmt = snowflake_stmt(sf);

    char* log_fp = "sql.log";
    remove(log_fp);

    FILE* fp = fopen(log_fp, "w+");
    assert_non_null(fp);
    log_set_lock(NULL);
    log_set_level(SF_LOG_DEBUG);
    log_set_quiet(1);
    log_set_fp(fp);

    char* query_text = "select ?,?,?,?,?,?,?,?,?";


    // test with parameters more than 8
    SF_STATUS status = snowflake_prepare(
        stmt,
        query_text,
        0
    );

    for (string_input.idx = 1; string_input.idx <= 9; string_input.idx++)
    {
        status = snowflake_bind_param(stmt, &string_input);
        assert_int_equal(status, SF_STATUS_SUCCESS);
    }

    status = snowflake_execute(stmt);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    validate_log_file(fp, query_text, log_query_text, log_query_parameters);

    log_close();
    remove(log_fp);

    snowflake_stmt_term(stmt);
    snowflake_term(sf);
}

void test_log_query_text_enabled(void** unused)
{
    SF_UNUSED(unused);
    test_normal_query_helper(SF_BOOLEAN_TRUE);
}

void test_log_query_text_disabled(void** unused)
{
    SF_UNUSED(unused);
    test_normal_query_helper(SF_BOOLEAN_FALSE);
}

void test_log_query_params_disabled_with_array_binding(void** unused)
{
    SF_UNUSED(unused);
    test_log_query_params_array_binding_helper(SF_BOOLEAN_FALSE);
}

void test_log_query_params_enabled_with_array_binding(void** unused)
{
    SF_UNUSED(unused);
    test_log_query_params_array_binding_helper(SF_BOOLEAN_TRUE);
}

void test_log_query_text_and_params_both_disabled_with_single_binding(void** unused)
{
    SF_UNUSED(unused);
    test_log_query_params_single_binding_helper(SF_BOOLEAN_FALSE, SF_BOOLEAN_FALSE);
}

void test_log_query_text_disabled_and_params_enabled_with_single_binding(void** unused)
{
    SF_UNUSED(unused);
    test_log_query_params_single_binding_helper(SF_BOOLEAN_FALSE, SF_BOOLEAN_TRUE);
}

void test_log_query_text_and_params_both_enabled_with_single_binding(void** unused)
{
    SF_UNUSED(unused);
    test_log_query_params_single_binding_helper(SF_BOOLEAN_TRUE, SF_BOOLEAN_TRUE);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_log_query_text_enabled),
        cmocka_unit_test(test_log_query_text_disabled),
        cmocka_unit_test(test_log_query_params_enabled_with_array_binding),
        cmocka_unit_test(test_log_query_params_disabled_with_array_binding),
        cmocka_unit_test(test_log_query_text_and_params_both_disabled_with_single_binding),
        cmocka_unit_test(test_log_query_text_disabled_and_params_enabled_with_single_binding),
        cmocka_unit_test(test_log_query_text_and_params_both_enabled_with_single_binding),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
