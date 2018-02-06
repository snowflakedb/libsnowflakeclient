//
// Created by hyu on 2/5/18.
//

#ifndef SNOWFLAKECLIENT_ISTORAGEOBJECTMETADATA_HPP
#define SNOWFLAKECLIENT_ISTORAGEOBJECTMETADATA_HPP

namespace Snowflake
{
  namespace Client
  {
    class IStorageObjectMetadata
    {
    public:
      virtual void addUserMetadata(std::string *key, std::string *value) = 0;

      virtual long getContentLength() = 0;

      virtual void setContentLength(long contentLength) = 0;
    };
  }
}


#endif //SNOWFLAKECLIENT_ISTORAGEOBJECTMETADATA_HPP
