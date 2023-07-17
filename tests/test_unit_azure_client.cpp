/*
 * Copyright (c) 2023 Snowflake Computing, Inc. All rights reserved.
 */

/**
 * Testing Put retry
 *
 * Note: Azure client in this class is mocked
 */

#include "snowflake/IStatementPutGet.hpp"
#include "snowflake/PutGetParseResponse.hpp"
#include <memory>
#include "FileTransferAgent.hpp"
#include "SnowflakeAzureClient.hpp""
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

    IStorageClient* getAzureClient(std::string *command, std::string expectedErrorMsg)
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
  MockedStatementPut(std::string fileName)
    : IStatementPutGet()
  {
      m_stageInfo.stageType = StageType::AZURE;
      char azure_sas_key[] = "test_azure_sas_key";
      m_stageInfo.credentials.insert({ {"AZURE_SAS_KEY", azure_sas_key } });

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

IStorageClient* createAzureClient(std::string fileName, TransferConfig *transferConfig, std::string expectedErrorMsg)
{
  std::string cmd = std::string("put file://") + fileName + std::string("@odbctestStage AUTO_COMPRESS=false OVERWRITE=true");

  MockedStatementPut mockedStatementPut(fileName);

  MockedPutGetAgent agent(&mockedStatementPut, transferConfig);

  return agent.getAzureClient(&cmd, expectedErrorMsg);
}

void restoreEnvVariables(
    bool hasCAfileEnvChanged,
    const char* origCABundleFile,
    bool hasCAfileGlobalEnvChanged,
    const char* origCABundleFileGlobal)
{
    if (hasCAfileEnvChanged && origCABundleFile)
    {
        sf_setenv("SNOWFLAKE_TEST_CA_BUNDLE_FILE", origCABundleFile);
    }
    else if (hasCAfileEnvChanged && !origCABundleFile)
    {
        sf_unsetenv("SNOWFLAKE_TEST_CA_BUNDLE_FILE");
    }
    
    if (hasCAfileGlobalEnvChanged && origCABundleFileGlobal[0] != 0)
    {
        snowflake_global_set_attribute(SF_GLOBAL_CA_BUNDLE_FILE, origCABundleFileGlobal);
    }
}

// TEST CASES

// CA bundle file in TransferConfig is set to a long path.
void test_azure_cafile_path_too_long(void ** unused)
{
    std::string fileName = "small_file.csv.gz";
    std::string expectedErrorMsg = "Internal error: CA bundle file path too long.";
    TransferConfig transferConfig;
    char cafile[] = "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";
    transferConfig.caBundleFile = cafile;

    IStorageClient* storageClient = createAzureClient(fileName, &transferConfig, expectedErrorMsg);
    assert_null(storageClient);
}

// CA bundle file in TransferConfig is set to empty.
// The environment variables SNOWFLAKE_TEST_CA_BUNDLE_FILE and SF_GLOBAL_CA_BUNDLE_FILE will not be set.
void test_azure_empty_cafile_noenv(void ** unused)
{
    bool hasCAfileEnvChanged = false;
    bool hasCAfileGlobalEnvChanged = false;
    std::string fileName = "small_file.csv.gz";
    std::string expectedErrorMsg = "Internal error: CA bundle file is empty.";
    TransferConfig transferConfig;
    char cafile[] = "";
    transferConfig.caBundleFile = cafile;

    // Modify the environment variable temporarily and restore at the end of test
    const char* currentCABundleFile = sf_getenv("SNOWFLAKE_TEST_CA_BUNDLE_FILE");
    if (currentCABundleFile)
    {
        hasCAfileEnvChanged = true;
        sf_unsetenv("SNOWFLAKE_TEST_CA_BUNDLE_FILE");
    }

    char currentCABundleFileGlobal[MAX_PATH] = {0};
    snowflake_global_get_attribute(SF_GLOBAL_CA_BUNDLE_FILE, currentCABundleFileGlobal, sizeof(currentCABundleFileGlobal));
    if (currentCABundleFileGlobal[0] != 0)
    {
        hasCAfileGlobalEnvChanged = true;
        snowflake_global_set_attribute(SF_GLOBAL_CA_BUNDLE_FILE, NULL);
    }

    IStorageClient* storageClient = createAzureClient(fileName, &transferConfig, expectedErrorMsg);

    // Restore the original value of the environment variables
    restoreEnvVariables(hasCAfileEnvChanged, currentCABundleFile, hasCAfileGlobalEnvChanged, currentCABundleFileGlobal);

    assert_null(storageClient);
}

// CA bundle file in TransferConfig is set to empty.
// The environment variable SNOWFLAKE_TEST_CA_BUNDLE_FILE will be set but not SF_GLOBAL_CA_BUNDLE_FILE.
void test_azure_empty_cafile_from_env(void ** unused)
{
    bool hasCAfileEnvChanged = false;
    bool hasCAfileGlobalEnvChanged = false;
    std::string fileName = "small_file.csv.gz";
    std::string expectedErrorMsg = "Should have no error.";
    TransferConfig transferConfig;
    char cafile[] = "";
    transferConfig.caBundleFile = cafile;

    // Modify the environment variable temporarily and restore at the end of test
    const char* currentCABundleFile = sf_getenv("SNOWFLAKE_TEST_CA_BUNDLE_FILE");
    sf_setenv("SNOWFLAKE_TEST_CA_BUNDLE_FILE", "/tmp/cafile/p1.pem");
    hasCAfileEnvChanged = true;

    char currentCABundleFileGlobal[MAX_PATH] = { 0 };
    snowflake_global_get_attribute(SF_GLOBAL_CA_BUNDLE_FILE, currentCABundleFileGlobal, sizeof(currentCABundleFileGlobal));
    if (currentCABundleFileGlobal[0] != 0)
    {
        hasCAfileGlobalEnvChanged = true;
        snowflake_global_set_attribute(SF_GLOBAL_CA_BUNDLE_FILE, NULL);
    }

    IStorageClient* storageClient = createAzureClient(fileName, &transferConfig, expectedErrorMsg);

    // Restore the original value of the environment variables
    restoreEnvVariables(hasCAfileEnvChanged, currentCABundleFile, hasCAfileGlobalEnvChanged, currentCABundleFileGlobal);

    assert_non_null(storageClient);
}

// CA bundle file in TransferConfig is set to empty.
// The environment variable SF_GLOBAL_CA_BUNDLE_FILE will be set but not SNOWFLAKE_TEST_CA_BUNDLE_FILE.
void test_azure_empty_cafile_from_global_env(void ** unused)
{
    bool hasCAfileEnvChanged = false;
    bool hasCAfileGlobalEnvChanged = false;
    std::string fileName = "small_file.csv.gz";
    std::string expectedErrorMsg = "Internal error: CA bundle file is empty.";
    TransferConfig transferConfig;
    char cafile[] = "";
    transferConfig.caBundleFile = cafile;

    // Modify the environment variable temporarily and restore at the end of test
    const char* currentCABundleFile = sf_getenv("SNOWFLAKE_TEST_CA_BUNDLE_FILE");
    if (currentCABundleFile)
    {
        hasCAfileEnvChanged = true;
        sf_unsetenv("SNOWFLAKE_TEST_CA_BUNDLE_FILE");
    }

    char currentCABundleFileGlobal[MAX_PATH] = { 0 };
    snowflake_global_get_attribute(SF_GLOBAL_CA_BUNDLE_FILE, currentCABundleFileGlobal, sizeof(currentCABundleFileGlobal));
    snowflake_global_set_attribute(SF_GLOBAL_CA_BUNDLE_FILE, "/tmp/cafile/p1.pem");
    hasCAfileGlobalEnvChanged = true;

    IStorageClient* storageClient = createAzureClient(fileName, &transferConfig, expectedErrorMsg);

    // Restore the original value of the environment variables
    restoreEnvVariables(hasCAfileEnvChanged, currentCABundleFile, hasCAfileGlobalEnvChanged, currentCABundleFileGlobal);

    assert_null(storageClient);
}

// CA bundle file in TransferConfig is set to NULL.
// The environment variable SNOWFLAKE_TEST_CA_BUNDLE_FILE will be set but not SF_GLOBAL_CA_BUNDLE_FILE.
void test_azure_cafile_path_too_long_from_env_transferconfig_null(void ** unused)
{
    bool hasCAfileEnvChanged = false;
    bool hasCAfileGlobalEnvChanged = false;
    std::string fileName = "small_file.csv.gz";
    std::string expectedErrorMsg = "Internal error: CA bundle file path too long.";
    TransferConfig transferConfig;
    transferConfig.caBundleFile = NULL;
    char cafile[] = "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";

    // Modify the environment variable temporarily and restore at the end of test
    const char* currentCABundleFile = sf_getenv("SNOWFLAKE_TEST_CA_BUNDLE_FILE");
    sf_setenv("SNOWFLAKE_TEST_CA_BUNDLE_FILE", cafile);
    hasCAfileEnvChanged = true;

    char currentCABundleFileGlobal[MAX_PATH] = { 0 };
    snowflake_global_get_attribute(SF_GLOBAL_CA_BUNDLE_FILE, currentCABundleFileGlobal, sizeof(currentCABundleFileGlobal));
    if (currentCABundleFileGlobal[0] != 0)
    {
        hasCAfileGlobalEnvChanged = true;
        snowflake_global_set_attribute(SF_GLOBAL_CA_BUNDLE_FILE, NULL);
    }

    IStorageClient* storageClient = createAzureClient(fileName, &transferConfig, expectedErrorMsg);

    // Restore the original value of the environment variables
    restoreEnvVariables(hasCAfileEnvChanged, currentCABundleFile, hasCAfileGlobalEnvChanged, currentCABundleFileGlobal);

    assert_null(storageClient);
}

// CA bundle file in TransferConfig is set to NULL.
// The environment variable SF_GLOBAL_CA_BUNDLE_FILE will be set but not SNOWFLAKE_TEST_CA_BUNDLE_FILE.
void test_azure_cafile_path_too_long_from_global_env_transferconfig_null(void ** unused)
{
    bool hasCAfileEnvChanged = false;
    bool hasCAfileGlobalEnvChanged = false;
    std::string fileName = "small_file.csv.gz";
    std::string expectedErrorMsg = "Internal error: CA bundle file path too long.";
    TransferConfig transferConfig;
    transferConfig.caBundleFile = NULL;
    char cafile[] = "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";

    // Modify the environment variable temporarily and restore at the end of test
    const char* currentCABundleFile = sf_getenv("SNOWFLAKE_TEST_CA_BUNDLE_FILE");
    if (currentCABundleFile)
    {
        hasCAfileEnvChanged = true;
        sf_unsetenv("SNOWFLAKE_TEST_CA_BUNDLE_FILE");
    }

    char currentCABundleFileGlobal[MAX_PATH] = { 0 };
    snowflake_global_get_attribute(SF_GLOBAL_CA_BUNDLE_FILE, currentCABundleFileGlobal, sizeof(currentCABundleFileGlobal));
    snowflake_global_set_attribute(SF_GLOBAL_CA_BUNDLE_FILE, cafile);
    hasCAfileGlobalEnvChanged = true;

    IStorageClient* storageClient = createAzureClient(fileName, &transferConfig, expectedErrorMsg);

    // Restore the original value of the environment variables
    restoreEnvVariables(hasCAfileEnvChanged, currentCABundleFile, hasCAfileGlobalEnvChanged, currentCABundleFileGlobal);

    assert_null(storageClient);
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_azure_cafile_path_too_long),
    cmocka_unit_test(test_azure_empty_cafile_noenv),
    cmocka_unit_test(test_azure_empty_cafile_from_env),
    cmocka_unit_test(test_azure_empty_cafile_from_global_env),
    cmocka_unit_test(test_azure_cafile_path_too_long_from_env_transferconfig_null),
    cmocka_unit_test(test_azure_cafile_path_too_long_from_global_env_transferconfig_null),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  return ret;
}

