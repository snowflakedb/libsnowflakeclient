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
#include "SnowflakeS3Client.hpp"
#include "StorageClientFactory.hpp"
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"

using namespace ::Snowflake::Client;

class MockedPutGetAgent : public Snowflake::Client::IFileTransferAgent
{
public:
    MockedPutGetAgent(IStatementPutGet *statement,
                      TransferConfig *transferConfig)
      : Snowflake::Client::IFileTransferAgent(),
      m_stmtPutGet{statement},
      m_transferConfig(transferConfig) {}

    ~MockedPutGetAgent()
    {
      // delete storage client to avoid heap issue
      delete m_storageClient;
    }

    IStorageClient* getS3Client(std::string *command, std::string expectedErrorMsg)
    {
        m_storageClient = NULL;

        assert_true(m_stmtPutGet->parsePutGetCommand(command,
            &response));

        try
        {
            m_storageClient = StorageClientFactory::getClient(&response.stageInfo,
                (unsigned int)response.parallel,
                response.threshold,
                m_transferConfig,
                m_stmtPutGet);
        }
        catch (std::exception& e)
        {
            assert_string_equal(e.what(), expectedErrorMsg.c_str());
        }

        return m_storageClient;
    }

    const char * getStageEndpoint(std::string *command)
    {
      m_storageClient = getS3Client(command, "");
      assert_non_null(m_storageClient);

      // Since we know that the object is originally pointing to SnowflakeS3Client
      // we can assume that the dynamic cast will work. If this test were to be made
      // generic such we don't know what the underlying object type is this piece of
      // code might cause a null pointer dereference.
      return (dynamic_cast<SnowflakeS3Client *>(m_storageClient))->GetClientConfigStageEndpoint();
    }

    // Not used implemented to prevent abstract class
    virtual ITransferResult *execute(std::string *command)
    {
      return nullptr;
    }

    // Not used implemented to prevent abstract class
    virtual void setUploadStream(std::basic_iostream<char> * uploadStream,
                                 size_t dataSize)
    {
      return;
    }
private:
    IStatementPutGet *m_stmtPutGet;

    IStorageClient *m_storageClient;

    TransferConfig *m_transferConfig;

    PutGetParseResponse response;
};

class MockedStatementPut : public Snowflake::Client::IStatementPutGet
{
public:
  MockedStatementPut(std::string fileName,
                     std::string stageEndpoint,
                     std::string region)
    : IStatementPutGet()
  {
    m_stageInfo.stageType = StageType::S3;
    m_stageInfo.endPoint = stageEndpoint;
    m_stageInfo.region = region;
    char aws_token[] = "AWS_TOKEN";
    char aws_key_id[] = "AWS_KEY_ID";
    char aws_secret_key[] = "AWS_SECRET_KEY";
    m_stageInfo.credentials.insert( {{"AWS_TOKEN", aws_token},
                                     {"AWS_KEY_ID", aws_key_id},
                                     {"AWS_SECRET_KEY", aws_secret_key}});

    std::string dataDir = TestSetup::getDataDir();
    m_srcLocations.push_back(dataDir + fileName);
    m_encryptionMaterial.emplace_back(
      (char *)"3dOoaBhkB1wSw4hyfA5DJw==\0",
      (char *)"1234\0",
      1234);
  }

