#include "../include/snowflake/PlatformDetection.hpp"
#include "cJSON.h"

#include "snowflake/HttpClient.hpp"

namespace Snowflake
{
namespace Client
{
namespace PlatformDetection
{

enum PlatformDetectionStatus
{
  PLATFORM_DETECTED,
  PLATFORM_NOT_DETECTED,
  PLATFORM_DETECTION_TIMEOUT
};

typedef PlatformDetectionStatus (*PlatformDetectorFunc)(long timeout);

static const std::string awsMetadataBaseURL = "http://169.254.169.254";

PlatformDetectionStatus detectWithEndpoint(const HttpRequest& req, long timeout)
{
  HttpClientConfig cfg = { timeout };
  std::unique_ptr<IHttpClient> httpClient;
  httpClient.reset(IHttpClient::createSimple(cfg));

  auto responseOpt = httpClient->run(req);
  if (!responseOpt)
  {
    return PLATFORM_NOT_DETECTED;
  }

  const auto& response = responseOpt.get();
  if (response.code != 200)
  {
    return PLATFORM_NOT_DETECTED;
  }

  return PLATFORM_DETECTED;
}

PlatformDetectionStatus detectEc2Instance(long timeout)
{
  const auto url = boost::urls::url(awsMetadataBaseURL + "/latest/dynamic/instance-identity/document");
  HttpRequest req{
    HttpRequest::Method::GET,
    url,
    {}
  };

  return detectWithEndpoint(req, timeout);
}

static const std::map <std::string, PlatformDetectorFunc> detectors =
{
  {"is_ec2_instance", detectEc2Instance}
};

void getDetectedPlatforms(std::vector<std::string>& detectedPlatforms)
{
  static SF_MUTEX_HANDLE cacheMutex;
  static auto mutexInit = _mutex_init(&cacheMutex);
  static std::vector <std::string> detectedPlatformsCache;

  try
  {
    _mutex_lock(&cacheMutex);
    if (detectedPlatformsCache.empty())
    {
      // TODO: add checking disable env later
      for (const auto& pair : detectors)
      {
        // TODO: set timeout to 1 second for now, need to expand
        // IHttpClient to allow timeout in millisecond (we need 200ms)
        if (pair.second(1) == PLATFORM_DETECTED)
        {
          detectedPlatformsCache.push_back(pair.first);
        }
      }
    }
    detectedPlatforms = detectedPlatformsCache;
    _mutex_unlock(&cacheMutex);
  }
  catch (...)
  {
    // TODO: log error
    _mutex_unlock(&cacheMutex);
  }

  return;
}

} // namespace PlatformDetection
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

