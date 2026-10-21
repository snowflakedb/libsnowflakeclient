#include <algorithm>
#include <fstream>
#include <vector>
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"
#include "snowflake/client.h"
#include "snowflake/IStatementPutGet.hpp"
#include "StatementPutGet.hpp"
#include "FileTransferAgent.hpp"
#include "snowflake/SFURL.hpp"
#include "snowflake/CurlDescPool.hpp"

/*
 * Test cases for TLS version setting (SF_GLOBAL_SSL_VERSION)
 * Each test case to set SF_GLOBAL_SSL_VERSION with different values:
 *   - unset
 *   - TLS1.2
 *   - TLS1.3
 * Needs to manually combine with TLS-terminating proxy to cover all scenarios
 *   - deault (allow both TLS 1.2 and 1.3)
 *   - capped at TLS 1.2 (test case with SF_GLOBAL_SSL_VERSION set to 1.3 would fail
 * Without a TLS-terminating proxy all test cases would always pass while we can verify
 * the TLS version being used through log output
 *
 * The negotiated version is asserted for the Snowflake API connection after every step,
 * and for the stage connection after PUT/GET only on GCS through the C++ API, where stage
 * requests go through the libsnowflakeclient curl pool. S3 and Azure stage requests use the
 * SDK curl handles, so their negotiated version is not asserted here.
 */

using namespace Snowflake::Client;

class RecordingStatementPutGet : public StatementPutGet
{
public:
  explicit RecordingStatementPutGet(SF_STMT *stmt) : StatementPutGet(stmt) {}

  bool http_put(std::string const& url,
                std::vector<std::string> const& headers,
                std::basic_iostream<char>& payload,
                size_t payloadLen,
                std::string& responseHeaders) override
  {
    m_stageUrl = url;
    return StatementPutGet::http_put(url, headers, payload, payloadLen, responseHeaders);
  }

  bool http_get(std::string const& url,
                std::vector<std::string> const& headers,
                std::basic_iostream<char>* payload,
                std::string& responseHeaders,
                bool headerOnly) override
  {
    m_stageUrl = url;
    return StatementPutGet::http_get(url, headers, payload, responseHeaders, headerOnly);
  }

  const std::string& getStageUrl() const
  {
    return m_stageUrl;
  }

private:
  std::string m_stageUrl;
};

/*
 * Verify the negotiated TLS version of the most recently used curl descriptor for the url.
 * The sub pool lookup mirrors get_curl_desc_from_pool(), including the proxy settings,
 * and the sub pool hands back the most recently freed descriptor first.
 */
void verify_negotiated_version(SF_CONNECT * sf, const std::string& urlStr,
                               const std::vector<std::string>& expected)
{
  SFURL url = SFURL::parse(urlStr);
  if (sf->proxy)
  {
    Util::Proxy proxy(sf->proxy);
    if (sf->no_proxy)
    {
      proxy.setNoProxy(sf->no_proxy);
    }
    url.setProxy(proxy);
  }
  CurlDescPool::SubPool& subPool = ClientCurlDescPool::getInstance().getSubPool(url);

  std::unique_ptr<CurlDesc> desc;
  subPool.newCurlDesc(desc);
  assert_non_null(desc.get());
  std::string negotiated = desc->getNegotiatedSSLVersion();
  assert_true(std::find(expected.begin(), expected.end(), negotiated) != expected.end());

  // recreate the handle so the next step negotiates and records its own version
  desc->reset(true);
  subPool.freeCurlDesc(desc);

  subPool.newCurlDesc(desc);
  assert_non_null(desc.get());
  assert_true(desc->getNegotiatedSSLVersion().empty());
  subPool.freeCurlDesc(desc);
}

void verify_api_negotiated_version(SF_CONNECT * sf, const std::vector<std::string>& expected)
{
  verify_negotiated_version(sf, std::string(sf->protocol) + "://" + sf->host + ":" + sf->port,
                            expected);
}

void verify_stage_negotiated_version(SF_CONNECT * sf, const RecordingStatementPutGet& stmtPutGet,
                                     const std::vector<std::string>& expected)
{
  if (!stmtPutGet.getStageUrl().empty())
  {
    verify_negotiated_version(sf, stmtPutGet.getStageUrl(), expected);
  }
}

