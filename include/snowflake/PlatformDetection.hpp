#ifndef SNOWFLAKE_PLATFORMDETECTION_HPP
#define SNOWFLAKE_PLATFORMDETECTION_HPP

#include <string>
#include <vector>
#include <future>

namespace Snowflake::Client::PlatformDetection
{
  /**
    * fill the picojson object with platforms detected.
    */
  void getDetectedPlatforms(std::vector<std::string>& detectedPlatforms);

}
#endif //SNOWFLAKE_PLATFORMDETECTION_HPP
