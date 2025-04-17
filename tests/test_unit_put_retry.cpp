/**
 * Testing Put retry
 *
 * Note: s3 client in this class is mocked
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

using namespace ::Snowflake::Client;

class MockedFailedParseStmt : public Snowflake::Client::IStatementPutGet
{
public:
  MockedFailedParseStmt()
      : Snowflake::Client::IStatementPutGet() {}

  virtual bool parsePutGetCommand(std::string *sql,
                                  PutGetParseResponse *putGetParseResponse)
  {
    return false;
  }
};

class MockedStatementGet : public Snowflake::Client::IStatementPutGet
{
public:
  MockedStatementGet()
      : Snowflake::Client::IStatementPutGet()
  {
    m_stageInfo.stageType = Snowflake::Client::StageType::MOCKED_STAGE_TYPE;
    m_stageInfo.location = "fake s3 location";
    m_encryptionMaterial.emplace_back(
        (char *)"dvkZi0dkBfrcHr6YxXLRFg==\0",
        (char *)"1234\0",
        1234);
    m_srcLocations.push_back("fake s3 location");
  }

  virtual bool parsePutGetCommand(std::string *sql,
                                  PutGetParseResponse *putGetParseResponse)
  {
    putGetParseResponse->stageInfo = m_stageInfo;
    putGetParseResponse->command = CommandType::DOWNLOAD;
    putGetParseResponse->sourceCompression = (char *)"NONE";
    putGetParseResponse->srcLocations = m_srcLocations;
    putGetParseResponse->autoCompress = false;
    putGetParseResponse->parallel = 4;
    putGetParseResponse->encryptionMaterials = m_encryptionMaterial;
    putGetParseResponse->localLocation = (char *)"/tmp\0";

    return true;
  }

private:
  StageInfo m_stageInfo;

  std::vector<EncryptionMaterial> m_encryptionMaterial;

  std::vector<std::string> m_srcLocations;
};

class MockedStatementPut : public Snowflake::Client::IStatementPutGet
{
public:
  MockedStatementPut(std::string fileName)
      : IStatementPutGet()
  {
    m_stageInfo.stageType = StageType::MOCKED_STAGE_TYPE;
    std::string dataDir = TestSetup::getDataDir();
    m_srcLocations.push_back(dataDir + fileName);
    m_encryptionMaterial.emplace_back(
        (char *)"3dOoaBhkB1wSw4hyfA5DJw==\0",
        (char *)"1234\0",
        1234);
  }

  virtual bool parsePutGetCommand(std::string *sql,
                                  PutGetParseResponse *putGetParseResponse)
  {
    putGetParseResponse->stageInfo = m_stageInfo;
    putGetParseResponse->command = CommandType::UPLOAD;
    putGetParseResponse->sourceCompression = (char *)"NONE";
    putGetParseResponse->srcLocations = m_srcLocations;
    putGetParseResponse->threshold = DEFAULT_UPLOAD_DATA_SIZE_THRESHOLD;
    putGetParseResponse->autoCompress = false;
    putGetParseResponse->parallel = 4;
    putGetParseResponse->encryptionMaterials = m_encryptionMaterial;

    return true;
  }

private:
  StageInfo m_stageInfo;

  std::vector<EncryptionMaterial> m_encryptionMaterial;

  std::vector<std::string> m_srcLocations;
};

class MockedStorageClient : public Snowflake::Client::IStorageClient
{
public:
  MockedStorageClient() : m_putSuccess(false)
  {
  }

  ~MockedStorageClient()
  {
  }

  /**
   * Initially returns put failed.
   * And this triggers retry and then returns success.
   * @param fileMetadata
   * @param dataStream
   * @return put command status
   */
  virtual RemoteStorageRequestOutcome upload(FileMetadata *fileMetadata,
                                             std::basic_iostream<char> *dataStream)
  {
    bool shouldReturnPutFailed = false;

    if (!m_putSuccess)
    {
      shouldReturnPutFailed = !m_putSuccess;
      m_putSuccess = true;
    }
    return shouldReturnPutFailed ? FAILED : SUCCESS;
  }

  virtual RemoteStorageRequestOutcome GetRemoteFileMetadata(
      std::string *filePathFull, FileMetadata *fileMetadata)
  {
    return SUCCESS;
  }

  virtual RemoteStorageRequestOutcome download(FileMetadata *fileMetadata,
                                               std::basic_iostream<char> *dataStream)
  {
    return SUCCESS;
  }

private:
  bool m_putSuccess;
};

/**
 * Simple put test case
 * @param fileName
 */
void test_simple_put_retry_core(std::string fileName)
{
  IStorageClient *client = new MockedStorageClient();
  StorageClientFactory::injectMockedClient(client);

  std::string cmd = std::string("put file://") + fileName + std::string("@odbctestStage AUTO_COMPRESS=false OVERWRITE=true");

  MockedStatementPut mockedStatementPut(fileName);

  Snowflake::Client::FileTransferAgent agent(&mockedStatementPut);

  ITransferResult *result = agent.execute(&cmd);

  std::string put_status;
  while (result->next())
  {
    result->getColumnAsString(6, put_status);
    assert_string_equal("UPLOADED", put_status.c_str());
  }
}

void test_simple_put_retry(void **unused)
{
  test_simple_put_retry_core("small_file.csv.gz");
}

void test_set_max_retry_core()
{
  int maxRetries = 2;

  Snowflake::Client::RetryContext retry_context("dummyFile.csv", maxRetries);
  int retryCount = 0;
  do
  {
    retry_context.waitForNextRetry();
    ++retryCount;
  } while (retry_context.isRetryable(FAILED));

  // The first run is not a retry.
  assert_int_equal(retryCount - 1, maxRetries);
}
void test_set_max_retry(void **unused)
{
  test_set_max_retry_core();
}

static int gr_setup(void **unused)
{
  initialize_test(SF_BOOLEAN_FALSE);
  return 0;
}

int main(void)
{
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_simple_put_retry),
      cmocka_unit_test(test_set_max_retry),
  };
  int ret = cmocka_run_group_tests(tests, gr_setup, NULL);
  return ret;
}
