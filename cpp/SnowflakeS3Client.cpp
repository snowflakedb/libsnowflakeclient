#include "SnowflakeS3Client.hpp"
#include "FileTransferAgent.hpp"
#include "FileMetadataInitializer.hpp"
#include "snowflake/client.h"
#include "util/Base64.hpp"
#include "util/ByteArrayStreamBuf.hpp"
#include "snowflake/Proxy.hpp"
#include "crypto/CipherStreamBuf.hpp"
#include "logger/SFAwsLogger.hpp"
#include "logger/SFLogger.hpp"
#include "snowflake/AWSUtils.hpp"
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
#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>



#define CONTENT_TYPE_OCTET_STREAM "application/octet-stream"
#define AMZ_KEY "x-amz-key"
#define AMZ_IV "x-amz-iv"
#define AMZ_MATDESC "x-amz-matdesc"

#define AWS_KEY_ID "AWS_KEY_ID"
#define AWS_SECRET_KEY "AWS_SECRET_KEY"
#define AWS_TOKEN "AWS_TOKEN"
#define SFC_DIGEST "sfc-digest"

namespace
{
  const Aws::S3::Model::ChecksumAlgorithm INVALID_CHECKSUM = (Aws::S3::Model::ChecksumAlgorithm)(10);
}

namespace Snowflake
{
namespace Client
{


SnowflakeS3Client::SnowflakeS3Client(StageInfo *stageInfo,
                                     unsigned int parallel,
                                     size_t uploadThreshold,
                                     TransferConfig *transferConfig,
                                     IStatementPutGet* statement) :
  m_stageInfo(stageInfo),
  m_threadPool(nullptr),
  m_uploadThreshold(uploadThreshold),
  m_parallel(std::min(parallel, std::thread::hardware_concurrency()))
{
  AwsUtils::initAwsSdk();
  Aws::String caFile;
  if ((transferConfig != nullptr) && (transferConfig->caBundleFile != nullptr))
  {
    caFile = Aws::String(transferConfig->caBundleFile);
    if (caFile.length() > MAX_PATH - 1) {
        throw SnowflakeTransferException(TransferError::INTERNAL_ERROR,
            "CA bundle file path too long.");
    }
  }
  else
  {
    char caBundleFile[MAX_PATH] = {0};
    SF_STATUS status = snowflake_global_get_attribute(SF_GLOBAL_CA_BUNDLE_FILE, caBundleFile, sizeof(caBundleFile));
    if (status == SF_STATUS_ERROR_BUFFER_TOO_SMALL) {
        throw SnowflakeTransferException(TransferError::INTERNAL_ERROR,
            "CA bundle file path too long.");
    }
    caFile = Aws::String(caBundleFile);
  }
  if(caFile.empty()){
    CXX_LOG_ERROR("CA Bundle file empty.");
    throw SnowflakeTransferException(TransferError::INTERNAL_ERROR,
                                     "CA bundle file is empty.");
  }

  // ClientConfiguration needs to be initialized after Aws::InitAPI() is called
  // so we can't keep it in member variable. Add a new member variable m_stageEndpoint
  // to keep the overriden endpoint.
  // SNOW-1637718: disable IMDS. We don't really need it as we use region returned
  // from server.
  Aws::Client::ClientConfiguration clientConfiguration("", true);
  clientConfiguration.region = stageInfo->region;
  clientConfiguration.caFile = caFile;
  clientConfiguration.requestTimeoutMs = 40000;
  clientConfiguration.connectTimeoutMs = 30000;

  // FIPS mode check
  if (!(m_stageInfo->endPoint.empty()))
  {
    // FIPS mode is enabled, use the endpoint provided by GS directly
    clientConfiguration.endpointOverride = Aws::String(stageInfo->endPoint);
  }
  else if ((transferConfig != nullptr && transferConfig->useS3regionalUrl) ||
           (m_stageInfo->useS3RegionalUrl) || (m_stageInfo->useRegionalUrl))
  {
    clientConfiguration.endpointOverride = Aws::String("s3.")
        + Aws::String(clientConfiguration.region)
        + Aws::String(".")
        + Aws::String(AwsUtils::getDomainSuffixForRegionalUrl(stageInfo->region));
  }
  m_stageEndpoint = clientConfiguration.endpointOverride;

  Util::Proxy proxy;
  if ((transferConfig != nullptr) && (transferConfig->proxy != nullptr))
  {
    proxy = *(transferConfig->proxy);
  }
  else if (statement->get_proxy())
  {
    proxy = *(statement->get_proxy());
  }
  else
  {
    proxy.setProxyFromEnv();
  }

  // Set Proxy
  if (!proxy.getMachine().empty()) {
    clientConfiguration.proxyHost = Aws::String(proxy.getMachine());
    clientConfiguration.proxyScheme = proxy.getScheme() == Snowflake::Client::Util::Proxy::Protocol::HTTPS ?
      Aws::Http::Scheme::HTTPS : Aws::Http::Scheme::HTTP;
    CXX_LOG_DEBUG("Proxy host: %s, proxy scheme: %d", proxy.getMachine().c_str(), static_cast<int>(proxy.getScheme()));
  }
  if (!proxy.getUser().empty() && !proxy.getPwd().empty()) {
    clientConfiguration.proxyUserName = proxy.getUser();
    clientConfiguration.proxyPassword = proxy.getPwd();
    CXX_LOG_DEBUG("Proxy user: %s, proxy password: XXXXXXXXX", proxy.getUser().c_str());
    proxy.clearPwd();
  }
  if (proxy.getPort() != 0) {
    clientConfiguration.proxyPort = proxy.getPort();
    CXX_LOG_DEBUG("Proxy port: %d", proxy.getPort());
  }

  if (!proxy.getNoProxy().empty())
  {
    auto nonProxyHosts = Aws::Utils::StringUtils::Split(Aws::String(proxy.getNoProxy()), ',');
    clientConfiguration.nonProxyHosts = Aws::Utils::Array<Aws::String>(
                                          nonProxyHosts.data(), nonProxyHosts.size());
  }

  CXX_LOG_DEBUG("CABundleFile used in aws sdk: %s", caFile.c_str());

  Aws::Auth::AWSCredentials credentials(
    Aws::String(stageInfo->credentials.at(AWS_KEY_ID)),
    Aws::String(stageInfo->credentials.at(AWS_SECRET_KEY)),
    Aws::String(stageInfo->credentials.at(AWS_TOKEN)));

  s3Client = new Aws::S3::S3Client(credentials,
          clientConfiguration,
          Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never,
          true); // explicitly set virtual addressing style to be true

  //Ensure the stage location ended with /
  if ((!m_stageInfo->location.empty()) && (m_stageInfo->location.back() != '/'))
  {
    m_stageInfo->location.push_back('/');
  }

  CXX_LOG_TRACE("Successfully created s3 client. End of constructor.");
}

SnowflakeS3Client::~SnowflakeS3Client()
{
  delete s3Client;
  if (m_threadPool != nullptr)
  {
    delete m_threadPool;
  }
}

RemoteStorageRequestOutcome SnowflakeS3Client::upload(FileMetadata *fileMetadata,
                                          std::basic_iostream<char> *dataStream)
{
  // first call metadata request to deduplicate files
  // If overWrite is true we do not perform file exist check.
  CXX_LOG_DEBUG("Entrance S3 upload.");
  if(! fileMetadata->overWrite ) {
    CXX_LOG_DEBUG("Check if File already exists");
    Aws::S3::Model::HeadObjectRequest headObjectRequest;

      std::string bucket, key;
      std::string filePathFull = m_stageInfo->location
                                 + fileMetadata->destFileName;
      extractBucketAndKey(&filePathFull, bucket, key);
      headObjectRequest.SetBucket(bucket);
      headObjectRequest.SetKey(key);

      Aws::S3::Model::HeadObjectOutcome outcome =
              s3Client->HeadObject(headObjectRequest);

      if (outcome.IsSuccess()) {
          CXX_LOG_DEBUG("File %s already exists in the staging area. skip upload", fileMetadata->srcFileToUpload.c_str());
          return RemoteStorageRequestOutcome::SKIP_UPLOAD_FILE;
      } else {
          CXX_LOG_WARN("Listing file metadata failed: %s",
                       outcome.GetError().GetMessage().c_str());
      }
    CXX_LOG_DEBUG("End check file already exists.");
  }
  if (fileMetadata->srcFileSize > m_uploadThreshold)
    return doMultiPartUpload(fileMetadata, dataStream);
  else
    return doSingleUpload(fileMetadata, dataStream);
}

RemoteStorageRequestOutcome SnowflakeS3Client::doSingleUpload(FileMetadata *fileMetadata,
  std::basic_iostream<char> *dataStream)
{
  CXX_LOG_DEBUG("Start single part upload for file %s",
               fileMetadata->srcFileToUpload.c_str());

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

  // In newer version of aws sdk (when updating to 1.11.283), it forcely use
  // checksum that requires the data stream to be read by twice (one for
  // checksum and one for uploading) while the cipherStream can only be read
  // once and the second time will return error. It's defaults to MD5 when
  // set it to NOT_SET so the only way to disable it is to set it to an
  // invalid value.
  putObjectRequest.SetChecksumAlgorithm(INVALID_CHECKSUM);

  Aws::S3::Model::PutObjectOutcome outcome = s3Client->PutObject(
    putObjectRequest);
  if (outcome.IsSuccess())
  {
    CXX_LOG_DEBUG("%s file uploaded successfully.", fileMetadata->srcFileToUpload.c_str());
    return RemoteStorageRequestOutcome::SUCCESS;
  } else
  {
    CXX_LOG_ERROR("%s file upload failed.", fileMetadata->srcFileToUpload.c_str());
    return handleError(outcome.GetError());
  }
}

void Snowflake::Client::SnowflakeS3Client::uploadParts(MultiUploadCtx * uploadCtx)
{
  Aws::S3::Model::UploadPartRequest uploadPartRequest;

  uploadPartRequest.WithBucket(uploadCtx->m_bucket)
                   .WithKey(uploadCtx->m_key);

  uploadPartRequest.SetContentType(CONTENT_TYPE_OCTET_STREAM);
  uploadPartRequest.SetContentLength(uploadCtx->buf->getSize());
  uploadPartRequest.SetBody(Aws::MakeShared<Aws::IOStream>("", uploadCtx->buf));
  uploadPartRequest.SetUploadId(uploadCtx->m_uploadId);
  uploadPartRequest.SetPartNumber(uploadCtx->m_partNumber);
  uploadPartRequest.SetChecksumAlgorithm(INVALID_CHECKSUM);

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
}

RemoteStorageRequestOutcome SnowflakeS3Client::doMultiPartUpload(FileMetadata *fileMetadata,
  std::basic_iostream<char> *dataStream)
{
  CXX_LOG_DEBUG("Start multi part upload for file %s",
               fileMetadata->srcFileToUpload.c_str());

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

    Util::StreamSplitter splitter(dataStream, m_parallel, m_uploadThreshold);
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
      m_threadPool->AddJob([&splitter, i, this, &uploadParts]()->void
                           {
                             int tid = m_threadPool->GetThreadIdx();
                             int partId;
                             Util::ByteArrayStreamBuf * buf = splitter.FillAndGetBuf(tid, partId);
                             uploadParts[partId].buf = buf;
                             char retryPartBuflog[200];
                             sf_sprintf(retryPartBuflog, sizeof (retryPartBuflog), "Retrying partNumber=%d threadID=%d partId=%d.", i, tid, partId);
                             RetryContext partRetryCtx(retryPartBuflog, m_maxRetries);
                             do
                             {
                               //Sleeps only when its a retry
                               partRetryCtx.waitForNextRetry();
                               this->uploadParts(&uploadParts[partId]);
                             } while(partRetryCtx.isRetryable(uploadParts[partId].m_outcome));
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
      CXX_LOG_DEBUG("Complete multi part upload request succeed. %s file uploaded successfully.", fileMetadata->srcFileToUpload.c_str());
      return RemoteStorageRequestOutcome::SUCCESS;
    }
    else
    {
      CXX_LOG_DEBUG("%s file upload failed.", fileMetadata->srcFileToUpload.c_str());
      return handleError(outcome.GetError());
    }
  }
  else
  {
    CXX_LOG_DEBUG("%s file upload failed.", fileMetadata->srcFileToUpload.c_str());
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
    CXX_LOG_WARN("Token expired.");
    return RemoteStorageRequestOutcome::TOKEN_EXPIRED;
  }
  else
  {
    CXX_LOG_ERROR("S3 request failed: %s",
                 error.GetMessage().c_str());
    return RemoteStorageRequestOutcome::FAILED;
  }
}

RemoteStorageRequestOutcome SnowflakeS3Client::download(
  FileMetadata *fileMetadata,
  std::basic_iostream<char>* dataStream)
{
  if (fileMetadata->isLarge)
    return doMultiPartDownload(fileMetadata, dataStream);
  else
    return doSingleDownload(fileMetadata, dataStream);
}

RemoteStorageRequestOutcome SnowflakeS3Client::doMultiPartDownload(
  FileMetadata *fileMetadata,
  std::basic_iostream<char> * dataStream)
{
  CXX_LOG_DEBUG("Start multi part download for file %s, parallel: %d",
               fileMetadata->srcFileName.c_str(), m_parallel);

  if (m_threadPool == nullptr)
  {
    m_threadPool = new Util::ThreadPool(m_parallel);
  }

  std::string bucket, key;
  extractBucketAndKey(&fileMetadata->srcFileName, bucket, key);
  unsigned int partNum = (unsigned int)(fileMetadata->srcFileSize / DOWNLOAD_DATA_SIZE_THRESHOLD) + 1;
  CXX_LOG_DEBUG("Construct get object request: bucket: %s, key: %s, ",
               bucket.c_str(), key.c_str());

  Util::StreamAppender appender(dataStream, partNum, m_parallel, DOWNLOAD_DATA_SIZE_THRESHOLD);
  std::vector<MultiDownloadCtx> downloadParts;
  for (size_t i = 0; i < partNum; i++)
  {
    std::stringstream rangeStream;
    rangeStream << "bytes=" << i * DOWNLOAD_DATA_SIZE_THRESHOLD << '-' <<
      ((i == partNum - 1) ? fileMetadata->srcFileSize - 1
                          : ((i+1)*DOWNLOAD_DATA_SIZE_THRESHOLD-1));

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
                       (size_t)ctx.m_partNumber * DOWNLOAD_DATA_SIZE_THRESHOLD)
                     : DOWNLOAD_DATA_SIZE_THRESHOLD;
      Util::ByteArrayStreamBuf * buf = appender.GetBuffer(
        m_threadPool->GetThreadIdx());

      CXX_LOG_DEBUG("Start downloading part %d, range: %s, part size: %d",
                   ctx.m_partNumber, ctx.getObjectRequest.GetRange().c_str(),
                   partSize);
      // Clear the stream as aws sdk expects a clean stream returned from
      // response stream factory each time when retry on network errors
      ctx.getObjectRequest.SetResponseStreamFactory([&buf]()-> Aws::IOStream *
        { buf->reset(); return Aws::New<Aws::IOStream>("SF_MULTI_PART_DOWNLOAD", buf); });

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

  return RemoteStorageRequestOutcome::SUCCESS;
}

RemoteStorageRequestOutcome SnowflakeS3Client::doSingleDownload(
  FileMetadata *fileMetadata,
  std::basic_iostream<char> * dataStream)
{
  CXX_LOG_DEBUG("Start single part download for file %s",
               fileMetadata->srcFileName.c_str());

  std::string bucket, key;
  extractBucketAndKey(&fileMetadata->srcFileName, bucket, key);

  Aws::S3::Model::GetObjectRequest getObjectRequest;
  getObjectRequest.WithKey(key)
    .WithBucket(bucket);

  std::function<Aws::IOStream *(void)> responseStreamCb = [&]() ->
    Aws::IOStream * {
      // Clear the stream as aws sdk expects a clean stream returned from
      // response stream factory each time when retry on network errors
      dataStream->clear(); dataStream->seekg(0); dataStream->seekp(0);
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
    fileMetadata->srcFileSize = (size_t)outcome.GetResult().GetContentLength();
    CXX_LOG_INFO("Remote file %s content length: %ld.",
                  key.c_str(), fileMetadata->srcFileSize);

    std::string iv = outcome.GetResult().GetMetadata().at(AMZ_IV);

    Util::Base64::decode(iv.c_str(), iv.size(), fileMetadata->
      encryptionMetadata.iv.data);
    if (outcome.GetResult().GetMetadata().find(AMZ_KEY) !=
        outcome.GetResult().GetMetadata().end())
    {
      fileMetadata->encryptionMetadata.enKekEncoded = outcome.GetResult()
        .GetMetadata().at(AMZ_KEY);
    }

    return RemoteStorageRequestOutcome::SUCCESS;
  }
  else
  {
    return handleError(outcome.GetError());
  }
}

void SnowflakeS3Client::setMaxRetries(unsigned int maxRetries)
{
  m_maxRetries = maxRetries;
}

const char * SnowflakeS3Client::GetClientConfigStageEndpoint()
{
  return m_stageEndpoint.c_str();
}

}
}
