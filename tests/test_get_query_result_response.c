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

    status = snowflake_prepare(sfstmt, "select randstr(100,random()) from table(generator(rowcount=>2))", 0);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // Issue the query
    status = snowflake_execute_with_capture(sfstmt, result_capture);
    assert_int_equal(status, SF_STATUS_SUCCESS);
    char *resultBuffer = result_capture->capture_buffer;

    // Parse the JSON, and grab a few values to verify correctness
    cJSON *parsedJSON = snowflake_cJSON_Parse(resultBuffer);

    sf_bool success;
    success = snowflake_cJSON_IsTrue(snowflake_cJSON_GetObjectItem(parsedJSON, "success")) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
    assert_int_equal(success, SF_BOOLEAN_TRUE);

    cJSON *data = snowflake_cJSON_GetObjectItem(
            parsedJSON, "data");
    assert_int_equal(
            strlen(snowflake_cJSON_GetObjectItem(data, "queryID")->valuestring) + 1,
            SF_UUID4_LEN);

    snowflake_cJSON_Delete(parsedJSON);
    snowflake_query_result_capture_term(result_capture);
    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

/**
 * Tests fetching the query result response with failed query
 * @param unused
 */
void test_get_query_result_response_failed(void **unused) {
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

    status = snowflake_prepare(sfstmt, "wrong query", 0);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // Issue the query
    status = snowflake_execute_with_capture(sfstmt, result_capture);
    assert_int_equal(status, SF_STATUS_ERROR_GENERAL);
    assert_int_not_equal(result_capture->actual_response_size, 0);

    snowflake_query_result_capture_term(result_capture);
    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_get_query_result_response),
            cmocka_unit_test(test_get_query_result_response_failed),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
