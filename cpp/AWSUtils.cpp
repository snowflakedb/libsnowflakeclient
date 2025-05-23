
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
      static Aws::SDKOptions options;
      static bool awssdkInitialized = false;

      class AwsSdkInitialized {
      public:
        AwsSdkInitialized() {
          CXX_LOG_INFO("Initializing AWS SDK");
          Aws::InitAPI(options);
          Aws::Utils::Logging::InitializeAWSLogging(
              Aws::MakeShared<Snowflake::Client::SFAwsLogger>(""));
        }

        ~AwsSdkInitialized() {
          // Don't call ShutdownAPI() here to avoid memory issues caused by the
          // nondeterministic order of static variable destruction.
          // https://docs.aws.amazon.com/sdk-for-cpp/v1/developer-guide/basic-use.html
        }
      };

      void initAwsSdk() {
        static AwsSdkInitialized awssdk;
        awssdkInitialized = true;
      }

      void shutdownAwsSdk() {
        if (awssdkInitialized)
        {
          CXX_LOG_INFO("Shutting down AWS SDK");
          Aws::Utils::Logging::ShutdownAWSLogging();
          Aws::ShutdownAPI(options);
        }
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

          initAwsSdk();
          Aws::Internal::EC2MetadataClient metadataClient;
          std::string region = metadataClient.GetCurrentRegion();
          if (!region.empty()) {
            return region;
          }

          CXX_LOG_INFO("Failed to get EC2 region");
          return boost::none;
        }

        boost::optional<std::string> getArn() override {
          initAwsSdk();
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
          initAwsSdk();
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

extern "C" {
  void awssdk_shutdown()
  {
    Snowflake::Client::AwsUtils::shutdownAwsSdk();
  }
}
