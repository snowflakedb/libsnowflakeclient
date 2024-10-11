/*
 * Copyright (c) 2024 Snowflake Computing, Inc. All rights reserved.
 */

#include "utils/test_setup.h"

#define FAIL_OPEN_DEFAULT SF_BOOLEAN_FALSE

void setCacheFile(char *cache_file)
{
#ifdef __linux__
    char *home_env = getenv("HOME");
    strcpy(cache_file, (home_env == NULL ? (char*)"/tmp" : home_env));
    strcat(cache_file, "/.cache");
    strcat(cache_file, "/snowflake");
    strcat(cache_file, "/ocsp_response_cache.json");
#elif defined(__APPLE__)
    char *home_env = getenv("HOME");
    strcpy(cache_file, (home_env == NULL ? (char*)"/tmp" : home_env));
    strcat(cache_file, "/Library");
    strcat(cache_file, "/Caches");
    strcat(cache_file, "/Snowflake");
    strcat(cache_file, "/ocsp_response_cache.json");
#elif  defined(_WIN32)
    char *home_env = getenv("USERPROFILE");
    if (home_env == NULL)
    {
        home_env = getenv("TMP");
    }
    if (home_env == NULL)
    {
        home_env = getenv("TEMP");
    }
    strcpy(cache_file, (home_env == NULL ? (char*)"c:\\temp" : home_env));
    strcat(cache_file, "\\AppData");
    strcat(cache_file, "\\Local");
    strcat(cache_file, "\\Snowflake");
    strcat(cache_file, "\\Caches");
    strcat(cache_file, "\\ocsp_response_cache.json");
#endif
}

void test_fail_open_revoked(void **unused) {
    char cache_file[4096];
    setCacheFile(cache_file);
    remove(cache_file);
    sf_setenv("SF_OCSP_TEST_MODE", "true");
    sf_setenv("SF_TEST_OCSP_CERT_STATUS_REVOKED", "true");
    sf_setenv("SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED", "false");

    SF_CONNECT *sf = setup_snowflake_connection();

    sf_bool value = SF_BOOLEAN_TRUE;
    if (value != FAIL_OPEN_DEFAULT)
    {
        snowflake_set_attribute(sf, SF_CON_OCSP_FAIL_OPEN, &value);
    }

    SF_STATUS ret = snowflake_connect(sf);
    assert_int_not_equal(ret, SF_STATUS_SUCCESS); // must fail
    SF_ERROR_STRUCT *sferr = snowflake_error(sf);
    if (sferr->error_code != SF_STATUS_ERROR_CURL) {
        dump_error(sferr);
    }
    assert_int_equal(sferr->error_code, SF_STATUS_ERROR_CURL);
    snowflake_term(sf);
}

void test_fail_close_timeout(void** unused) {
    char cache_file[4096];
    setCacheFile(cache_file);
    remove(cache_file);
    sf_setenv("SF_OCSP_TEST_MODE", "true");
    sf_setenv("SF_TEST_CA_OCSP_RESPONDER_CONNECTION_TIMEOUT", "5");
    sf_setenv("SF_TEST_OCSP_URL", "http://httpbin.org/delay/10");
    sf_setenv("SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED", "false");

    SF_CONNECT* sf = setup_snowflake_connection();
    sf_bool value = SF_BOOLEAN_FALSE;
    if (value != FAIL_OPEN_DEFAULT)
    {
        snowflake_set_attribute(sf, SF_CON_OCSP_FAIL_OPEN, &value);
    }

    SF_STATUS ret = snowflake_connect(sf);
    assert_int_not_equal(ret, SF_STATUS_SUCCESS); // must fail
    SF_ERROR_STRUCT* sferr = snowflake_error(sf);
    if (sferr->error_code != SF_STATUS_ERROR_CURL) {
        dump_error(sferr);
    }
    assert_int_equal(sferr->error_code, SF_STATUS_ERROR_CURL);
    snowflake_term(sf);
}

void test_fail_open_timeout(void** unused) {
    char cache_file[4096];
    setCacheFile(cache_file);
    remove(cache_file);
    sf_setenv("SF_OCSP_TEST_MODE", "true");
    sf_setenv("SF_TEST_CA_OCSP_RESPONDER_CONNECTION_TIMEOUT", "5");
    sf_setenv("SF_TEST_OCSP_URL", "http://httpbin.org/delay/10");
    sf_setenv("SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED", "false");

    SF_CONNECT* sf = setup_snowflake_connection();
    sf_bool value = SF_BOOLEAN_TRUE;
    if (value != FAIL_OPEN_DEFAULT)
    {
        snowflake_set_attribute(sf, SF_CON_OCSP_FAIL_OPEN, &value);
    }

    SF_STATUS ret = snowflake_connect(sf);
    if (ret != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(ret, SF_STATUS_SUCCESS);
    snowflake_term(sf);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_fail_open_revoked),
        cmocka_unit_test(test_fail_close_timeout),
        cmocka_unit_test(test_fail_open_timeout),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
