#ifndef SNOWFLAKECLIENT_AWSUTILS_HPP
#define SNOWFLAKECLIENT_AWSUTILS_HPP

#include <string>
#include <memory>
#include <boost/optional.hpp>
#include <aws/core/auth/AWSCredentials.h>

namespace Snowflake {
  namespace Client {
    namespace AwsUtils {
      /*
       * Shared initialization of aws sdk to avoid multiple initialization.
       * To initialize aws sdk, call initAwsSdk() and hold the returned shared_ptr as long as you use AWS SDK.
       * shutdown = true to release resources hold for aws sdk.
       */
      class AwsSdkInitialized;
      std::shared_ptr<AwsSdkInitialized> initAwsSdk(bool shutdown = false);

      std::string getDomainSuffixForRegionalUrl(const std::string &regionName);

      // STS endpoint used by every AWS WIF call. authority is the SigV4 Host
      // value (host[:port]). baseUrl is the scheme plus that authority, with
      // trailing slashes removed. overridden is true when wif_host was set.
      struct AwsStsEndpoint {
        std::string authority;
        std::string baseUrl;
        bool overridden = false;
      };

      // Resolves the STS endpoint for region. A non-empty configuredHost
      // (bare host or http(s) URL) wins. Otherwise the AWS SDK partition
      // rules supply the plain regional endpoint, with FIPS, dual-stack, and
      // the legacy global endpoint pinned off. On failure, error is set and
      // boost::none is returned.
      boost::optional<AwsStsEndpoint> resolveAwsStsEndpoint(
          const std::string& configuredHost,
          const std::string& region,
          std::string& error);

      class ISdkWrapper {
      public:
        virtual boost::optional<std::string> getEC2Region() = 0;
        virtual Aws::Auth::AWSCredentials getCredentials() = 0;
        // Calls STS:AssumeRole with the given credentials and target role
        // ARN. Returns temporary credentials on success, or boost::none on
        // failure. Used by the WIF role-assumption (impersonation) chain.
        // endpoint selects the STS host; when it is not an override, ambient
        // AWS_USE_FIPS_ENDPOINT and AWS_USE_DUALSTACK_ENDPOINT still apply.
        virtual boost::optional<Aws::Auth::AWSCredentials> assumeRole(
            const Aws::Auth::AWSCredentials& currentCreds,
            const std::string& roleArn,
            const std::string& region,
            const AwsStsEndpoint& endpoint) = 0;
        // Calls AWS STS GetWebIdentityToken for outbound identity federation
        // (SNOW-2919437). Returns the signed JWT on success, or boost::none on
        // any failure. endpoint is interpreted the same way as in assumeRole.
        virtual boost::optional<std::string> getWebIdentityToken(
            const Aws::Auth::AWSCredentials& creds,
            const std::string& region,
            const std::string& audience,
            const std::string& signingAlgorithm,
            const AwsStsEndpoint& endpoint) = 0;
        virtual ~ISdkWrapper() = default;
        static ISdkWrapper* getInstance();
      };
    }
  }
}

#endif //SNOWFLAKECLIENT_AWSUTILS_HPP
