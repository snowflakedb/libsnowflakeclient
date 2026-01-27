#include "../include/snowflake/PlatformDetection.hpp"
#include "cJSON.h"

#include "snowflake/HttpClient.hpp"

namespace Snowflake::Client::PlatformDetection
{

enum PlatformDetectionStatus
{
  PLATFORM_DETECTED,
  PLATFORM_NOT_DETECTED,
  PLATFORM_DETECTION_TIMEOUT
};

typedef PlatformDetectionStatus (*PlatformDetectorFunc)(long timeout);

#define AWS_METADATA_BASE_URL "http://169.254.169.254"

static std::string awsMetadataBaseURL = AWS_METADATA_BASE_URL;

std::string getEnvironmentVariableValue(const std::string& envVarName)
{
  char envbuf[MAX_PATH + 1];
  if (char* value = sf_getenv_s(envVarName.c_str(), envbuf, sizeof(envbuf)))
  {
    return std::string(value);
  }
  return "";
}

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

PlatformDetectionStatus detectAwsLambda(long timeout)
{
  return getEnvironmentVariableValue("LAMBDA_TASK_ROOT").empty() ? PLATFORM_NOT_DETECTED : PLATFORM_DETECTED;
}

PlatformDetectionStatus detectAzureFunction(long timeout)
{
  if (getEnvironmentVariableValue("FUNCTIONS_WORKER_RUNTIME").empty() ||
    getEnvironmentVariableValue("FUNCTIONS_EXTENSION_VERSION").empty() ||
    getEnvironmentVariableValue("AzureWebJobsStorage").empty())
  {
    return PLATFORM_NOT_DETECTED;
  }
  return PLATFORM_DETECTED;
}

PlatformDetectionStatus detectGceCloudRunService(long timeout)
{
  return (getEnvironmentVariableValue("K_SERVICE").empty() ||
    getEnvironmentVariableValue("K_REVISION").empty() ||
    getEnvironmentVariableValue("K_CONFIGURATION").empty())
    ? PLATFORM_NOT_DETECTED
    : PLATFORM_DETECTED;
}

PlatformDetectionStatus detectGceCloudRunJob(long timeout)
{
  return getEnvironmentVariableValue("CLOUD_RUN_JOB").empty() ? PLATFORM_NOT_DETECTED : PLATFORM_DETECTED;
}

PlatformDetectionStatus detectGithubAction(long timeout)
{
  return getEnvironmentVariableValue("GITHUB_ACTIONS").empty() ? PLATFORM_NOT_DETECTED : PLATFORM_DETECTED;
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
  {"is_aws_lambda", detectAwsLambda},
  {"is_azure_function", detectAzureFunction},
  {"is_gce_cloud_run_service", detectGceCloudRunService},
  {"is_gce_cloud_run_job", detectGceCloudRunJob},
  {"is_github_action", detectGithubAction},
  {"is_ec2_instance", detectEc2Instance}
};

static bool detectionDone = false;

void getDetectedPlatforms(std::vector<std::string>& detectedPlatforms)
{
  static SF_MUTEX_HANDLE cacheMutex;
  static auto mutexInit = _mutex_init(&cacheMutex);
  static std::vector <std::string> detectedPlatformsCache;

  try
  {
    _mutex_lock(&cacheMutex);
    if (!detectionDone)
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
      detectionDone = true;
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

} // namespace



// wrapper functions for C
extern "C"
{

using namespace Snowflake::Client::PlatformDetection;
cJSON * get_detected_platforms()
{
  // to ensure no exception thrown from C interface
  try
  {
    std::vector<std::string> detectedPlatforms;
    getDetectedPlatforms(detectedPlatforms);
    cJSON* platformsJson = snowflake_cJSON_CreateArray();
    for (auto platform : detectedPlatforms)
    {
      cJSON* val = snowflake_cJSON_CreateString(platform.c_str());
      snowflake_cJSON_AddItemToArray(platformsJson, val);
    }
    return platformsJson;
  }
  catch (...)
  {
    // TODO: log error
  }
  return NULL;
}

// Functions for test purpose
void resetDetection()
{
  detectionDone = false;
}

void redirectMetadataBaseUrl(const char* url)
{
  awsMetadataBaseURL = url;
}

void restoreMetadataBaseUrl()
{
  awsMetadataBaseURL = AWS_METADATA_BASE_URL;
}

} // extern "C"

