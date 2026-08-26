/*
 * Manual test: verify the per-connection TLS version actually governs the
 * protocol negotiated on the wire.
 *
 * Run manually against a real account (like test_manual_connect):
 *   set SNOWFLAKE_MANUAL_TEST_TYPE=test_manual_tls_negotiation
 *   plus SNOWFLAKE_TEST_ACCOUNT/USER/PASSWORD (+ HOST/PORT/PROTOCOL/CA_BUNDLE).
 * Otherwise it self-skips.
 *
 * Flow: connect with an exact-pinned TLS version (real creds -> the handshake
 * completes and the pooled CurlDesc captures the negotiated version via its
 * PREREQ callback), then pull that CurlDesc from the same ClientCurlDescPool
 * sub-pool (keyed by endpoint + TLS version) and read getNegotiatedSSLVersion().
 * Exact pinning (| CURL_SSLVERSION_MAX_*) makes it deterministic: TLS 1.2 and
 * TLS 1.3 must produce different, matching strings.
 */

// C++ (std-lib-bearing) headers must precede cmocka (utils/test_setup.h):
// cmocka macroizes "inline", which the C++ standard headers reject afterwards.
#include <cstring>
#include <memory>
#include <string>
#include "snowflake/client.h"
#include "snowflake/CurlDescPool.hpp"
#include "snowflake/SFURL.hpp"
#include <curl/curl.h>
#include "utils/test_setup.h"

using namespace Snowflake::Client;

// Connect with the given exact-pinned TLS version (same attribute setup as
// test_mfa_connect_with_duo_push), then read the negotiated protocol back from
// the CurlDesc the connection used.
static std::string connect_and_get_negotiated(int tlsVersion)
{
  SF_CONNECT* sf = snowflake_init();
  snowflake_set_attribute(sf, SF_CON_ACCOUNT, getenv("SNOWFLAKE_TEST_ACCOUNT"));
  snowflake_set_attribute(sf, SF_CON_USER, getenv("SNOWFLAKE_TEST_USER"));
  snowflake_set_attribute(sf, SF_CON_PASSWORD, getenv("SNOWFLAKE_TEST_PASSWORD"));

  char* host = getenv("SNOWFLAKE_TEST_HOST");
  if (host)
  {
    snowflake_set_attribute(sf, SF_CON_HOST, host);
  }
  char* port = getenv("SNOWFLAKE_TEST_PORT");
  if (port)
  {
    snowflake_set_attribute(sf, SF_CON_PORT, port);
  }
  char* protocol = getenv("SNOWFLAKE_TEST_PROTOCOL");
  if (protocol)
  {
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, protocol);
  }

  snowflake_set_attribute(sf, SF_CON_TLS_VERSION, &tlsVersion);

  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS)
  {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

  // Rebuild the endpoint URL from the fields the C core used
  // (connection.c builds "<protocol>://<host>:<port>"), so the sub-pool key
  // matches exactly regardless of whether host came from env or the account.
  SFURL url = SFURL::parse(std::string(sf->protocol) + "://" + sf->host + ":" + sf->port);
  url.setTlsVersion(tlsVersion);

  std::unique_ptr<CurlDesc> desc;
  ClientCurlDescPool::getInstance().getSubPool(url).newCurlDesc(desc);
  std::string neg = desc ? desc->getNegotiatedSSLVersion() : std::string();

  snowflake_term(sf);
  return neg;
}

void test_manual_tls_negotiation(void** unused)
{
  SF_UNUSED(unused);
  const char* manual_test = getenv("SNOWFLAKE_MANUAL_TEST_TYPE");
  if (manual_test == NULL ||
      strcmp(manual_test, "test_manual_tls_negotiation") != 0)
  {
    printf("This test was skipped.\n");
    return;
  }

  int v12 = CURL_SSLVERSION_TLSv1_2 | CURL_SSLVERSION_MAX_TLSv1_2;
  int v13 = CURL_SSLVERSION_TLSv1_3 | CURL_SSLVERSION_MAX_TLSv1_3;

  assert_string_equal(connect_and_get_negotiated(v12).c_str(), "TLSv1.2");
  assert_string_equal(connect_and_get_negotiated(v13).c_str(), "TLSv1.3");
}

int main(void)
{
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_manual_tls_negotiation),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  snowflake_global_term();
  return ret;
}
