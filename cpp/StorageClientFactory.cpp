/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "StorageClientFactory.hpp"
#include "SnowflakeS3Client.hpp"
#include "logger/SFLogger.hpp"

namespace Snowflake
{
namespace Client
{

IStorageClient * StorageClientFactory::injectedClient = nullptr;

IStorageClient * StorageClientFactory::getClient(
  StageInfo *stageInfo, unsigned int parallel)
{
  switch (stageInfo->stageType)
  {
    case StageType::S3:
      CXX_LOG_INFO("Creating S3 client");
      return new SnowflakeS3Client(stageInfo, parallel);
    case StageType::MOCKED_STAGE_TYPE:
      return injectedClient;
    default:
      // invalid stage type
      throw;
  }
}

void StorageClientFactory::injectMockedClient(IStorageClient *client)
{
  injectedClient = client;
}

}
}
