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

class MockedPutGetAgent : public Snowflake::Client::FileTransferAgent
{
public:
    MockedTransferAgent(IStatementPutGet *statement)
      : m_stmtPutGet(statement) {}

    virtual const char * getStageEndpoint(string *command)
    {
      assert_true(m_stmtPutGet->parsePutGetCommand(&response));
      m_storageClient = StorageClientFactory::getClient(&response.stageInfo,
                                                        (unsigned int) response.parallel,
                                                        response.threshold,
                                                        m_transferConfig,
                                                        m_stmtPutGet);
      return ((Snowflake::Client::SnowflakeS3Client)m_storageClient)->GetClientConfigStageEndpoint();
    }
private:
    IStatementPutGet *m_stmtPutGet;

    IStorageClient *m_storageClient;

    PutGetParseResponse response;
};

class MockedStatementPut : public Snowflake::Client::IStatementPutGet
{
public:
  MockedStatementPut(std::string fileName,
                     std::string stageEndpoint)
    : IStatementPutGet()
  {
    m_stageInfo.stageType = StageType::S3;
    m_stageInfo.endPoint = stageEndpoint;
    std::string dataDir = TestSetup::getDataDir();
    m_srcLocations.push_back(dataDir + fileName);
    m_encryptionMaterial.emplace_back(
      (char *)"3dOoaBhkB1wSw4hyfA5DJw==\0",
      (char *)"1234\0",
      1234);
  }

  virtual bool parsePutGetCommand(PutGetParseResponse *putGetParseResponse)
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

/**
 * Simple put test case 
 * @param fileName
 */
void test_simple_put_stage_endpoint_core(std::string fileName,
                                std::string stageEndpoint)
{
  std::string cmd = std::string("put file://") + fileName + std::string("@odbctestStage AUTO_COMPRESS=false OVERWRITE=true");

  MockedStatementPut mockedStatementPut(fileName,
                                        stageEndpoint);

  MockedPutGetAgent agent(&mockedStatementPut);

  const char *cfg_stageEndpoint = agent.getStageEndpoint();

  assert_string_equal(stageEndpoint, std::string(cfg_stageEndpoint));
}

void test_simple_put_stage_endpoint(void ** unused)
{
  test_simple_put_stage_endpoint_core("small_file.csv.gz",
                                      "abc.testendpoint.us-east-1.snowflakecomputing.com");
}


static int gr_setup(void **unused)
{
  initialize_test(SF_BOOLEAN_FALSE);
  return 0;
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_simple_put_stage_endpoint),
  };
  int ret = cmocka_run_group_tests(tests, gr_setup, NULL);
  return ret;
}

