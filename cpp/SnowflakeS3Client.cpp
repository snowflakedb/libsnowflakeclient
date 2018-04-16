/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "SnowflakeS3Client.hpp"
#include "FileMetadataInitializer.hpp"
#include "snowflake/client.h"
#include "util/Base64.hpp"
#include "util/StreamSplitter.hpp"
#include <aws/core/Aws.h>
#include <aws/s3/model/CreateMultipartUploadRequest.h>
#include <aws/s3/model/CompleteMultipartUploadRequest.h>
#include <aws/s3/model/HeadObjectRequest.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/UploadPartRequest.h>
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


SnowflakeS3Client::SnowflakeS3Client(StageInfo *stageInfo, unsigned int parallel):
  m_stageInfo(stageInfo),
  m_threadPool(nullptr),
  m_parallel(parallel)
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
  if (m_threadPool != nullptr)
  {
    delete m_threadPool;
  }
}

TransferOutcome SnowflakeS3Client::upload(FileMetadata *fileMetadata,
                                          std::basic_iostream<char> *dataStream)
{
  if (fileExist(fileMetadata))
  {
    return TransferOutcome::SKIPPED;
  }

  if (fileMetadata->srcFileSize > DATA_SIZE_THRESHOLD)
    return doMultiPartUpload(fileMetadata, dataStream);
  else
    return doSingleUpload(fileMetadata, dataStream);
}

TransferOutcome SnowflakeS3Client::doSingleUpload(FileMetadata *fileMetadata,
  std::basic_iostream<char> *dataStream)
{
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

void *Snowflake::Client::SnowflakeS3Client::uploadParts(MultiUploadCtx * uploadCtx)
{
  Aws::S3::Model::UploadPartRequest uploadPartRequest;

  uploadPartRequest.WithBucket(*uploadCtx->m_bucket)
                   .WithKey(*uploadCtx->m_key);

  uploadPartRequest.SetContentType(CONTENT_TYPE_OCTET_STREAM);
  uploadPartRequest.SetContentLength(uploadCtx->buf->getSize());
  uploadPartRequest.SetBody(Aws::MakeShared<Aws::IOStream>("", uploadCtx->buf));
  uploadPartRequest.SetUploadId(uploadCtx->m_uploadId);
  uploadPartRequest.SetPartNumber(uploadCtx->m_partNumber);

  Aws::S3::Model::UploadPartOutcome outcome = s3Client->UploadPart(uploadPartRequest);

  if (outcome.IsSuccess())
  {
    uploadCtx->m_outcome = TransferOutcome::SUCCESS;
    uploadCtx->m_etag = outcome.GetResult().GetETag();
  }
  else
  {
    uploadCtx->m_outcome = TransferOutcome::FAILED;
  }
}

TransferOutcome SnowflakeS3Client::doMultiPartUpload(FileMetadata *fileMetadata,
  std::basic_iostream<char> *dataStream)
{
  if (m_threadPool == nullptr)
  {
    m_threadPool = new Util::ThreadPool(m_parallel);
  }

  std::map<std::string, std::string> userMetadata;
  addUserMetadata(&userMetadata, fileMetadata);

  std::string bucket, key;
  extractBucketAndKey(fileMetadata, bucket, key);

  Aws::S3::Model::CreateMultipartUploadRequest request;
  request.WithBucket(bucket)
    .WithKey(key)
    .WithContentType(CONTENT_TYPE_OCTET_STREAM)
    .WithMetadata(userMetadata) ;

  auto createMultiPartResp = s3Client->CreateMultipartUpload(request);
  if (createMultiPartResp.IsSuccess())
  {
    Aws::String uploadId = createMultiPartResp.GetResult().GetUploadId();

    Util::StreamSplitter splitter(dataStream, m_parallel, DATA_SIZE_THRESHOLD);
    unsigned int totalParts = splitter.getTotalParts(
      fileMetadata->encryptionMetadata.cipherStreamSize);

    //std::vector<MultiUploadCtx> uploadParts;
    MultiUploadCtx ** uploadParts = new MultiUploadCtx*[totalParts];

    for (unsigned int i = 0; i < totalParts; i++)
    {
      uploadParts[i] = new MultiUploadCtx(uploadId, i+1, &bucket, &key);
      m_threadPool->AddJob([&splitter, i, this, &uploadParts]()->void
                           {
                             Util::ByteArrayStreamBuf * buf = splitter.getNextSplitPart();
                             MultiUploadCtx * ctx = uploadParts[i];
                             ctx->buf = buf;
                             this->uploadParts(ctx);
                             splitter.markDone(buf);
                           });
    }

    m_threadPool->WaitAll();

    Aws::S3::Model::CompletedMultipartUpload completedMultipartUpload;
    for (unsigned int i=0; i< totalParts; i++)
    {
      if (uploadParts[i]->m_outcome == TransferOutcome::SUCCESS)
      {
        Aws::S3::Model::CompletedPart completedPart;
        completedPart.WithETag(uploadParts[i]->m_etag)
          .WithPartNumber(uploadParts[i]->m_partNumber);

        completedMultipartUpload.AddParts(completedPart);
      }
      else
      {
        //TOOD abort already uploaded parts
        return TransferOutcome::FAILED;
      }
    }

    Aws::S3::Model::CompleteMultipartUploadRequest completeRequest;
    completeRequest.WithBucket(bucket)
                   .WithKey(key)
                   .WithUploadId(uploadId)
                   .WithMultipartUpload(completedMultipartUpload);

    Aws::S3::Model::CompleteMultipartUploadOutcome outcome =
      s3Client->CompleteMultipartUpload(completeRequest);

    if (outcome.IsSuccess())
    {
      return TransferOutcome::SUCCESS;
    }
    else
    {
      return TransferOutcome::FAILED;
    }
  }
  else
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
