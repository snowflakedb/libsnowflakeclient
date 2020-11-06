/*
 * Copyright (c) 2019-2020 Snowflake Computing, Inc. All rights reserved.
 */

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

std::string getTestFileMatchDir()
{
  std::string srcLocation = TestSetup::getDataDir();
  srcLocation += "test_file_match_dir";
  srcLocation += PATH_SEP;
  return srcLocation;
}

class MockedFailedParseStmt : public Snowflake::Client::IStatementPutGet
{
public:
  MockedFailedParseStmt()
    : Snowflake::Client::IStatementPutGet(){}

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
      1234
    );
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
    putGetParseResponse->parallel = 1;
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
    std::string dataDir = getTestFileMatchDir();
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
    putGetParseResponse->parallel = 1;
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
    std::size_t found = fileMetadata->srcFileName.find_last_of("/\\");
    std::string fileName = fileMetadata->srcFileName.substr(found+1);
    if(fileName.compare("file2.csv") == 0)
      return FAILED;
    //Lets make succesful files wait
    //So the failed ones can catch up with retries and set fastFail.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return SUCCESS;
  }

  virtual RemoteStorageRequestOutcome GetRemoteFileMetadata(
    std::string * filePathFull, FileMetadata *fileMetadata)
  {
      return SUCCESS;
  }

  virtual RemoteStorageRequestOutcome download(FileMetadata * fileMetadata,
                                               std::basic_iostream<char>* dataStream)
  {
    return SUCCESS;
  }

private:
  bool m_putSuccess;

};

void test_put_fast_fail_core()
{
  std::string matchDir = getTestFileMatchDir();
  matchDir += "*.csv";
  IStorageClient * client = new MockedStorageClient();
  StorageClientFactory::injectMockedClient(client);

  std::string cmd = std::string("put file://") + matchDir + std::string("@odbctestStage AUTO_COMPRESS=false OVERWRITE=true");

  MockedStatementPut mockedStatementPut("*.csv");

  Snowflake::Client::FileTransferAgent agent(&mockedStatementPut);

  agent.setPutFastFail(true);

  try
  {
    ITransferResult *result = agent.execute(&cmd);
    assert_int_equal(result->getResultSize(), -1); //If this reaches here then fast fail failed.
  }
  catch(SnowflakeTransferException &ex)
  {
    assert_int_equal(ex.getCode(), TransferError::FAST_FAIL_ENABLED_SKIP_UPLOADS);
  }
}
void test_put_fast_fail(void **unused)
{
  test_put_fast_fail_core();
}

static int gr_setup(void **unused)
{
  initialize_test(SF_BOOLEAN_FALSE);
  return 0;
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_put_fast_fail),
  };
  int ret = cmocka_run_group_tests(tests, gr_setup, NULL);
  return ret;
}

