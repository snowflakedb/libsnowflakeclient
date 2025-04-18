
#include <snowflake/WifAttestation.hpp>
#include "util/Base64.hpp"
#include "logger/SFLogger.hpp"
#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/auth/AWSAuthSigner.h>
#include <aws/core/http/HttpClient.h>
#include <aws/sts/STSClient.h>
#include "AWSUtils.hpp"

namespace Snowflake {
  namespace Client {
    boost::optional<Attestation> createAwsAttestation(const AttestationConfig&) {
      auto awsSdkInit = AwsUtils::initAwsSdk();
      auto credentialsProvider = Aws::MakeShared<Aws::Auth::DefaultAWSCredentialsProviderChain>({});
      auto creds = credentialsProvider->GetAWSCredentials();
      if (creds.IsEmpty()) {
        CXX_LOG_INFO("Failed to get AWS credentials");
        return boost::none;
      }

      std::string region = AwsUtils::getRegion();
      std::string domain = AwsUtils::getDomainSuffixForRegionalUrl(region);
      std::string host = std::string("sts") + "." + region + "." + domain;
      std::string url = std::string("https://") + host + "/?Action=GetCallerIdentity&Version=2011-06-15";

      auto request = Aws::Http::CreateHttpRequest(
          Aws::String(url),
          Aws::Http::HttpMethod::HTTP_POST,
          Aws::Utils::Stream::DefaultResponseStreamFactoryMethod
      );

      request->SetHeaderValue("Host", host);
      request->SetHeaderValue("X-Snowflake-Audience", "snowflakecomputing.com");

      request->AddContentBody(Aws::MakeShared<Aws::StringStream>(""));

      Aws::Client::ClientConfiguration clientConfig;
      Aws::Client::AWSAuthV4Signer signer(credentialsProvider, "sts", Aws::String(region));

      // Sign the request
      if (!signer.SignRequest(*request)) {
        CXX_LOG_ERROR("Failed to sign request");
        return boost::none;
      }

      picojson::object obj;
      obj["url"] = picojson::value(request->GetURIString());
      obj["method"] = picojson::value("POST");
      picojson::object headers;
      for (const auto &h: request->GetHeaders()) {
        headers[h.first] = picojson::value(h.second);
      }
      obj["headers"] = picojson::value(headers);
      std::string json = picojson::value(obj).serialize(true);
      std::string base64;
      Util::Base64::encodePadding(json.begin(), json.end(), std::back_inserter(base64));
      return Attestation{AttestationType::AWS, base64};
    }
  }
}