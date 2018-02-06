//
// Created by hyu on 2/5/18.
//

#ifndef SNOWFLAKECLIENT_SNOWFLAKES3CLIENT_HPP
#define SNOWFLAKECLIENT_SNOWFLAKES3CLIENT_HPP

#include "IStorageClient.hpp"

namespace Snowflake
{
  namespace Client
  {
    /**
     * Wrapper over Amazon s3 client
     */
    class SnowflakeS3Client : Snowflake::Client::IStorageClient
    {

    };
  }
}

#endif //SNOWFLAKECLIENT_SNOWFLAKES3CLIENT_HPP
