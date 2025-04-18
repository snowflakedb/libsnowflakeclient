#ifndef SNOWFLAKECLIENT_AWSUTILS_HPP
#define SNOWFLAKECLIENT_AWSUTILS_HPP

#include <string>
#include <memory>

namespace Snowflake {
  namespace Client {
    namespace AwsUtils {
      std::string getRegion();
      std::string getDomainSuffixForRegionalUrl(const std::string &regionName);

      class AwsSdkInitialized;
      /*
       * Shared initialization of aws sdk to avoid multiple initialization.
       * To initialize aws sdk, call initAwsSdk() and hold the returned shared_ptr as long as you use AWS SDK.
       */
      std::shared_ptr<AwsSdkInitialized> initAwsSdk();
    }
  }
}

#endif //SNOWFLAKECLIENT_AWSUTILS_HPP
