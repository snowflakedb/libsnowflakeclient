/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "SnowflakeAzureClient.hpp"
#include "FileMetadataInitializer.hpp"
#include "snowflake/client.h"
#include "util/Base64.hpp"
#include "util/ByteArrayStreamBuf.hpp"
#include "util/Proxy.hpp"
#include "crypto/CipherStreamBuf.hpp"
#include "logger/SFAwsLogger.hpp"
#include "logger/SFLogger.hpp"
#include "SnowflakeS3Client.hpp"
#include "storage_credential.h"
#include "storage_account.h"
#include "blob/blob_client.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>



#define CONTENT_TYPE_OCTET_STREAM "application/octet-stream"

#define SFC_DIGEST "sfc-digest"

namespace Snowflake
{
namespace Client
{


SnowflakeAzureClient::SnowflakeAzureClient(StageInfo *stageInfo, unsigned int parallel,
  TransferConfig * transferConfig):
  m_stageInfo(stageInfo),
  m_threadPool(nullptr),
  m_parallel(std::min(parallel, std::thread::hardware_concurrency()))
{
    const std::string azuresaskey("AZURE_SAS_KEY");
  std::string capath="/etc/pki/tls/certs";

  std::string account_name = m_stageInfo->storageAccount;
  std::string sas_key = m_stageInfo->credentials[azuresaskey];
  std::string endpoint = account_name + "." + m_stageInfo->endPoint;
  std::shared_ptr<azure::storage_lite::storage_credential>  cred = std::make_shared<azure::storage_lite::shared_access_signature_credential>(sas_key);
  std::shared_ptr<azure::storage_lite::storage_account> account = std::make_shared<azure::storage_lite::storage_account>(account_name, cred, false, endpoint);
  auto bc = std::make_shared<azure::storage_lite::blob_client>(account, parallel);
  bc->set_ca_path(capath);
  m_blobclient= new azure::storage_lite::blob_client_wrapper(bc);

  CXX_LOG_TRACE("Successfully created Azure client. End of constructor.");
}

SnowflakeAzureClient::~SnowflakeAzureClient()
{
//  delete AzureClient;
  /*
  //TODO move this to global shutdown
  //Aws::ShutdownAPI(options);
  if (m_threadPool != nullptr)
  {
    delete m_threadPool;
  }
   */
}

RemoteStorageRequestOutcome SnowflakeAzureClient::upload(FileMetadata *fileMetadata,
                                          std::basic_iostream<char> *dataStream)
{
    return doSingleUpload(fileMetadata, dataStream);
  // first call metadata request to deduplicate files
  /*
  Aws::Azure::Model::HeadObjectRequest headObjectRequest;

  std::string bucket, key;
  std::string filePathFull = m_stageInfo->location
                             + fileMetadata->destFileName;
  extractBucketAndKey(&filePathFull, bucket, key);
  headObjectRequest.SetBucket(bucket);
  headObjectRequest.SetKey(key);

  Aws::S3::Model::HeadObjectOutcome outcome =
    AzureClient->HeadObject(headObjectRequest);

  if (outcome.IsSuccess())
  {
    std::string sfcDigest = outcome.GetResult().GetMetadata().at(
      SFC_DIGEST);
    if (sfcDigest == fileMetadata->sha256Digest)
    {
      CXX_LOG_INFO("File %s with same name and sha256 existed. Skipped.",
               fileMetadata->srcFileToUpload.c_str());
      return RemoteStorageRequestOutcome::SKIP_UPLOAD_FILE;
    }
  }
  else
  {
    CXX_LOG_WARN("Listing file metadata failed: %s",
                outcome.GetError().GetMessage().c_str());
  }

  if (fileMetadata->srcFileSize > DATA_SIZE_THRESHOLD)
    return doMultiPartUpload(fileMetadata, dataStream);
  else
    return doSingleUpload(fileMetadata, dataStream);
    */
  return SUCCESS;
}

RemoteStorageRequestOutcome SnowflakeAzureClient::doSingleUpload(FileMetadata *fileMetadata,
  std::basic_iostream<char> *dataStream)
{
  CXX_LOG_DEBUG("Start single part upload for file %s",
               fileMetadata->srcFileToUpload.c_str());
  std::string containerName = m_stageInfo->location;
  std::string blobName = m_stageInfo->location;
  blobName=containerName;
  std::string uploadFile = fileMetadata->srcFileName;

  m_blobclient->upload_block_blob_from_stream(containerName, blobName, dataStream);
/*
  Aws::S3::Model::PutObjectRequest putObjectRequest;

  std::map<std::string, std::string> userMetadata;
  addUserMetadata(&userMetadata, fileMetadata);
  putObjectRequest.SetMetadata(userMetadata);

  // figure out bucket and path
  std::string bucket, key;
  std::string filePathFull = m_stageInfo->location + fileMetadata->destFileName;
  extractBucketAndKey(&filePathFull, bucket, key);
  putObjectRequest.SetBucket(bucket);
  putObjectRequest.SetKey(key);

  putObjectRequest.SetContentType(CONTENT_TYPE_OCTET_STREAM);
  putObjectRequest.SetContentLength(
    fileMetadata->encryptionMetadata.cipherStreamSize);
  putObjectRequest.SetBody(
    Aws::MakeShared<Aws::IOStream>("", dataStream->rdbuf()));

  Aws::S3::Model::PutObjectOutcome outcome = AzureClient->PutObject(
    putObjectRequest);
  if (outcome.IsSuccess())
  {
    return RemoteStorageRequestOutcome::SUCCESS;
  } else
  {
    return handleError(outcome.GetError());
  }
  */
  return SUCCESS;
}

void Snowflake::Client::SnowflakeAzureClient::uploadParts(MultiUploadCtx_a * uploadCtx)
{
  /*
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
    CXX_LOG_INFO("Upload parts request succeed. part number %d, etag %s",
                uploadCtx->m_partNumber, uploadCtx->m_etag.c_str());
  }
  else
  {
    uploadCtx->m_outcome = handleError(outcome.GetError());
  }
   */
}

RemoteStorageRequestOutcome SnowflakeAzureClient::doMultiPartUpload(FileMetadata *fileMetadata,
  std::basic_iostream<char> *dataStream)
{
  CXX_LOG_DEBUG("Start multi part upload for file %s",
               fileMetadata->srcFileToUpload.c_str());
/*
  if (m_threadPool == nullptr)
  {
    m_threadPool = new Util::ThreadPool(m_parallel);
  }

  std::map<std::string, std::string> userMetadata;
  addUserMetadata(&userMetadata, fileMetadata);

  std::string bucket, key;
  std::string fileFullPath = m_stageInfo->location + fileMetadata->destFileName;
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
    CXX_LOG_DEBUG("Create multi part upload request succeed, uploadId: %s",
                  uploadId.c_str());

    Util::StreamSplitter splitter(dataStream, m_parallel, DATA_SIZE_THRESHOLD);
    unsigned int totalParts = splitter.getTotalParts(
      fileMetadata->encryptionMetadata.cipherStreamSize);
    CXX_LOG_INFO("Total file size: %d, split into %d parts.",
                fileMetadata->encryptionMetadata.cipherStreamSize, totalParts);

    std::vector<MultiUploadCtx> uploadParts;
    uploadParts.reserve(totalParts);

    for (unsigned int i = 0; i < totalParts; i++)
    {
      uploadParts.emplace_back(uploadId, i+1, key, bucket);
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
      CXX_LOG_DEBUG("Complete multi part upload request succeed.");
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
  */
}

void SnowflakeAzureClient::addUserMetadata(
  std::map<std::string, std::string> *userMetadata,
  FileMetadata *fileMetadata)
{
  /*
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
   */
}

void SnowflakeAzureClient::extractBucketAndKey(std::string *fileFullPath,
                                            std::string &bucket,
                                            std::string &key)
{
  size_t sepIndex = fileFullPath->find_first_of('/');
  bucket = fileFullPath->substr(0, sepIndex);
  key = fileFullPath->substr(sepIndex + 1);
}

/*
RemoteStorageRequestOutcome SnowflakeAzureClient::handleError(
  const Aws::Client::AWSError<Aws::S3::S3Errors> & error)
{
  if (error.GetExceptionName() == "ExpiredToken")
  {
    CXX_LOG_WARN("Token expired.");
    return RemoteStorageRequestOutcome::TOKEN_EXPIRED;
  }
  else
  {
    CXX_LOG_ERROR("S3 request failed failed: %s",
                 error.GetMessage().c_str());
    return RemoteStorageRequestOutcome::FAILED;
  }
}
*/
RemoteStorageRequestOutcome SnowflakeAzureClient::download(
  FileMetadata *fileMetadata,
  std::basic_iostream<char>* dataStream)
{
  if (fileMetadata->srcFileSize > DATA_SIZE_THRESHOLD)
    return doMultiPartDownload(fileMetadata, dataStream);
  else
    return doSingleDownload(fileMetadata, dataStream);
}

RemoteStorageRequestOutcome SnowflakeAzureClient::doMultiPartDownload(
  FileMetadata *fileMetadata,
  std::basic_iostream<char> * dataStream)
{
  CXX_LOG_DEBUG("Start multi part download for file %s, parallel: %d",
               fileMetadata->srcFileName.c_str(), m_parallel);
/*
  if (m_threadPool == nullptr)
  {
    m_threadPool = new Util::ThreadPool(m_parallel);
  }

  std::string bucket, key;
  extractBucketAndKey(&fileMetadata->srcFileName, bucket, key);
  int partNum = (int)(fileMetadata->srcFileSize / DATA_SIZE_THRESHOLD) + 1;
  CXX_LOG_DEBUG("Construct get object request: bucket: %s, key: %s, ",
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

      CXX_LOG_DEBUG("Start downloading part %d, range: %s, part size: %d",
                   ctx.m_partNumber, ctx.getObjectRequest.GetRange().c_str(),
                   partSize);
      ctx.getObjectRequest.SetResponseStreamFactory([&buf]()-> Aws::IOStream *
        { return Aws::New<Aws::IOStream>("SF_MULTI_PART_DOWNLOAD", buf); });

      Aws::S3::Model::GetObjectOutcome outcome = s3Client->GetObject(
        ctx.getObjectRequest);

      buf->updateSize(partSize);
      if (outcome.IsSuccess())
      {
        CXX_LOG_DEBUG("Download part %d succeed, download size: %d",
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
  */

  return RemoteStorageRequestOutcome::SUCCESS;
}

RemoteStorageRequestOutcome SnowflakeAzureClient::doSingleDownload(
  FileMetadata *fileMetadata,
  std::basic_iostream<char> * dataStream)
{
  CXX_LOG_DEBUG("Start single part download for file %s",
               fileMetadata->srcFileName.c_str());
/*
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
    CXX_LOG_DEBUG("Single part download for file %s succeed",
                 fileMetadata->srcFileName.c_str());
    return RemoteStorageRequestOutcome::SUCCESS;
  }
  else
  {
    return handleError(outcome.GetError());
  }
  */
}

RemoteStorageRequestOutcome SnowflakeAzureClient::GetRemoteFileMetadata(
  std::string *filePathFull, FileMetadata *fileMetadata)
{
  std::string key, bucket;
  /*
  extractBucketAndKey(filePathFull, bucket, key);

  Aws::S3::Model::HeadObjectRequest headObjectRequest;
  headObjectRequest.SetBucket(bucket);
  headObjectRequest.SetKey(key);

  Aws::S3::Model::HeadObjectOutcome outcome =
    s3Client->HeadObject(headObjectRequest);

  if (outcome.IsSuccess())
  {
    fileMetadata->srcFileSize = outcome.GetResult().GetContentLength();
    CXX_LOG_INFO("Remote file %s content length: %ld.",
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
  */
}

}
}
