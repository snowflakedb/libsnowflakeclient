
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
        AwsSdkInitialized() : options{} {
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
      };

      std::shared_ptr<AwsSdkInitialized> initAwsSdk(bool shutdown) {
        static Snowflake::Client::AwsMutex s_sdkMutex;
        static std::shared_ptr<AwsSdkInitialized> awssdk = std::make_shared<AwsSdkInitialized>();
        // To fix hanging issue when calling ShutdownAPI(), calling it earlier
        // from application when calling snowflake_global_term()
        s_sdkMutex.lock();
        if (shutdown)
        {
          awssdk.reset();
        }
        else if (!awssdk)
        {
          awssdk = std::make_shared<AwsSdkInitialized>();
        }
        s_sdkMutex.unlock();
        return awssdk;
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

          auto awsSdk = initAwsSdk();
          Aws::Internal::EC2MetadataClient metadataClient;
          std::string region = metadataClient.GetCurrentRegion();
          if (!region.empty()) {
            return region;
          }

          CXX_LOG_INFO("Failed to get EC2 region");
          return boost::none;
        }

        Aws::Auth::AWSCredentials getCredentials() override {
          auto awsSdk = initAwsSdk();
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
    Snowflake::Client::AwsUtils::initAwsSdk(true);
  }
}
