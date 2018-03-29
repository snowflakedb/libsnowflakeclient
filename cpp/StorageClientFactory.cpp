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

IStorageClient *StorageClientFactory::getClient(StageInfo *stageInfo)
{
  switch (stageInfo->getStageType())
  {
    case StageType::S3:
      return new SnowflakeS3Client(stageInfo);
    default:
      // invalid stage type
      throw;
  }
}

}
}
