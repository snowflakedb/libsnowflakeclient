/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "StorageClientFactory.hpp"
#include "StageInfo.hpp"
#include "SnowflakeS3Client.hpp"

namespace Snowflake
{
namespace Client
{

IStorageClient * StorageClientFactory::injectedClient = nullptr;

IStorageClient * StorageClientFactory::getClient(
  StageInfo *stageInfo, unsigned int parallel)
{
  switch (stageInfo->getStageType())
  {
    case StageType::S3:
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
