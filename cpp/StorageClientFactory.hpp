//
// Created by hyu on 2/5/18.
//

#ifndef SNOWFLAKECLIENT_STORAGECLIENTFACTORY_HPP
#define SNOWFLAKECLIENT_STORAGECLIENTFACTORY_HPP

#include "IStorageClient.hpp"
#include "StageInfo.hpp"

namespace Snowflake
{
namespace Client
{
class IStorageClient;

/**
 * Factory class used to create client. Currently only s3 is supported.
 * Later, Azure support will be added.
 */
class StorageClientFactory
{
public:

  /**
   * Return a newly created storage client. Caller need to delete the instance
   * @param stageInfo
   * @return
   */
  static IStorageClient *getClient(StageInfo *stageInfo);
};
}
}
#endif //SNOWFLAKECLIENT_STORAGECLIENTFACTORY_HPP
