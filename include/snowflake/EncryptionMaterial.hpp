/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_REMOTESTOREFILEENCRYPTIONMATERIAL_HPP
#define SNOWFLAKECLIENT_REMOTESTOREFILEENCRYPTIONMATERIAL_HPP

#include <string>

namespace Snowflake
{
namespace Client
{
struct EncryptionMaterial
{
  /// master key to encrypt file key
  std::string queryStageMasterKey;

  ///  query id
  std::string queryId;

  /// smk id
  long smkId;
};


}
}

#endif //SNOWFLAKECLIENT_REMOTESTOREFILEENCRYPTIONMATERIAL_HPP
