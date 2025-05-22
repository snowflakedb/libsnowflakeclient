
#include "snowflake/AWSUtils.hpp"
#include <aws/core/Aws.h>
#include "logger/SFLogger.hpp"
#include "logger/SFAwsLogger.hpp"
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/utils/logging/AWSLogging.h>
#include <aws/core/utils/logging/LogLevel.h>
#include <aws/sts/STSClient.h>
#include <aws/sts/model/GetCallerIdentityRequest.h>
#include <aws/core/auth/AWSCredentialsProvider.h>

namespace Snowflake {
  namespace Client {
    namespace AwsUtils {
      class AwsSdkInitialized {
      public:
        AwsSdkInitialized() : options{}, refCount(0) {
          CXX_LOG_INFO("Initializing AWS SDK");
          Aws::InitAPI(options);
          Aws::Utils::Logging::InitializeAWSLogging(
              Aws::MakeShared<Snowflake::Client::SFAwsLogger>(""));
        }

        ~AwsSdkInitialized() {
          CXX_LOG_INFO("Shutting down AWS SDK");
          Aws::Utils::Logging::ShutdownAWSLogging();
          ShutdownAPI(options);
        }

        Aws::SDKOptions options;
        int refCount;
      };

      Snowflake::Client::AwsMutex s_sdkMutex;
      std::shared_ptr<AwsSdkInitialized> s_awssdk;

      AwsSdkInstance::AwsSdkInstance() {
        s_sdkMutex.lock();
        if (!s_awssdk) {
          s_awssdk = std::make_unique<AwsSdkInitialized>();
        }
        s_awssdk->refCount++;
        s_sdkMutex.unlock();
        CXX_LOG_TRACE("AWS SDK instance created");
      }

      AwsSdkInstance::~AwsSdkInstance() {
        CXX_LOG_TRACE("AWS SDK instance removed");
        s_sdkMutex.lock();
        if (s_awssdk) {
          s_awssdk->refCount--;
          if (s_awssdk->refCount <= 0) {
            s_awssdk = NULL;
          }
        }
        s_sdkMutex.unlock();
      }

      std::string getDomainSuffixForRegionalUrl(const std::string &regionName) {
        // use .cn if the region name starts with "cn-"
        return (regionName.find("cn-") == 0) ? "amazonaws.com.cn" : "amazonaws.com";
      }

      class SdkWrapper : public ISdkWrapper {
      public:

        boost::optional<std::string> getEC2Region() override {
          auto awsRegion = std::getenv("AWS_REGION");
          if (awsRegion) {
            return std::string(awsRegion);
          }

          AwsUtils::AwsSdkInstance awsSdkInstance;
          Aws::Internal::EC2MetadataClient metadataClient;
          std::string region = metadataClient.GetCurrentRegion();
          if (!region.empty()) {
            return region;
          }

          CXX_LOG_INFO("Failed to get EC2 region");
          return boost::none;
        }

        boost::optional<std::string> getArn() override {
          AwsUtils::AwsSdkInstance awsSdkInstance;
          Aws::STS::STSClient stsClient;
          Aws::STS::Model::GetCallerIdentityRequest request;

          auto outcome = stsClient.GetCallerIdentity(request);

          // Check if the call was successful
          if (!outcome.IsSuccess()) {
            CXX_LOG_INFO("Failed to get caller identity: %s", outcome.GetError().GetMessage().c_str());
            return boost::none;
          }

          const auto &result = outcome.GetResult();
          return result.GetArn();
        }

        Aws::Auth::AWSCredentials getCredentials() override {
          AwsUtils::AwsSdkInstance awsSdkInstance;
          auto credentialsProvider = Aws::MakeShared<Aws::Auth::DefaultAWSCredentialsProviderChain>({});
          auto creds = credentialsProvider->GetAWSCredentials();
          return creds;
        }
      };

      ISdkWrapper* ISdkWrapper::getInstance() {
        static auto instance = std::make_unique<SdkWrapper>();
        return instance.get();
      }
    }
  }
}
