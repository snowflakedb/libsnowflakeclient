/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_JWTEXCEPTION_HPP
#define SNOWFLAKECLIENT_JWTEXCEPTION_HPP

#include <exception>

namespace Snowflake
{
namespace Client
{
namespace Jwt
{

struct JwtMemoryAllocationFailure : public std::exception
{
  const char *what() noexcept
  {
    return "Memory Allocation Failed";
  }
};

struct JwtParseFailure : public std::exception
{
  const char *what() noexcept
  {
    return "Parsing failure";
  }
};

struct JwtNotImplementedException : public std::exception
{
  const char *what() noexcept
  {
    return "Not Implemented";
  }
};

}
} // namespace Client
}
#endif //SNOWFLAKECLIENT_JWTEXCEPTION_HPP
