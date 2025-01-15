/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */
#include <string.h>
#include "utils/test_setup.h"

char* g_pat;

int initialize_pat(char **token_name) {
    SF_CONNECT *sf = setup_snowflake_connection();
    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        log_error("Could not connect to Snowflake server, PAT wasn't created");
        dump_error(&(sf->error));
        snowflake_term(sf);
        return EXIT_FAILURE;
    }

    SF_STMT *sfstmt = snowflake_stmt(sf);
    char* create_pat_command = "ALTER USER SNOWMAN ADD PROGRAMMATIC ACCESS TOKEN TEST_PAT DAYS_TO_EXPIRY=1 COMMENT='programmatic access token created for e2e test'";
    status = snowflake_query(sfstmt, create_pat_command, 0);
    if (status != SF_STATUS_SUCCESS) {
        log_error("Could not execute query, PAT wasn't created");
        dump_error(&(sfstmt->error));
        snowflake_stmt_term(sfstmt);
        snowflake_term(sf);
        return EXIT_FAILURE;
    }

    if (snowflake_num_rows(sfstmt) != 1) {
        return EXIT_FAILURE;
    }

    snowflake_fetch(sfstmt);
    g_pat = NULL;
    size_t pat_len = 0;
    size_t max_pat_len = 0;
    snowflake_column_as_str(sfstmt, 2, &g_pat, &pat_len, &max_pat_len);

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);

    return EXIT_SUCCESS;
}

void test_select1(void **unused) {
    SF_CONNECT *sf = setup_snowflake_connection();

    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_PAT);
    snowflake_set_attribute(sf, SF_CON_PAT, "invalid_pat");

    SF_STATUS status = snowflake_connect(sf);
    assert_int_not_equal(status, SF_STATUS_SUCCESS);

    assert_non_null(g_pat);
    snowflake_set_attribute(sf, SF_CON_PAT, g_pat);

    status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    SF_STMT *sfstmt = snowflake_stmt(sf);


    /* query */
    status = snowflake_query(sfstmt, "select 1;", 0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);


    int64 out = 0;
    assert_int_equal(snowflake_num_rows(sfstmt), 1);

    while ((status = snowflake_fetch(sfstmt)) == SF_STATUS_SUCCESS) {
        snowflake_column_as_int64(sfstmt, 1, &out);
        assert_int_equal(out, 1);
    }
    if (status != SF_STATUS_EOF) {
        dump_error(&(sfstmt->error));
    }
    assert_int_equal(status, SF_STATUS_EOF);

    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

int pat_term(char** token_name) {
    SF_CONNECT *sf = setup_snowflake_connection();
    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        log_error("Could not connect to Snowflake server, PAT wasn't cleared");
        dump_error(&(sf->error));
        snowflake_term(sf);
        return EXIT_FAILURE;
    }

    SF_STMT *sfstmt = snowflake_stmt(sf);
    status = snowflake_query(sfstmt, "ALTER USER SNOWMAN DROP PROGRAMMATIC ACCESS TOKEN TEST_PAT;", 0);
    if (status != SF_STATUS_SUCCESS) {
        log_error("Could not execute query, PAT wasn't cleared");
        dump_error(&(sfstmt->error));
        snowflake_stmt_term(sfstmt);
        snowflake_term(sf);
        return EXIT_FAILURE;
    }


    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);

    return EXIT_SUCCESS;
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    char* pat_name = "TEST_PAT";
    initialize_pat(&pat_name);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_select1),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    pat_term(&pat_name);
    snowflake_global_term();
    return ret;
}
