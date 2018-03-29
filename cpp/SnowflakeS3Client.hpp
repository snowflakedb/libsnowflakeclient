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

namespace Snowflake
{
namespace Client
{
/**
 * Wrapper over Amazon s3 client
 */
class SnowflakeS3Client : public Snowflake::Client::IStorageClient
{
public:
  SnowflakeS3Client(StageInfo *stageInfo);

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

};
}
}

#endif //SNOWFLAKECLIENT_SNOWFLAKES3CLIENT_HPP
