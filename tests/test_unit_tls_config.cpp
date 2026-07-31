/*
 * Unit tests for the shared TLS-version configuration module (tls_config) and
 * the AWS custom curl HTTP client that carries a per-connection TLS version
 * into the AWS SDK. No network I/O: assertions are on CURLcode return values
 * and the stamped tls_version (via a test-seam accessor).
 */

#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/http/HttpClientFactory.h>

#include <curl/curl.h>
#include "utils/test_setup.h"
#include "../lib/tls_config.h"
#include "snowflake/client.h"
#include "snowflake/AWSUtils.hpp"
#include "AwsTlsHttpClient.hpp"
#include "StatementPutGet.hpp"

using namespace Snowflake::Client;

// ---- sf_apply_tls_version --------------------------------------------------

void test_apply_tls_version_null_handle_is_noop(void **) {
  assert_int_equal(sf_apply_tls_version(NULL, CURL_SSLVERSION_TLSv1_2), CURLE_OK);
}

void test_apply_tls_version_unset_is_noop(void **) {
  CURL *handle = curl_easy_init();
  assert_non_null(handle);
  assert_int_equal(sf_apply_tls_version(handle, SF_TLS_VERSION_UNSET), CURLE_OK);
  curl_easy_cleanup(handle);
}

void test_apply_tls_version_sets_version(void **) {
  CURL *handle = curl_easy_init();
  assert_non_null(handle);
  assert_int_equal(sf_apply_tls_version(handle, CURL_SSLVERSION_TLSv1_2), CURLE_OK);
  curl_easy_cleanup(handle);
}

// ---- AWS custom client layer ----------------------------------------------

// Direct construction stamps the requested version.
void test_tls_curl_http_client_stamps_version(void **) {
  auto awsSdk = AwsUtils::initAwsSdk();
  Aws::Client::ClientConfiguration cfg;

  TlsCurlHttpClient direct(cfg, CURL_SSLVERSION_TLSv1_3);
  assert_int_equal((int)direct.getConfiguredTlsVersion(), CURL_SSLVERSION_TLSv1_3);
}

// The registered factory reads the ScopedAwsTlsVersion thread-local: inside the
// scope the created client is stamped with the scoped version; outside, UNSET.
void test_scoped_aws_tls_version_flows_through_factory(void **) {
  auto awsSdk = AwsUtils::initAwsSdk();
  Aws::Client::ClientConfiguration cfg;

  {
    ScopedAwsTlsVersion guard(CURL_SSLVERSION_TLSv1_2);
    auto client = Aws::Http::CreateHttpClient(cfg);
    auto *tlsClient = dynamic_cast<TlsCurlHttpClient *>(client.get());
    assert_non_null(tlsClient);
    assert_int_equal((int)tlsClient->getConfiguredTlsVersion(), CURL_SSLVERSION_TLSv1_2);
  }

  auto client2 = Aws::Http::CreateHttpClient(cfg);
  auto *tlsClient2 = dynamic_cast<TlsCurlHttpClient *>(client2.get());
  assert_non_null(tlsClient2);
  assert_int_equal((int)tlsClient2->getConfiguredTlsVersion(), SF_TLS_VERSION_UNSET);
}

// ---- StatementPutGet::get_tls_version() ------------------------------------

// The C++ storage path reads the session's tls_version through this bridge.
void test_statement_put_get_tls_version(void **) {
  SF_CONNECT *conn = snowflake_init();
  conn->tls_version = CURL_SSLVERSION_TLSv1_2;
  SF_STMT *stmt = snowflake_stmt(conn);

  StatementPutGet sp(stmt);
  assert_int_equal(sp.get_tls_version(), CURL_SSLVERSION_TLSv1_2);

  snowflake_stmt_term(stmt);
  snowflake_term(conn);
}

// A statement with no connection falls back to UNSET (no override).
void test_statement_put_get_tls_version_null_stmt(void **) {
  StatementPutGet sp(NULL);
  assert_int_equal(sp.get_tls_version(), SF_TLS_VERSION_UNSET);
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_apply_tls_version_null_handle_is_noop),
    cmocka_unit_test(test_apply_tls_version_unset_is_noop),
    cmocka_unit_test(test_apply_tls_version_sets_version),
    cmocka_unit_test(test_tls_curl_http_client_stamps_version),
    cmocka_unit_test(test_scoped_aws_tls_version_flows_through_factory),
    cmocka_unit_test(test_statement_put_get_tls_version),
    cmocka_unit_test(test_statement_put_get_tls_version_null_stmt),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