  bool parsePutGetCommand(std::string *sql,
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

IStorageClient* createS3Client(
    std::string fileName,
    std::string stageEndpoint,
    std::string region,
    TransferConfig *transferConfig,
    std::string expectedErrorMsg)
{
    std::string cmd = std::string("put file://") + fileName + std::string("@odbctestStage AUTO_COMPRESS=false OVERWRITE=true");

    MockedStatementPut mockedStatementPut(fileName, stageEndpoint, region);

    MockedPutGetAgent agent(&mockedStatementPut, transferConfig);

    return agent.getS3Client(&cmd, expectedErrorMsg);
}

/**
 * Simple put test case 
 * @param fileName
 */
void test_simple_put_stage_endpoint_core(std::string fileName,
                                         std::string stageEndpoint,
                                         TransferConfig *transferConfig)
{
  std::string cmd = std::string("put file://") + fileName + std::string("@odbctestStage AUTO_COMPRESS=false OVERWRITE=true");

  MockedStatementPut mockedStatementPut(fileName,
                                        stageEndpoint,
                                        "");

  MockedPutGetAgent agent(&mockedStatementPut, transferConfig);

  const char *cfg_stageEndpoint = agent.getStageEndpoint(&cmd);

  assert_non_null(cfg_stageEndpoint);
  assert_string_equal(stageEndpoint.c_str(), cfg_stageEndpoint);
}

void test_simple_put_stage_endpoint_no_regional(std::string fileName,
                                                std::string stageEndpoint)
{
  TransferConfig transferConfig;
  char cafile[] = "/tmp/cafile";
  transferConfig.useS3regionalUrl = false;
  transferConfig.caBundleFile = cafile;
  test_simple_put_stage_endpoint_core(fileName,
                                      stageEndpoint,
                                      &transferConfig);
}

void test_simple_put_stage_endpoint_with_regional(std::string fileName,
                                                std::string stageEndpoint)
{
  TransferConfig transferConfig;
  transferConfig.useS3regionalUrl = true;
  transferConfig.caBundleFile = nullptr;
  test_simple_put_stage_endpoint_core(fileName,
                                      stageEndpoint,
                                      &transferConfig);
}

void test_simple_put_stage_endpoint(void ** unused)
{
  test_simple_put_stage_endpoint_no_regional("small_file.csv.gz",
                                             "abc.testendpoint.us-east-1.snowflakecomputing.com");

  test_simple_put_stage_endpoint_with_regional("small_file.csv.gz",
                                               "abc.testendpoint.us-east-1.snowflakecomputing.com");
}

void test_simple_put_stage_regional_core(std::string fileName,
  std::string region,
  std::string expectedStageEndpoint)
{
  std::string cmd = std::string("put file://") + fileName + std::string("@odbctestStage AUTO_COMPRESS=false OVERWRITE=true");

  MockedStatementPut mockedStatementPut(fileName, "", region);

  TransferConfig transferConfig;
  transferConfig.useS3regionalUrl = true;
  transferConfig.caBundleFile = nullptr;

  MockedPutGetAgent agent(&mockedStatementPut, &transferConfig);

  const char* cfg_stageEndpoint = agent.getStageEndpoint(&cmd);

  assert_non_null(cfg_stageEndpoint);
  assert_string_equal(expectedStageEndpoint.c_str(), cfg_stageEndpoint);
}

void test_simple_put_stage_regional(void** unused)
{
  test_simple_put_stage_regional_core("small_file.csv.gz",
                                      "us-east-1",
                                      "s3.us-east-1.amazonaws.com");

  test_simple_put_stage_regional_core("small_file.csv.gz",
                                      "cn-northwest-1",
                                      "s3.cn-northwest-1.amazonaws.com.cn");
}

void test_s3_cafile_path_empty(void ** unused)
{
    std::string fileName = "small_file.csv.gz";
    std::string stageEndpoint = "abc.testendpoint.us-east-1.snowflakecomputing.com";
    std::string expectedErrorMsg = "Internal error: CA bundle file is empty.";
    TransferConfig transferConfig;
    char cafile[] = "";
    transferConfig.caBundleFile = cafile;

    IStorageClient* storageClient = createS3Client(fileName, stageEndpoint, "", & transferConfig, expectedErrorMsg);
    assert_null(storageClient);
}

void test_s3_cafile_path_too_long(void ** unused)
{
    std::string fileName = "small_file.csv.gz";
    std::string stageEndpoint = "abc.testendpoint.us-east-1.snowflakecomputing.com";
    std::string expectedErrorMsg = "Internal error: CA bundle file path too long.";
    TransferConfig transferConfig;

    char cafile[MAX_PATH + 1] = { 0 };
    std::string cafileStr(MAX_PATH, 'a');
    sf_strcpy(cafile, MAX_PATH + 1, cafileStr.c_str());
    transferConfig.caBundleFile = cafile;

    IStorageClient* storageClient = createS3Client(fileName, stageEndpoint, "", & transferConfig, expectedErrorMsg);
    assert_null(storageClient);
}

void test_s3_global_cafile_path_too_long(void ** unused)
{
    std::string fileName = "small_file.csv.gz";
    std::string stageEndpoint = "abc.testendpoint.us-east-1.snowflakecomputing.com";
    std::string expectedErrorMsg = "Internal error: CA bundle file path too long.";
    TransferConfig transferConfig;
    transferConfig.caBundleFile = NULL;
    std::string cafile(MAX_PATH, 'a');

    // Modify the environment variable temporarily and restore at the end of test
    char currentCABundleFileGlobal[MAX_PATH] = { 0 };
    snowflake_global_get_attribute(SF_GLOBAL_CA_BUNDLE_FILE, currentCABundleFileGlobal, sizeof(currentCABundleFileGlobal));
    snowflake_global_set_attribute(SF_GLOBAL_CA_BUNDLE_FILE, cafile.c_str());

    IStorageClient* storageClient = createS3Client(fileName, stageEndpoint, "", & transferConfig, expectedErrorMsg);

    // Restore the original value of the environment variables
    if (currentCABundleFileGlobal[0] != 0)
    {
        snowflake_global_set_attribute(SF_GLOBAL_CA_BUNDLE_FILE, currentCABundleFileGlobal);
    }

    assert_null(storageClient);
}

static int gr_setup(void **unused)
{
  initialize_test(SF_BOOLEAN_FALSE);
  return 0;
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_simple_put_stage_endpoint),
    cmocka_unit_test(test_simple_put_stage_regional),
    cmocka_unit_test(test_s3_cafile_path_empty),
    cmocka_unit_test(test_s3_cafile_path_too_long),
    cmocka_unit_test(test_s3_global_cafile_path_too_long)
  };
  int ret = cmocka_run_group_tests(tests, gr_setup, NULL);
  AwsUtils::shutdownAwsSdk();
  return ret;
}

