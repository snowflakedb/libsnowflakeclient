/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_SNOWFLAKES3CLIENT_HPP
#define SNOWFLAKECLIENT_SNOWFLAKES3CLIENT_HPP

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/s3/S3Client.h>
#include "IStorageClient.hpp"
#include "StageInfo.hpp"
#include "FileMetadata.hpp"
#include "util/ThreadPool.hpp"
#include "util/StreamSplitter.hpp"

namespace Snowflake
{
namespace Client
{

/**
 * Context passed to upload thread
 */
struct MultiUploadCtx
{
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
  TransferOutcome m_outcome;
};

/**
 * Wrapper over Amazon s3 client
 */
class SnowflakeS3Client : public Snowflake::Client::IStorageClient
{
public:
  SnowflakeS3Client(StageInfo *stageInfo, unsigned int parallel);

  ~SnowflakeS3Client();

  /**
   * Upload data to s3 bucket. Object metadata will be retrieved to
   * deduplicate file
   * @param fileMetadata
   * @param dataStream
   * @return
   */
  TransferOutcome upload(FileMetadata *fileMetadata,
                         std::basic_iostream<char> *dataStream);

private:
  Aws::SDKOptions options;

  Aws::Client::ClientConfiguration clientConfiguration;

  Aws::S3::S3Client *s3Client;

  StageInfo * m_stageInfo;

  Util::ThreadPool * m_threadPool;

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
   * Check if file with same name and digest exists or not.
   * @param fileMetadata
   * @return true if file exists otherwise return false
   */
  bool fileExist(FileMetadata *fileMetadata);

  /**
   * Compose bucket and key value used for s3 request.
   * @param fileMetadata
   * @param bucket
   * @param key
   */
  void extractBucketAndKey(FileMetadata *fileMetadata, std::string &bucket,
                           std::string &key);

  TransferOutcome doSingleUpload(FileMetadata * fileMetadata,
                                 std::basic_iostream<char> *dataStream);

  TransferOutcome doMultiPartUpload(FileMetadata * fileMetadata,
                                    std::basic_iostream<char> *dataStream);

  void *uploadParts(MultiUploadCtx * uploadCtx);
};
}
}

#endif //SNOWFLAKECLIENT_SNOWFLAKES3CLIENT_HPP
