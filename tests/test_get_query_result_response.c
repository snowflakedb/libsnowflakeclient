/*
 * Copyright (c) 2018-2020 Snowflake Computing, Inc. All rights reserved.
 */
#include "utils/test_setup.h"
#include <cJSON.h>
#include <string.h>
#include <connection.h>
#include <memory.h>
#include <error.h>

/**
 * Tests fetching the query result response directly from SFSTMT
 * @param unused
 */
void test_get_query_result_response(void **unused) {
    SF_CONNECT *sf = setup_snowflake_connection();
    SF_STATUS status = snowflake_connect(sf);

    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);

    /* query */
    SF_STMT *sfstmt = snowflake_stmt(sf);
    SF_QUERY_RESULT_CAPTURE *result_capture;
    snowflake_query_result_capture_init(&result_capture);

    // Create a space for storing the query response text
    size_t buffer_size = 5000;
    result_capture->capture_buffer = (char *) SF_CALLOC(1, buffer_size);

    clear_snowflake_error(&sfstmt->error);
    status = snowflake_prepare(sfstmt, "select randstr(100,random()) from table(generator(rowcount=>2))", 0);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // Try first with too small of a buffer size, we should return an error.
    result_capture->buffer_size = 100;
    status = snowflake_execute_with_capture(sfstmt, result_capture);
    assert_int_equal(status, SF_STATUS_ERROR_BUFFER_TOO_SMALL);

    // Now use the actual size
    result_capture->buffer_size = buffer_size;
    clear_snowflake_error(&sfstmt->error);
    status = snowflake_execute_with_capture(sfstmt, result_capture);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // Parse the JSON, and grab a few values to verify correctness
    cJSON *parsedJSON = snowflake_cJSON_Parse(result_capture->capture_buffer);

    sf_bool success;
    json_copy_bool(&success, parsedJSON, "success");
    assert_int_equal(success, SF_BOOLEAN_TRUE);

    cJSON *data = snowflake_cJSON_GetObjectItem(
            parsedJSON, "data");
    assert_int_equal(
            strlen(snowflake_cJSON_GetObjectItem(data, "queryID")->valuestring) + 1,
            SF_UUID4_LEN);

    snowflake_query_result_capture_term(result_capture);
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
