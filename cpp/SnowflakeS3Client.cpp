/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "SnowflakeS3Client.hpp"
#include "FileMetadataInitializer.hpp"
#include "snowflake/client.h"
#include "util/Base64.hpp"
#include "util/ByteArrayStreamBuf.hpp"
#include "crypto/CipherStreamBuf.hpp"
#include "logger/SFAwsLogger.hpp"
#include <aws/core/Aws.h>
#include <aws/s3/model/CreateMultipartUploadRequest.h>
#include <aws/s3/model/CompleteMultipartUploadRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/HeadObjectRequest.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/UploadPartRequest.h>
#include <aws/core/utils/logging/AWSLogging.h>
#include <aws/core/utils/logging/DefaultLogSystem.h>
#include <aws/core/utils/logging/ConsoleLogSystem.h>
#include <iostream>
#include <fstream>

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
  m_parallel(std::min(parallel, std::thread::hardware_concurrency()))
{
  Aws::Utils::Logging::InitializeAWSLogging(
    Aws::MakeShared<Snowflake::Client::Logger::SFAwsLogger>(""));

  char caBundleFile[200] = {0};
  snowflake_global_get_attribute(SF_GLOBAL_CA_BUNDLE_FILE, caBundleFile);

  //TODO move this to global init
  Aws::InitAPI(options);
  clientConfiguration.region = *stageInfo->getRegion();
  clientConfiguration.caFile = Aws::String(caBundleFile);
  clientConfiguration.requestTimeoutMs = 40000;
  clientConfiguration.connectTimeoutMs = 30000;

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

RemoteStorageRequestOutcome SnowflakeS3Client::upload(FileMetadata *fileMetadata,
                                          std::basic_iostream<char> *dataStream)
{
  // first call metadata request to deduplicate files
  Aws::S3::Model::HeadObjectRequest headObjectRequest;

  std::string bucket, key;
  std::string filePathFull = *m_stageInfo->getLocation()
                             + fileMetadata->destFileName;
  extractBucketAndKey(&filePathFull, bucket, key);
  headObjectRequest.SetBucket(bucket);
  headObjectRequest.SetKey(key);

  Aws::S3::Model::HeadObjectOutcome outcome =
    s3Client->HeadObject(headObjectRequest);

  if (outcome.IsSuccess())
  {
    std::string sfcDigest = outcome.GetResult().GetMetadata().at(
      SFC_DIGEST);
    if (sfcDigest == fileMetadata->sha256Digest)
    {
      sf_log_info(CXX_LOG_NS, "File %s with same name and sha256 existed. Skipped.",
               fileMetadata->srcFileToUpload.c_str());
      return RemoteStorageRequestOutcome::SKIP_UPLOAD_FILE;
    }
  }
  else
  {
    sf_log_warn(CXX_LOG_NS, "Listing file metadata failed: %s",
                outcome.GetError().GetMessage().c_str());
  }

  if (fileMetadata->srcFileSize > DATA_SIZE_THRESHOLD)
    return doMultiPartUpload(fileMetadata, dataStream);
  else
    return doSingleUpload(fileMetadata, dataStream);
}

RemoteStorageRequestOutcome SnowflakeS3Client::doSingleUpload(FileMetadata *fileMetadata,
  std::basic_iostream<char> *dataStream)
{
  sf_log_debug(CXX_LOG_NS, "Start single part upload for file %s",
               fileMetadata->srcFileToUpload.c_str());

  Aws::S3::Model::PutObjectRequest putObjectRequest;

  std::map<std::string, std::string> userMetadata;
  addUserMetadata(&userMetadata, fileMetadata);
  putObjectRequest.SetMetadata(userMetadata);

  // figure out bucket and path
  std::string bucket, key;
  std::string filePathFull = *m_stageInfo->getLocation() + fileMetadata->destFileName;
  extractBucketAndKey(&filePathFull, bucket, key);
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
    return RemoteStorageRequestOutcome::SUCCESS;
  } else
  {
    return handleError(outcome.GetError());
  }
}