/*
 * Helper function to execute same queries for each test scenario
 * 1. connect
 * 2. execute a simple select 1 query
 * 3. execute put query with a small file
 * 4. execute get query with the small file from the previous put
 * varify the negotiated TLS version with the latest request matches expected after each step
 */
void test_tls_version_core(const std::vector<std::string>& expected, bool native)
{
  /* init */
  SF_STATUS status;
  SF_CONNECT *sf;

  /*
   * Disable OCSP/Verify peer when manual testing with TLS terminating proxy.
   * When doing so, set/unset NO_PROXY with snowflakecomputing.com and test with
   * S3/Azure/GCP accounts to cover both GS/stage routes.
  sf_bool value = SF_BOOLEAN_FALSE;
  status = snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &value);
  assert_int_equal(SF_STATUS_SUCCESS, status);
  value = SF_BOOLEAN_TRUE;
  status = snowflake_global_set_attribute(SF_GLOBAL_DISABLE_VERIFY_PEER, &value);
  assert_int_equal(SF_STATUS_SUCCESS, status);
   */

  sf = setup_snowflake_connection();

  /* connect */
  status = snowflake_connect(sf);
  assert_int_equal(SF_STATUS_SUCCESS, status);
  verify_api_negotiated_version(sf, expected);
  
  SF_STMT *sfstmt = NULL;
  SF_STATUS ret;
  sfstmt = snowflake_stmt(sf);

  /* select 1*/
  ret = snowflake_query(sfstmt, "select 1", 0);
  assert_int_equal(SF_STATUS_SUCCESS, ret);
  verify_api_negotiated_version(sf, expected);

  /* put */
  std::string create_table("create or replace table test_small_put(c1 number"
                                    ", c2 number, c3 string)");
  ret = snowflake_query(sfstmt, create_table.c_str(), create_table.size());
  assert_int_equal(SF_STATUS_SUCCESS, ret);
  verify_api_negotiated_version(sf, expected);

  std::string dataDir = TestSetup::getDataDir();
  std::string file = dataDir + "small_file.csv";
  std::string putCommand = "put file://" + file + " @%test_small_put";

  if (native)
  {
    ret = snowflake_query(sfstmt, putCommand.c_str(), putCommand.size());
    assert_int_equal(SF_STATUS_SUCCESS, ret);
    assert_int_equal(snowflake_num_rows(sfstmt), 1);
    ret = snowflake_fetch(sfstmt);
    assert_int_equal(SF_STATUS_SUCCESS, ret);
    const char *out;
    snowflake_column_as_const_str(sfstmt, 7, &out);
    assert_string_equal("UPLOADED", out);
  }
  else
  {
    try
    {
      RecordingStatementPutGet stmtPutGet(sfstmt);

      TransferConfig transConfig;
      int32 tlsVersion = 0;
      ret = snowflake_global_get_attribute(SF_GLOBAL_SSL_VERSION, &tlsVersion, 0);
      assert_int_equal(SF_STATUS_SUCCESS, ret);
      transConfig.tlsVersion = (long)tlsVersion;
      Snowflake::Client::FileTransferAgent agent(&stmtPutGet, &transConfig);

      ITransferResult * results = agent.execute(&putCommand);
      assert_int_equal(1, results->getResultSize());
      assert_true(results->next());

      std::string value;
      results->getColumnAsString(6, value);
      assert_string_equal("UPLOADED", value.c_str());

      verify_stage_negotiated_version(sf, stmtPutGet, expected);
    }
    catch (...)
    {
      fail_msg("unexpected exception");
    }
  }
  verify_api_negotiated_version(sf, expected);

  /* get */
  std::string getCommand = "get @%test_small_put/small_file.csv.gz file://.";
  if (native)
  {
    status = snowflake_query(sfstmt, getCommand.c_str(), getCommand.size());
    assert_int_equal(SF_STATUS_SUCCESS, status);
    assert_int_equal(SF_STATUS_SUCCESS, ret);
    assert_int_equal(snowflake_num_rows(sfstmt), 1);
    ret = snowflake_fetch(sfstmt);
    assert_int_equal(SF_STATUS_SUCCESS, ret);
    const char *out;
    snowflake_column_as_const_str(sfstmt, 3, &out);
    assert_string_equal("DOWNLOADED", out);
  }
  else
  {
    try
    {
      RecordingStatementPutGet stmtPutGet(sfstmt);

      TransferConfig transConfig;
      int32 tlsVersion = 0;
      ret = snowflake_global_get_attribute(SF_GLOBAL_SSL_VERSION, &tlsVersion, 0);
      assert_int_equal(SF_STATUS_SUCCESS, ret);
      transConfig.tlsVersion = (long)tlsVersion;
      Snowflake::Client::FileTransferAgent agent(&stmtPutGet, &transConfig);

      ITransferResult * results = agent.execute(&getCommand);
      assert_int_equal(1, results->getResultSize());
      assert_true(results->next());

      std::string value;
      results->getColumnAsString(2, value);
      assert_string_equal("DOWNLOADED", value.c_str());

      verify_stage_negotiated_version(sf, stmtPutGet, expected);
    }
    catch (...)
    {
      fail_msg("unexpected exception");
    }
  }
  verify_api_negotiated_version(sf, expected);

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
}

