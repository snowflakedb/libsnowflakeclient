/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <cstdlib>
#include "TestSetup.hpp"

std::string TestSetup::getDataDir()
{
  return std::string("data") + PATH_SEP;
}
