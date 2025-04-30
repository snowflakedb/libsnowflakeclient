/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include "connection.h"
#include "client_int.h"
#include "utils/test_setup.h"
#ifndef _WIN32
#include <unistd.h>
#endif

#define URL_QUERY "http://snowflake.com/queries/v1/query-request?request_guid=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
#define URL_NON_QUERY_WITH_GUID "http://snowflake.com/other-path?request_guid=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
#define URL_NO_GUID "http://snowflake.com/other-path"
#define GUID_LENGTH strlen("request_guid=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx")
#define CLIENT_START_TIME_LENGTH strlen("clientStartTime=xxxxxxxxxxxxx&")

#define URL_LOGIN "http://snowflake.com/session/v1/login-request"
#define URL_TOKEN "http://snowflake.com/session/token-request"
#define URL_AUTHENTICATOR "http://snowflake.com/session/authenticator-request"

void test_update_url_no_guid(void **unused) {
    SF_UNUSED(unused);
    char urlbuf[512];
    sf_sprintf(urlbuf, sizeof(urlbuf), "%s", URL_NO_GUID);
    RETRY_CONTEXT retry_ctx = {
      1,      //retry_count
      0,      // retry reason
      0,      // network_timeout
      1,      // time to sleep
      NULL    // Decorrelate jitter
    };

    sf_bool ret = retry_ctx_update_url(&retry_ctx, urlbuf, SF_BOOLEAN_TRUE);
    assert_int_equal(ret, SF_BOOLEAN_TRUE);
    assert_string_equal(urlbuf, URL_NO_GUID);
}

void test_update_other_url_with_guid(void **unused) {
  SF_UNUSED(unused);
  char urlbuf[512];
  sf_sprintf(urlbuf, sizeof(urlbuf), "%s", URL_NON_QUERY_WITH_GUID);
  RETRY_CONTEXT retry_ctx = {
    1,      //retry_count
    0,      // retry reason
    0,      // network_timeout
    1,      // time to sleep
    NULL    // Decorrelate jitter
  };

  sf_bool ret = retry_ctx_update_url(&retry_ctx, urlbuf, SF_BOOLEAN_TRUE);
  assert_int_equal(ret, SF_BOOLEAN_TRUE);

  // should only replace the guid and no retry count/reason added
  assert_int_equal(strlen(urlbuf), strlen(URL_NON_QUERY_WITH_GUID));
  assert_null(strstr(urlbuf, URL_PARAM_RETRY_COUNT));
  assert_null(strstr(urlbuf, URL_PARAM_RETRY_REASON));
  assert_null(strstr(urlbuf, URL_PARAM_CLIENT_START_TIME));
  char* guid_ptr = strstr(urlbuf, URL_PARAM_REQEST_GUID);
  assert_non_null(guid_ptr);
  assert_int_equal(strlen(guid_ptr), GUID_LENGTH);
  *guid_ptr = '\0';
  assert_int_equal(strncmp(URL_NON_QUERY_WITH_GUID, urlbuf, strlen(urlbuf)), 0);
}

void test_update_query_url_with_retry_reason_disabled(void **unused) {
  SF_UNUSED(unused);
  char urlbuf[512];
  sf_sprintf(urlbuf, sizeof(urlbuf), "%s", URL_QUERY);
  RETRY_CONTEXT retry_ctx = {
    1,      //retry_count
    429,    // retry reason
    0,      // network_timeout
    1,      // time to sleep
    NULL,   // Decorrelate jitter
    sf_get_current_time_millis() // start time
  };

  sf_bool ret = retry_ctx_update_url(&retry_ctx, urlbuf, SF_BOOLEAN_FALSE);
  assert_int_equal(ret, SF_BOOLEAN_TRUE);

  // ended with guid
  char* guid_ptr = strstr(urlbuf, URL_PARAM_REQEST_GUID);
  assert_non_null(guid_ptr);
  assert_int_equal(strlen(guid_ptr), GUID_LENGTH);
  *guid_ptr = '\0';

  // should have client start time added
  char* starttime_ptr = strstr(urlbuf, URL_PARAM_CLIENT_START_TIME);
  assert_non_null(starttime_ptr);
  assert_int_equal(strlen(starttime_ptr), CLIENT_START_TIME_LENGTH);
  *starttime_ptr = '\0';

  // should not have retry reason added
  char* retryreason_ptr = strstr(urlbuf, URL_PARAM_RETRY_REASON);
  assert_null(retryreason_ptr);

  // should have retry count added
  char* retrycount_ptr = strstr(urlbuf, URL_PARAM_RETRY_COUNT);
  assert_non_null(retrycount_ptr);
  assert_string_equal(retrycount_ptr, "retryCount=1&");
  *retrycount_ptr = '\0';

  // rest part should be unchanged
  assert_int_equal(strncmp(URL_QUERY, urlbuf, strlen(urlbuf)), 0);

  // recover the updated URL for testing next retry
  *guid_ptr = 'r';
  *retrycount_ptr = 'r';
  *starttime_ptr = 'c';
  retry_ctx.retry_count = 2;
  retry_ctx.retry_reason = 503;

  ret = retry_ctx_update_url(&retry_ctx, urlbuf, SF_BOOLEAN_FALSE);
  assert_int_equal(ret, SF_BOOLEAN_TRUE);

  // ended with guid
  guid_ptr = strstr(urlbuf, URL_PARAM_REQEST_GUID);
  assert_non_null(guid_ptr);
  assert_int_equal(strlen(guid_ptr), GUID_LENGTH);
  *guid_ptr = '\0';

  // should have client start time added
  starttime_ptr = strstr(urlbuf, URL_PARAM_CLIENT_START_TIME);
  assert_non_null(starttime_ptr);
  assert_int_equal(strlen(starttime_ptr), CLIENT_START_TIME_LENGTH);
  *starttime_ptr = '\0';

  // should not have retry reason added
  retryreason_ptr = strstr(urlbuf, URL_PARAM_RETRY_REASON);
  assert_null(retryreason_ptr);

  // should have retry count/reason updated
  retrycount_ptr = strstr(urlbuf, URL_PARAM_RETRY_COUNT);
  assert_non_null(retrycount_ptr);
  assert_string_equal(retrycount_ptr, "retryCount=2&");
  *retrycount_ptr = '\0';

  // rest part should be unchanged
  assert_int_equal(strncmp(URL_QUERY, urlbuf, strlen(urlbuf)), 0);
}

