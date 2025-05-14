#include <string.h>
#include "utils/test_setup.h"
#include "connection.h"
#include "./authenticator.h"
#include "memory.h"

/*
 * Test auth connection when the token was not provided.
 */
void test_oauth_with_no_token(void** unused) 
{
   SF_CONNECT *sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT,"test_account");
    snowflake_set_attribute(sf, SF_CON_USER, "test_user");
    snowflake_set_attribute(sf, SF_CON_HOST, "host");
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "oauth");

    SF_STATUS status = snowflake_connect(sf);
    assert_int_not_equal(status, SF_STATUS_SUCCESS);
    SF_ERROR_STRUCT* error = snowflake_error(sf);
    assert_int_equal(error->error_code, SF_STATUS_ERROR_BAD_CONNECTION_PARAMS);
    snowflake_term(sf);
}

/*
 * Test the request body with the oauth connection.
 */
void test_json_data_in_oauth(void** unused) 
{
    SF_CONNECT* sf = (SF_CONNECT*)SF_CALLOC(1, sizeof(SF_CONNECT));
    sf->account = "testaccount";
    sf->host = "testaccount.snowflakecomputing.com";
    sf->user = "testuser";
    sf->password = "testpassword";
    sf->authenticator = SF_AUTHENTICATOR_DEFAULT;
    sf->application_name = SF_API_NAME;
    sf->application_version = SF_API_VERSION;
    sf->authenticator = "oauth";
    sf->oauth_token = "mock_token";

    cJSON* body = create_auth_json_body(
        sf,
        sf->application,
        sf->application_name,
        sf->application_version,
        sf->timezone,
        sf->autocommit);
    cJSON* data = snowflake_cJSON_GetObjectItem(body, "data");

    assert_string_equal(snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(data, "authenticator")), "oauth");
    assert_string_equal(snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(data, "token")), "mock_token");

    body = snowflake_cJSON_CreateObject();
    auth_update_json_body(sf, body);
    data = snowflake_cJSON_GetObjectItem(body, "data");
    assert_string_equal(snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(data, "authenticator")), "oauth");
    assert_string_equal(snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(data, "token")), "mock_token");
}

int main(void) 
{
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_oauth_with_no_token),
      cmocka_unit_test(test_json_data_in_oauth)
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