void tls_version_unset_cpp(void **unused)
{
  SF_UNUSED(unused);
  // Without setting max the negotiated version could be 1.3
  test_tls_version_core({"TLSv1.2", "TLSv1.3"}, false);
}

void tls_version_unset_native(void **unused)
{
  SF_UNUSED(unused);
  // Without setting max the negotiated version could be 1.3
  test_tls_version_core({"TLSv1.2", "TLSv1.3"}, true);
}

void tls_version_v12_cpp(void **unused)
{
  SF_UNUSED(unused);
  int32 tlsVersion = (int32)(CURL_SSLVERSION_TLSv1_2 | CURL_SSLVERSION_MAX_TLSv1_2);
  SF_STATUS ret = snowflake_global_set_attribute(SF_GLOBAL_SSL_VERSION, &tlsVersion);
  assert_int_equal(SF_STATUS_SUCCESS, ret);
  test_tls_version_core({"TLSv1.2"}, false);
}

void tls_version_v12_native(void **unused)
{
  SF_UNUSED(unused);
  int32 tlsVersion = (int32)(CURL_SSLVERSION_TLSv1_2 | CURL_SSLVERSION_MAX_TLSv1_2);
  SF_STATUS ret = snowflake_global_set_attribute(SF_GLOBAL_SSL_VERSION, &tlsVersion);
  assert_int_equal(SF_STATUS_SUCCESS, ret);
  test_tls_version_core({"TLSv1.2"}, true);
}

void tls_version_v13_cpp(void **unused)
{
  SF_UNUSED(unused);
  int32 tlsVersion = (int32)(CURL_SSLVERSION_TLSv1_3 | CURL_SSLVERSION_MAX_TLSv1_3);
  SF_STATUS ret = snowflake_global_set_attribute(SF_GLOBAL_SSL_VERSION, &tlsVersion);
  assert_int_equal(SF_STATUS_SUCCESS, ret);
  test_tls_version_core({"TLSv1.3"}, false);
}

void tls_version_v13_native(void **unused)
{
  SF_UNUSED(unused);
  int32 tlsVersion = (int32)(CURL_SSLVERSION_TLSv1_3 | CURL_SSLVERSION_MAX_TLSv1_3);
  SF_STATUS ret = snowflake_global_set_attribute(SF_GLOBAL_SSL_VERSION, &tlsVersion);
  assert_int_equal(SF_STATUS_SUCCESS, ret);
  test_tls_version_core({"TLSv1.3"}, true);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);

    const struct CMUnitTest tests[] = {
      cmocka_unit_test(tls_version_unset_cpp),
      cmocka_unit_test(tls_version_unset_native),
      cmocka_unit_test(tls_version_v12_cpp),
      cmocka_unit_test(tls_version_v12_native),
      cmocka_unit_test(tls_version_v13_cpp),
      cmocka_unit_test(tls_version_v13_native),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
