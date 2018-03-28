//
// Created by hyu on 2/5/18.
//

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
