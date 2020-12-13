/*
 * Copyright (c) 2018-2020 Snowflake Computing, Inc. All rights reserved.
 */
#include "utils/test_setup.h"
#include <cJSON.h>
#include <string.h>
#include <connection.h>
#include <memory.h>

/**
 * Tests fetching the query result response directly from SFSTMT
 * @param unused
 */
void test_get_query_result_response(void **unused) {
    SF_CONNECT *sf = setup_snowflake_connection();
    snowflake_connect(sf);
    SF_STATUS status = enable_arrow_force(sf);

    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* query */
    SF_STMT *sfstmt = snowflake_stmt(sf);
    status = snowflake_query(
            sfstmt, "select randstr(100,random()) from table(generator(rowcount=>2))",
            0);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sfstmt->error));
    }

    // Parse the JSON, and grab a few values to verify correctness
    cJSON* parsedJSON = snowflake_cJSON_Parse(sfstmt->query_response_text);

    sf_bool success;
    json_copy_bool(&success, parsedJSON, "success");
    assert_int_equal(success, SF_BOOLEAN_TRUE);

    cJSON *data = snowflake_cJSON_GetObjectItem(
            parsedJSON, "data");
    assert_int_equal(
            strlen(snowflake_cJSON_GetObjectItem(data, "queryID")->valuestring) + 1,
            SF_UUID4_LEN);

    snowflake_cJSON_Delete(parsedJSON);
    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_get_query_result_response),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
