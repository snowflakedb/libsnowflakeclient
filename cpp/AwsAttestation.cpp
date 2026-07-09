#include "snowflake/AWSUtils.hpp"
#include "snowflake/WifAttestation.hpp"
#include "logger/SFLogger.hpp"
#include <sstream>
#include <string>
#include <vector>

namespace Snowflake::Client {
  namespace {
    // AWS WIF (SNOW-2919437): the JWT obtained from STS:GetWebIdentityToken is
    // bound to this audience and signed with this algorithm. GS verifies both
    // when validating the inbound JWT.
    constexpr const char* AWS_WIF_SIGNING_ALGORITHM = "ES384";
  }

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

    CXX_LOG_INFO(
        "Requesting AWS WIF JWT (STS:GetWebIdentityToken) in region %s",
        region.c_str());
    auto jwtOpt = config.awsSdkWrapper->getWebIdentityToken(
        creds, region, config.getAudience(), AWS_WIF_SIGNING_ALGORITHM, config.getWifHost());
    if (!jwtOpt) {
      CXX_LOG_ERROR("Failed to obtain AWS WIF JWT token");
      return boost::none;
    }
    const auto &jwt = jwtOpt.get();
    CXX_LOG_DEBUG("AWS WIF JWT token prefix: %.10s", jwt.c_str());
    return Attestation::makeAws(jwt);
  }
}
