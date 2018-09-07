/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */


#include "ClaimSet.hpp"

namespace Snowflake
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
} // namespace Snowflake
