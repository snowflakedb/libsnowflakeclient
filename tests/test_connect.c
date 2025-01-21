//
// Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
//

#include "utils/test_setup.h"

/**
 * Test connection with null context
 */
void test_null_sf_connect(void **unused) {
    SF_CONNECT *sf = NULL;
    // Try connecting with a NULL connection struct, should fail
    SF_STATUS status = snowflake_connect(sf);
    assert_int_not_equal(status, SF_STATUS_SUCCESS);
}

/**
 * Test connection without parameters
 */
void test_no_connection_parameters(void **unused) {
    SF_CONNECT *sf = snowflake_init();
    SF_STATUS status = snowflake_connect(sf);
    assert_int_not_equal(status, SF_STATUS_SUCCESS);
    SF_ERROR_STRUCT *error = snowflake_error(sf);
    assert_int_equal(error->error_code, SF_STATUS_ERROR_BAD_CONNECTION_PARAMS);
    snowflake_term(sf);
}

/**
 * Test connection with minimum parameter set
 */
void test_connect_with_minimum_parameters(void **unused) {
    SF_CONNECT *sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT,
                            getenv("SNOWFLAKE_TEST_ACCOUNT"));
    snowflake_set_attribute(sf, SF_CON_USER, getenv("SNOWFLAKE_TEST_USER"));
    snowflake_set_attribute(sf, SF_CON_PASSWORD,
                            getenv("SNOWFLAKE_TEST_PASSWORD"));
    char *host, *port, *protocol;
    host = getenv("SNOWFLAKE_TEST_HOST");
    if (host) {
        snowflake_set_attribute(sf, SF_CON_HOST, host);
    }
    port = getenv("SNOWFLAKE_TEST_PORT");
    if (port) {
        snowflake_set_attribute(sf, SF_CON_PORT, port);
    }
    protocol = getenv("SNOWFLAKE_TEST_PROTOCOL");
    if (protocol) {
        snowflake_set_attribute(sf, SF_CON_PROTOCOL, protocol);
    }

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    snowflake_term(sf);
}

/**
 * Test connection with full parameter set
 */
void test_connect_with_full_parameters(void **unused) {
    SF_CONNECT *sf = setup_snowflake_connection();

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    snowflake_term(sf); // purge snowflake context
}

/**
* Test connection with disableQueryContextCache
*/
void test_connect_with_disable_qcc(void **unused) {
  SF_CONNECT *sf = setup_snowflake_connection();

  sf_bool disable_qcc = SF_BOOLEAN_TRUE;
  void* value = NULL;
  snowflake_set_attribute(sf, SF_CON_DISABLE_QUERY_CONTEXT_CACHE, &disable_qcc);

  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
    }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  snowflake_get_attribute(sf, SF_CON_DISABLE_QUERY_CONTEXT_CACHE, &value);
  assert_true(*((sf_bool *)value) == SF_BOOLEAN_TRUE);

  snowflake_term(sf); // purge snowflake context
}

/**
* Test connection with includeRetryReason
*/
void test_connect_with_include_retry_context(void **unused) {
  SF_CONNECT *sf = setup_snowflake_connection();

  sf_bool include_retry_reason = SF_BOOLEAN_FALSE;
  void* value = NULL;
  snowflake_set_attribute(sf, SF_CON_INCLUDE_RETRY_REASON, &include_retry_reason);

  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  snowflake_get_attribute(sf, SF_CON_INCLUDE_RETRY_REASON, &value);
  assert_true(*((sf_bool *)value) == SF_BOOLEAN_FALSE);

  snowflake_term(sf); // purge snowflake context
}

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
	if (home_env == NULL)
    {
      home_env = getenv("TEMP");
    }
  }
  strcpy(cache_file, (home_env == NULL ? (char*)"c:\\temp" : home_env));
  strcat(cache_file, "\\AppData");
  strcat(cache_file, "\\Local");
  strcat(cache_file, "\\Snowflake");
  strcat(cache_file, "\\Caches");
  strcat(cache_file, "\\ocsp_response_cache.json");
#endif
}

/**
 * Test connection with OCSP cache server off
 */
void test_connect_with_ocsp_cache_server_off(void **unused) {
    char cache_file[4096];
    setCacheFile(cache_file);
    remove(cache_file);
    sf_setenv("SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED", "false");
    SF_CONNECT *sf = setup_snowflake_connection();

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    snowflake_term(sf); // purge snowflake context
}

/**
 * Test connection with OCSP cache server on
 */
