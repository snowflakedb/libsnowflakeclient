/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_STORAGECLIENTFACTORY_HPP
#define SNOWFLAKECLIENT_STORAGECLIENTFACTORY_HPP

#include <memory>
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
  static std::shared_ptr<IStorageClient> getClient(StageInfo *stageInfo,
                                                   unsigned int parallel);
};
}
}
#endif //SNOWFLAKECLIENT_STORAGECLIENTFACTORY_HPP
