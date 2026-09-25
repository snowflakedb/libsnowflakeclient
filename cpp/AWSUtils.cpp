
#include "snowflake/AWSUtils.hpp"
#include <aws/core/Aws.h>
#include "logger/SFLogger.hpp"
#include "logger/SFAwsLogger.hpp"
#include "snowflake/SFURL.hpp"
#include "util/SnowflakeCommon.hpp"
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/utils/logging/AWSLogging.h>
#include <aws/core/utils/logging/LogLevel.h>
#include <aws/sts/STSClient.h>
#include <aws/sts/STSEndpointProvider.h>
#include <aws/sts/model/AssumeRoleRequest.h>
#include <aws/sts/model/GetCallerIdentityRequest.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/auth/signer/AWSAuthV4Signer.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/http/HttpClient.h>
#include <aws/core/http/HttpClientFactory.h>
#include <aws/core/http/HttpRequest.h>
#include <aws/core/http/HttpResponse.h>
#include <aws/core/utils/StringUtils.h>
#include <aws/core/utils/UUID.h>
#include <aws/core/utils/memory/AWSMemory.h>
#include <aws/core/utils/memory/stl/AWSStringStream.h>
#include <aws/core/utils/stream/ResponseStream.h>
#include <aws/core/utils/xml/XmlSerializer.h>

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

      namespace {
      boost::optional<AwsStsEndpoint> defaultStsEndpoint(const std::string& region) {
          using Origin = Aws::Endpoint::EndpointParameter::ParameterOrigin;

          Aws::STS::Endpoint::STSEndpointProvider provider;
          Aws::Endpoint::EndpointParameters params;
          params.emplace_back("Region", Aws::String(region.c_str()), Origin::BUILT_IN);
          params.emplace_back("UseFIPS", false, Origin::BUILT_IN);
          params.emplace_back("UseDualStack", false, Origin::BUILT_IN);
          params.emplace_back("UseGlobalEndpoint", false, Origin::BUILT_IN);

          auto outcome = provider.ResolveEndpoint(params);
          if (!outcome.IsSuccess()) {
              CXX_LOG_ERROR("could not resolve an STS endpoint for region \"%s\": %s",
                  region.c_str(), outcome.GetError().GetMessage().c_str());
              return boost::none;
          }

          const Aws::Http::URI& uri = outcome.GetResult().GetURI();
          const std::string scheme = Aws::Http::SchemeMapper::ToString(uri.GetScheme());

          std::string authority = uri.GetAuthority().c_str();
          const uint16_t defaultPort = (uri.GetScheme() == Aws::Http::Scheme::HTTPS) ? 443 : 80;
          if (!authority.empty() && uri.GetPort() != defaultPort) {
              authority += ":" + std::to_string(uri.GetPort());
          }

          if (authority.empty()) {
              CXX_LOG_ERROR("resolved STS endpoint for region \"%s\" has no host: \"%s\"",
                  region.c_str(), outcome.GetResult().GetURL().c_str());
              return boost::none;
          }

          std::string baseUrl = scheme + "://" + authority + uri.GetURLEncodedPath().c_str();
          Util::trimTrailingSlashes(baseUrl);

          return AwsStsEndpoint{ authority, baseUrl };
      }

      boost::optional<AwsStsEndpoint> parseWorkloadIdentityHost(const std::string& rawHost) {
          std::string host = Util::trimWhitespace(rawHost);
          if (host.empty()) {
              CXX_LOG_ERROR("workloadIdentityHost is empty");
              return boost::none;
          }

          if (host.find("://") == std::string::npos) {
              host = "https://" + host;
          }

          SFURL url;
          try {
              url = SFURL::parse(host);
          } catch (const SFURLParseError&) {
              CXX_LOG_ERROR("workloadIdentityHost \"%s\" is malformed", host.c_str());
              return boost::none;
          }

          if (url.scheme() != "https" && url.scheme() != "http") {
              CXX_LOG_ERROR("workloadIdentityHost \"%s\" must use https or http, got scheme \"%s\"",
                  host.c_str(), url.scheme().c_str());
              return boost::none;
          }
          if (url.host().empty()) {
              CXX_LOG_ERROR("workloadIdentityHost \"%s\" does not contain a hostname", host.c_str());
              return boost::none;
          }
          if (!url.userInfo().empty() || url.getParamsSize() != 0 || !url.fragment().empty()) {
              CXX_LOG_ERROR("workloadIdentityHost \"%s\" must not contain user info, a query or a fragment",
                  host.c_str());
              return boost::none;
          }

          std::string authority = url.host();
          if (!url.port().empty()) {
              authority += ":" + url.port();
          }

          std::string baseUrl = url.scheme() + "://" + authority + url.path();
          Util::trimTrailingSlashes(baseUrl);
          return AwsStsEndpoint{ authority, baseUrl };
      }
      }

      boost::optional<AwsStsEndpoint> resolveStsEndpoint(const std::string& region,
          const std::string& configuredHost) {
          if (!configuredHost.empty()) {
              return parseWorkloadIdentityHost(configuredHost);
          }
          return defaultStsEndpoint(region);
      }

      std::string getDomainSuffixForRegionalUrl(const std::string &regionName) {
        // use .cn if the region name starts with "cn-"
        return (regionName.find("cn-") == 0) ? "amazonaws.com.cn" : "amazonaws.com";
      }

      namespace {
        // Bound the GetWebIdentityToken response body we materialize into
        // memory. Real STS responses for this API are ~1-5 KiB (a JWT plus
        // XML wrapping). The cap is defense in depth so a misbehaving
        // intermediary returning a runaway body can't OOM the process.
        constexpr size_t MAX_STS_RESPONSE_BODY_BYTES = 64 * 1024;

        // Connect / request timeouts for the STS GetWebIdentityToken HTTP
        // call. Mirrors the values used by SnowflakeS3Client for AWS HTTP
        // clients so users get a uniform, bounded failure mode when STS is
        // unreachable or the network blackholes.
        constexpr long STS_CONNECT_TIMEOUT_MS = 30000;
        constexpr long STS_REQUEST_TIMEOUT_MS = 40000;

        // Truncate a string for safe error logging. STS error responses are not
        // sensitive themselves, but bodies of unexpected non-error responses
        // might contain unexpected data; truncate as defense in depth.
        std::string truncateForLog(const std::string &s, size_t max = 256) {
          if (s.size() <= max) return s;
          return s.substr(0, max) + "...(truncated)";
        }

        // Read up to maxBytes from a response body stream into a string.
        // Pairs with MAX_STS_RESPONSE_BODY_BYTES to bound peak memory.
        std::string readBoundedResponseBody(std::istream &stream, size_t maxBytes) {
          std::string buf;
          buf.resize(maxBytes);
          stream.read(&buf[0], static_cast<std::streamsize>(maxBytes));
          buf.resize(static_cast<size_t>(stream.gcount()));
          return buf;
        }
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
            const std::string &configuredHost) override {
          auto awsSdk = initAwsSdk();

          CXX_LOG_DEBUG("Assuming AWS role: %s", roleArn.c_str());

          const auto endpointOpt = resolveStsEndpoint(region, configuredHost);
          if (!endpointOpt) {
            CXX_LOG_ERROR("Failed to resolve STS endpoint for region %s", region.c_str());
            return boost::none;
          }

          Aws::STS::STSClientConfiguration clientConfig;
          clientConfig.region = region;
          if (!configuredHost.empty()) {
            clientConfig.endpointOverride = endpointOpt->baseUrl;
          }
          const Aws::STS::STSClient stsClient(currentCreds, nullptr, clientConfig);

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
            const std::string &configuredHost
        ) override {
          auto awsSdk = initAwsSdk();

          const auto endpointOpt = resolveStsEndpoint(region, configuredHost);
          if (!endpointOpt) {
            CXX_LOG_ERROR("Failed to resolve STS endpoint for region %s", region.c_str());
            return boost::none;
          }
          const AwsStsEndpoint &endpoint = endpointOpt.get();
          const std::string url = endpoint.baseUrl;

          // Query-protocol form body. URL-encode user-supplied parameters.
          const std::string body =
              "Action=GetWebIdentityToken&Version=2011-06-15"
              "&Audience.member.1=" + Aws::Utils::StringUtils::URLEncode(audience.c_str()) +
              "&SigningAlgorithm=" + Aws::Utils::StringUtils::URLEncode(signingAlgorithm.c_str());

          auto request = Aws::Http::CreateHttpRequest(
              Aws::String(url),
              Aws::Http::HttpMethod::HTTP_POST,
              Aws::Utils::Stream::DefaultResponseStreamFactoryMethod);
          request->SetHeaderValue("Host", endpoint.authority);
          request->SetContentType("application/x-www-form-urlencoded");

          auto bodyStream = Aws::MakeShared<Aws::StringStream>("getWebIdentityToken");
          *bodyStream << body;
          request->AddContentBody(bodyStream);
          request->SetContentLength(std::to_string(body.size()).c_str());

          auto credProvider = Aws::MakeShared<Aws::Auth::SimpleAWSCredentialsProvider>(
              "getWebIdentityToken", creds);
          // Default RequestDependent policy with a raw HttpRequest signs the
          // body (signBody=true in the SignRequest(request) overload), giving
          // us full SigV4 with payload SHA256.
          Aws::Client::AWSAuthV4Signer signer(credProvider, "sts", region);
          if (!signer.SignRequest(*request)) {
            CXX_LOG_ERROR("Failed to sign STS GetWebIdentityToken request");
            return boost::none;
          }

          Aws::Client::ClientConfiguration clientConfig;
          clientConfig.region = region;
          clientConfig.connectTimeoutMs = STS_CONNECT_TIMEOUT_MS;
          clientConfig.requestTimeoutMs = STS_REQUEST_TIMEOUT_MS;
          auto httpClient = Aws::Http::CreateHttpClient(clientConfig);
          auto response = httpClient->MakeRequest(request);
          if (!response) {
            CXX_LOG_ERROR("STS GetWebIdentityToken: no HTTP response");
            return boost::none;
          }

          const auto status = response->GetResponseCode();
          // Read the body once, bounded, so we can both log it on failure
          // and parse it on success without risk of unbounded materialization.
          const std::string responseBody = readBoundedResponseBody(
              response->GetResponseBody(), MAX_STS_RESPONSE_BODY_BYTES);

          if (status != Aws::Http::HttpResponseCode::OK) {
            CXX_LOG_ERROR(
                "STS GetWebIdentityToken failed: HTTP %d, body: %s",
                static_cast<int>(status),
                truncateForLog(responseBody).c_str());
            return boost::none;
          }

          auto doc = Aws::Utils::Xml::XmlDocument::CreateFromXmlString(
              Aws::String(responseBody.begin(), responseBody.end()));
          if (!doc.WasParseSuccessful()) {
            CXX_LOG_ERROR(
                "STS GetWebIdentityToken: XML parse failed: %s",
                doc.GetErrorMessage().c_str());
            return boost::none;
          }

          // Expected shape:
          //   <GetWebIdentityTokenResponse>
          //     <GetWebIdentityTokenResult>
          //       <WebIdentityToken>...</WebIdentityToken>
          //       <Expiration>...</Expiration>
          //     </GetWebIdentityTokenResult>
          //     <ResponseMetadata>...</ResponseMetadata>
          //   </GetWebIdentityTokenResponse>
          auto root = doc.GetRootElement();
          if (root.IsNull()) {
            CXX_LOG_ERROR("STS GetWebIdentityToken: empty XML root");
            return boost::none;
          }
          auto result = root.FirstChild("GetWebIdentityTokenResult");
          if (result.IsNull()) {
            CXX_LOG_ERROR("STS GetWebIdentityToken: missing GetWebIdentityTokenResult element");
            return boost::none;
          }
          auto tokenNode = result.FirstChild("WebIdentityToken");
          if (tokenNode.IsNull()) {
            CXX_LOG_ERROR("STS GetWebIdentityToken: missing WebIdentityToken element");
            return boost::none;
          }
          const Aws::String token = tokenNode.GetText();
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
