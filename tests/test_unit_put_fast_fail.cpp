/*
 * Copyright (c) 2019-2020 Snowflake Computing, Inc. All rights reserved.
 */

/**
 * Testing Put/Get retry
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
int filesize = 1000;
int timeOfFail = 2;
size_t putThreshold = DEFAULT_UPLOAD_DATA_SIZE_THRESHOLD;
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
    m_stageInfo.location = "fake s3 location/";
    for (int i = 0; i < 10; i++)
    {
      m_encryptionMaterial.emplace_back(
        (char *)"YwBGVekuIpW20JAsAEQDng==\0",
        (char *)"1234\0",
        92019674385866
      );
      m_srcLocations.push_back("file" + std::to_string(i) + ".csv");
    }
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
    putGetParseResponse->threshold = putThreshold;
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
  MockedStorageClient() : m_failedTimes(0)
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
      if (m_failedTimes < timeOfFail)
      {
        m_failedTimes++;
        return FAILED;
      }
      return SUCCESS;
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
    fileMetadata->srcFileSize = (size_t)filesize;
    std::string iv = "fDnCvS9AFFdXnyM9bsEXcA==";
    Util::Base64::decode(iv.c_str(), iv.size(), fileMetadata->
                         encryptionMetadata.iv.data);
    fileMetadata->encryptionMetadata.enKekEncoded = "whKlah/HwHnm9RZw/h8gU5PLRg0IOXvwANx7cyLC4Fg=";
    return SUCCESS;
  }

  virtual RemoteStorageRequestOutcome download(FileMetadata * fileMetadata,
                                               std::basic_iostream<char>* dataStream)
  {
    std::size_t found = fileMetadata->srcFileName.find_last_of("/\\");
    std::string fileName = fileMetadata->srcFileName.substr(found+1);
    if(fileName.compare("file2.csv") == 0)
    {
      if (m_failedTimes < timeOfFail)
      {
        m_failedTimes++;
        return FAILED;
      }
      return SUCCESS;
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

private:
  int m_failedTimes;
};

void test_put_fast_fail_core(bool successWithRetry)
{
  // run test only on github as for some unknow reason
  // this test case take too much time on jenkins
  char *githubenv = getenv("GITHUB_ACTIONS");
  if (!githubenv || (strlen(githubenv) == 0))
    return;

  std::string matchDir = getTestFileMatchDir();
  matchDir += "*.csv";
  IStorageClient * client = new MockedStorageClient();
  StorageClientFactory::injectMockedClient(client);

  std::string cmd = std::string("put file://") + matchDir + std::string("@odbctestStage AUTO_COMPRESS=false OVERWRITE=true");

  MockedStatementPut mockedStatementPut("*.csv");

  Snowflake::Client::FileTransferAgent agent(&mockedStatementPut);

  agent.setPutFastFail(true);

  if (successWithRetry)
  {
    agent.setPutMaxRetries(timeOfFail);
  }
  else
  {
    agent.setPutMaxRetries(timeOfFail - 1);
  }

  try
  {
    ITransferResult *result = agent.execute(&cmd);

    if (successWithRetry)
    {
      assert_int_equal(10, result->getResultSize());
      while(result->next())
      {
        std::string value;
        result->getColumnAsString(6, value);
        assert_string_equal("UPLOADED", value.c_str());
      }
    }
	else
    {
      assert_int_equal(result->getResultSize(), -1); //If this reaches here then fast fail failed.
    }
  }
  catch(SnowflakeTransferException &ex)
  {
    if (successWithRetry)
    {
      assert_int_equal(ex.getCode(), -1);//If this reaches here then fast fail failed.
    }
    else
    {
      assert_int_equal(ex.getCode(), TransferError::FAST_FAIL_ENABLED_SKIP_UPLOADS);
    }
  }
}

void test_get_fast_fail_core(bool successWithRetry)
{
  // run test only on github as for some unknow reason
  // this test case take too much time on jenkins
  char *githubenv = getenv("GITHUB_ACTIONS");
  if (!githubenv || (strlen(githubenv) == 0))
    return;

  std::string matchDir = getTestFileMatchDir();
  IStorageClient * client = new MockedStorageClient();
  StorageClientFactory::injectMockedClient(client);

  std::string cmd = std::string("get @odbctestStage file://") + matchDir;

  MockedStatementGet mockedStatementget;

  Snowflake::Client::FileTransferAgent agent(&mockedStatementget);

  agent.setGetFastFail(true);

  if (successWithRetry)
  {
    agent.setGetMaxRetries(timeOfFail);
  }
  else
  {
    agent.setGetMaxRetries(timeOfFail - 1);
  }

  try
  {
    ITransferResult *result = agent.execute(&cmd);

    if (successWithRetry)
    {
      assert_int_equal(10, result->getResultSize());
      while(result->next())
      {
        std::string value;
        result->getColumnAsString(2, value);
        assert_string_equal("DOWNLOADED", value.c_str());
      }
    }
    else
    {
      assert_int_equal(result->getResultSize(), -1); //If this reaches here then fast fail failed.
    }
  }
  catch(SnowflakeTransferException &ex)
  {
    if (successWithRetry)
    {
      assert_int_equal(ex.getCode(), -1);//If this reaches here then fast fail failed.
    }
    else
    {
      assert_int_equal(ex.getCode(), TransferError::FAST_FAIL_ENABLED_SKIP_DOWNLOADS);
    }
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
  test_put_fast_fail_core(true);
  test_put_fast_fail_core(false);
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
  test_put_fast_fail_core(true);
  test_put_fast_fail_core(false);
}

void test_put_fast_fail_large_sequential(void **unused)
{
  putThreshold = 1;
  test_put_fast_fail_core(true);
  test_put_fast_fail_core(false);
}

void test_get_fast_fail_sequential(void **unused)
{
  parallelThreads = 1;
  test_get_fast_fail_core(true);
  test_get_fast_fail_core(false);
}

void test_get_fast_fail_parallel(void **unused)
{
  parallelThreads = 3;
  test_get_fast_fail_core(true);
  test_get_fast_fail_core(false);
}

void test_get_fast_fail_large_sequential(void **unused)
{
  filesize = 10 * 1024 * 1024;
  test_get_fast_fail_core(true);
  test_get_fast_fail_core(false);
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
    cmocka_unit_test(test_put_fast_fail_large_sequential),
    cmocka_unit_test(test_get_fast_fail_sequential),
    cmocka_unit_test(test_get_fast_fail_parallel),
    cmocka_unit_test(test_get_fast_fail_large_sequential),
  };
  int ret = cmocka_run_group_tests(tests, gr_setup, gr_teardown);
  return ret;
}

