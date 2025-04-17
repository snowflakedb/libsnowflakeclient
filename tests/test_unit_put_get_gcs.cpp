/**
 * Testing Put Get on GCS
 *
 * Note: IStatementPutGet is mocked
 */

#include "snowflake/IStatementPutGet.hpp"
#include "snowflake/PutGetParseResponse.hpp"
#include <fstream>
#include <memory>
#include <cstdio>
#include <snowflake/SnowflakeTransferException.hpp>
#include "util/Base64.hpp"
#include "FileTransferExecutionResult.hpp"
#include "FileTransferAgent.hpp"
#include "StorageClientFactory.hpp"
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"
#ifdef _WIN32
#include <Shellapi.h>
#define strcasecmp _stricmp
#endif

using namespace ::Snowflake::Client;

// Mocked statement for GCS down scoped token
class MockedGCSStatement : public Snowflake::Client::IStatementPutGet
{
public:
  MockedGCSStatement(bool useGcsToken,
                     bool useRegionalUrl = false,
                     std::string region = "",
                     std::string endpoint = "",
                     std::string expectedEndpoint = "storage.googleapis.com")
    : m_useGcsToken(useGcsToken),
      m_firstTime(true),
      m_isPut(false),
      Snowflake::Client::IStatementPutGet()
  {
    m_stageInfo.stageType = Snowflake::Client::StageType::GCS;
    m_stageInfo.useRegionalUrl = useRegionalUrl;
    m_stageInfo.region = region;
    m_stageInfo.endPoint = endpoint;
    if (m_useGcsToken)
    {
      m_stageInfo.credentials = { { "GCS_ACCESS_TOKEN", (char*)"fake gcs token"} };
      m_expectedUrl = std::string("https://") + expectedEndpoint + "/FakeGcsLocation/small_file.csv.gz";
    }
    else
    {
      m_stageInfo.credentials = { { "GCS_ACCESS_TOKEN", (char*)"" } };
      m_stageInfo.presignedUrl = "https://faked.presigned.url";
      m_expectedUrl = m_stageInfo.presignedUrl;
    }
    m_stageInfo.location = "FakeGcsLocation";
    std::string dataDir = TestSetup::getDataDir();
    m_srcLocationsPut.push_back(dataDir + "small_file.csv.gz");
    m_srcLocationsGet.push_back("small_file.csv.gz");
  }

  virtual bool parsePutGetCommand(std::string *sql,
                                  PutGetParseResponse *putGetParseResponse) override
  {
    putGetParseResponse->stageInfo = m_stageInfo;
    if (strcasecmp(sql->substr(0, 3).c_str(), "get") == 0)
    {
      putGetParseResponse->command = CommandType::DOWNLOAD;
      putGetParseResponse->srcLocations = m_srcLocationsGet;
    }
    else
    {
      m_isPut = true;
      putGetParseResponse->command = CommandType::UPLOAD;
      putGetParseResponse->srcLocations = m_srcLocationsPut;
      putGetParseResponse->threshold = DEFAULT_UPLOAD_DATA_SIZE_THRESHOLD;
      if (sql->find("OVERWRITE=true") != std::string::npos)
      {
        putGetParseResponse->overwrite = true;
      }
      else
      {
        putGetParseResponse->overwrite = false;
      }
    }
    if (!m_useGcsToken)
    {
      putGetParseResponse->presignedUrls.push_back(m_expectedUrl);
    }
    putGetParseResponse->sourceCompression = (char *)"NONE";
    putGetParseResponse->autoCompress = false;
    putGetParseResponse->parallel = 4;
    putGetParseResponse->encryptionMaterials = m_encryptionMaterial;
    putGetParseResponse->localLocation = (char *)".";

    return true;
  }

  virtual bool http_put(std::string const& url,
                        std::vector<std::string> const& headers,
                        std::basic_iostream<char>& payload,
                        size_t payloadLen,
                        std::string& responseHeaders) override
  {
    if (strcasecmp(url.c_str(), m_expectedUrl.c_str()) != 0) return false;

    if (!m_useGcsToken) return true;

    bool tokenFound = false;
    for (int i = 0; i < headers.size(); i++)
    {
      if (strcmp(headers[i].c_str(), "Authorization: Bearer fake gcs token") == 0)
        tokenFound = true;
    }

    return tokenFound;
  }

  virtual bool http_get(std::string const& url,
                        std::vector<std::string> const& headers,
                        std::basic_iostream<char>* payload,
                        std::string& responseHeaders,
                        bool headerOnly) override
  {
    if (headerOnly) // try getting metadata
    {
      if (m_isPut)
      {
        if ((strcasecmp(url.c_str(), m_expectedUrl.c_str()) == 0) && m_firstTime)
        {
          // first time retry false to mock the case of file doesn't exist
          m_firstTime = false;
          return false;
        }
      }
      responseHeaders = "HTTP/1.1 200 OK\n"
                        "x-goog-meta-encryptiondata: {\"WrappedContentKey\":{\"KeyId\":\"symmKey1\",\"EncryptedKey\":\"MyZBZNLcndKTeR+xC8Msle5IcSYKsx/nYNn93OONSqs=\",\"Algorithm\":\"AES_CBC_256\"},\"ContentEncryptionIV\":\"30fmhKrf1aKyWidrv06NNA==\"}\n"
                        "Content-Type: application/octet-stream\n"
                        "Content-Length: 0\n";
    }
    else
    {
      if (strcasecmp(url.c_str(), m_expectedUrl.c_str()) != 0) return false;

      if (!m_useGcsToken) return true;

      bool tokenFound = false;
      for (int i = 0; i < headers.size(); i++)
      {
        if (strcmp(headers[i].c_str(), "Authorization: Bearer fake gcs token") == 0)
          tokenFound = true;
      }

      return tokenFound;
    }
    return true;
  }

private:
  StageInfo m_stageInfo;

