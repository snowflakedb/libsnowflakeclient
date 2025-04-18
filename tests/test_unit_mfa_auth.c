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

int main(void)
{
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_json_data_in_MFA_Auth),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