void test_update_query_url_with_retry_reason_enabled(void **unused) {
  SF_UNUSED(unused);
  char urlbuf[512];
  sf_sprintf(urlbuf, sizeof(urlbuf), "%s", URL_QUERY);
  RETRY_CONTEXT retry_ctx = {
    1,      //retry_count
    429,    // retry reason
    0,      // network_timeout
    1,      // time to sleep
    NULL,    // Decorrelate jitter
    sf_get_current_time_millis() // start time
  };
  sf_bool ret = retry_ctx_update_url(&retry_ctx, urlbuf, SF_BOOLEAN_TRUE);
  assert_int_equal(ret, SF_BOOLEAN_TRUE);

  // ended with guid
  char* guid_ptr = strstr(urlbuf, URL_PARAM_REQEST_GUID);
  assert_non_null(guid_ptr);
  assert_int_equal(strlen(guid_ptr), GUID_LENGTH);
  *guid_ptr = '\0';
  
  // should have client start time added
  char* starttime_ptr = strstr(urlbuf, URL_PARAM_CLIENT_START_TIME);
  assert_non_null(starttime_ptr);
  assert_int_equal(strlen(starttime_ptr), CLIENT_START_TIME_LENGTH);
  *starttime_ptr = '\0';

  // should have retry count/reason added
  char* retrycount_ptr = strstr(urlbuf, URL_PARAM_RETRY_COUNT);
  assert_non_null(retrycount_ptr);
  assert_string_equal(retrycount_ptr, "retryCount=1&retryReason=429&");
  *retrycount_ptr = '\0';

  // rest part should be unchanged
  assert_int_equal(strncmp(URL_QUERY, urlbuf, strlen(urlbuf)), 0);

  // recover the updated URL for testing next retry
  *guid_ptr = 'r';
  *retrycount_ptr = 'r';
  *starttime_ptr = 'c';
  retry_ctx.retry_count = 2;
  retry_ctx.retry_reason = 503;

  ret = retry_ctx_update_url(&retry_ctx, urlbuf, SF_BOOLEAN_TRUE);
  assert_int_equal(ret, SF_BOOLEAN_TRUE);

  // ended with guid
  guid_ptr = strstr(urlbuf, URL_PARAM_REQEST_GUID);
  assert_non_null(guid_ptr);
  assert_int_equal(strlen(guid_ptr), GUID_LENGTH);
  *guid_ptr = '\0';

  // should have client start time added
  starttime_ptr = strstr(urlbuf, URL_PARAM_CLIENT_START_TIME);
  assert_non_null(starttime_ptr);
  assert_int_equal(strlen(starttime_ptr), CLIENT_START_TIME_LENGTH);
  *starttime_ptr = '\0';

  // should have retry count/reason updated
  retrycount_ptr = strstr(urlbuf, URL_PARAM_RETRY_COUNT);
  assert_non_null(retrycount_ptr);
  assert_string_equal(retrycount_ptr, "retryCount=2&retryReason=503&");
  *retrycount_ptr = '\0';

  // rest part should be unchanged
  assert_int_equal(strncmp(URL_QUERY, urlbuf, strlen(urlbuf)), 0);
}

