#include <string.h>
#include <stdio.h>
#include "utils/test_setup.h"
#include <snowflake/logger.h>

#include "utils/test_setup.h"
#include <snowflake/client_config_parser.h>
#include "log_file_util.h"
#include "memory.h"
#include <stdio.h>
#include <sys/stat.h>

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

    /* Enable the logging */
    char log_buf[16500] = { 0 };
    FILE* log_fp = tmpfile();
    assert_non_null(log_fp);
    log_set_lock(NULL);
    log_set_level(SF_LOG_DEBUG);
    log_set_quiet(1);
    log_set_fp(log_fp);

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

    /* Validate that the query text is masked in the log */
    fflush(log_fp);
    rewind(log_fp);
    fread(log_buf, 1, sizeof(log_buf) - 1, log_fp);
    fclose(log_fp);
    log_set_fp(NULL);

    assert_null(strstr(log_buf, "begin;"));
    assert_null(strstr(log_buf, "delete from test_multi_txn"));
    assert_non_null(strstr(log_buf, "sqlText:****"));

    status = snowflake_query(sfstmt, query_text, 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
    remove(log_fp);
}

void test_log_query_parameters(void** unused)
{
    SF_UNUSED(unused);
    const char* logtext[][2] = {
     {//0
         "sqlText: select 1",
         "sqlText: ****"
     },
     {
         "parameters: {",
         "parameters: ****"
      }
    };

    SF_CONNECT* sf = validate_connection();
    SF_STMT* sfstmt = snowflake_stmt(sf);

    char query[1024];
    int64 multi_stmt_count = 3;
    SF_STATUS status = snowflake_stmt_set_attr(sfstmt, SF_STMT_MULTI_STMT_COUNT, &multi_stmt_count);

    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    sprintf(query, "%s", "select 1; select 2; select 3");

    /* Enable the logging */
    FILE* fp = fopen("dummy.log", "w+");
    assert_non_null(fp);
    log_set_lock(NULL);
    log_set_level(SF_LOG_TRACE);
    log_set_quiet(1);
    log_set_fp(fp);

    status = snowflake_query(sfstmt, query, 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);

    char* line = NULL;
    size_t len = 0;
    for (int i = 0; i < 13; i++)
    {
        fseek(fp, 0, SEEK_SET);
        log_trace("%s", logtext[i][0]);
        fseek(fp, 0, SEEK_SET);
        len = getline(&line, &len, fp);
        if (i != 0)
        {
            assert_null(strstr(line, logtext[i][0]));
        }
        assert_non_null(strstr(line, logtext[i][1]));
    }

    free(line);
    fclose(fp);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_log_query_text),
        cmocka_unit_test(test_log_query_parameters),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
