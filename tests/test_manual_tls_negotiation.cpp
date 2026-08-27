#include <cstring>
#include <memory>
#include <string>
#include "snowflake/client.h"
#include "snowflake/CurlDescPool.hpp"
#include "snowflake/SFURL.hpp"
#include <curl/curl.h>
#include "utils/test_setup.h"

using namespace Snowflake::Client;

static std::string connect_and_get_negotiated(int tlsVersion)
{
  SF_CONNECT * sf = setup_snowflake_connection();
  snowflake_set_attribute(sf, SF_CON_TLS_VERSION, &tlsVersion);

  SF_STATUS status = snowflake_connect(sf);
  if (status != SF_STATUS_SUCCESS)
  {
    dump_error(&(sf->error));
  }
  assert_int_equal(status, SF_STATUS_SUCCESS);

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
