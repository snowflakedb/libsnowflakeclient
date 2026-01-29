#include "snowflake/PlatformDetection.hpp"
#include "snowflake/platform.h"
#include "snowflake/HttpClient.hpp"
#include "snowflake/AWSUtils.hpp"
#include <aws/core/Aws.h>
#include <aws/sts/STSClient.h>
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

typedef PlatformDetectionStatus (*PlatformDetectorFunc)(long timeout);

#define AWS_METADATA_BASE_URL "http://169.254.169.254"
#define AZURE_METADATA_BASE_URL "http://169.254.169.254"
#define GCE_METADATA_ROOT_URL "http://metadata.google.internal"
#define GCP_METADATA_BASE_URL "http://metadata.google.internal/computeMetadata/v1"

static std::string awsMetadataBaseURL = AWS_METADATA_BASE_URL;
static std::string azureMetadataBaseURL = AZURE_METADATA_BASE_URL;
static std::string gceMetadataRootURL = GCE_METADATA_ROOT_URL;
static std::string gcpMetadataBaseURL = GCP_METADATA_BASE_URL;
static std::string gcpMetadataFlavorHeaderName = "Metadata-Flavor";
static std::string gcpMetadataFlavor = "Google";

PlatformDetectionStatus detectWithEndpoint(
  const HttpRequest& req,
  long timeout,
  std::map <std::string, std::string> * respHeaders = NULL)
{
  // use timeout in milliseconds
  HttpClientConfig cfg = { 0, timeout };
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

// TODO:: remove the temporary prototype
PlatformDetectionStatus detectAzureFunctionEnv(long timeout)
{
  return PLATFORM_DETECTED;
}

PlatformDetectionStatus detectAzureManagedIdentity(long timeout)
{
  char envbuf[32768];
  if ((PLATFORM_DETECTED == detectAzureFunctionEnv(timeout)) &&
      (sf_getenv_s("IDENTITY_HEADER", envbuf, sizeof(envbuf))))
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
  auto url = boost::urls::url(gceMetadataRootURL);
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
  auto url = boost::urls::url(gcpMetadataBaseURL + "/instance/service-accounts/default/email");
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
  auto awsSdkInit = AwsUtils::initAwsSdk();
  Aws::Client::ClientConfiguration clientConfig;
  clientConfig.connectTimeoutMs = timeout;
  clientConfig.requestTimeoutMs = timeout;
  Aws::STS::STSClient stsClient(clientConfig);
  Aws::STS::Model::GetCallerIdentityRequest request;
  Aws::STS::Model::GetCallerIdentityOutcome outcome = stsClient.GetCallerIdentity(request);

  if (outcome.IsSuccess())
  {
    return PLATFORM_DETECTED;
  }
  Aws::STS::STSErrors errType = outcome.GetError().GetErrorType();
  if (Aws::STS::STSErrors::REQUEST_TIMEOUT == errType)
  {
    return PLATFORM_DETECTION_TIMEOUT;
  }

  return PLATFORM_NOT_DETECTED;
}

static const std::map <std::string, PlatformDetectorFunc> detectors =
{
  {"is_ec2_instance", detectEc2Instance},
  {"has_aws_identity", detectAwsIdentity},
  {"is_azure_vm", detectAzureVM},
  {"has_azure_managed_identity", detectAzureManagedIdentity},
  {"is_gce_vm", detectGceVM},
  {"has_gcp_identity", detectGcpIdentity}
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
      detectedPlatformsCache.clear();
      // TODO: add checking disable env later
      for (const auto& pair : detectors)
      {
        if (pair.second(200) == PLATFORM_DETECTED)
        {
          detectedPlatformsCache.push_back(pair.first);
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
}

void redirectMetadataBaseUrl(const char* url)
{
  awsMetadataBaseURL = url;
  azureMetadataBaseURL = url;
  gceMetadataRootURL = url;
  gcpMetadataBaseURL = url;
}

void restoreMetadataBaseUrl()
{
  awsMetadataBaseURL = AWS_METADATA_BASE_URL;
  awsMetadataBaseURL = AZURE_METADATA_BASE_URL;
  gceMetadataRootURL = GCE_METADATA_ROOT_URL;
  gcpMetadataBaseURL = GCP_METADATA_BASE_URL;
}

} // extern "C"

