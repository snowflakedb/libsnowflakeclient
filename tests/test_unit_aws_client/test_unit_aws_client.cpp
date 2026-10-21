#include <fstream>
#include <vector>
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

// Serves bytes from an in-memory buffer and, like the real CipherStreamBuf,
// implements only underflow() -- it does not override seekoff()/seekpos(),
// so std::streambuf's default (always fails, returning pos_type(-1))
// applies. This is what SNOW-4219220 actually depends on: the AWS SDK
// rewinds the PutObject body with seekg(0) before an internal retry, which
// silently no-ops on a genuinely non-seekable stream instead of rewinding.
class NonSeekableStreamBuf : public std::streambuf
{
public:
  NonSeekableStreamBuf(const char* data, size_t size)
  {
    char* p = const_cast<char*>(data);
    setg(p, p, p + size);
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

void test_retry_single_part_uploading(void** unused)
{
  SF_UNUSED(unused);
  wiremock->resetMapping();
  wiremock->initMappingFromFile("fail_first_single_upload_attempt.json");
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

  // Below the upload threshold, so this exercises doSingleUpload() rather
  // than the multipart path covered by test_retry_parts_uploading.
  std::string datafile = "../data/public_domain_books.csv";
  struct stat fileStatus;
  stat(datafile.c_str(), &fileStatus);
  size_t filesize = (size_t) fileStatus.st_size;

  // Read the whole file into memory and serve it through a streambuf that
  // cannot seek (NonSeekableStreamBuf, above). A plain std::fstream here is
  // seekable, so the AWS SDK's internal retry can rewind it with seekg(0)
  // and resend the full body even without the production fix -- making the
  // test pass vacuously regardless of the fix. A real encrypted upload
  // hands the SDK a non-seekable CipherStreamBuf, which is what actually
  // triggers SNOW-4219220.
  std::vector<char> fileBytes(filesize);
  std::ifstream fileReader(datafile.c_str(), ::std::ios_base::in | ::std::ios_base::binary);
  if (!fileReader.is_open())
  {
    std::cerr << "Failed to open file!" << std::endl;
    return;
  }
  fileReader.read(fileBytes.data(), filesize);
  fileReader.close();

  NonSeekableStreamBuf nonSeekableBuf(fileBytes.data(), filesize);
  std::iostream dataStream(&nonSeekableBuf);

  FileMetadata fileMetadata;
  fileMetadata.srcFileName = datafile;
  fileMetadata.srcFileToUpload = datafile;
  fileMetadata.requireCompress = false;
  fileMetadata.srcFileSize = filesize;
  fileMetadata.srcFileToUploadSize = filesize;
  fileMetadata.sha256Digest = "dummydigest";
  fileMetadata.destFileName = "dummydst.csv";
  fileMetadata.encryptionMetadata.cipherStreamSize = (long long)filesize;
  fileMetadata.destFileSize = filesize;
  fileMetadata.encryptionMetadata.fileKey.nbBits = 0;
  fileMetadata.sourceCompression = &FileCompressionType::NONE;
  fileMetadata.targetCompression = &FileCompressionType::NONE;
  fileMetadata.isLarge = false;
  fileMetadata.overWrite = true;

  MockedStatementPutGet mockedStatement;
  SnowflakeS3Client s3Client(&stageInfo, 2, 20 * 1024 * 1024, &transferConfig, &mockedStatement);
  auto outcome = s3Client.upload(&fileMetadata, (std::basic_iostream<char>*)&dataStream);
  assert_true(outcome == RemoteStorageRequestOutcome::SUCCESS);
}

int main(void) {
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_retry_parts_uploading),
    cmocka_unit_test(test_retry_single_part_uploading),
  };
  int ret = cmocka_run_group_tests(tests, group_setup, group_teardown);
  snowflake_global_term();
  return ret;
}
