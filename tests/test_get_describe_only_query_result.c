#include "utils/test_setup.h"
#include <cJSON.h>
#include <string.h>
#include <connection.h>
#include <memory.h>
#include <error.h>

/**
 * Tests fetching the describe only query result response directly from SFSTMT
 * @param unused
 */
void test_get_describe_only_query_result(void **unused) {
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

    clear_snowflake_error(&sfstmt->error);
    status = snowflake_prepare(sfstmt, "select randstr(100,random()) from table(generator(rowcount=>2))", 0);
    assert_int_equal(status, SF_STATUS_SUCCESS);

    // Issue the query
    clear_snowflake_error(&sfstmt->error);
    status = snowflake_describe_with_capture(sfstmt, result_capture);
    assert_int_equal(status, SF_STATUS_SUCCESS);
    char *resultBuffer = result_capture->capture_buffer;

    // Parse the JSON, and grab a few values to verify correctness
    cJSON *parsedJSON = snowflake_cJSON_Parse(resultBuffer);

    sf_bool success;
    json_copy_bool(&success, parsedJSON, "success");
    assert_int_equal(success, SF_BOOLEAN_TRUE);

    cJSON *data = snowflake_cJSON_GetObjectItem(parsedJSON, "data");
    assert_int_equal(
            strlen(snowflake_cJSON_GetObjectItem(data, "queryID")->valuestring) + 1,
            SF_UUID4_LEN);

    // Make sure that the query is run in describe only mode and the actual result is empty
    assert_int_equal(snowflake_cJSON_GetArraySize(snowflake_cJSON_GetObjectItem(data, "rowset")), 0);
    // Make sure row types are returned
    cJSON *rowtype = snowflake_cJSON_GetArrayItem(snowflake_cJSON_GetObjectItem(data, "rowtype"), 0);
    assert_string_equal(snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(rowtype, "type")), "text");

    snowflake_cJSON_Delete(parsedJSON);
    snowflake_query_result_capture_term(result_capture);
    snowflake_stmt_term(sfstmt);
    snowflake_term(sf);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_get_describe_only_query_result),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
