#include <fstream>
#include "../wiremock/wiremock.hpp"
#include "snowflake/IStatementPutGet.hpp"
#include "snowflake/PutGetParseResponse.hpp"
#include <snowflake/SnowflakeTransferException.hpp>
#include "util/Base64.hpp"
#include "FileTransferExecutionResult.hpp"
#include "FileTransferAgent.hpp"
#include "SnowflakeS3Client.hpp"
#include "StorageClientFactory.hpp"
#include "../utils/test_setup.h"
#include "../utils/TestSetup.hpp"

using namespace Snowflake::Client;

WiremockRunner* wiremock = NULL;
static int group_setup(void**)
{
  wiremock = new WiremockRunner();
  return 0;
}

static int group_teardown(void**)
{
  if (wiremock)
  {
    delete wiremock;
    wiremock = nullptr;
  }
  return 0;
}

class MockedStatementPutGet : public Snowflake::Client::IStatementPutGet
{
public:
  MockedStatementPutGet()
  {
  }

  bool parsePutGetCommand(std::string *sql,
      PutGetParseResponse *putGetParseResponse)
  {
    SF_UNUSED(sql);
    SF_UNUSED(putGetParseResponse);
    return true;
  }
};

void test_retry_parts_uploading(void** unused)
{
  SF_UNUSED(unused);
  wiremock->resetMapping();
  wiremock->initMappingFromFile("fail_first_upload_attempt.json");
  TransferConfig transferConfig;
  char cafile[] = "../wiremock/ca-cert.pem";
  transferConfig.caBundleFile = cafile;
  StageInfo stageInfo;
  stageInfo.stageType = StageType::S3;
  stageInfo.endPoint = std::string("https://") + wiremockHost + ":" + wiremockPort;
  stageInfo.location = "testbucket/";
  char aws_token[] = "AWS_TOKEN";
  char aws_key_id[] = "AWS_KEY_ID";
  char aws_secret_key[] = "AWS_SECRET_KEY";
  stageInfo.credentials.insert( {{"AWS_TOKEN", aws_token},
                                   {"AWS_KEY_ID", aws_key_id},
                                   {"AWS_SECRET_KEY", aws_secret_key}});

  std::string datafile = "../data/large_file.csv.gz";
  struct stat fileStatus;
  stat(datafile.c_str(), &fileStatus);
  size_t filesize = (size_t) fileStatus.st_size;
  std::fstream dataStream(datafile.c_str(), ::std::ios_base::in | ::std::ios_base::binary);
  if (!dataStream.is_open())
  {
    std::cerr << "Failed to open file!" << std::endl;
    return;
  }

  FileMetadata fileMetadata;
  fileMetadata.srcFileName = datafile;
  fileMetadata.srcFileToUpload = datafile;
  fileMetadata.requireCompress = false;
  fileMetadata.srcFileSize = filesize;
  fileMetadata.srcFileToUploadSize = filesize;
  fileMetadata.sha256Digest = "dummydigest";
  fileMetadata.destFileName = "dummydst.csv.gz";
  fileMetadata.encryptionMetadata.cipherStreamSize = (long long)filesize;
  fileMetadata.destFileSize = filesize;
  fileMetadata.encryptionMetadata.fileKey.nbBits = 0;
  fileMetadata.sourceCompression = &FileCompressionType::GZIP;
  fileMetadata.targetCompression = &FileCompressionType::GZIP;
  fileMetadata.isLarge = true;
  fileMetadata.overWrite = true;

  MockedStatementPutGet mockedStatement;
  SnowflakeS3Client s3Client(&stageInfo, 2, 40 * 1024 * 1024, &transferConfig, &mockedStatement);
  auto outcome = s3Client.upload(&fileMetadata, (std::basic_iostream<char>*)&dataStream);
  assert_true(outcome == RemoteStorageRequestOutcome::SUCCESS);

  dataStream.close();
}

int main(void) {
  initialize_test(SF_BOOLEAN_TRUE);
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_retry_parts_uploading),
  };
  int ret = cmocka_run_group_tests(tests, group_setup, group_teardown);
  snowflake_global_term();
  return ret;
}