  std::vector<EncryptionMaterial> m_encryptionMaterial;

  std::vector<std::string> m_srcLocationsPut;

  std::vector<std::string> m_srcLocationsGet;

  bool m_useGcsToken;

  std::string m_expectedUrl;

  bool m_firstTime;

  bool m_isPut;
};

// test helper for put
void put_gcs_test_core(bool useGcsToken)
{
  std::string cmd = std::string("put file://small_file.csv.gz @odbctestStage AUTO_COMPRESS=false");

  MockedGCSStatement mockedStatementPut(useGcsToken);

  Snowflake::Client::FileTransferAgent agent(&mockedStatementPut);

  ITransferResult * result = agent.execute(&cmd);

  std::string put_status;
  while (result->next())
  {
    result->getColumnAsString(6, put_status);
    assert_string_equal("UPLOADED", put_status.c_str());
  }

  if (useGcsToken)
  {
    result = agent.execute(&cmd);
    while (result->next())
    {
      result->getColumnAsString(6, put_status);
      assert_string_equal("SKIPPED", put_status.c_str());
    }
  }

  cmd += " OVERWRITE=true";
  result = agent.execute(&cmd);
  while (result->next())
  {
    result->getColumnAsString(6, put_status);
    assert_string_equal("UPLOADED", put_status.c_str());
  }
}

// test helper for get
void get_gcs_test_core(bool useGcsToken)
{
  std::string cmd = std::string("get @odbctestStage file://");

  MockedGCSStatement mockedStatementGet(useGcsToken);

  Snowflake::Client::FileTransferAgent agent(&mockedStatementGet);

  ITransferResult * result = agent.execute(&cmd);

  std::string get_status;
  while (result->next())
  {
    result->getColumnAsString(2, get_status);
    assert_string_equal("DOWNLOADED", get_status.c_str());
  }
}

// test helper for regional url
void gcs_regional_url_test_core(bool useRegionalUrl, std::string region, std::string endpoint, std::string expectedEndpoint)
{
  std::string cmd = std::string("put file://small_file.csv.gz @odbctestStage AUTO_COMPRESS=false");

  MockedGCSStatement mockedStatementPut(true, useRegionalUrl, region, endpoint, expectedEndpoint);

  Snowflake::Client::FileTransferAgent agent(&mockedStatementPut);

  ITransferResult* result = agent.execute(&cmd);

  std::string put_status;
  while (result->next())
  {
    result->getColumnAsString(6, put_status);
    assert_string_equal("UPLOADED", put_status.c_str());
  }
}

/**
 * Simple put test case with gcs token
 */
void test_simple_put_gcs_with_token(void ** unused)
{
  put_gcs_test_core(true);
}

/**
 * Simple get test case gcs token
 */
void test_simple_get_gcs_with_token(void ** unused)
{
  get_gcs_test_core(true);
}

/**
* Simple put test case with presigned url
*/
void test_simple_put_gcs_with_presignedurl(void ** unused)
{
  put_gcs_test_core(false);
}

/**
* Simple get test case gcs presigned url
*/
void test_simple_get_gcs_with_presignedurl(void ** unused)
{
  get_gcs_test_core(false);
}

void test_gcs_use_regional_url(void** unused)
{
  gcs_regional_url_test_core(true, "testregion", "", "storage.testregion.rep.googleapis.com");
}

void test_gcs_use_me2_region(void** unused)
{
  gcs_regional_url_test_core(false, "me-central2", "", "storage.me-central2.rep.googleapis.com");
}

void test_gcs_override_endpoint(void** unused)
{
  gcs_regional_url_test_core(false, "testregion", "testendpoint.googleapis.com", "testendpoint.googleapis.com");
}

static int gr_setup(void **unused)
{
  initialize_test(SF_BOOLEAN_FALSE);
  return 0;
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_simple_put_gcs_with_token),
    cmocka_unit_test(test_simple_get_gcs_with_token),
    cmocka_unit_test(test_simple_put_gcs_with_presignedurl),
    cmocka_unit_test(test_simple_get_gcs_with_presignedurl),
    cmocka_unit_test(test_gcs_use_regional_url),
    cmocka_unit_test(test_gcs_use_me2_region),
    cmocka_unit_test(test_gcs_override_endpoint),
  };
  int ret = cmocka_run_group_tests(tests, gr_setup, NULL);
  return ret;
}