void *Snowflake::Client::SnowflakeS3Client::uploadParts(MultiUploadCtx * uploadCtx)
{
  Aws::S3::Model::UploadPartRequest uploadPartRequest;

  uploadPartRequest.WithBucket(uploadCtx->m_bucket)
                   .WithKey(uploadCtx->m_key);

  uploadPartRequest.SetContentType(CONTENT_TYPE_OCTET_STREAM);
  uploadPartRequest.SetContentLength(uploadCtx->buf->getSize());
  uploadPartRequest.SetBody(Aws::MakeShared<Aws::IOStream>("", uploadCtx->buf));
  uploadPartRequest.SetUploadId(uploadCtx->m_uploadId);
  uploadPartRequest.SetPartNumber(uploadCtx->m_partNumber);

  Aws::S3::Model::UploadPartOutcome outcome = s3Client->UploadPart(uploadPartRequest);

  if (outcome.IsSuccess())
  {
    uploadCtx->m_outcome = RemoteStorageRequestOutcome::SUCCESS;
    uploadCtx->m_etag = outcome.GetResult().GetETag();
    sf_log_info(CXX_LOG_NS, "Upload parts request succeed. part number %d, etag %s",
                uploadCtx->m_partNumber, uploadCtx->m_etag.c_str());
  }
  else
  {
    uploadCtx->m_outcome = handleError(outcome.GetError());
  }
}

