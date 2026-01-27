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
  static HttpClientConfig cfg = { timeout };
  static std::unique_ptr<IHttpClient> httpClient =
	  std::make_unique<IHttpClient>(IHttpClient::createSimple(cfg));

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

std::map <std::string, PlatformDetectorFunc> detectors =
{
  {"is_ec2_instance", detectEc2Instance}
};

std::vector <std::string> detectedPlatformsCache;

void getDetectedPlatforms(std::vector<std::string>& detectedPlatforms)
{
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

