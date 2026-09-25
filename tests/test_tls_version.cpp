#include <fstream>
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
 */

/*
 * Helper function to get negotiated TLS version for the last request from the connection
 */
using namespace Snowflake::Client;
std::string get_negotiated(SF_CONNECT * sf)
{
  SFURL url = SFURL::parse(std::string(sf->protocol) + "://" + sf->host + ":" + sf->port);
  std::unique_ptr<CurlDesc> desc;
  // assume this would resuse the curl instance from the latest request
  ClientCurlDescPool::getInstance().getSubPool(url).newCurlDesc(desc);
  return desc ? desc->getNegotiatedSSLVersion() : std::string();
}

/*
 * Helper function to execute same queries for each test scenario
 * 1. connect
 * 2. execute a simple select 1 query
 * 3. execute put query with a small file
 * 4. execute get query with the small file from the previous put
 * varify the negotiated TLS version with the latest request matches expected after each step
 */
void test_tls_version_core(const std::string expected, bool native)
{
  /* init */
  SF_STATUS status;
  SF_CONNECT *sf;

  sf = setup_snowflake_connection();

  /* connect */
  status = snowflake_connect(sf);
  assert_int_equal(SF_STATUS_SUCCESS, status);
  assert_string_equal(get_negotiated(sf).c_str(), expected.c_str());
  
  SF_STMT *sfstmt = NULL;
  SF_STATUS ret;
  sfstmt = snowflake_stmt(sf);

  /* select 1*/
  ret = snowflake_query(sfstmt, "select 1", 0);
  assert_int_equal(SF_STATUS_SUCCESS, ret);
  assert_string_equal(get_negotiated(sf).c_str(), expected.c_str());

  /* put */
  std::string create_table("create or replace table test_small_put(c1 number"
                                    ", c2 number, c3 string)");
  ret = snowflake_query(sfstmt, create_table.c_str(), create_table.size());
  assert_int_equal(SF_STATUS_SUCCESS, ret);
  assert_string_equal(get_negotiated(sf).c_str(), expected.c_str());

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
      std::unique_ptr<IStatementPutGet> stmtPutGet;
      stmtPutGet = std::unique_ptr
       <StatementPutGet>(new Snowflake::Client::StatementPutGet(sfstmt));

      TransferConfig transConfig;
      int32 tlsVersion = 0;
      ret = snowflake_global_get_attribute(SF_GLOBAL_SSL_VERSION, &tlsVersion, 0);
      assert_int_equal(SF_STATUS_SUCCESS, ret);
	  transConfig.tlsVersion = (long)tlsVersion;
      Snowflake::Client::FileTransferAgent agent(stmtPutGet.get(), &transConfig);

      ITransferResult * results = agent.execute(&putCommand);
      assert_int_equal(1, results->getResultSize());
      assert_true(results->next());

      std::string value;
      results->getColumnAsString(6, value);
      assert_string_equal("UPLOADED", value.c_str());
    }
    catch (...)
    {
      fail_msg("unexpected exception");
    }
  }
  assert_string_equal(get_negotiated(sf).c_str(), expected.c_str());

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
      std::unique_ptr<IStatementPutGet> stmtPutGet;
      stmtPutGet = std::unique_ptr
       <StatementPutGet>(new Snowflake::Client::StatementPutGet(sfstmt));

      TransferConfig transConfig;
      int32 tlsVersion = 0;
      ret = snowflake_global_get_attribute(SF_GLOBAL_SSL_VERSION, &tlsVersion, 0);
      assert_int_equal(SF_STATUS_SUCCESS, ret);
	  transConfig.tlsVersion = (long)tlsVersion;
      Snowflake::Client::FileTransferAgent agent(stmtPutGet.get(), &transConfig);

      ITransferResult * results = agent.execute(&getCommand);
      assert_int_equal(1, results->getResultSize());
      assert_true(results->next());

      std::string value;
      results->getColumnAsString(2, value);
      assert_string_equal("DOWNLOADED", value.c_str());
    }
    catch (...)
    {
      fail_msg("unexpected exception");
    }
  }
  assert_string_equal(get_negotiated(sf).c_str(), expected.c_str());

  snowflake_stmt_term(sfstmt);
  snowflake_term(sf);
}

void tls_version_unset_cpp(void **unused)
{
  SF_UNUSED(unused);
  test_tls_version_core("TLSv1.2", false);
}

void tls_version_unset_native(void **unused)
{
  SF_UNUSED(unused);
  test_tls_version_core("TLSv1.2", true);
}

void tls_version_v12_cpp(void **unused)
{
  SF_UNUSED(unused);
  int32 tlsVersion = (int32)CURL_SSLVERSION_TLSv1_2;
  SF_STATUS ret = snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &tlsVersion);
  assert_int_equal(SF_STATUS_SUCCESS, ret);
  test_tls_version_core("TLSv1.2", false);
}

void tls_version_v12_native(void **unused)
{
  SF_UNUSED(unused);
  int32 tlsVersion = (int32)CURL_SSLVERSION_TLSv1_2;
  SF_STATUS ret = snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &tlsVersion);
  assert_int_equal(SF_STATUS_SUCCESS, ret);
  test_tls_version_core("TLSv1.2", true);
}

void tls_version_v13_cpp(void **unused)
{
  SF_UNUSED(unused);
  int32 tlsVersion = (int32)CURL_SSLVERSION_TLSv1_3;
  SF_STATUS ret = snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &tlsVersion);
  assert_int_equal(SF_STATUS_SUCCESS, ret);
  test_tls_version_core("TLSv1.3", false);
}

void tls_version_v13_native(void **unused)
{
  SF_UNUSED(unused);
  int32 tlsVersion = (int32)CURL_SSLVERSION_TLSv1_3;
  SF_STATUS ret = snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &tlsVersion);
  assert_int_equal(SF_STATUS_SUCCESS, ret);
  test_tls_version_core("TLSv1.3", true);
}

int main(void) {
    initialize_test(SF_BOOLEAN_TRUE);
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
