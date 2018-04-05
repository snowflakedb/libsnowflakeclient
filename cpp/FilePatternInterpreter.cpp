/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "FilePatternInterpreter.hpp"
#include <dirent.h>

void Snowflake::Client::FilePatternInterpreter::populateSrcLocations(
  std::string &sourceLocation)
{
  unsigned long dirSep = sourceLocation.find_last_of('/');
  std::string dirPath = sourceLocation.substr(0, dirSep);
  std::string filePattern = sourceLocation.substr(dirSep + 1);

  DIR * dir = nullptr;
  if ((dir = opendir(dirPath.c_str())) != NULL)
  {
     
  }
  else
  {
    // open dir failed
    throw;
  }


}
