#include <string.h>
#include "utils/test_setup.h"

void test_multi_stmt_transaction(void **unused)
{
    SF_UNUSED(unused);
    SF_CONNECT *sf = setup_snowflake_connection();
    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* query */
    SF_STMT *sfstmt = snowflake_stmt(sf);
    status = snowflake_query(sfstmt, "create or replace temporary table test_multi_txn(c1 number, c2 string) as select 10, 'z'", 0);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    int64 multi_stmt_count = 5;
    status = snowflake_stmt_set_attr(sfstmt, SF_STMT_MULTI_STMT_COUNT, &multi_stmt_count);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_query(sfstmt,
                             "begin;\n"
                             "delete from test_multi_txn;\n"
                             "insert into test_multi_txn values (1, 'a'), (2, 'b');\n"
                             "commit;\n"
                             "select count(*) from test_multi_txn",
                             0);
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

    int counter = 0;
    int64 out;
    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_int64(sfstmt, 1, &out);
        assert_int_equal(out, 2);
        ++counter;
    }
    assert_int_equal(status, SF_STATUS_EOF);
    assert_int_equal(counter, 1);

    // no more result
    assert_int_equal(snowflake_next_result(sfstmt), SF_STATUS_EOF);

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_multi_stmt_transaction_rollback(void **unused)
{
    SF_UNUSED(unused);
    SF_CONNECT *sf = setup_snowflake_connection();
    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* query */
    SF_STMT *sfstmt = snowflake_stmt(sf);
    status = snowflake_query(sfstmt, "create or replace temporary table test_multi_txn(c1 number, c2 string) as select 10, 'z'", 0);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    int64 multi_stmt_count = 5;
    status = snowflake_stmt_set_attr(sfstmt, SF_STMT_MULTI_STMT_COUNT, &multi_stmt_count);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_query(sfstmt,
                             "begin;\n"
                             "delete from test_multi_txn;\n"
                             "insert into test_multi_txn values (1, 'a'), (2, 'b');\n"
                             "rollback;\n"
                             "select count(*) from test_multi_txn",
                             0);
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

    // fourth statement (rollback)
    assert_int_equal(snowflake_next_result(sfstmt), SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(sfstmt), 1);
    assert_int_equal(snowflake_affected_rows(sfstmt), 1);

    // fifth statement (select)
    assert_int_equal(snowflake_next_result(sfstmt), SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(sfstmt), 1);

    int counter = 0;
    int64 out;
    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_int64(sfstmt, 1, &out);
        assert_int_equal(out, 1);
        ++counter;
    }
    assert_int_equal(status, SF_STATUS_EOF);
    assert_int_equal(counter, 1);

    // no more result
    assert_int_equal(snowflake_next_result(sfstmt), SF_STATUS_EOF);

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

void test_multi_stmt_with_large_result(void **unused)
{
    SF_UNUSED(unused);
    const int rownum = 100000;
    SF_CONNECT *sf = setup_snowflake_connection();

    // TODO SNOW-1526335
    // Sometime we can't get OCSP response from cache server or responder
    // Usually happen on GCP and should be ignored by FAIL_OPEN
    // Unfortunately libsnowflakeclient doesn't support FAIL_OPEN for now
    // so we have to disable OCSP validation to around it.
    // Will remove this code when adding support for FAIL_OPEN (which is
    // the default behavior for all other drivers)
    char *cenv = getenv("CLOUD_PROVIDER");
    if (cenv && !strncmp(cenv, "GCP", 4)) {
        sf_bool insecure_mode = SF_BOOLEAN_TRUE;
        snowflake_set_attribute(sf, SF_CON_INSECURE_MODE, &insecure_mode);
    }

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

    status = snowflake_query(sfstmt,
                             "create or replace temporary table test_multi_large(c1 number, c2 number);\n"
                             "insert into test_multi_large select seq4(), TO_VARCHAR(seq4()) from table(generator(rowcount => 100000));\n"
                             "select * from test_multi_large",
                             0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // first statement (begin)
    assert_int_equal(snowflake_num_rows(sfstmt), 1);
    assert_int_equal(snowflake_affected_rows(sfstmt), 1);

    // second statement (insert)
    assert_int_equal(snowflake_next_result(sfstmt), SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(sfstmt), 1);
    assert_int_equal(snowflake_affected_rows(sfstmt), rownum);

    // third statement (select)
    assert_int_equal(snowflake_next_result(sfstmt), SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(sfstmt), rownum);

    int counter = 0;
    int64 intout;
    const char* strout;
    char strexp[64];
    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_int64(sfstmt, 1, &intout);
        assert_int_equal(intout, counter);
        snowflake_column_as_const_str(sfstmt, 2, &strout);
        sprintf(strexp, "%d", counter);
        assert_string_equal(strout, strexp);
        ++counter;
    }
    assert_int_equal(status, SF_STATUS_EOF);
    assert_int_equal(counter, rownum);

    // no more result
    assert_int_equal(snowflake_next_result(sfstmt), SF_STATUS_EOF);

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

/* helper function for testing multi_stmt_count, running a query with
 * multiple statements of 3.
 * @param use_session_param Whethter to set MULTI_STATEMENT_COUNT through
 *        session parameter or statement attribute.
 * @param count The count number to be set
 * @return True if the query succeeded, otherwise false.
 */
sf_bool test_multi_stmt_core(sf_bool use_session_param, int count)
{
    SF_CONNECT *sf = setup_snowflake_connection();
    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    SF_STMT* sfstmt = snowflake_stmt(sf);

    char query[1024];
    int64 multi_stmt_count = count;
    if (SF_BOOLEAN_TRUE == use_session_param)
    {
        sprintf(query, "alter session set MULTI_STATEMENT_COUNT=%d", count);
        status = snowflake_query(sfstmt, query, 0);
    }
    else
    {
        status = snowflake_stmt_set_attr(sfstmt, SF_STMT_MULTI_STMT_COUNT, &multi_stmt_count);
    }
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    sprintf(query, "%s", "select 1; select 2; select 3");
    status = snowflake_query(sfstmt, query, 0);

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);

    return (status == SF_STATUS_SUCCESS) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
}

