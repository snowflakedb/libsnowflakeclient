/*
 * Copyright (c) 2023 Snowflake Computing, Inc. All rights reserved.
 */

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
  MockedGCSStatement(bool useGcsToken)
    : m_useGcsToken(useGcsToken), Snowflake::Client::IStatementPutGet()
  {
    m_stageInfo.stageType = Snowflake::Client::StageType::GCS;
    if (m_useGcsToken)
    {
      m_stageInfo.credentials = { { "GCS_ACCESS_TOKEN", (char*)"fake gcs token"} };
      m_expectedUrl = "https://storage.googleapis.com/FakeGcsLocation/small_file.csv.gz";
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
                                  PutGetParseResponse *putGetParseResponse)
  {
    putGetParseResponse->stageInfo = m_stageInfo;
    if (strcasecmp(sql->substr(0, 3).c_str(), "get") == 0)
    {
      putGetParseResponse->command = CommandType::DOWNLOAD;
      putGetParseResponse->srcLocations = m_srcLocationsGet;
    }
    else
    {
      putGetParseResponse->command = CommandType::UPLOAD;
      putGetParseResponse->srcLocations = m_srcLocationsPut;
      putGetParseResponse->threshold = DEFAULT_UPLOAD_DATA_SIZE_THRESHOLD;
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
                        std::string& responseHeaders)
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
    if (headerOnly)
    {
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
};

// test helper for put
void put_gcs_test_core(bool useGcsToken)
{
  std::string cmd = std::string("put file://small_file.csv.gz @odbctestStage AUTO_COMPRESS=false OVERWRITE=true");

  MockedGCSStatement mockedStatementPut(useGcsToken);

  Snowflake::Client::FileTransferAgent agent(&mockedStatementPut);

  ITransferResult * result = agent.execute(&cmd);

  std::string put_status;
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
  };
  int ret = cmocka_run_group_tests(tests, gr_setup, NULL);
  return ret;
}

