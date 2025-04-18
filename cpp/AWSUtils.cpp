
#include "AWSUtils.hpp"
#include <aws/core/Aws.h>
#include "logger/SFLogger.hpp"
#include "logger/SFAwsLogger.hpp"
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/utils/logging/AWSLogging.h>
#include <aws/core/utils/logging/DefaultLogSystem.h>
#include <aws/core/utils/logging/ConsoleLogSystem.h>

namespace Snowflake {
  namespace Client {
    namespace AwsUtils {
      class AwsSdkInitialized
      {
      public:
        AwsSdkInitialized() : options{}
        {
          CXX_LOG_INFO("Initializing AWS SDK");
          Aws::InitAPI(options);
          Aws::Utils::Logging::InitializeAWSLogging(
              Aws::MakeShared<Snowflake::Client::SFAwsLogger>(""));
        }

        ~AwsSdkInitialized()
        {
          CXX_LOG_INFO("Shutting down AWS SDK");
          Aws::Utils::Logging::ShutdownAWSLogging();
          ShutdownAPI(options);
        }

        Aws::SDKOptions options;
      };

      std::shared_ptr<AwsSdkInitialized> initAwsSdk() {
        static std::shared_ptr<AwsSdkInitialized> awssdk = std::make_shared<AwsSdkInitialized>();
        return awssdk;
      }

      std::string getDomainSuffixForRegionalUrl(const std::string &regionName) {
        // use .cn if the region name starts with "cn-"
        return (regionName.find("cn-") == 0) ? "amazonaws.com.cn" : "amazonaws.com";
      }

      std::string getRegion() {
        auto awsRegion = std::getenv("AWS_REGION");
        if (awsRegion) {
          return awsRegion;
        }

        auto profile_name = Aws::Auth::GetConfigProfileName();
        if (Aws::Config::HasCachedConfigProfile(profile_name))
        {
          auto profile = Aws::Config::GetCachedConfigProfile(profile_name);
          auto region = profile.GetRegion();
          if (!region.empty())
          {
            return region;
          }
        }

        if (Aws::Config::HasCachedCredentialsProfile(profile_name))
        {
          auto profile = Aws::Config::GetCachedCredentialsProfile(profile_name);
          auto region = profile.GetRegion();
          if (!region.empty())
          {
            return region;
          }
        }

        return Aws::Region::US_EAST_1;
      }
    }
  }
}
