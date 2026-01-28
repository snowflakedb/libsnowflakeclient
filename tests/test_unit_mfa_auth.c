#include <string.h>
#include "utils/test_setup.h"
#include "connection.h"
#include "memory.h"

/**
 * Test json body is properly updated.
 */
void test_json_data_in_MFA_Auth(void **unused)
{
    SF_CONNECT* sf = (SF_CONNECT*)SF_CALLOC(1, sizeof(SF_CONNECT));
    sf->account = "testaccount";
    sf->host = "testaccount.snowflakecomputing.com";
    sf->user = "testuser";
    sf->password = "testpassword";
    sf->authenticator = SF_AUTHENTICATOR_DEFAULT;
    sf->application_name = SF_API_NAME;
    sf->application_version = SF_API_VERSION;

    cJSON *body =  create_auth_json_body(
        sf,
        sf->application,
        sf->application_name,
        sf->application_version,
        sf->timezone,
        sf->autocommit);
    cJSON* data = snowflake_cJSON_GetObjectItem(body, "data");

    assert_string_equal(snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(data, "EXT_AUTHN_DUO_METHOD")), "push");

    sf->passcode = "123456";
    body = create_auth_json_body(
        sf,
        sf->application,
        sf->application_name,
        sf->application_version,
        sf->timezone,
        sf->autocommit);
    data = snowflake_cJSON_GetObjectItem(body, "data");

    assert_string_equal(snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(data, "EXT_AUTHN_DUO_METHOD")), "passcode");
    assert_string_equal(snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(data, "passcode")), "123456");

    sf->passcode_in_password = SF_BOOLEAN_TRUE;

    body = create_auth_json_body(
        sf,
        sf->application,
        sf->application_name,
        sf->application_version,
        sf->timezone,
        sf->autocommit);
    data = snowflake_cJSON_GetObjectItem(body, "data");

    assert_string_equal(snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(data, "EXT_AUTHN_DUO_METHOD")), "passcode");
    assert_int_equal(snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(data, "passcode")), NULL);

    sf_bool passcodeInPassword;
    json_copy_bool(&passcodeInPassword, data, "passcodeInPassword");
    assert_int_equal(passcodeInPassword, SF_BOOLEAN_TRUE);

    SF_FREE(sf);
}

/**
 * Test that application_path override is used in CLIENT_ENVIRONMENT.
 */
void test_application_path_override(void **unused)
{
    SF_CONNECT* sf = (SF_CONNECT*)SF_CALLOC(1, sizeof(SF_CONNECT));
    sf->account = "testaccount";
    sf->host = "testaccount.snowflakecomputing.com";
    sf->user = "testuser";
    sf->password = "testpassword";
    sf->authenticator = SF_AUTHENTICATOR_DEFAULT;
    sf->application_name = SF_API_NAME;
    sf->application_version = SF_API_VERSION;

    // Test 1: Without override, APPLICATION_PATH should be auto-detected
    // (In mock mode it will be "/app/path")
    cJSON *body = create_auth_json_body(
        sf,
        sf->application,
        sf->application_name,
        sf->application_version,
        sf->timezone,
        sf->autocommit);
    cJSON* data = snowflake_cJSON_GetObjectItem(body, "data");
    cJSON* client_env = snowflake_cJSON_GetObjectItem(data, "CLIENT_ENVIRONMENT");
    cJSON* app_path = snowflake_cJSON_GetObjectItem(client_env, "APPLICATION_PATH");

    assert_non_null(app_path);
    // In non-mock mode this would be the executable path, in mock mode it's "/app/path"
    assert_non_null(snowflake_cJSON_GetStringValue(app_path));
    snowflake_cJSON_Delete(body);

    // Test 2: With override, APPLICATION_PATH should use the custom value
    sf->application_path = "/custom/path/to/my_script.php";

    body = create_auth_json_body(
        sf,
        sf->application,
        sf->application_name,
        sf->application_version,
        sf->timezone,
        sf->autocommit);
    data = snowflake_cJSON_GetObjectItem(body, "data");
    client_env = snowflake_cJSON_GetObjectItem(data, "CLIENT_ENVIRONMENT");
    app_path = snowflake_cJSON_GetObjectItem(client_env, "APPLICATION_PATH");

    assert_non_null(app_path);
    assert_string_equal(snowflake_cJSON_GetStringValue(app_path), "/custom/path/to/my_script.php");
    snowflake_cJSON_Delete(body);

    SF_FREE(sf);
}

int main(void)
{
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_json_data_in_MFA_Auth),
        cmocka_unit_test(test_application_path_override),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