void test_multi_stmt_count_session_param_off(void** unused)
{
    SF_UNUSED(unused);
    // disable multiple statements by setting session parameter to 1
    // the query is expected to fail
    assert_int_equal(test_multi_stmt_core(SF_BOOLEAN_TRUE, 1), SF_BOOLEAN_FALSE);
}

void test_multi_stmt_count_session_param_on(void** unused)
{
    SF_UNUSED(unused);
    // enable multiple statements by setting session parameter to 0
    // the query should work
    assert_int_equal(test_multi_stmt_core(SF_BOOLEAN_TRUE, 0), SF_BOOLEAN_TRUE);
}

void test_multi_stmt_count_stmt_attr_match(void** unused)
{
    SF_UNUSED(unused);
    // set statement attribute with match number
    // the query should work
    assert_int_equal(test_multi_stmt_core(SF_BOOLEAN_FALSE, 3), SF_BOOLEAN_TRUE);
}

void test_multi_stmt_count_stmt_attr_mismatch(void** unused)
{
    SF_UNUSED(unused);
    // set statement attribute with mismatch number
    // the query is expected to fail
    assert_int_equal(test_multi_stmt_core(SF_BOOLEAN_FALSE, 2), SF_BOOLEAN_FALSE);
}

void test_multi_stmt_arrow_format(void **unused)
{
    SF_UNUSED(unused);
    /* use large result set to confirm the format of both query response and result chunks */
    const int rownum = 100000;
    SF_CONNECT *sf = setup_snowflake_connection();

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    SF_STMT *sfstmt = snowflake_stmt(sf);

    /* set parameters */
	/* use arrow format */
    status = snowflake_query(sfstmt,
                             "alter session set C_API_QUERY_RESULT_FORMAT=ARROW",
                             0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* Enable fix for multi-statement with arrow.
     * Ingore failure since the test user might not be able to change the parameter.
     * In such case assume the parameter has been enabled on the test account.
     */
    status = snowflake_query(sfstmt,
                             "alter session set ENABLE_FIX_1758055_ADD_ARROW_SUPPORT_FOR_MULTI_STMTS=true",
                             0);

    /* query */
    int64 multi_stmt_count = 3;
    status = snowflake_stmt_set_attr(sfstmt, SF_STMT_MULTI_STMT_COUNT, &multi_stmt_count);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    status = snowflake_query(sfstmt,
                             "create or replace temporary table test_multi_large(c1 number, c2 number);\n"
                             "insert into test_multi_large select seq4(), TO_VARCHAR(seq4()) from table(generator(rowcount => 100000));\n"
                             "select * from test_multi_large",
                             0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* first statement (begin) */
    assert_int_equal(snowflake_num_rows(sfstmt), 1);
    assert_int_equal(snowflake_affected_rows(sfstmt), 1);

    /* second statement (insert) */
    assert_int_equal(snowflake_next_result(sfstmt), SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(sfstmt), 1);
    assert_int_equal(snowflake_affected_rows(sfstmt), rownum);

    /* third statement (select) */
    assert_int_equal(snowflake_next_result(sfstmt), SF_STATUS_SUCCESS);
    assert_int_equal(snowflake_num_rows(sfstmt), rownum);

/* skip Windows 32-bit which arrow is not available */
#if !defined (_WIN32) || defined(_WIN64)
    /* the format of select result should be arrow */
    assert_int_equal(sfstmt->qrf, SF_ARROW_FORMAT);
#endif

    int counter = 0;
    int64 intout;
    const char* strout;
    char strexp[64];
    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_int64(sfstmt, 1, &intout);
        assert_int_equal(intout, counter);
        snowflake_column_as_const_str(sfstmt, 2, &strout);
        sprintf(strexp, "%d", counter);
        assert_string_equal(strout, strexp);
        ++counter;
    }
    assert_int_equal(status, SF_STATUS_EOF);
    assert_int_equal(counter, rownum);

    // no more result
    assert_int_equal(snowflake_next_result(sfstmt), SF_STATUS_EOF);

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_multi_stmt_transaction),
        cmocka_unit_test(test_multi_stmt_transaction_rollback),
        cmocka_unit_test(test_multi_stmt_with_large_result),
        cmocka_unit_test(test_multi_stmt_count_session_param_off),
        cmocka_unit_test(test_multi_stmt_count_session_param_on),
        cmocka_unit_test(test_multi_stmt_count_stmt_attr_match),
        cmocka_unit_test(test_multi_stmt_count_stmt_attr_mismatch),
        cmocka_unit_test(test_multi_stmt_arrow_format),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
