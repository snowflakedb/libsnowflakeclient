#ifndef SNOWFLAKE_PLATFORMDETECTION_HPP
#define SNOWFLAKE_PLATFORMDETECTION_HPP

#include <string>
#include <vector>

namespace Snowflake::Client::PlatformDetection
{
  /**
    * fill the picojson object with platforms detected.
    */
  void getDetectedPlatforms(std::vector<std::string>& detectedPlatforms);

}
#endif //SNOWFLAKE_PLATFORMDETECTION_HPP
