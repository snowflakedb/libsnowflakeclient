/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */


#include "ClaimSet.hpp"

namespace Snowflake
{
namespace Client
{
namespace Jwt
{

IClaimSet *IClaimSet::buildClaimSet()
{
  return new CJSONClaimSet();
}

IClaimSet *IClaimSet::parseClaimset(const std::string &text)
{
  return new CJSONClaimSet(text);
}
} // namespace Jwt
} // namespace Client
} // namespace Snowflake
