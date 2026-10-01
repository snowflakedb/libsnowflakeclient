#include <string.h>
#include "utils/test_setup.h"
#include "connection.h"
#include "memory.h"

/**
 * Test json body is properly updated.
 */
void test_private_link_core(void** unused)
{
    SF_UNUSED(unused);
    char* original_env = getenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL");
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "testaccount");
    snowflake_set_attribute(sf, SF_CON_USER, "testuser");
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "testpassword");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_DEFAULT);

    _snowflake_check_connection_parameters(sf);
    assert_int_equal(NULL, getenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL"));
    sf_unsetenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL");

    snowflake_set_attribute(sf, SF_CON_HOST, "account.privateLINK.snowflakecomputING.com");
    _snowflake_check_connection_parameters(sf);
    assert_int_equal(NULL, getenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL"));
    sf_unsetenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL");

    snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &SF_BOOLEAN_TRUE);
    snowflake_set_attribute(sf, SF_CON_HOST, "account.privateLINK.snowflakecomputING.com");
    _snowflake_check_connection_parameters(sf);
    assert_string_equal("http://ocsp.account.privateLINK.snowflakecomputING.com/ocsp_response_cache.json", getenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL"));
    sf_unsetenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL");

    snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &SF_BOOLEAN_FALSE);
    snowflake_set_attribute(sf, SF_CON_HOST, "account.snowflakecomputing.com");
    _snowflake_check_connection_parameters(sf);
    assert_int_equal(NULL, getenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL"));
    sf_unsetenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL");

    snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &SF_BOOLEAN_TRUE);
    snowflake_set_attribute(sf, SF_CON_HOST, "account.privatelink.snowflakecomputing.cn");
    _snowflake_check_connection_parameters(sf);
    assert_string_equal("http://ocsp.account.privatelink.snowflakecomputing.cn/ocsp_response_cache.json", getenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL"));
    sf_unsetenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL");

    if (original_env) {
        sf_setenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL", original_env);
    }
    else {
        sf_unsetenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL");
    }
    snowflake_term(sf);
}

int main(void)
{
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_private_link_core),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