RemoteStorageRequestOutcome SnowflakeS3Client::doMultiPartUpload(FileMetadata *fileMetadata,
  std::basic_iostream<char> *dataStream)
{
  sf_log_debug(CXX_LOG_NS, "Start multi part upload for file %s",
               fileMetadata->srcFileToUpload.c_str());

  if (m_threadPool == nullptr)
  {
    m_threadPool = new Util::ThreadPool(m_parallel);
  }

  std::map<std::string, std::string> userMetadata;
  addUserMetadata(&userMetadata, fileMetadata);

  std::string bucket, key;
  std::string fileFullPath = *m_stageInfo->getLocation()
                             + fileMetadata->destFileName;
  extractBucketAndKey(&fileFullPath, bucket, key);

  Aws::S3::Model::CreateMultipartUploadRequest request;
  request.WithBucket(bucket)
    .WithKey(key)
    .WithContentType(CONTENT_TYPE_OCTET_STREAM)
    .WithMetadata(userMetadata) ;

  auto createMultiPartResp = s3Client->CreateMultipartUpload(request);
  if (createMultiPartResp.IsSuccess())
  {
    Aws::String uploadId = createMultiPartResp.GetResult().GetUploadId();
    sf_log_debug(CXX_LOG_NS, "Create multi part upload request succeed, uploadId: %s",
                  uploadId.c_str());

    Util::StreamSplitter splitter(dataStream, m_parallel, DATA_SIZE_THRESHOLD);
    unsigned int totalParts = splitter.getTotalParts(
      fileMetadata->encryptionMetadata.cipherStreamSize);
    sf_log_info(CXX_LOG_NS, "Total file size: %d, split into %d parts.",
                fileMetadata->encryptionMetadata.cipherStreamSize, totalParts);

    MultiUploadCtx uploadParts[totalParts];

    for (unsigned int i = 0; i < totalParts; i++)
    {
      uploadParts[i].m_uploadId = uploadId;
      uploadParts[i].m_partNumber = i+1;
      uploadParts[i].m_bucket = bucket;
      uploadParts[i].m_key = key;
    }

    for (unsigned int i = 0; i < totalParts; i++)
    {
      m_threadPool->AddJob([&splitter, this, &uploadParts]()->void
                           {
                             int tid = m_threadPool->GetThreadIdx();
                             int partId;
                             Util::ByteArrayStreamBuf * buf = splitter.FillAndGetBuf(tid, partId);
                             uploadParts[partId].buf = buf;
                             this->uploadParts(&uploadParts[partId]);
                           });
    }

    m_threadPool->WaitAll();

    Aws::S3::Model::CompletedMultipartUpload completedMultipartUpload;
    for (unsigned int i=0; i< totalParts; i++)
    {
      if (uploadParts[i].m_outcome == RemoteStorageRequestOutcome::SUCCESS)
      {
        Aws::S3::Model::CompletedPart completedPart;
        completedPart.WithETag(uploadParts[i].m_etag)
          .WithPartNumber(uploadParts[i].m_partNumber);

        completedMultipartUpload.AddParts(completedPart);
      }
      else
      {
        //TODO abort existing upload
        return uploadParts[i].m_outcome;
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
      sf_log_debug(CXX_LOG_NS, "Complete multi part upload request succeed.");
      return RemoteStorageRequestOutcome::SUCCESS;
    }
    else
    {
      return handleError(outcome.GetError());
    }
  }
  else
  {
    return handleError(createMultiPartResp.GetError());
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

void SnowflakeS3Client::extractBucketAndKey(std::string *fileFullPath,
                                            std::string &bucket,
                                            std::string &key)
{
  size_t sepIndex = fileFullPath->find_first_of('/');
  bucket = fileFullPath->substr(0, sepIndex);
  key = fileFullPath->substr(sepIndex + 1);
}

RemoteStorageRequestOutcome SnowflakeS3Client::handleError(
  const Aws::Client::AWSError<Aws::S3::S3Errors> & error)
{
  if (error.GetExceptionName() == "ExpiredToken")
  {
    sf_log_warn(CXX_LOG_NS, "Token expired.");
    return RemoteStorageRequestOutcome::TOKEN_EXPIRED;
  }
  else
  {
    sf_log_error(CXX_LOG_NS, "S3 request failed failed: %s",
                 error.GetMessage().c_str());
    return RemoteStorageRequestOutcome::FAILED;
  }
}

RemoteStorageRequestOutcome SnowflakeS3Client::download(
  FileMetadata *fileMetadata,
  std::basic_iostream<char>* dataStream)
{
  if (fileMetadata->srcFileSize > DATA_SIZE_THRESHOLD)
    return doMultiPartDownload(fileMetadata, dataStream);
  else
    return doSingleDownload(fileMetadata, dataStream);
}

RemoteStorageRequestOutcome SnowflakeS3Client::doMultiPartDownload(
  FileMetadata *fileMetadata,
  std::basic_iostream<char> * dataStream)
{
  sf_log_debug(CXX_LOG_NS, "Start multi part download for file %s, parallel: %d",
               fileMetadata->srcFileName.c_str(), m_parallel);

  if (m_threadPool == nullptr)
  {
    m_threadPool = new Util::ThreadPool(m_parallel);
  }

  std::string bucket, key;
  extractBucketAndKey(&fileMetadata->srcFileName, bucket, key);
  int partNum = (int)(fileMetadata->srcFileSize / DATA_SIZE_THRESHOLD) + 1;
  sf_log_debug(CXX_LOG_NS, "Construct get object request: bucket: %s, key: %s, ",
               bucket.c_str(), key.c_str());

  Util::StreamAppender appender(dataStream, partNum, m_parallel, DATA_SIZE_THRESHOLD);
  std::vector<MultiDownloadCtx> downloadParts;
  for (unsigned int i = 0; i < partNum; i++)
  {
    std::stringstream rangeStream;
    rangeStream << "bytes=" << i * DATA_SIZE_THRESHOLD << '-' <<
      ((i == partNum - 1) ? fileMetadata->srcFileSize - 1
                          : ((i+1)*DATA_SIZE_THRESHOLD-1));

    downloadParts.emplace_back();
    downloadParts.back().m_partNumber = i;
    downloadParts.back().getObjectRequest
      .WithBucket(bucket)
      .WithKey(key)
      .WithRange(rangeStream.str());
  }

  for (int i = 0; i < downloadParts.size(); i++)
  {
    MultiDownloadCtx &ctx = downloadParts[i];

    m_threadPool->AddJob([&]()-> void {
      int partSize = ctx.m_partNumber == partNum - 1 ?
                     (int)(fileMetadata->srcFileSize -
                       ctx.m_partNumber * DATA_SIZE_THRESHOLD)
                     : DATA_SIZE_THRESHOLD;
      Util::ByteArrayStreamBuf * buf = appender.GetBuffer(
        m_threadPool->GetThreadIdx());

      sf_log_debug(CXX_LOG_NS, "Start downloading part %d, range: %s, part size: %d",
                   ctx.m_partNumber, ctx.getObjectRequest.GetRange().c_str(),
                   partSize);
      ctx.getObjectRequest.SetResponseStreamFactory([&buf]()-> Aws::IOStream *
        { return Aws::New<Aws::IOStream>("SF_MULTI_PART_DOWNLOAD", buf); });

      Aws::S3::Model::GetObjectOutcome outcome = s3Client->GetObject(
        ctx.getObjectRequest);

      buf->updateSize(partSize);
      if (outcome.IsSuccess())
      {
        sf_log_debug(CXX_LOG_NS, "Download part %d succeed, download size: %d",
                     ctx.m_partNumber, partSize);
        ctx.m_outcome = RemoteStorageRequestOutcome::SUCCESS;
        appender.WritePartToOutputStream(m_threadPool->GetThreadIdx(),
          ctx.m_partNumber);
      }
      else
      {
        ctx.m_outcome = handleError(outcome.GetError());
      }
    });
  }

  m_threadPool->WaitAll();

  for (unsigned int i = 0; i < partNum; i++)
  {
    if (downloadParts[i].m_outcome != RemoteStorageRequestOutcome::SUCCESS)
    {
      return downloadParts[i].m_outcome;
    }
  }

  return RemoteStorageRequestOutcome::SUCCESS;
}

RemoteStorageRequestOutcome SnowflakeS3Client::doSingleDownload(
  FileMetadata *fileMetadata,
  std::basic_iostream<char> * dataStream)
{
  sf_log_debug(CXX_LOG_NS, "Start single part download for file %s",
               fileMetadata->srcFileName.c_str());

  std::string bucket, key;
  extractBucketAndKey(&fileMetadata->srcFileName, bucket, key);

  Aws::S3::Model::GetObjectRequest getObjectRequest;
  getObjectRequest.WithKey(key)
    .WithBucket(bucket);

  std::function<Aws::IOStream *(void)> responseStreamCb = [&]() ->
    Aws::IOStream * {
      return Aws::New<Aws::IOStream>("SF_SINGLE_PART_DOWNLOAD", dataStream->rdbuf());
  };

  getObjectRequest.SetResponseStreamFactory(responseStreamCb);

  Aws::S3::Model::GetObjectOutcome outcome = s3Client->GetObject(
    getObjectRequest);

  if (outcome.IsSuccess())
  {
    sf_log_debug(CXX_LOG_NS, "Single part download for file %s succeed",
                 fileMetadata->srcFileName.c_str());
    return RemoteStorageRequestOutcome::SUCCESS;
  }
  else
  {
    return handleError(outcome.GetError());
  }
}

RemoteStorageRequestOutcome SnowflakeS3Client::GetRemoteFileMetadata(
  std::string *filePathFull, FileMetadata *fileMetadata)
{
  std::string key, bucket;
  extractBucketAndKey(filePathFull, bucket, key);

  Aws::S3::Model::HeadObjectRequest headObjectRequest;
  headObjectRequest.SetBucket(bucket);
  headObjectRequest.SetKey(key);

  Aws::S3::Model::HeadObjectOutcome outcome =
    s3Client->HeadObject(headObjectRequest);

  if (outcome.IsSuccess())
  {
    fileMetadata->srcFileSize = outcome.GetResult().GetContentLength();
    sf_log_info(CXX_LOG_NS, "Remote file %s content length: %ld.",
                  key.c_str(), fileMetadata->srcFileSize);

    std::string iv = outcome.GetResult().GetMetadata().at(AMZ_IV);

    Util::Base64::decode(iv.c_str(), iv.size(), fileMetadata->
      encryptionMetadata.iv.data);
    fileMetadata->encryptionMetadata.enKekEncoded = outcome.GetResult()
      .GetMetadata().at(AMZ_KEY);

    return RemoteStorageRequestOutcome::SUCCESS;
  }
  else
  {
    return handleError(outcome.GetError());
  }
}

}
}
