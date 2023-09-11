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

void test_update_url_no_guid(void **unused) {
    char urlbuf[512];
    sb_sprintf(urlbuf, sizeof(urlbuf), "%s", URL_NO_GUID);
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
  char urlbuf[512];
  sb_sprintf(urlbuf, sizeof(urlbuf), "%s", URL_NON_QUERY_WITH_GUID);
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
  char* guid_ptr = strstr(urlbuf, URL_PARAM_REQEST_GUID);
  assert_non_null(guid_ptr);
  assert_int_equal(strlen(guid_ptr), GUID_LENGTH);
  *guid_ptr = '\0';
  assert_int_equal(strncmp(URL_NON_QUERY_WITH_GUID, urlbuf, strlen(urlbuf)), 0);
}

void test_update_query_url_with_retry_reason_disabled(void **unused) {
  char urlbuf[512];
  sb_sprintf(urlbuf, sizeof(urlbuf), "%s", URL_QUERY);
  RETRY_CONTEXT retry_ctx = {
    1,      //retry_count
    429,    // retry reason
    0,      // network_timeout
    1,      // time to sleep
    NULL    // Decorrelate jitter
  };

  sf_bool ret = retry_ctx_update_url(&retry_ctx, urlbuf, SF_BOOLEAN_FALSE);
  assert_int_equal(ret, SF_BOOLEAN_TRUE);

  // ended with guid
  char* guid_ptr = strstr(urlbuf, URL_PARAM_REQEST_GUID);
  assert_non_null(guid_ptr);
  assert_int_equal(strlen(guid_ptr), GUID_LENGTH);
  *guid_ptr = '\0';

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
  retry_ctx.retry_count = 2;
  retry_ctx.retry_reason = 503;

  ret = retry_ctx_update_url(&retry_ctx, urlbuf, SF_BOOLEAN_FALSE);
  assert_int_equal(ret, SF_BOOLEAN_TRUE);

  // ended with guid
  guid_ptr = strstr(urlbuf, URL_PARAM_REQEST_GUID);
  assert_non_null(guid_ptr);
  assert_int_equal(strlen(guid_ptr), GUID_LENGTH);
  *guid_ptr = '\0';

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
  char urlbuf[512];
  sb_sprintf(urlbuf, sizeof(urlbuf), "%s", URL_QUERY);
  RETRY_CONTEXT retry_ctx = {
    1,      //retry_count
    429,    // retry reason
    0,      // network_timeout
    1,      // time to sleep
    NULL    // Decorrelate jitter
  };

  sf_bool ret = retry_ctx_update_url(&retry_ctx, urlbuf, SF_BOOLEAN_TRUE);
  assert_int_equal(ret, SF_BOOLEAN_TRUE);

  // ended with guid
  char* guid_ptr = strstr(urlbuf, URL_PARAM_REQEST_GUID);
  assert_non_null(guid_ptr);
  assert_int_equal(strlen(guid_ptr), GUID_LENGTH);
  *guid_ptr = '\0';
  
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
  retry_ctx.retry_count = 2;
  retry_ctx.retry_reason = 503;

  ret = retry_ctx_update_url(&retry_ctx, urlbuf, SF_BOOLEAN_TRUE);
  assert_int_equal(ret, SF_BOOLEAN_TRUE);

  // ended with guid
  guid_ptr = strstr(urlbuf, URL_PARAM_REQEST_GUID);
  assert_non_null(guid_ptr);
  assert_int_equal(strlen(guid_ptr), GUID_LENGTH);
  *guid_ptr = '\0';

  // should have retry count/reason updated
  retrycount_ptr = strstr(urlbuf, URL_PARAM_RETRY_COUNT);
  assert_non_null(retrycount_ptr);
  assert_string_equal(retrycount_ptr, "retryCount=2&retryReason=503&");
  *retrycount_ptr = '\0';

  // rest part should be unchanged
  assert_int_equal(strncmp(URL_QUERY, urlbuf, strlen(urlbuf)), 0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_update_url_no_guid),
        cmocka_unit_test(test_update_other_url_with_guid),
        cmocka_unit_test(test_update_query_url_with_retry_reason_disabled),
        cmocka_unit_test(test_update_query_url_with_retry_reason_enabled),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