void test_connect_with_ocsp_cache_server_on(void **unused) {
    char cache_file[4096];
    setCacheFile(cache_file);
    remove(cache_file);
    sf_setenv("SF_OCSP_RESPONSE_CACHE_SERVER_ENABLED", "true");
    SF_CONNECT *sf = setup_snowflake_connection();

    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    }
    assert_int_equal(status, SF_STATUS_SUCCESS);
    snowflake_term(sf); // purge snowflake context
}

/**
* Test connection with proxy parameter
* We don't really test with proxy because that would need a proxy server to be
* available in the test environment. Instead, set invalid proxy in environment
* variables and disable the proxy through proxy parameter to ensure the settings
* in parameter are being used.
*/
void test_connect_with_proxy(void** unused) {
  SKIP_IF_PROXY_ENV_IS_SET;

  // set invalid proxy in environment variables
  sf_setenv("https_proxy", "a.b.c");
  sf_setenv("http_proxy", "a.b.c");
  sf_unsetenv("no_proxy");
  SF_CONNECT* sf = setup_snowflake_connection();

  // ensure the connection fails with invalid proxy
  SF_STATUS status = snowflake_connect(sf);
  assert_int_not_equal(status, SF_STATUS_SUCCESS); // must fail
  snowflake_term(sf);

  // test proxy setting
  sf = setup_snowflake_connection();
  snowflake_set_attribute(sf, SF_CON_PROXY, "");
  status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);
  snowflake_term(sf);

  // test no proxy setting
  sf = setup_snowflake_connection();
  snowflake_set_attribute(sf, SF_CON_PROXY, "a.b.c");
  snowflake_set_attribute(sf, SF_CON_NO_PROXY, "*");
  status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);
  snowflake_term(sf);

  sf_unsetenv("https_proxy");
  sf_unsetenv("http_proxy");
}

/**
* Test connection with session token renew
* Need accountadmin privilege to change parameters.
* Ignore when changing parameter fails.
*/

void renew_session_token_setup() {
  SF_CONNECT* sf = setup_snowflake_connection();

  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  SF_STMT *sfstmt = snowflake_stmt(sf);
  status = snowflake_query(sfstmt, "use role accountadmin;", 0);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
  }
  status = snowflake_query(sfstmt, "alter system set MASTER_TOKEN_VALIDITY=600, SESSION_TOKEN_VALIDITY=5;", 0);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
  }

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
}

void renew_session_token_teardown() {
  SF_CONNECT* sf = setup_snowflake_connection();

  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  SF_STMT* sfstmt = snowflake_stmt(sf);
  status = snowflake_query(sfstmt, "use role accountadmin;", 0);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
  }
  status = snowflake_query(sfstmt, "alter system set MASTER_TOKEN_VALIDITY=default, SESSION_TOKEN_VALIDITY=default;", 0);
  if (status != SF_STATUS_SUCCESS) {
    dump_error(&(sfstmt->error));
  }

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
}

void test_connect_with_renew(void** unused) {
  UNUSED(unused);

  renew_session_token_setup();

  SF_CONNECT* sf = setup_snowflake_connection();

  SF_STATUS status = snowflake_connect(sf);
  assert_int_equal(status, SF_STATUS_SUCCESS);

  SF_STMT* sfstmt = snowflake_stmt(sf);

  // The query will be completed.
  status = snowflake_query(sfstmt, "select seq8() from table(generator(timelimit=>7))", 0);
  assert_int_equal(status, SF_STATUS_SUCCESS);

  // The query will renew the session token in the beginning and be completed.
  status = snowflake_query(sfstmt, "select seq8() from table(generator(timelimit=>3))", 0);
  assert_int_equal(status, SF_STATUS_SUCCESS);

  // The query will renew the session token in the middle of running.
  status = snowflake_query(sfstmt, "select seq8() from table(generator(timelimit=>60))", 0);
  assert_int_equal(status, SF_STATUS_SUCCESS);

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
  renew_session_token_teardown();
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_null_sf_connect),
      cmocka_unit_test(test_no_connection_parameters),
      cmocka_unit_test(test_connect_with_minimum_parameters),
      cmocka_unit_test(test_connect_with_full_parameters),
      cmocka_unit_test(test_connect_with_ocsp_cache_server_off),
      cmocka_unit_test(test_connect_with_ocsp_cache_server_on),
      cmocka_unit_test(test_connect_with_proxy),
      cmocka_unit_test(test_connect_with_renew),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
