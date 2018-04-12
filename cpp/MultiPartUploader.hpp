/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_MULTIPARTUPLOADER_HPP
#define SNOWFLAKECLIENT_MULTIPARTUPLOADER_HPP

#include <iostream>
#include <aws/s3/S3Client.h>
#include "FileMetadata.hpp"
#include "TransferOutcome.hpp"

namespace Snowflake
{
namespace Client
{
class MultiPartUploader
{
public:
  MultiPartUploader(std::basic_iostream<char> * dataStream,
                    FileMetadata * fileMetadata,
                    Aws::S3::S3Client * s3Client,
                    std::string & bucket, std::string & key,
                    std::map<std::string, std::string> &metadata);

  TransferOutcome upload();

private:
  std::string * m_key;
  std::string * m_bucket;
  std::map<std::string, std::string> m_metadata;
};
}
}
#endif //SNOWFLAKECLIENT_MULTIPARTUPLOADER_HPP
