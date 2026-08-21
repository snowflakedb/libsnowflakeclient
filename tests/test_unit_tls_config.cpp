/*
 * Unit tests for the shared TLS-version configuration module (tls_config) and
 * the StatementPutGet bridge that exposes the session's per-connection TLS
 * version. No network I/O: assertions are on CURLcode return values and the
 * bridged tls_version.
 *
 * The AWS SDK path (per-client TLS via the patched ClientConfiguration.tlsVersion
 * field applied in CurlHttpClient) is exercised at integration level with a
 * rebuilt AWS SDK; the ISdkWrapper tlsVersion passthrough is covered by
 * test_create_wif_attestation.
 */

// StatementPutGet.hpp (via <iostream>) must precede cmocka (utils/test_setup.h):
// cmocka macroizes "inline", which the C++ standard library headers reject if
// they are processed afterwards.
#include "snowflake/client.h"
#include "StatementPutGet.hpp"
#include <curl/curl.h>
#include "../lib/tls_config.h"
#include "utils/test_setup.h"

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

// ---- sf_resolve_tls_version ------------------------------------------------

// An explicit per-connection value wins over the global SSL_VERSION.
void test_resolve_tls_version_override_wins(void **) {
  assert_int_equal(sf_resolve_tls_version(CURL_SSLVERSION_TLSv1_3),
                   CURL_SSLVERSION_TLSv1_3);
}

// UNSET falls back to the global SSL_VERSION.
void test_resolve_tls_version_falls_back_to_global(void **) {
  int32 restore = CURL_SSLVERSION_TLSv1_2;
  snowflake_global_get_attribute(SF_GLOBAL_SSL_VERSION, &restore, sizeof(restore));

  int32 v = CURL_SSLVERSION_TLSv1_3;
  snowflake_global_set_attribute(SF_GLOBAL_SSL_VERSION, &v);
  assert_int_equal(sf_resolve_tls_version(SF_TLS_VERSION_UNSET),
                   CURL_SSLVERSION_TLSv1_3);

  snowflake_global_set_attribute(SF_GLOBAL_SSL_VERSION, &restore);
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
    cmocka_unit_test(test_resolve_tls_version_override_wins),
    cmocka_unit_test(test_resolve_tls_version_falls_back_to_global),
    cmocka_unit_test(test_statement_put_get_tls_version),
    cmocka_unit_test(test_statement_put_get_tls_version_null_stmt),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
