#include "../include/snowflake/PlatformDetection.hpp"
#include "cJSON.h"

#include "snowflake/HttpClient.hpp"

namespace Snowflake
{
namespace Client
{

enum PlatformDetectionStatus
{
  PLATFORM_DETECTED,
  PLATFORM_NOT_DETECTED,
  PLATFORM_DETECTION_TIMEOUT
};

typedef PlatformDetectionStatus (*PlatformDetectorFunc)(int timeout);

static const std::string awsMetadataBaseURL = "http://169.254.169.254";

PlatformDetectionStatus detectEc2Instance(int timeout)
{
  return PLATFORM_NOT_DETECTED;
}

std::map <std::string, PlatformDetectorFunc> detectors =
{
  {"is_ec2_instance", detectEc2Instance}
};

std::vector <std::string> detectedPlatformsCache;

void getDetectedPlatforms(std::vector<std::string>& detectedPlatforms)
{
  return;
}

} // namespace Client
} // namespace Snowflake



// wrapper functions for C
extern "C"
{

cJSON * get_detected_platforms()
{
  return NULL;
}

} // extern "C"

