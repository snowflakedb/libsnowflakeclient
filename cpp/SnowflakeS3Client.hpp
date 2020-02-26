/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_SNOWFLAKES3CLIENT_HPP
#define SNOWFLAKECLIENT_SNOWFLAKES3CLIENT_HPP

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#include "snowflake/IFileTransferAgent.hpp"
#include "IStorageClient.hpp"
#include "snowflake/PutGetParseResponse.hpp"
#include "FileMetadata.hpp"
#include "util/ThreadPool.hpp"
#include "util/ByteArrayStreamBuf.hpp"

#ifdef _WIN32
 // see https://github.com/aws/aws-sdk-cpp/issues/402
#undef GetMessage
#undef GetObject
#undef min
#endif

namespace Snowflake
{
namespace Client
{

/**
 * Context passed to upload thread
 */
struct MultiUploadCtx
{
  MultiUploadCtx(Aws::String &uploadId,
    unsigned int partNumber,
    std::string &key,
    std::string &bucket)
  {
    m_uploadId = uploadId;
    m_partNumber = partNumber;
    m_key = key;
    m_bucket = bucket;
  }

  /// in memory buffer used to store current part data
  Util::ByteArrayStreamBuf *buf;

  /// s3 key
  std::string m_key;

  /// s3 bucket
  std::string m_bucket;

  /// upload id
  Aws::String m_uploadId;

  /// part etag returned by s3
  Aws::String m_etag;

  /// part number
  unsigned int m_partNumber;

  /// upload outcome
  RemoteStorageRequestOutcome m_outcome;
};

struct MultiDownloadCtx
{
  /// in memory buffer used to store current part data
  Util::ByteArrayStreamBuf *buf;

  ///
  Aws::S3::Model::GetObjectRequest getObjectRequest;

  /// part number
  unsigned int m_partNumber;

  /// upload outcome
  RemoteStorageRequestOutcome m_outcome;
};

/**
 * Wrapper over Amazon s3 client
 */
class SnowflakeS3Client : public Snowflake::Client::IStorageClient
{
public:
  SnowflakeS3Client(StageInfo *stageInfo,
                    unsigned int parallel,
                    size_t uploadThreshold,
                    TransferConfig *transferConfig);

  ~SnowflakeS3Client();

  /**
   * Upload data to s3 bucket. Object metadata will be retrieved to
   * deduplicate file
   * @param fileMetadata
   * @param dataStream
   * @return
   */
  RemoteStorageRequestOutcome upload(FileMetadata *fileMetadata,
                         std::basic_iostream<char> *dataStream);

  RemoteStorageRequestOutcome download(FileMetadata *fileMetadata,
    std::basic_iostream<char>* dataStream);

  RemoteStorageRequestOutcome doSingleDownload(FileMetadata *fileMetadata,
    std::basic_iostream<char>* dataStream);

  RemoteStorageRequestOutcome doMultiPartDownload(FileMetadata * fileMetadata,
    std::basic_iostream<char> *dataStream);

  RemoteStorageRequestOutcome GetRemoteFileMetadata(
    std::string * filePathFull, FileMetadata *fileMetadata);

private:
  Aws::SDKOptions options;

  Aws::Client::ClientConfiguration clientConfiguration;

  Aws::S3::S3Client *s3Client;

  StageInfo * m_stageInfo;

  Util::ThreadPool * m_threadPool;

  const size_t m_uploadThreshold;

  unsigned int m_parallel;

  /**
   * Add snowflake specific metadata to the put object metadata.
   * This includes encryption metadata and source file
   * message digest (after compression)
   * @param userMetadata
   * @param fileMetadata
   */
  void addUserMetadata(std::map<std::string, std::string> *userMetadata,
                       FileMetadata *fileMetadata);

  /**
   * Compose bucket and key value used for s3 request.
   * @param fileMetadata
   * @param bucket
   * @param key
   */
  void extractBucketAndKey(std::string *fileFullPath, std::string &bucket,
                           std::string &key);

  RemoteStorageRequestOutcome doSingleUpload(FileMetadata * fileMetadata,
                                 std::basic_iostream<char> *dataStream);

  RemoteStorageRequestOutcome doMultiPartUpload(FileMetadata * fileMetadata,
                                    std::basic_iostream<char> *dataStream);

  void uploadParts(MultiUploadCtx * uploadCtx);

  RemoteStorageRequestOutcome handleError(const Aws::Client::AWSError<Aws::S3::S3Errors> &error);
};
}
}

#endif //SNOWFLAKECLIENT_SNOWFLAKES3CLIENT_HPP
