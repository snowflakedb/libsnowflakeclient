#include "auth_utils.h"

extern void test_oauth_successful_connection(void **state);
extern void test_oauth_invalid_token(void **state);
extern void test_oauth_mismatched_username(void **state);
extern void test_okta_successful_connection(void **state);
extern void test_okta_wrong_credentials(void **state);
extern void test_okta_invalid_url(void **state);
extern void test_okta_invalid_url_no_okta_path(void **state);
extern void test_external_browser_successful_connection(void **state);
extern void test_external_browser_mismatched_username(void **state);
extern void test_external_browser_wrong_credentials(void **state);
extern void test_mfa_totp_authentication(void **state);
extern void test_aws_wif_authentication(void **state);
extern void test_gcp_wif_authentication(void **state);
extern void test_azure_wif_authentication(void **state);
extern void test_wif_no_cloud_credentials(void **state);
extern void test_wif_missing_provider(void **state);
extern void test_wif_invalid_authenticator(void **state);
extern void test_wif_valid_authenticator(void **state);
extern void test_wif_multiple_connections(void **state);
extern void test_wif_explicit_provider_integration(void **state);
extern void test_wif_invalid_provider_fails(void **state);
extern void test_wif_get_attributes(void **state);


int main(void) {
    snowflake_global_init(NULL, SF_LOG_INFO, NULL);
    snowflake_global_set_attribute(SF_GLOBAL_CA_BUNDLE_FILE, getenv("SNOWFLAKE_TEST_CA_BUNDLE_FILE"));
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_oauth_successful_connection),
            cmocka_unit_test(test_oauth_invalid_token),
            cmocka_unit_test(test_oauth_mismatched_username),
            cmocka_unit_test(test_okta_successful_connection),
            cmocka_unit_test(test_okta_wrong_credentials),
            cmocka_unit_test(test_okta_invalid_url),
            cmocka_unit_test(test_okta_invalid_url_no_okta_path),
            cmocka_unit_test(test_external_browser_successful_connection),
            cmocka_unit_test(test_external_browser_mismatched_username),
            cmocka_unit_test(test_external_browser_wrong_credentials),
            cmocka_unit_test(test_mfa_totp_authentication),
            cmocka_unit_test(test_aws_wif_authentication),
            cmocka_unit_test(test_gcp_wif_authentication),
            cmocka_unit_test(test_azure_wif_authentication),
            cmocka_unit_test(test_wif_no_cloud_credentials),
            cmocka_unit_test(test_wif_missing_provider),
            cmocka_unit_test(test_wif_invalid_authenticator),
            cmocka_unit_test(test_wif_valid_authenticator),
            cmocka_unit_test(test_wif_multiple_connections),
            cmocka_unit_test(test_wif_explicit_provider_integration),
            cmocka_unit_test(test_wif_invalid_provider_fails),
            cmocka_unit_test(test_wif_get_attributes)
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}