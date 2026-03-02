#include "utils/test_setup.h"
#include <snowflake/client_config_parser.h>
#include "log_file_util.h"
#include "memory.h"
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#include <pwd.h>
#define SF_TMP_FOLDER "/tmp/sf_client_config_folder"
#else
#define F_OK 0
inline int access(const char* pathname, int mode) {
    return _access(pathname, mode);
}
#endif
#define SF_TMP_FOLDER "/tmp/sf_client_config_folder"

SF_CONNECT* validate_connection()
{
    SF_CONNECT* sf = setup_snowflake_connection();
    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    return sf;
}

void test_log_query_text(void** unused)
{
    SF_UNUSED(unused);
    SF_CONNECT* sf = validate_connection();

    /* query */
    SF_STMT* sfstmt = snowflake_stmt(sf);
    SF_STATUS status = snowflake_query(sfstmt, "create or replace temporary table test_multi_txn(c1 number, c2 string) as select 10, 'z'", 0);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    int64 multi_stmt_count = 5;
    status = snowflake_stmt_set_attr(sfstmt, SF_STMT_MULTI_STMT_COUNT, &multi_stmt_count);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    char* log_fp = "sql.log";
    remove(log_fp);

    FILE* fp = fopen(log_fp, "w+");
    assert_non_null(fp);
    log_set_lock(NULL);
    log_set_level(SF_LOG_DEBUG);
    log_set_quiet(1);
    log_set_fp(fp);

    const char* query_text =
        "begin;\n"
        "delete from test_multi_txn;\n"
        "insert into test_multi_txn values (1, 'a'), (2, 'b');\n"
        "commit;\n"
        "select count(*) from test_multi_txn";

    status = snowflake_query(sfstmt, query_text, 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // first statement (begin)
    assert_int_equal(snowflake_num_rows(sfstmt), 1);
    assert_int_equal(snowflake_affected_rows(sfstmt), 1);

    // second statement (delete)
    assert_int_equal(snowflake_next_result(sfstmt), SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(sfstmt), 1);
    assert_int_equal(snowflake_affected_rows(sfstmt), 1);

    // third statement (insert)
    assert_int_equal(snowflake_next_result(sfstmt), SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(sfstmt), 1);
    assert_int_equal(snowflake_affected_rows(sfstmt), 2);

    // fourth statement (commit)
    assert_int_equal(snowflake_next_result(sfstmt), SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(sfstmt), 1);
    assert_int_equal(snowflake_affected_rows(sfstmt), 1);

    // fifth statement (select)
    assert_int_equal(snowflake_next_result(sfstmt), SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(sfstmt), 1);


    sf_bool found_query_text = SF_BOOLEAN_FALSE;
    sf_bool masked_sql_found = SF_BOOLEAN_FALSE;
    sf_bool found_query_parameters = SF_BOOLEAN_FALSE;
    sf_bool masked_query_parameter_found = SF_BOOLEAN_FALSE;
    char line[1024];

    fseek(fp, 0, SEEK_SET);
    while (fgets(line, sizeof(line), fp) != NULL) {
        if(strstr(line, "insert into test_multi_txn values (1, 'a'), (2, 'b');") != NULL) 
        {
            found_query_text = SF_BOOLEAN_TRUE;
        }

        if (strstr(line, "sqlText\":	\"****\"") != NULL)
        {
            masked_sql_found = SF_BOOLEAN_TRUE;
        }

        if (strstr(line, "MULTI_STATEMENT_COUNT") != NULL)
        {
            found_query_parameters = SF_BOOLEAN_TRUE;
        }

        if (strstr(line, "parameters\":	\"****\"") != NULL)
        {
            masked_query_parameter_found = SF_BOOLEAN_TRUE;
        }
    }

    assert_false(found_query_text);
    assert_false(found_query_parameters);

    assert_true(masked_sql_found);
    assert_true(masked_query_parameter_found);

    fclose(fp);
    remove(log_fp);


    fp = fopen(log_fp, "w+");
    assert_non_null(fp);
    log_set_fp(fp);
    //Enable the log query text.
    sf->log_query_text = SF_BOOLEAN_TRUE;
    sf->log_query_parameters = SF_BOOLEAN_TRUE;

    status = snowflake_query(sfstmt, query_text, 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    
    //Reset the parameter
    found_query_text = SF_BOOLEAN_FALSE;
    masked_sql_found = SF_BOOLEAN_FALSE;
    found_query_parameters = SF_BOOLEAN_FALSE;
    masked_query_parameter_found = SF_BOOLEAN_FALSE;
    fseek(fp, 0, SEEK_SET);
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, "insert into test_multi_txn values (1, 'a'), (2, 'b');") != NULL)
        {
            found_query_text = SF_BOOLEAN_TRUE;
        }

        if (strstr(line, "sqlText\":	\"****\"") != NULL)
        {
            masked_sql_found = SF_BOOLEAN_TRUE;
        }

        if (strstr(line, "MULTI_STATEMENT_COUNT") != NULL)
        {
            found_query_parameters = SF_BOOLEAN_TRUE;
        }

        if (strstr(line, "parameters\":	\"****\"") != NULL)
        {
            masked_query_parameter_found = SF_BOOLEAN_TRUE;
        }
    }

    assert_true(found_query_text);
    assert_true(found_query_parameters);

    assert_false(masked_sql_found);
    assert_false(masked_query_parameter_found);

    fclose(fp);
    log_close();
    remove(log_fp);

}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_log_query_text),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