void test_new_retry_strategy(void **unused) {
  SF_UNUSED(unused);
  DECORRELATE_JITTER_BACKOFF djb = {
    SF_BACKOFF_BASE,    //base
    SF_NEW_STRATEGY_BACKOFF_CAP      //cap
  };
  RETRY_CONTEXT curl_retry_ctx = {
    0,      //retry_count
    0,      // retry reason
    SF_RETRY_TIMEOUT,
    djb.base,      // time to sleep
    &djb,    // Decorrelate jitter
    sf_get_current_time_millis() // start time
  };

  uint32 error_codes[SF_MAX_RETRY] = {429, 503, 403, 503, 408, 538, 525};
  uint32 backoff = SF_BACKOFF_BASE;
  uint32 next_sleep_in_secs = 0;
  uint32 total_backoff = 0;
  int retry_count = 0;

  srand(time(NULL));

  for (;retry_count < SF_MAX_RETRY; retry_count++)
  {
    assert_int_equal(is_retryable_http_code(error_codes[retry_count]), SF_BOOLEAN_TRUE);
    next_sleep_in_secs = retry_ctx_next_sleep(&curl_retry_ctx);
    // change the start time to mock the time elapsed
    curl_retry_ctx.start_time -= next_sleep_in_secs * 1000;
    total_backoff += next_sleep_in_secs;

    if (total_backoff + next_sleep_in_secs >= SF_LOGIN_TIMEOUT)
    {
      retry_count++;
      break;
    }
  }

  int delta = 10; // delta in case the time counter is not accurate
  if (total_backoff < SF_RETRY_TIMEOUT - delta)
  {
    // not reached the timeout, must reached the retry number first
    assert_int_equal(retry_count, SF_MAX_RETRY);
  }
  else
  {
    // reached the timeout
    assert_in_range(retry_count, 0, SF_MAX_RETRY);
    assert_in_range(total_backoff, SF_RETRY_TIMEOUT - delta, SF_RETRY_TIMEOUT + delta);
  }
}

void test_retry_request_header(void **unused) {
  SF_UNUSED(unused);
  struct TESTCASE {
    const char* url;
    sf_bool has_app_header;
  };

  struct TESTCASE cases[] = {
    { URL_LOGIN, SF_BOOLEAN_TRUE },
    { URL_TOKEN, SF_BOOLEAN_TRUE },
    { URL_AUTHENTICATOR, SF_BOOLEAN_TRUE },
    { URL_QUERY, SF_BOOLEAN_FALSE },
  };

  int casenum = sizeof(cases) / sizeof(struct TESTCASE);

  SF_HEADER* header = NULL;
  SF_CONNECT sf;
  sf.application_name = "test_app_name";
  sf.application_version = "0.0.0";
  for (int i = 0; i < casenum; i++)
  {
    header = sf_header_create();
    if (is_new_retry_strategy_url(cases[i].url))
    {
      add_appinfo_header(&sf, header, NULL);
    }
    sf_bool has_app_id = SF_BOOLEAN_FALSE;
    sf_bool has_app_ver = SF_BOOLEAN_FALSE;
    struct curl_slist* header_item = header->header;
    while (header_item)
    {
      if (strcmp(header_item->data, "CLIENT_APP_ID: test_app_name") == 0)
      {
        has_app_id = SF_BOOLEAN_TRUE;
      }
      if (strcmp(header_item->data, "CLIENT_APP_VERSION: 0.0.0") == 0)
      {
        has_app_ver = SF_BOOLEAN_TRUE;
      }
      header_item = header_item->next;
    }
    assert_int_equal(has_app_id, cases[i].has_app_header);
    assert_int_equal(has_app_ver, cases[i].has_app_header);

    sf_header_destroy(header);
  }
}

void test_retryable_http_code(void **unused) {
  SF_UNUSED(unused);
  struct TEST_CODE {
    uint32 code;
    sf_bool retryable;
  };

  struct TEST_CODE cases[] = {
    { 400, SF_BOOLEAN_FALSE },
    { 403, SF_BOOLEAN_TRUE },
    { 404, SF_BOOLEAN_FALSE },
    { 408, SF_BOOLEAN_TRUE },
    { 429, SF_BOOLEAN_TRUE },
    { 503, SF_BOOLEAN_TRUE },
    { 600, SF_BOOLEAN_FALSE },
  };

  for (unsigned i = 0; i < sizeof(cases) / sizeof(struct TEST_CODE); i++)
  {
    assert_int_equal(is_retryable_http_code(cases[i].code), cases[i].retryable);
  }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_update_url_no_guid),
        cmocka_unit_test(test_update_other_url_with_guid),
        cmocka_unit_test(test_update_query_url_with_retry_reason_disabled),
        cmocka_unit_test(test_update_query_url_with_retry_reason_enabled),
        cmocka_unit_test(test_new_retry_strategy),
        cmocka_unit_test(test_retry_request_header),
        cmocka_unit_test(test_retryable_http_code),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
