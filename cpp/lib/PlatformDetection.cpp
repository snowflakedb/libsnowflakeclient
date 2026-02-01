#include <chrono>
#include "snowflake/PlatformDetection.hpp"
#include "snowflake/platform.h"
#include "snowflake/HttpClient.hpp"
#include "snowflake/AWSUtils.hpp"
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/Aws.h>
#include <boost/algorithm/string.hpp>
#include <exception>
#include "../util/SnowflakeCommon.hpp"
#include "../logger/SFLogger.hpp"

namespace Snowflake::Client::PlatformDetection
{

enum PlatformDetectionStatus
{
  PLATFORM_DETECTED,
  PLATFORM_NOT_DETECTED,
  PLATFORM_DETECTION_TIMEOUT
};

typedef PlatformDetectionStatus (*PlatformDetectorEnvFunc)();
typedef PlatformDetectionStatus(*PlatformDetectorEndpointFunc)(long timeout);

#define AWS_METADATA_BASE_URL "http://169.254.169.254"
#define AZURE_METADATA_BASE_URL "http://169.254.169.254"
#define GCP_METADATA_BASE_URL "http://metadata.google.internal"

#define MAX_ENV_VARIABLE_LENGTH 32767

static std::string awsMetadataBaseURL = AWS_METADATA_BASE_URL;
static std::string azureMetadataBaseURL = AZURE_METADATA_BASE_URL;
static std::string gcpMetadataBaseURL = GCP_METADATA_BASE_URL;
static std::string gcpMetadataFlavorHeaderName = "Metadata-Flavor";
static std::string gcpMetadataFlavor = "Google";

std::string getEnvironmentVariableValue(const std::string& envVarName)
{
  char envbuf[MAX_ENV_VARIABLE_LENGTH];
  if (char* value = sf_getenv_s(envVarName.c_str(), envbuf, sizeof(envbuf)))
  {
    return std::string(value);
  }
  return "";
}

PlatformDetectionStatus detectWithEndpoint(
  const HttpRequest& req,
  long timeout,
  std::map <std::string, std::string> * respHeaders = NULL)
{
  // use timeout in milliseconds
  HttpClientConfig cfg = { 0, timeout, 0, timeout };
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

  if (respHeaders)
  {
    Snowflake::Client::Util::parseHttpRespHeaders(response.getHeader(), *respHeaders);
  }

  return PLATFORM_DETECTED;
}

PlatformDetectionStatus detectAwsLambdaEnv()
{
  return getEnvironmentVariableValue("LAMBDA_TASK_ROOT").empty() ? PLATFORM_NOT_DETECTED : PLATFORM_DETECTED;
}

PlatformDetectionStatus detectAzureFunctionEnv()
{
  if (getEnvironmentVariableValue("FUNCTIONS_WORKER_RUNTIME").empty() ||
    getEnvironmentVariableValue("FUNCTIONS_EXTENSION_VERSION").empty() ||
    getEnvironmentVariableValue("AzureWebJobsStorage").empty())
  {
    return PLATFORM_NOT_DETECTED;
  }
  return PLATFORM_DETECTED;

}
PlatformDetectionStatus detectGceCloudRunServiceEnv()
{
  return (getEnvironmentVariableValue("K_SERVICE").empty() ||
    getEnvironmentVariableValue("K_REVISION").empty() ||
    getEnvironmentVariableValue("K_CONFIGURATION").empty())
    ? PLATFORM_NOT_DETECTED
    : PLATFORM_DETECTED;
}

PlatformDetectionStatus detectGceCloudRunJobEnv()
{
  return getEnvironmentVariableValue("CLOUD_RUN_JOB").empty() ||
    getEnvironmentVariableValue("CLOUD_RUN_EXECUTION").empty()
    ? PLATFORM_NOT_DETECTED
    : PLATFORM_DETECTED;
}

PlatformDetectionStatus detectGithubActionEnv()
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

PlatformDetectionStatus detectAzureVM(long timeout)
{
  const auto url = boost::urls::url(azureMetadataBaseURL + "/metadata/instance?api-version=2019-03-11");
  HttpRequest req{
    HttpRequest::Method::GET,
    url,
    {
      {"Metadata", "true"},
    },
  };

  return detectWithEndpoint(req, timeout);
}

PlatformDetectionStatus detectAzureManagedIdentity(long timeout)
{
  if ((PLATFORM_DETECTED == detectAzureFunctionEnv()) &&
      (!getEnvironmentVariableValue("IDENTITY_HEADER").empty()))
  {
    return PLATFORM_DETECTED;
  }
  auto url = boost::urls::url(azureMetadataBaseURL + "/metadata/identity/oauth2/token");
  url.params().set("api-version", "2018-02-01");
  url.params().set("resource", "https://management.azure.com");
  HttpRequest req{
    HttpRequest::Method::GET,
    url,
    {
      {"Metadata", "True"},
    },
  };

  return detectWithEndpoint(req, timeout);
}

PlatformDetectionStatus detectGceVM(long timeout)
{
  auto url = boost::urls::url(gcpMetadataBaseURL);
  HttpRequest req{
    HttpRequest::Method::GET,
    url,
    {}
  };

  std::map <std::string, std::string> respHeaders;

  PlatformDetectionStatus ret = detectWithEndpoint(req, timeout, &respHeaders);
  if (ret != PLATFORM_DETECTED)
  {
    return ret;
  }

  if (respHeaders[gcpMetadataFlavorHeaderName] == gcpMetadataFlavor)
  {
    return PLATFORM_DETECTED;
  }

  return PLATFORM_NOT_DETECTED;
}

PlatformDetectionStatus detectGcpIdentity(long timeout)
{
  auto url = boost::urls::url(gcpMetadataBaseURL + "/computeMetadata/v1/instance/service-accounts/default/email");
  HttpRequest req{
    HttpRequest::Method::GET,
    url,
    {
      {gcpMetadataFlavorHeaderName, gcpMetadataFlavor},
    },
  };

  return detectWithEndpoint(req, timeout);
}

PlatformDetectionStatus detectAwsIdentity(long timeout)
{
  SF_UNUSED(timeout);
  auto awsSdkInit = AwsUtils::initAwsSdk();
  Aws::Auth::DefaultAWSCredentialsProviderChain credentialsProvider;
  auto creds = credentialsProvider.GetAWSCredentials();
  std::string accessKey = creds.GetAWSAccessKeyId();
  std::string secretKey = creds.GetAWSSecretKey();
  boost::trim(accessKey);
  boost::trim(secretKey);

  if (!accessKey.empty() && !secretKey.empty())
  {
    return PLATFORM_DETECTED;
  }
  return PLATFORM_NOT_DETECTED;
}

static const std::map <std::string, PlatformDetectorEnvFunc> envDetectors =
{
  {"is_aws_lambda", detectAwsLambdaEnv},
  {"is_azure_function", detectAzureFunctionEnv},
  {"is_gce_cloud_run_service", detectGceCloudRunServiceEnv},
  {"is_gce_cloud_run_job", detectGceCloudRunJobEnv},
  {"is_github_action", detectGithubActionEnv},
};

static const std::map <std::string, PlatformDetectorEndpointFunc> endpointDetectors =
{
  {"is_ec2_instance", detectEc2Instance},
  {"has_aws_identity", detectAwsIdentity},
  {"is_azure_vm", detectAzureVM},
  {"has_azure_managed_identity", detectAzureManagedIdentity},
  {"is_gce_vm", detectGceVM},
  {"has_gcp_identity", detectGcpIdentity}
};

static bool detectionDone = false;
static std::vector<std::string> detectedPlatformsCache;

void getDetectedPlatforms(std::vector<std::string>& detectedPlatforms, long timeoutMs)
{
  static SF_MUTEX_HANDLE cacheMutex;
  static auto mutexInit = _mutex_init(&cacheMutex);
  SF_UNUSED(mutexInit);

  try
  {
    _mutex_lock(&cacheMutex);
    if (!detectionDone)
    {
      detectedPlatformsCache.clear();
      if (!getEnvironmentVariableValue("SNOWFLAKE_DISABLE_PLATFORM_DETECTION").empty())
      {
        detectedPlatformsCache.push_back("disabled");
      }
      else
      {
        std::vector<std::future<std::string> > futures;
        futures.reserve(endpointDetectors.size());

        for (const auto& pair : endpointDetectors)
        {
          futures.push_back(std::async(std::launch::async, [detector = pair.second, platform = pair.first, timeoutMs] {
            return detector(timeoutMs) == PLATFORM_DETECTED ? platform : "";
            }));
        }

        for (const auto& pair : envDetectors)
        {
          if (pair.second() == PLATFORM_DETECTED)
          {
            detectedPlatformsCache.push_back(pair.first);
          }
        }

        auto endTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        for (auto& fut : futures)
        {
          std::chrono::nanoseconds remainTime(0);
          auto curTime = std::chrono::steady_clock::now();
          if (curTime < endTime)
          {
            remainTime = endTime - curTime;
          }
          if (fut.wait_for(remainTime) == std::future_status::ready)
          {
            std::string result = fut.get();
            if (!result.empty())
            {
              detectedPlatformsCache.push_back(result);
            }
          }
        }
      }
      detectionDone = true;
    }
    detectedPlatforms = detectedPlatformsCache;
    _mutex_unlock(&cacheMutex);
  }
  catch (const std::exception& e)
  {
    _mutex_unlock(&cacheMutex);
    CXX_LOG_TRACE("getDetectedPlatforms caught exception: %s", e.what());
  }
  catch (...)
  {
    _mutex_unlock(&cacheMutex);
    CXX_LOG_TRACE("getDetectedPlatforms caught unknown exception");
  }

  return;
}

} // namespace



// wrapper functions for C
extern "C"
{

using namespace Snowflake::Client::PlatformDetection;
// Functions for test purpose
void resetDetection()
{
  detectionDone = false;
  detectedPlatformsCache.clear();
}

void redirectMetadataBaseUrl(const char* url)
{
  awsMetadataBaseURL = url;
  azureMetadataBaseURL = url;
  gcpMetadataBaseURL = url;
}

void restoreMetadataBaseUrl()
{
  awsMetadataBaseURL = AWS_METADATA_BASE_URL;
  awsMetadataBaseURL = AZURE_METADATA_BASE_URL;
  gcpMetadataBaseURL = GCP_METADATA_BASE_URL;
}

} // extern "C"

