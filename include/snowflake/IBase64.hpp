/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_IBASE64_HPP
#define SNOWFLAKECLIENT_IBASE64_HPP

#include <vector>
#include <string>

namespace Snowflake
{
namespace Client
{
namespace Util
{
  class IBase64
  {
   static std::string encodeURLNoPadding(const std::vector<char> &bytes);

   static std::vector<char> decodeURLNoPadding(const std::string &code);

   static std::string encodePadding(const std::vector<char> &bytes);

   static std::vector<char> decodePadding(const std::string &code);
  };
} // namespace Jwt
} // namespace Client
} // namespace Snowflake

#endif //SNOWFLAKECLIENT_IBASE64_HPP
