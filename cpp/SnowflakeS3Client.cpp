/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "SnowflakeS3Client.hpp"
#include "snowflake/client.h"
#include "util/Base64.hpp"
#include <aws/core/Aws.h>
#include <aws/s3/model/HeadObjectRequest.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/core/utils/logging/AWSLogging.h>
#include <aws/core/utils/logging/DefaultLogSystem.h>
#include <aws/core/utils/logging/ConsoleLogSystem.h>
#include <iostream>

#define CONTENT_TYPE_OCTET_STREAM "application/octet-stream"
#define AMZ_KEY "x-amz-key"
#define AMZ_IV "x-amz-iv"
#define AMZ_MATDESC "x-amz-matdesc"

#define AWS_KEY_ID "AWS_KEY_ID"
#define AWS_SECRET_KEY "AWS_SECRET_KEY"
#define AWS_TOKEN "AWS_TOKEN"
#define SFC_DIGEST "sfc-digest"

namespace Snowflake
{
namespace Client
{

SnowflakeS3Client::SnowflakeS3Client(StageInfo *stageInfo):
  m_stageInfo(stageInfo)
{
  /*Aws::Utils::Logging::InitializeAWSLogging(
    Aws::MakeShared<Aws::Utils::Logging::DefaultLogSystem>(
      "RunUnitTests", Aws::Utils::Logging::LogLevel::Trace, "aws_sdk_"));*/
  /*Aws::Utils::Logging::InitializeAWSLogging(
    Aws::MakeShared<Aws::Utils::Logging::ConsoleLogSystem>(
      "RunUnitTests", Aws::Utils::Logging::LogLevel::Trace));*/

  char caBundleFile[200] = {0};
  snowflake_global_get_attribute(SF_GLOBAL_CA_BUNDLE_FILE, caBundleFile);

  //TODO move this to global init
  Aws::InitAPI(options);
  clientConfiguration.region = *stageInfo->getRegion();
  clientConfiguration.caFile = Aws::String(caBundleFile);

  Aws::Auth::AWSCredentials credentials(
    Aws::String(stageInfo->getCredentials()->at(AWS_KEY_ID)),
    Aws::String(stageInfo->getCredentials()->at(AWS_SECRET_KEY)),
    Aws::String(stageInfo->getCredentials()->at(AWS_TOKEN)));

  s3Client = new Aws::S3::S3Client(credentials, clientConfiguration);
}

SnowflakeS3Client::~SnowflakeS3Client()
{
  delete s3Client;
  //TODO move this to global shutdown
  //Aws::ShutdownAPI(options);
}

TransferOutcome SnowflakeS3Client::upload(FileMetadata *fileMetadata,
                                          std::basic_iostream<char> *dataStream)
{
  if (fileExist(fileMetadata))
  {
    return TransferOutcome::SKIPPED;
  }

  Aws::S3::Model::PutObjectRequest putObjectRequest;

  std::map<std::string, std::string> userMetadata;
  addUserMetadata(&userMetadata, fileMetadata);
  putObjectRequest.SetMetadata(userMetadata);

  // figure out bucket and path
  std::string bucket, key;
  extractBucketAndKey(fileMetadata, bucket, key);
  putObjectRequest.SetBucket(bucket);
  putObjectRequest.SetKey(key);

  putObjectRequest.SetContentType(CONTENT_TYPE_OCTET_STREAM);
  putObjectRequest.SetContentLength(
    fileMetadata->encryptionMetadata.cipherStreamSize);
  putObjectRequest.SetBody(
    Aws::MakeShared<Aws::IOStream>("", dataStream->rdbuf()));

  Aws::S3::Model::PutObjectOutcome outcome = s3Client->PutObject(
    putObjectRequest);
  if (outcome.IsSuccess())
  {
    return TransferOutcome::SUCCESS;
  } else
  {
    return TransferOutcome::FAILED;
  }
}

void SnowflakeS3Client::addUserMetadata(
  std::map<std::string, std::string> *userMetadata,
  FileMetadata *fileMetadata)
{
  userMetadata->insert(
    {AMZ_KEY, fileMetadata->encryptionMetadata.enKekEncoded});
  userMetadata->insert(
    {AMZ_MATDESC, fileMetadata->encryptionMetadata.matDesc});

  char ivEncoded[64];
  Snowflake::Client::Util::Base64::encode(
    fileMetadata->encryptionMetadata.iv.data,
    Crypto::cryptoAlgoBlockSize(Crypto::CryptoAlgo::AES),
    ivEncoded);

  size_t ivEncodeSize = Snowflake::Client::Util::Base64::encodedLength(
    Crypto::cryptoAlgoBlockSize(Crypto::CryptoAlgo::AES));

  userMetadata->insert({AMZ_IV, std::string(ivEncoded, ivEncodeSize)});
  userMetadata->insert({SFC_DIGEST, fileMetadata->sha256Digest});
}

bool SnowflakeS3Client::fileExist(FileMetadata *fileMetadata)
{
  Aws::S3::Model::HeadObjectRequest headObjectRequest;

  std::string bucket, key;
  extractBucketAndKey(fileMetadata, bucket, key);
  headObjectRequest.SetBucket(bucket);
  headObjectRequest.SetKey(key);

  Aws::S3::Model::HeadObjectOutcome outcome =
    s3Client->HeadObject(headObjectRequest);

  if (outcome.IsSuccess())
  {
    std::string sfcDigest = outcome.GetResult().GetMetadata().at(
      SFC_DIGEST);
    return sfcDigest == fileMetadata->sha256Digest;
  } else
  {
    //TODO Handle token renew
  }

  return false;
}


void SnowflakeS3Client::extractBucketAndKey(FileMetadata *fileMetadata,
                                            std::string &bucket,
                                            std::string &key)
{
  size_t sepIndex = m_stageInfo->getLocation()->find_first_of('/');
  bucket = m_stageInfo->getLocation()->substr(0, sepIndex);
  key = m_stageInfo->getLocation()->substr(sepIndex + 1)
        + fileMetadata->destFileName;
}

}
}
