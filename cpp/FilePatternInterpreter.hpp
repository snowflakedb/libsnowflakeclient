/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_FILEPATTERNINTERPRETER_HPP
#define SNOWFLAKECLIENT_FILEPATTERNINTERPRETER_HPP

#include <string>

namespace Snowflake
{
namespace Client
{
/**
 * Class used to handle source locations.
 * i.e. expand ~ match file name and pattern
 */
class FilePatternInterpreter
{
public:
  /**
   * Give file pattern return a list of all matching file names
   */
  void matchFilePattern(std::string & sourceLocation);

  void populateSrcLocations(std::string &sourceLocation);
};
}
}

#endif //SNOWFLAKECLIENT_FILEPATTERNINTERPRETER_HPP
