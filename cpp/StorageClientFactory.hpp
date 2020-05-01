/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_STORAGECLIENTFACTORY_HPP
#define SNOWFLAKECLIENT_STORAGECLIENTFACTORY_HPP

#include <memory>
#include "IStorageClient.hpp"
#include "snowflake/PutGetParseResponse.hpp"
#include "snowflake/IFileTransferAgent.hpp"

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
  static IStorageClient *getClient(StageInfo *stageInfo,
                                   unsigned int parallel,
                                   size_t uploadThreshold,
                                   TransferConfig * transferConfig = nullptr,
                                   IStatementPutGet* statement = nullptr);

  /**
   * Testing method. Used to inject a mocked remote storage client.
   */
  static void injectMockedClient(IStorageClient *client);

private:

  static IStorageClient * injectedClient;
};
}
}
#endif //SNOWFLAKECLIENT_STORAGECLIENTFACTORY_HPP
