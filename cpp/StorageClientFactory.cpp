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

std::shared_ptr<IStorageClient> StorageClientFactory::getClient(
  StageInfo *stageInfo, unsigned int parallel)
{
  switch (stageInfo->getStageType())
  {
    case StageType::S3:
      return std::make_shared<SnowflakeS3Client>(stageInfo, parallel);
    default:
      // invalid stage type
      throw;
  }
}

}
}
