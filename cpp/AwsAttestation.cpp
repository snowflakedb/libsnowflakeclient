#include "snowflake/AWSUtils.hpp"
#include "snowflake/WifAttestation.hpp"
#include <picojson.h>
#include "util/Base64.hpp"
#include "logger/SFLogger.hpp"
#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/sts/STSClient.h>
#include <aws/sts/model/AssumeRoleRequest.h>
#include <aws/core/utils/UUID.h>
#include <sstream>

namespace Snowflake::Client {
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

  // Assumes a single AWS role and returns temporary credentials
  boost::optional<Aws::Auth::AWSCredentials> assumeAwsRole(
    const Aws::Auth::AWSCredentials &currentCreds,
    const std::string &roleArn) {

    CXX_LOG_DEBUG("Assuming AWS role: %s", roleArn.c_str());

    const Aws::STS::STSClient stsClient(currentCreds);

    Aws::STS::Model::AssumeRoleRequest assumeRoleRequest;
    assumeRoleRequest.SetRoleArn(roleArn.c_str());

    const std::string sessionName = "snowflake-wif-" + std::string(Aws::Utils::UUID::PseudoRandomUUID());
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
      credentials.GetSessionToken()
    );
  }

  // Assumes a chain of AWS roles sequentially
  boost::optional<Aws::Auth::AWSCredentials> assumeAwsRoleChain(
    const Aws::Auth::AWSCredentials &initialCreds,
    const std::vector<std::string> &roleArnChain) {

    if (roleArnChain.empty()) {
      CXX_LOG_ERROR("Role ARN chain is empty");
      return boost::none;
    }

    Aws::Auth::AWSCredentials currentCreds = initialCreds;

    for (const auto &roleArn: roleArnChain) {
      auto assumedCredsOpt = assumeAwsRole(currentCreds, roleArn);
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

      auto assumedCredsOpt = assumeAwsRoleChain(creds, roleArnChain);
      if (!assumedCredsOpt) {
        CXX_LOG_ERROR("Failed to assume role chain");
        return boost::none;
      }

      creds = assumedCredsOpt.get();
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