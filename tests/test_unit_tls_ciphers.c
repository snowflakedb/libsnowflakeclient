#include <string.h>
#include <curl/curl.h>
#include "utils/test_setup.h"
#include "connection.h"
#include "memory.h"

#define TLS_CIPHERS_ENV "SNOWFLAKE_TLS_CIPHERS"
#define AES_256 "TLS_AES_256_GCM_SHA384"
#define AES_128 "TLS_AES_128_GCM_SHA256"

sf_bool validate_tls_ciphers(const char *cipher);

static char saved_env[1024];
static sf_bool has_saved_env = 0;

static int save_env(void **unused) {
    SF_UNUSED(unused);
    has_saved_env = sf_getenv_s(TLS_CIPHERS_ENV, saved_env, sizeof(saved_env)) != NULL;
    sf_unsetenv(TLS_CIPHERS_ENV);
    return 0;
}

static int restore_env(void **unused) {
    SF_UNUSED(unused);
    if (has_saved_env) {
        sf_setenv(TLS_CIPHERS_ENV, saved_env);
    } else {
        sf_unsetenv(TLS_CIPHERS_ENV);
    }
    return 0;
}

static SF_CONNECT *create_connection(const char *tls_ciphers) {
    SF_CONNECT *sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "testaccount");
    snowflake_set_attribute(sf, SF_CON_USER, "testuser");
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "testpassword");
    if (tls_ciphers) {
        snowflake_set_attribute(sf, SF_CON_TLS_CIPHERS, tls_ciphers);
    }
    return sf;
}

/**
 * validate_tls_ciphers accepts RFC 8446 TLS 1.3 cipher suites only.
 */
void test_validate_tls_ciphers_valid(void **unused) {
    SF_UNUSED(unused);
    assert_true(validate_tls_ciphers(AES_256));
    assert_true(validate_tls_ciphers(AES_128));
    assert_true(validate_tls_ciphers(AES_256 ":" AES_128));
    assert_true(validate_tls_ciphers(AES_128 ":" AES_256));
    assert_true(validate_tls_ciphers(AES_256 ":" AES_256));
    assert_true(validate_tls_ciphers(
        "TLS_AES_256_GCM_SHA384:TLS_AES_128_GCM_SHA256:TLS_CHACHA20_POLY1305_SHA256:"
        "TLS_AES_128_CCM_SHA256:TLS_AES_128_CCM_8_SHA256"));
}

void test_validate_tls_ciphers_unknown_name(void **unused) {
    SF_UNUSED(unused);
    // typo
    assert_false(validate_tls_ciphers("TLS_AES_128_GCM_SHA25"));
    // names are case-sensitive
    assert_false(validate_tls_ciphers("tls_aes_256_gcm_sha384"));
    // TLS 1.2 names (OpenSSL and IANA style)
    assert_false(validate_tls_ciphers("ECDHE-RSA-AES256-GCM-SHA384"));
    assert_false(validate_tls_ciphers("TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384"));
    // one bad entry fails the whole list
    assert_false(validate_tls_ciphers(AES_256 ":ECDHE-RSA-AES256-GCM-SHA384"));
    // wrong separator
    assert_false(validate_tls_ciphers(AES_256 "," AES_128));
    // whitespace inside a name is not trimmed
    assert_false(validate_tls_ciphers("TLS_AES_256 GCM_SHA384"));
}

void test_validate_tls_ciphers_empty_entry(void **unused) {
    SF_UNUSED(unused);
    assert_false(validate_tls_ciphers(""));
    assert_false(validate_tls_ciphers(":"));
    assert_false(validate_tls_ciphers(":" AES_256));
    assert_false(validate_tls_ciphers(AES_256 ":"));
    assert_false(validate_tls_ciphers(AES_256 "::" AES_128));
}

void test_tls_ciphers_unset(void **unused) {
    SF_UNUSED(unused);
    SF_CONNECT *sf = create_connection(NULL);
    assert_int_equal(_snowflake_check_connection_parameters(sf), SF_STATUS_SUCCESS);
    assert_null(sf->tls_ciphers);
    snowflake_term(sf);
}

void test_tls_ciphers_attribute(void **unused) {
    SF_UNUSED(unused);
    SF_CONNECT *sf = create_connection(AES_256 ":" AES_128);
    assert_int_equal(_snowflake_check_connection_parameters(sf), SF_STATUS_SUCCESS);
    assert_string_equal(sf->tls_ciphers, AES_256 ":" AES_128);

    char *value = NULL;
    assert_int_equal(snowflake_get_attribute(sf, SF_CON_TLS_CIPHERS, (void **)&value), SF_STATUS_SUCCESS);
    assert_string_equal(value, AES_256 ":" AES_128);
    snowflake_term(sf);
}

void test_tls_ciphers_attribute_invalid(void **unused) {
    SF_UNUSED(unused);
    SF_CONNECT *sf = create_connection("TLS_AES_256_GCM_SHA384:TLS_UNKNOWN");
    assert_int_equal(_snowflake_check_connection_parameters(sf), SF_STATUS_ERROR_GENERAL);
    assert_int_equal(sf->error.error_code, SF_STATUS_ERROR_BAD_CONNECTION_PARAMS);
    snowflake_term(sf);
}

void test_tls_ciphers_env(void **unused) {
    SF_UNUSED(unused);
    sf_setenv(TLS_CIPHERS_ENV, AES_128);
    SF_CONNECT *sf = create_connection(NULL);
    assert_int_equal(_snowflake_check_connection_parameters(sf), SF_STATUS_SUCCESS);
    assert_string_equal(sf->tls_ciphers, AES_128);
    snowflake_term(sf);
    sf_unsetenv(TLS_CIPHERS_ENV);
}

void test_tls_ciphers_env_invalid(void **unused) {
    SF_UNUSED(unused);
    sf_setenv(TLS_CIPHERS_ENV, "TLS_CHACHA20");
    SF_CONNECT *sf = create_connection(NULL);
    assert_int_equal(_snowflake_check_connection_parameters(sf), SF_STATUS_ERROR_GENERAL);
    assert_int_equal(sf->error.error_code, SF_STATUS_ERROR_BAD_CONNECTION_PARAMS);
    snowflake_term(sf);
    sf_unsetenv(TLS_CIPHERS_ENV);
}

void test_tls_ciphers_attribute_overrides_env(void **unused) {
    SF_UNUSED(unused);
    sf_setenv(TLS_CIPHERS_ENV, "TLS_INVALID");
    SF_CONNECT *sf = create_connection(AES_256);
    assert_int_equal(_snowflake_check_connection_parameters(sf), SF_STATUS_SUCCESS);
    assert_string_equal(sf->tls_ciphers, AES_256);
    snowflake_term(sf);
    sf_unsetenv(TLS_CIPHERS_ENV);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_validate_tls_ciphers_valid),
        cmocka_unit_test(test_validate_tls_ciphers_unknown_name),
        cmocka_unit_test(test_validate_tls_ciphers_empty_entry),
        cmocka_unit_test(test_tls_ciphers_unset),
        cmocka_unit_test(test_tls_ciphers_attribute),
        cmocka_unit_test(test_tls_ciphers_attribute_invalid),
        cmocka_unit_test(test_tls_ciphers_env),
        cmocka_unit_test(test_tls_ciphers_env_invalid),
        cmocka_unit_test(test_tls_ciphers_attribute_overrides_env),
    };
    int ret = cmocka_run_group_tests(tests, save_env, restore_env);
    return ret;
}
