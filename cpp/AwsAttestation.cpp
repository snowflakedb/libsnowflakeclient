#include "snowflake/AWSUtils.hpp"
#include "snowflake/WifAttestation.hpp"
#include <picojson.h>
#include "util/Base64.hpp"
#include "logger/SFLogger.hpp"
#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>

namespace Snowflake::Client {
  namespace {
    // opt-in env var that switches AWS WIF from the legacy
    // base64(GetCallerIdentity-presigned-URL) format to a JWT obtained via
    // STS:GetWebIdentityToken. Will be removed once the new flow is GA.
    constexpr const char* AWS_OUTBOUND_TOKEN_ENV_VAR =
        "SNOWFLAKE_ENABLE_AWS_WIF_OUTBOUND_TOKEN";
    constexpr const char* SNOWFLAKE_WIF_AUDIENCE = "snowflakecomputing.com";
    constexpr const char* AWS_WIF_SIGNING_ALGORITHM = "ES384";

    bool isAwsOutboundTokenEnabled() {
      const char* raw = std::getenv(AWS_OUTBOUND_TOKEN_ENV_VAR);
      if (!raw) return false;
      std::string s(raw);
      std::transform(s.begin(), s.end(), s.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      return s == "true";
    }
  }

  // We don't need x-amz-content-sha256 header, because there is no payload to be signed.
  // If x-amz-content-sha256 contain EMPTY_STRING_SHA256, the server responds with
  // "The AWS STS request contained unacceptable headers."
  class AWS_CORE_API AWSAuthV4SignerNoPayload : public Aws::Client::AWSAuthV4Signer
  {
  public:
    AWSAuthV4SignerNoPayload(const std::shared_ptr<Aws::Auth::AWSCredentialsProvider>& credentialsProvider, const char* serviceName, const Aws::String& region)
        : AWSAuthV4Signer(credentialsProvider, serviceName, region) { m_includeSha256HashHeader = false; }
  };

  // Splits comma-separated role ARN impersonation path
  std::vector<std::string> parseRoleArnChain(const std::string &path) {
    std::vector<std::string> result;
    std::stringstream ss(path);
    std::string item;

    while (std::getline(ss, item, ',')) {
      const auto start = item.find_first_not_of(" \t");
      const auto end = item.find_last_not_of(" \t");

      if (start != std::string::npos) {
        result.push_back(item.substr(start, end - start + 1));
      }
    }

    return result;
  }

  // Assumes a chain of AWS roles sequentially via the injected SDK wrapper.
  // Routing through ISdkWrapper (instead of constructing an STSClient inline)
  // is what makes the impersonation path hermetically testable.
  boost::optional<Aws::Auth::AWSCredentials> assumeAwsRoleChain(
    AwsUtils::ISdkWrapper &sdkWrapper,
    const Aws::Auth::AWSCredentials &initialCreds,
    const std::vector<std::string> &roleArnChain) {

    if (roleArnChain.empty()) {
      CXX_LOG_ERROR("Role ARN chain is empty");
      return boost::none;
    }

    Aws::Auth::AWSCredentials currentCreds = initialCreds;

    for (const auto &roleArn: roleArnChain) {
      auto assumedCredsOpt = sdkWrapper.assumeRole(currentCreds, roleArn);
      if (!assumedCredsOpt) {
        CXX_LOG_ERROR("Failed to assume role in chain: %s", roleArn.c_str());
        return boost::none;
      }
      currentCreds = assumedCredsOpt.get();
    }

    return currentCreds;
  }

  boost::optional<Attestation> createAwsAttestation(const AttestationConfig &config) {
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
    const std::string &region = regionOpt.get();

    // Check if role assumption chain is configured
    if (config.workloadIdentityImpersonationPath &&
        !config.workloadIdentityImpersonationPath.get().empty()) {

      CXX_LOG_INFO("Using AWS role assumption chain for impersonation");

      auto roleArnChain = parseRoleArnChain(config.workloadIdentityImpersonationPath.get());

      if (roleArnChain.empty()) {
        CXX_LOG_ERROR("Failed to parse role ARN chain");
        return boost::none;
      }

      CXX_LOG_DEBUG("Role ARN chain size: %zu", roleArnChain.size());

      auto assumedCredsOpt = assumeAwsRoleChain(*config.awsSdkWrapper, creds, roleArnChain);
      if (!assumedCredsOpt) {
        CXX_LOG_ERROR("Failed to assume role chain");
        return boost::none;
      }

      creds = assumedCredsOpt.get();
    }

    if (isAwsOutboundTokenEnabled()) {
      CXX_LOG_INFO(
          "Using AWS WIF outbound JWT token (STS:GetWebIdentityToken) in region %s",
          region.c_str());
      auto jwtOpt = config.awsSdkWrapper->getWebIdentityToken(
          creds, region, SNOWFLAKE_WIF_AUDIENCE, AWS_WIF_SIGNING_ALGORITHM);
      if (!jwtOpt) {
        CXX_LOG_ERROR("Failed to obtain AWS outbound WIF JWT token");
        return boost::none;
      }
      const auto &jwt = jwtOpt.get();
      CXX_LOG_DEBUG("AWS outbound token prefix: %.10s", jwt.c_str());
      return Attestation::makeAws(jwt);
    }

    const std::string domain = AwsUtils::getDomainSuffixForRegionalUrl(region);
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
    return Attestation::makeAws(base64);
  }
}