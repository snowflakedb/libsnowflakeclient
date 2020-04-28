/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_ISTORAGECLIENT_HPP
#define SNOWFLAKECLIENT_ISTORAGECLIENT_HPP

#include "RemoteStorageRequestOutcome.hpp"
#include "FileMetadata.hpp"

namespace Snowflake
{
namespace Client
{

/**
 * Virtual class used to communicate with remote storage.
 * For now only s3 client is implemented
 */
class IStorageClient
{
public:
  virtual ~IStorageClient() {};

  virtual RemoteStorageRequestOutcome download(FileMetadata * fileMetadata,
    std::basic_iostream<char>* dataStream) = 0;

  /**
   * Upload file stream data to remote storage
   * @param fileMetadata
   * @param dataStream
   * @return
   */
  virtual RemoteStorageRequestOutcome upload(FileMetadata *fileMetadata,
                                 std::basic_iostream<char> *dataStream) = 0;

  /**
   * Get size of a remote file and encryption metadata
   */
  virtual RemoteStorageRequestOutcome GetRemoteFileMetadata(
    std::string * filePathFull, FileMetadata *fileMetadata) = 0;

  /**
  * @return Whether this client requires the use of presigned URLs for upload
  * and download instead of credentials that work for all files uploaded/
  * downloaded to a stage path. True for GCS.
  */
  virtual bool requirePresignedUrl()
  {
    return false;
  }
};
}
}

#endif //SNOWFLAKECLIENT_ISTORAGECLIENT_HPP
