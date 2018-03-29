/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_ISTORAGECLIENT_HPP
#define SNOWFLAKECLIENT_ISTORAGECLIENT_HPP

#include "TransferOutcome.hpp"
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

  //virtual void shutDown() = 0;

  //virtual void download() = 0;

  virtual TransferOutcome upload(FileMetadata *fileMetadata,
                                 std::basic_iostream<char> *dataStream) = 0;

  //virtual void renewToken() = 0;
};
}
}

#endif //SNOWFLAKECLIENT_ISTORAGECLIENT_HPP
