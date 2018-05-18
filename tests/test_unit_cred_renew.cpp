/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

/**
 * Testing situation that aws credential is expired.
 *
 * Note: s3 client in this class is mocked
 */

#include "IStatementPutGet.hpp"
#include <fstream>
#include <memory>
#include "FileTransferExecutionResult.hpp"
#include "FileTransferAgent.hpp"
#include "StorageClientFactory.hpp"
#include "utils/test_setup.h"

void getDataDirectory(std::string& dataDir)
{
  const std::string current_file = __FILE__;
  std::string testsDir = current_file.substr(0, current_file.find_last_of('/'));
  dataDir = testsDir + "/data/";
}

class MockedStatementPutGet : public Snowflake::Client::IStatementPutGet
{
public:
  MockedStatementPutGet(std::string fileName) : IStatementPutGet()
  {
    m_stageInfo.SetStageType(StageType::MOCKED_STAGE_TYPE);
    std::string dataDir;
    getDataDirectory(dataDir);
    m_srcLocations.push_back(dataDir + fileName);
    m_encryptionMaterial.queryStageMasterKey = "3dOoaBhkB1wSw4hyfA5DJw==";
    m_encryptionMaterial.queryId="1234";
    m_encryptionMaterial.smkId=1234;
    numParseCalled = 0;
  }

  virtual bool parsePutGetCommand(std::string *sql,
                                  PutGetParseResponse *putGetParseResponse)
  {
    putGetParseResponse->SetStageInfo(m_stageInfo);
    putGetParseResponse->SetCommandType(CommandType::UPLOAD);
    putGetParseResponse->SetSourceCompression((char *)"NONE");
    putGetParseResponse->SetSourceLocations(m_srcLocations);
    putGetParseResponse->SetAutoCompress(false);
    putGetParseResponse->SetParallel(4);
    putGetParseResponse->SetEncryptionMaterial(m_encryptionMaterial);

    numParseCalled ++;

    return true;
  }

  unsigned int getNumParseCalled()
  {
    return numParseCalled;
  }

private:
  StageInfo m_stageInfo;

  EncryptionMaterial m_encryptionMaterial;

  std::vector<std::string> m_srcLocations;

  unsigned int numParseCalled;
};

class MockedStorageClient : public Snowflake::Client::IStorageClient
{
public:
  MockedStorageClient() :
    m_expiredReturned(false)
  {
    _mutex_init(&numRenewMutex);
  }

  ~MockedStorageClient()
  {
    _mutex_term(&numRenewMutex);
  }

  virtual RemoteStorageRequestOutcome upload(FileMetadata *fileMetadata,
                                 std::basic_iostream<char> *dataStream)
  {
    bool shouldReturnExpire = false;

    _mutex_lock(&numRenewMutex);
    if (!m_expiredReturned)
    {
      shouldReturnExpire = !m_expiredReturned;
      m_expiredReturned = true;
    }
    _mutex_unlock(&numRenewMutex);

    return shouldReturnExpire ? TOKEN_EXPIRED : SUCCESS;
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
  SF_MUTEX_HANDLE numRenewMutex;

  bool m_expiredReturned;
};

void test_token_renew_core(std::string fileName)
{
  IStorageClient * client = new MockedStorageClient();
  StorageClientFactory::injectMockedClient(client);

  std::string cmd = "fake put command";

  MockedStatementPutGet  mockedStatementPutGet(fileName);

  Snowflake::Client::FileTransferAgent agent(&mockedStatementPutGet);

  FileTransferExecutionResult * result = agent.execute(&cmd);

  while(result->next())
  {
    assert_string_equal("SUCCEED", result->getStatus());
  }

  // original parse command call + renew call
  assert_int_equal(2, mockedStatementPutGet.getNumParseCalled());
}

void test_token_renew_small_files(void ** unused)
{
  test_token_renew_core("*");
}

void test_token_renew_large_file(void ** unused)
{
  std::string dataDir;
  getDataDirectory(dataDir);
  std::string fullFileName = dataDir + "large_file.csv";

  std::ofstream ofs(fullFileName);
  assert_true(ofs.is_open());

  for (int i=0; i<500000; i++)
  {
    ofs << "test_string11111,test_string222222,test_string333333" << std::endl;
  }
  ofs.close();
  test_token_renew_core("large_file.csv");
}

static int large_file_removal(void **unused)
{
  std::string dataDir;
  getDataDirectory(dataDir);
  std::string fullFileName = dataDir + "large_file.csv";

  std::string rmCmd = "rm " + fullFileName;
  system(rmCmd.c_str());
  return 0;
}

static int gr_setup(void **unused)
{
  initialize_test(SF_BOOLEAN_FALSE);
  return 0;
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_token_renew_small_files),
    cmocka_unit_test_teardown(test_token_renew_large_file, large_file_removal),
  };
  int ret = cmocka_run_group_tests(tests, gr_setup, NULL);
  return ret;
}
