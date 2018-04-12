/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "MultiPartUploader.hpp"
#include "util/StreamSplitter.hpp"
#include <aws/s3/model/CreateMultipartUploadRequest.h>

#define CONTENT_TYPE_OCTET_STREAM "application/octet-stream"

Snowflake::Client::MultiPartUploader::MultiPartUploader(
  std::basic_iostream<char> *dataStream, FileMetadata *fileMetadata,
  Aws::S3::S3Client *s3Client, std::string &bucket, std::string & key,
  std::map<std::string, std::string>& metadata)
{
}

TransferOutcome Snowflake::Client::MultiPartUploader::upload()
{
  Aws::S3::Model::CreateMultipartUploadRequest request;
  request.WithBucket(bucket)
    .WithKey(key)
    .WithContentType(CONTENT_TYPE_OCTET_STREAM)
    .WithMetadata(metadata) ;

  auto createMultiPartResp = s3Client->CreateMultipartUpload(request);
  if (createMultiPartResp.IsSuccess())
  {
    m_uploadId = createMultiPartResp.GetResult().GetUploadId();
  }
  std::vector<SF_THREAD_HANDLE> threads;

  for (size_t i=0; i<4; i++)
  {
    SF_THREAD_HANDLE tid;
    _thread_init(&tid, , (void *)this);
    threads.push_back(tid);
  }

  for (auto &x : threads)
    _thread_join(x);
  }
}