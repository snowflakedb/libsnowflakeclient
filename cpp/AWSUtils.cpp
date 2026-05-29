
#include "snowflake/AWSUtils.hpp"
#include <aws/core/Aws.h>
#include "logger/SFLogger.hpp"
#include "logger/SFAwsLogger.hpp"
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/utils/logging/AWSLogging.h>
#include <aws/core/utils/logging/LogLevel.h>
#include <aws/sts/STSClient.h>
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

      std::string getDomainSuffixForRegionalUrl(const std::string &regionName) {
        // use .cn if the region name starts with "cn-"
        return (regionName.find("cn-") == 0) ? "amazonaws.com.cn" : "amazonaws.com";
      }

      namespace {
        // Truncate a string for safe error logging. STS error responses are not
        // sensitive themselves, but bodies of unexpected non-error responses
        // might contain unexpected data; truncate as defense in depth.
        std::string truncateForLog(const std::string &s, size_t max = 256) {
          if (s.size() <= max) return s;
          return s.substr(0, max) + "...(truncated)";
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
            const std::string &roleArn) override {
          auto awsSdk = initAwsSdk();

          CXX_LOG_DEBUG("Assuming AWS role: %s", roleArn.c_str());

          const Aws::STS::STSClient stsClient(currentCreds);

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
            const std::string &signingAlgorithm) override {
          auto awsSdk = initAwsSdk();

          const std::string host = "sts." + region + "." + getDomainSuffixForRegionalUrl(region);
          const std::string url = "https://" + host + "/";

          // Query-protocol form body. URL-encode user-supplied parameters.
          const std::string body =
              "Action=GetWebIdentityToken&Version=2011-06-15"
              "&Audience.member.1=" + Aws::Utils::StringUtils::URLEncode(audience.c_str()) +
              "&SigningAlgorithm=" + Aws::Utils::StringUtils::URLEncode(signingAlgorithm.c_str());

          auto request = Aws::Http::CreateHttpRequest(
              Aws::String(url),
              Aws::Http::HttpMethod::HTTP_POST,
              Aws::Utils::Stream::DefaultResponseStreamFactoryMethod);
          request->SetHeaderValue("Host", host);
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
          auto httpClient = Aws::Http::CreateHttpClient(clientConfig);
          auto response = httpClient->MakeRequest(request);
          if (!response) {
            CXX_LOG_ERROR("STS GetWebIdentityToken: no HTTP response");
            return boost::none;
          }

          const auto status = response->GetResponseCode();
          // Drain the response body once into a string so we can both inspect
          // it on failure and parse it on success.
          std::ostringstream bodyBuf;
          bodyBuf << response->GetResponseBody().rdbuf();
          const std::string responseBody = bodyBuf.str();

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
