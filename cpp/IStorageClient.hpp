//
// Created by hyu on 2/5/18.
//

#ifndef SNOWFLAKECLIENT_ISTORAGECLIENT_HPP
#define SNOWFLAKECLIENT_ISTORAGECLIENT_HPP

namespace Snowflake
{
  namespace Client
  {
    class IStorageClient
    {
    public:
      virtual void shutDown() = 0;

      virtual void download() = 0;

      virtual void upload() = 0;

      virtual void renewToken() = 0;
    };
  }
}

#endif //SNOWFLAKECLIENT_ISTORAGECLIENT_HPP
