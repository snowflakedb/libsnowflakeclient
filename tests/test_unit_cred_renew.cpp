/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

/**
 * Testing situation that aws credential is expired.
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
    numParseCalled = 0;
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

    numParseCalled ++;

    return true;
  }

  unsigned int getNumParseCalled()
  {
    return numParseCalled;
  }

private:
  StageInfo m_stageInfo;

  std::vector<EncryptionMaterial> m_encryptionMaterial;

  unsigned int numParseCalled;

  std::vector<std::string> m_srcLocations;
};

class MockedStatementGetSmall : public Snowflake::Client::IStatementPutGet
{
public:
  MockedStatementGetSmall()
    : Snowflake::Client::IStatementPutGet()
  {
    m_stageInfo.stageType = Snowflake::Client::StageType::MOCKED_STAGE_TYPE;
    m_stageInfo.location = "fake s3 location";
    for (int i = 0; i < 4; i++)
    {
        m_encryptionMaterial.emplace_back(
          (char *)"dvkZi0dkBfrcHr6YxXLRFg==\0",
          (char *)"1234\0",
          1234
        );
        m_srcLocations.push_back("fake s3 location");
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
    numParseCalled = 0;
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

    numParseCalled ++;

    return true;
  }

  unsigned int getNumParseCalled()
  {
    return numParseCalled;
  }

private:
  StageInfo m_stageInfo;

  std::vector<EncryptionMaterial> m_encryptionMaterial;

  std::vector<std::string> m_srcLocations;

  unsigned int numParseCalled;
};

class MockedStorageClient : public Snowflake::Client::IStorageClient
{
public:
  MockedStorageClient() :
    m_expiredReturned(false),
    m_numGetRemoteMetaCalled(0)
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
    m_numGetRemoteMetaCalled ++;
    bool shouldReturnExpire = false;
    if (!m_expiredReturned)
    {
      shouldReturnExpire = !m_expiredReturned;
      m_expiredReturned = true;
    }

    if (!shouldReturnExpire)
    {
      std::string iv = "ZxQiil366wJ+QqrhDKckBQ==";
      Snowflake::Client::Util::Base64::decode(iv.c_str(), iv.size(), fileMetadata->
        encryptionMetadata.iv.data);
      fileMetadata->encryptionMetadata.enKekEncoded = "rgANWKrHN14aKoHRxoIh9GtjXYScNdjseX4kmLZRnEc=";
      fileMetadata->srcFileSize = DOWNLOAD_DATA_SIZE_THRESHOLD + 1;
    }

    return shouldReturnExpire ? TOKEN_EXPIRED : SUCCESS;
  }

  virtual RemoteStorageRequestOutcome download(FileMetadata * fileMetadata,
                                               std::basic_iostream<char>* dataStream)
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

  int getNumGetRemoteMetaCalled()
  {
    return m_numGetRemoteMetaCalled;
  }


private:
  SF_MUTEX_HANDLE numRenewMutex;

  bool m_expiredReturned;

  int m_numGetRemoteMetaCalled;
};

class MockedExceptionStorageClient : public Snowflake::Client::IStorageClient
{
public:
  MockedExceptionStorageClient()
  {
    ; // Do nothing
  }

  ~MockedExceptionStorageClient()
  {
    ; // Do nothing
  }

  virtual RemoteStorageRequestOutcome upload(FileMetadata *fileMetadata,
                                 std::basic_iostream<char> *dataStream)
  {
      throw std::runtime_error("error");
      return SUCCESS;
  }

  virtual RemoteStorageRequestOutcome download(FileMetadata * fileMetadata,
                                               std::basic_iostream<char>* dataStream)
  {
      throw std::runtime_error("error");
      return SUCCESS;
  }

  virtual RemoteStorageRequestOutcome GetRemoteFileMetadata(
    std::string * filePathFull, FileMetadata *fileMetadata)
  {
    std::string iv = "ZxQiil366wJ+QqrhDKckBQ==";
    Snowflake::Client::Util::Base64::decode(iv.c_str(), iv.size(), fileMetadata->
      encryptionMetadata.iv.data);
    fileMetadata->encryptionMetadata.enKekEncoded = "rgANWKrHN14aKoHRxoIh9GtjXYScNdjseX4kmLZRnEc=";
    // return small files to test exception in threads.
    fileMetadata->srcFileSize = DOWNLOAD_DATA_SIZE_THRESHOLD - 1;
    return SUCCESS;
  }
};

void test_token_renew_core(std::string fileName)
{
  IStorageClient * client = new MockedStorageClient();
  StorageClientFactory::injectMockedClient(client);

  std::string cmd = "fake put command";

  MockedStatementPut mockedStatementPut(fileName);

  Snowflake::Client::FileTransferAgent agent(&mockedStatementPut);

  ITransferResult * result = agent.execute(&cmd);

  std::string put_status;
  while(result->next())
  {
    result->getColumnAsString(6, put_status);
    assert_string_equal("UPLOADED", put_status.c_str());
  }

  // original parse command call + renew call
  assert_int_equal(2, mockedStatementPut.getNumParseCalled());
}

void test_token_renew_small_files(void ** unused)
{
  test_token_renew_core("*");
}

void test_token_renew_large_file(void ** unused)
{
  std::string dataDir = TestSetup::getDataDir();
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

void test_token_renew_get_remote_meta(void **unused)
{
  MockedStorageClient * client = new MockedStorageClient();
  StorageClientFactory::injectMockedClient(client);

  std::string cmd = "fake get command";

  MockedStatementGet mockedStatementGet;

  Snowflake::Client::FileTransferAgent agent(&mockedStatementGet);

  ITransferResult * result = agent.execute(&cmd);

  std::string get_status;
  while(result->next())
  {
    result->getColumnAsString(2, get_status);
    assert_string_equal("DOWNLOADED", get_status.c_str());
  }

  // original parse call + token renew call
  assert_int_equal(2, mockedStatementGet.getNumParseCalled());
  assert_int_equal(2, client->getNumGetRemoteMetaCalled());
}

void test_parse_exception(void **unused)
{
  std::string cmd = "fake get command";

  MockedFailedParseStmt failedParseStmt;

  Snowflake::Client::FileTransferAgent agent(&failedParseStmt);

  try
  {
    ITransferResult *result = agent.execute(&cmd);
    assert_true(false);
  }
  catch (SnowflakeTransferException & e)
  {
    assert_int_equal(TransferError::INTERNAL_ERROR, e.getCode());

    //assert error message is formatted correctly
    const char * expectedMsg = "Internal error: Failed to parse response.";
    assert_memory_equal(e.what(), expectedMsg, strlen(expectedMsg));
  }
}

void test_transfer_exception_upload(void **unused)
{
  MockedExceptionStorageClient * client = new MockedExceptionStorageClient();
  StorageClientFactory::injectMockedClient(client);

  std::string cmd = "fake put command";

  // small files to test exception in threads
  MockedStatementPut mockedStatementPut("small*");

  Snowflake::Client::FileTransferAgent agent(&mockedStatementPut);

  try
  {
    ITransferResult *result = agent.execute(&cmd);
    assert_true(false);
  }
  catch (SnowflakeTransferException & e)
  {
    assert_int_equal(TransferError::FAILED_TO_TRANSFER, e.getCode());
  }
}

void test_transfer_exception_download(void **unused)
{
  MockedExceptionStorageClient * client = new MockedExceptionStorageClient();
  StorageClientFactory::injectMockedClient(client);

  std::string cmd = "fake get command";

  MockedStatementGetSmall mockedStatementGetSmall;

  Snowflake::Client::FileTransferAgent agent(&mockedStatementGetSmall);

  ITransferResult * result = agent.execute(&cmd);

  std::string get_status;
  while(result->next())
  {
    result->getColumnAsString(2, get_status);
    assert_string_equal("ERROR", get_status.c_str());
  }
}

static int large_file_removal(void **unused)
{
  std::string dataDir = TestSetup::getDataDir();
  std::string fullFileName = dataDir + "large_file.csv";

  std::remove(fullFileName.c_str());
  return 0;
}

static int gr_setup(void **unused)
{
  initialize_test(SF_BOOLEAN_FALSE);
  return 0;
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_parse_exception),
    cmocka_unit_test(test_token_renew_small_files),
    cmocka_unit_test_teardown(test_token_renew_large_file, large_file_removal),
    cmocka_unit_test(test_token_renew_get_remote_meta),
    cmocka_unit_test(test_transfer_exception_upload),
    cmocka_unit_test(test_transfer_exception_download)
  };
  int ret = cmocka_run_group_tests(tests, gr_setup, NULL);
  return ret;
}
