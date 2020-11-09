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
int parallelThreads = 1;
std::vector<std::string> fileList;

std::string getTestFileMatchDir()
{
  std::string srcLocation = TestSetup::getDataDir();
  srcLocation += "test_put_fast_fail";
  srcLocation += PATH_SEP;
  return srcLocation;
}

void createTestFiles(void)
{
  std::string testDir = getTestFileMatchDir();
#ifdef _WIN32
  _mkdir(testDir.c_str());
#else
  mkdir(testDir.c_str(), 0755);
#endif
  for(int count = 0; count < 10 ; count++)
  {
    char file[1024];
    sprintf(file, "%sfile%d.csv", testDir.c_str(),count);
    FILE *fp = fopen(file, "w");
    if(fp == NULL)
    {
      perror("Fopen: ");
      std::cout << "Could not create file." << std::endl;
    }
    fprintf(fp, "Test data Test data Test data\n");
    fclose(fp);
  }
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
    putGetParseResponse->parallel = parallelThreads;
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
    putGetParseResponse->parallel = parallelThreads;
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
   * The upload mimics the failed behavior of single file
   * during the parallel and sequential file uploads.
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
    {
      return FAILED;
    }
    //Lets make succesful files wait
    //So the failed ones can catch up with retries and set fastFail.
#ifdef _WIN32
	Sleep(5000);  // Sleep for sleepTime milli seconds (Sleep(<time in milliseconds>) in windows)
#else
	std::this_thread::sleep_for(std::chrono::milliseconds(std::chrono::milliseconds(5000)));
#endif
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

  agent.setPutMaxRetries(2);

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

/**
 * This test tests fast fail with single thread.
 * There are 10 files to upload and if when one of the files fail to upload
 * then we skip uploading rest of the files and throw file transfer exception with approriate error code.
 * @param unused
 */
void test_put_fast_fail_sequential(void **unused)
{
  parallelThreads = 1;
  test_put_fast_fail_core();
}

/**
 * This test tests fast fail with 3 threads.
 * There are 10 files to upload. When three threads start uploading three files initially
 * and (we fail one of them) two other files which are already in process will upload
 * but the remaining files wont be uploaded. And file transfer exception with appropriate error code is thrown.
 */
void test_put_fast_fail_parallel(void **unused)
{
  parallelThreads = 3;
  test_put_fast_fail_core();
}

static int gr_teardown(void **unused)
{
  std::string testDir = getTestFileMatchDir();
  char rmCmd[500];
#ifdef _WIN32
  sprintf(rmCmd, "rd /s /q %s", testDir.c_str());
  system(rmCmd);
#else
  sprintf(rmCmd, "rm -rf %s", testDir.c_str());
  system(rmCmd);
#endif
  return 0;
}

static int gr_setup(void **unused)
{
  initialize_test(SF_BOOLEAN_FALSE);
  createTestFiles();
  return 0;
}

int main(void) {
  void **unused;
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_put_fast_fail_sequential),
    cmocka_unit_test(test_put_fast_fail_parallel),
  };
  int ret = cmocka_run_group_tests(tests, gr_setup, gr_teardown);
  return ret;
}

