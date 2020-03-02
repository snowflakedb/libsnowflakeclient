/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_SNOWFLAKEAZURECLIENT_HPP
#define SNOWFLAKECLIENT_SNOWFLAKEAZURECLIENT_HPP

#include "snowflake/IFileTransferAgent.hpp"
#include "IStorageClient.hpp"
#include "snowflake/PutGetParseResponse.hpp"
#include "FileMetadata.hpp"
#include "util/ThreadPool.hpp"
#include "util/ByteArrayStreamBuf.hpp"
#include "storage_credential.h"
#include "storage_account.h"
#include "blob/blob_client.h"
#include <sstream>
#include <string>

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

struct MultiUploadCtx_a
{
  MultiUploadCtx_a(std::string &uploadId,
    unsigned int partNumber,
    std::string &key,
    std::string &bucket)
  {

  }

  /// in memory buffer used to store current part data
  Util::ByteArrayStreamBuf *buf;

  /// part number
  unsigned int m_partNumber;

  /// upload outcome
  RemoteStorageRequestOutcome m_outcome;
};

struct MultiDownloadCtx_a
{
  /// in memory buffer used to store current part data
  Util::ByteArrayStreamBuf *buf;

  ///Start byte of the chunk.
  unsigned long long startbyte;

  /// part number
  unsigned int m_partNumber;

  /// upload outcome
  RemoteStorageRequestOutcome m_outcome;
};

/**
 * Wrapper over Azure client
 */
class SnowflakeAzureClient : public Snowflake::Client::IStorageClient
{
public:
  SnowflakeAzureClient(StageInfo *stageInfo, unsigned int parallel, size_t uploadThreshold,
                       TransferConfig *transferConfig);

  ~SnowflakeAzureClient();

  /**
   * Upload data to Azure container. Object metadata will be retrieved to
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


  StageInfo * m_stageInfo;

  Util::ThreadPool * m_threadPool;
  azure::storage_lite::blob_client_wrapper *m_blobclient;

  const size_t m_uploadThreshold;
  unsigned int m_parallel;

  /**
   * Add snowflake specific metadata to the put object metadata.
   * This includes encryption metadata and source file
   * message digest (after compression)
   * @param userMetadata
   * @param fileMetadata
   */
  void addUserMetadata(std::vector<std::pair<std::string, std::string>> *userMetadata,
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

  void uploadParts(MultiUploadCtx_a * uploadCtx);

  //RemoteStorageRequestOutcome handleError(const Aws::Client::AWSError<Aws::S3::S3Errors> &error);
};
}
}

#endif //SNOWFLAKECLIENT_SNOWFLAKES3CLIENT_HPP
