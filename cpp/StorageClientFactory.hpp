//
// Created by hyu on 2/5/18.
//

#ifndef SNOWFLAKECLIENT_STORAGECLIENTFACTORY_HPP
#define SNOWFLAKECLIENT_STORAGECLIENTFACTORY_HPP

#include "IStorageClient.hpp"

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

      IStorageClient* createClient();
    };
  }
}
#endif //SNOWFLAKECLIENT_STORAGECLIENTFACTORY_HPP
