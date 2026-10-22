
#include "snowflake/AWSUtils.hpp"
#include <aws/core/Aws.h>
#include "logger/SFLogger.hpp"
#include "logger/SFAwsLogger.hpp"
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/utils/logging/AWSLogging.h>
#include <aws/core/utils/logging/LogLevel.h>
#include <aws/sts/STSClient.h>
#include <aws/sts/STSEndpointProvider.h>
#include <aws/sts/model/AssumeRoleRequest.h>
#include <aws/sts/model/GetWebIdentityTokenRequest.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/http/Scheme.h>
#include <aws/core/http/URI.h>
#include <aws/core/utils/UUID.h>
#include <aws/core/utils/memory/AWSMemory.h>
#include <boost/url.hpp>
#include <cctype>
#include <cstdlib>

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

      namespace {
        // Connect / request timeouts for STS calls. Mirrors the values used
        // by SnowflakeS3Client so users get a uniform, bounded failure mode
        // when STS is unreachable or the network blackholes.
        constexpr long STS_CONNECT_TIMEOUT_MS = 30000;
        constexpr long STS_REQUEST_TIMEOUT_MS = 40000;

        std::string trimCopy(const std::string &value) {
          const auto begin = value.find_first_not_of(" \t\r\n\f\v");
          if (begin == std::string::npos) {
            return "";
          }
          const auto end = value.find_last_not_of(" \t\r\n\f\v");
          return value.substr(begin, end - begin + 1);
        }

        void stripTrailingSlashes(std::string &value) {
          while (!value.empty() && value.back() == '/') {
            value.pop_back();
          }
        }

        std::string quote(const std::string &value) {
          return "\"" + value + "\"";
        }

        // Accepts a bare host or an http(s) URL. Mirrors gosnowflake's
        // parseWorkloadIdentityHost, including rejection of user info, query,
        // and fragment.
        boost::optional<AwsStsEndpoint> parseConfiguredStsEndpoint(
            const std::string &raw,
            std::string &error) {
          std::string host = trimCopy(raw);
          if (host.empty()) {
            error = "wif_host is empty";
            return boost::none;
          }
          if (host.find("://") == std::string::npos) {
            host = "https://" + host;
          }

          const auto parsed = boost::urls::parse_uri(host);
          if (!parsed) {
            error = "wif_host " + quote(host) + " is malformed";
            return boost::none;
          }
          const std::string scheme(parsed->scheme());
          if (scheme != "https" && scheme != "http") {
            error = "wif_host " + quote(host) +
                    " must use https or http, got scheme " + quote(scheme);
            return boost::none;
          }
          if (parsed->host().empty()) {
            error = "wif_host " + quote(host) + " does not contain a hostname";
            return boost::none;
          }
          if (parsed->has_userinfo() || !parsed->query().empty() ||
              !parsed->fragment().empty()) {
            error = "wif_host " + quote(host) +
                    " must not contain user info, a query or a fragment";
            return boost::none;
          }

          std::string authority(parsed->host());
          if (parsed->has_port()) {
            authority += ":";
            authority += std::string(parsed->port());
          }
          std::string baseUrl = scheme + "://" + authority + std::string(parsed->path());
          stripTrailingSlashes(baseUrl);

          AwsStsEndpoint endpoint;
          endpoint.authority = std::move(authority);
          endpoint.baseUrl = std::move(baseUrl);
          endpoint.overridden = true;
          return endpoint;
        }

        std::string authorityWithPort(const Aws::Http::URI &uri) {
          std::string authority(uri.GetAuthority().c_str());
          const auto scheme = uri.GetScheme();
          const uint16_t port = uri.GetPort();
          const bool defaultPort =
              port == 0 ||
              (scheme == Aws::Http::Scheme::HTTPS &&
               port == Aws::Http::HTTPS_DEFAULT_PORT) ||
              (scheme == Aws::Http::Scheme::HTTP &&
               port == Aws::Http::HTTP_DEFAULT_PORT);
          if (!authority.empty() && !defaultPort) {
            authority += ":" + std::to_string(port);
          }
          return authority;
        }

        bool envFlagEnabled(const char *name) {
          const char *value = std::getenv(name);
          if (value == nullptr || value[0] == '\0') {
            return false;
          }
          std::string normalized(value);
          for (char &ch : normalized) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
          }
          return normalized == "true" || normalized == "1";
        }

        // ClientConfiguration in aws-sdk-cpp 1.11 leaves useFIPS and
        // useDualStack false, so the ambient endpoint env vars have to be
        // applied here. An override clears them: they are endpoint-selection
        // inputs, and leaving them on makes the SDK reject the custom host.
        Aws::Client::ClientConfiguration stsClientConfiguration(
            const std::string &region,
            const AwsStsEndpoint &endpoint) {
          Aws::Client::ClientConfiguration clientConfig("", true);
          clientConfig.region = region.c_str();
          clientConfig.connectTimeoutMs = STS_CONNECT_TIMEOUT_MS;
          clientConfig.requestTimeoutMs = STS_REQUEST_TIMEOUT_MS;
          if (endpoint.overridden) {
            clientConfig.endpointOverride = endpoint.baseUrl.c_str();
            clientConfig.useFIPS = false;
            clientConfig.useDualStack = false;
            if (endpoint.baseUrl.compare(0, 7, "http://") == 0) {
              clientConfig.scheme = Aws::Http::Scheme::HTTP;
            }
            if (std::getenv("AWS_USE_FIPS_ENDPOINT") != nullptr ||
                std::getenv("AWS_USE_DUALSTACK_ENDPOINT") != nullptr) {
              CXX_LOG_WARN(
                  "wif_host is set; clearing FIPS/dualstack preferences to allow custom STS endpoint resolution");
            }
          } else {
            clientConfig.useFIPS = envFlagEnabled("AWS_USE_FIPS_ENDPOINT");
            clientConfig.useDualStack = envFlagEnabled("AWS_USE_DUALSTACK_ENDPOINT");
          }
          return clientConfig;
        }
      }

      boost::optional<AwsStsEndpoint> resolveAwsStsEndpoint(
          const std::string &configuredHost,
          const std::string &region,
          std::string &error) {
        if (!configuredHost.empty()) {
          return parseConfiguredStsEndpoint(configuredHost, error);
        }

        // Partition DNS suffixes are not derivable from the region name.
        // ISO and the European Sovereign Cloud do not use amazonaws.com.
        // This is a local evaluation of the SDK endpoint rules: no network
        // and no credentials. FIPS, dual-stack, and the legacy global
        // endpoint stay off so the signed GetCallerIdentity URL is the plain
        // regional host. SDK-driven calls resolve their own endpoints.
        auto awsSdk = initAwsSdk();
        (void)awsSdk;

        Aws::STS::Endpoint::STSEndpointProvider provider;
        provider.AccessBuiltInParameters().SetStringParameter("Region", region.c_str());
        provider.AccessBuiltInParameters().SetBooleanParameter("UseFIPS", false);
        provider.AccessBuiltInParameters().SetBooleanParameter("UseDualStack", false);
        provider.AccessClientContextParameters().SetBooleanParameter(
            "UseGlobalEndpoint", false);

        const auto outcome = provider.ResolveEndpoint({});
        if (!outcome.IsSuccess()) {
          error = "could not resolve an STS endpoint for region " + quote(region) +
                  ": " + outcome.GetError().GetMessage().c_str();
          return boost::none;
        }

        const auto &uri = outcome.GetResult().GetURI();
        std::string authority = authorityWithPort(uri);
        if (authority.empty()) {
          error = "resolved STS endpoint for region " + quote(region) + " has no host";
          return boost::none;
        }

        std::string baseUrl =
            std::string(Aws::Http::SchemeMapper::ToString(uri.GetScheme())) +
            "://" + authority;
        const std::string path(uri.GetPath().c_str());
        if (!path.empty() && path != "/") {
          baseUrl += path;
        }
        stripTrailingSlashes(baseUrl);

        AwsStsEndpoint endpoint;
        endpoint.authority = std::move(authority);
        endpoint.baseUrl = std::move(baseUrl);
        endpoint.overridden = false;
        return endpoint;
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

        // Calls STS:AssumeRole for the WIF role-assumption chain. Lifted from
        // a free function in AwsAttestation.cpp so tests can stub the STS
        // call through the same DI seam they use for getWebIdentityToken.
        boost::optional<Aws::Auth::AWSCredentials> assumeRole(
            const Aws::Auth::AWSCredentials &currentCreds,
            const std::string &roleArn,
            const std::string &region,
            const AwsStsEndpoint &endpoint) override {
          auto awsSdk = initAwsSdk();

          CXX_LOG_DEBUG("Assuming AWS role: %s", roleArn.c_str());

          const Aws::Client::ClientConfiguration clientConfig =
              stsClientConfiguration(region, endpoint);
          const Aws::STS::STSClient stsClient(currentCreds, clientConfig);

          Aws::STS::Model::AssumeRoleRequest assumeRoleRequest;
          assumeRoleRequest.SetRoleArn(roleArn.c_str());

          const std::string sessionName =
              "snowflake-wif-" + std::string(Aws::Utils::UUID::PseudoRandomUUID());
          assumeRoleRequest.SetRoleSessionName(sessionName.c_str());
          assumeRoleRequest.SetDurationSeconds(3600);

          const auto outcome = stsClient.AssumeRole(assumeRoleRequest);

          if (!outcome.IsSuccess()) {
            CXX_LOG_ERROR("Failed to assume role %s: %s",
                          roleArn.c_str(),
                          outcome.GetError().GetMessage().c_str());
            return boost::none;
          }

          const auto &credentials = outcome.GetResult().GetCredentials();
          return Aws::Auth::AWSCredentials(
              credentials.GetAccessKeyId(),
              credentials.GetSecretAccessKey(),
              credentials.GetSessionToken());
        }

        boost::optional<std::string> getWebIdentityToken(
            const Aws::Auth::AWSCredentials &creds,
            const std::string &region,
            const std::string &audience,
            const std::string &signingAlgorithm,
            const AwsStsEndpoint &endpoint
        ) override {
          auto awsSdk = initAwsSdk();

          const Aws::Client::ClientConfiguration clientConfig =
              stsClientConfiguration(region, endpoint);
          const Aws::STS::STSClient stsClient(creds, clientConfig);

          Aws::STS::Model::GetWebIdentityTokenRequest request;
          request.AddAudience(audience.c_str());
          request.SetSigningAlgorithm(signingAlgorithm.c_str());

          const auto outcome = stsClient.GetWebIdentityToken(request);
          if (!outcome.IsSuccess()) {
            CXX_LOG_ERROR(
                "STS GetWebIdentityToken failed: %s",
                outcome.GetError().GetMessage().c_str());
            return boost::none;
          }

          const Aws::String token = outcome.GetResult().GetWebIdentityToken();
          if (token.empty()) {
            CXX_LOG_ERROR("STS GetWebIdentityToken: empty WebIdentityToken element");
            return boost::none;
          }
          return std::string(token.c_str(), token.size());
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
