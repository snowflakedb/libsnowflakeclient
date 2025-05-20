#include "snowflake/AWSUtils.hpp"
#include "snowflake/WifAttestation.hpp"
#include <picojson.h>
#include "util/Base64.hpp"
#include "logger/SFLogger.hpp"
#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/auth/AWSAuthSigner.h>
#include "snowflake/AWSUtils.hpp"

namespace Snowflake {
  namespace Client {

    // We don't need x-amz-content-sha256 header, because there is no payload to be signed.
    // If x-amz-content-sha256 contain EMPTY_STRING_SHA256, the server responds with
    // "The AWS STS request contained unacceptable headers."
    class AWS_CORE_API AWSAuthV4SignerNoPayload : public Aws::Client::AWSAuthV4Signer
    {
    public:
      AWSAuthV4SignerNoPayload(const std::shared_ptr<Aws::Auth::AWSCredentialsProvider>& credentialsProvider, const char* serviceName, const Aws::String& region)
          : AWSAuthV4Signer(credentialsProvider, serviceName, region) { m_includeSha256HashHeader = false; }
    };

    boost::optional<Attestation> createAwsAttestation(const AttestationConfig& config) {
      auto awsSdkInit = AwsUtils::initAwsSdk();
      auto creds = config.awsSdkWrapper->getCredentials();
      if (creds.IsEmpty()) {
        CXX_LOG_INFO("Failed to get AWS credentials");
        return boost::none;
      }

      auto regionOpt = config.awsSdkWrapper->getEC2Region();
      if (!regionOpt) {
        CXX_LOG_INFO("Failed to get AWS region");
        return boost::none;
      }
      const std::string& region = regionOpt.get();

      auto arnOpt = config.awsSdkWrapper->getArn();
      if (!arnOpt) {
        CXX_LOG_INFO("Failed to get AWS ARN");
        return boost::none;
      }
      const std::string& arn = arnOpt.get();

      const std::string domain = "amazonaws.com";
      const std::string host = std::string("sts") + "." + region + "." + domain;
      const std::string url = std::string("https://") + host + "/?Action=GetCallerIdentity&Version=2011-06-15";

      auto request = Aws::Http::CreateHttpRequest(
          Aws::String(url),
          Aws::Http::HttpMethod::HTTP_POST,
          Aws::Utils::Stream::DefaultResponseStreamFactoryMethod
      );

      request->SetHeaderValue("Host", host);
      request->SetHeaderValue("X-Snowflake-Audience", "snowflakecomputing.com");

      auto simpleCredProvider = std::make_shared<Aws::Auth::SimpleAWSCredentialsProvider>(creds);
      AWSAuthV4SignerNoPayload signer(simpleCredProvider, "sts", region);

      // Sign the request
      if (!signer.SignRequest(*request)) {
        CXX_LOG_ERROR("Failed to sign request");
        return boost::none;
      }

      picojson::object obj;
      obj["url"] = picojson::value(request->GetURIString());
      obj["method"] = picojson::value(Aws::Http::HttpMethodMapper::GetNameForHttpMethod(request->GetMethod()));
      picojson::object headers;
      for (const auto &h: request->GetHeaders()) {
        headers[h.first] = picojson::value(h.second);
      }
      obj["headers"] = picojson::value(headers);
      std::string json = picojson::value(obj).serialize(true);
      std::string base64;
      Util::Base64::encodePadding(json.begin(), json.end(), std::back_inserter(base64));
      return Attestation::makeAws(base64, arn);
    }
  }
}