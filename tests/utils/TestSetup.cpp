/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <cstdlib>
#include "TestSetup.hpp"

std::string TestSetup::getDataDir()
{
#ifdef _WIN32
  char appveyorBuildDir[500];
  DWORD dwRet = 0;
  dwRet= GetEnvironmentVariable("APPVEYOR_BUILD_FOLDER", appveyorBuildDir, sizeof(appveyorBuildDir));
  if (dwRet != 0)
  {
    return std::string(appveyorBuildDir) + PATH_SEP + "tests" + PATH_SEP + "data" + PATH_SEP;
  }
#else
  const char * travisBuildDir = getenv("TRAVIS_BUILD_DIR");
  if (travisBuildDir)
  {
    return std::string(travisBuildDir) + PATH_SEP + "tests" + PATH_SEP + "data" + PATH_SEP;
  }
#endif

  const std::string current_file = __FILE__;
  std::string utilDir = current_file.substr(0, current_file.find_last_of(PATH_SEP));
  return utilDir + PATH_SEP + ".." + PATH_SEP + "data" + PATH_SEP;
}
