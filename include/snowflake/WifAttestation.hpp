
#ifndef SNOWFLAKECLIENT_CSPATESTATIONS_HPP
#define SNOWFLAKECLIENT_CSPATESTATIONS_HPP

#include <string>
#include <boost/optional.hpp>
#include "snowflake/client.h"

namespace Snowflake {

namespace Client {

  enum class AttestationType {
    AWS,
    AZURE,
    GCP,
    OIDC
  };

  inline const char* stringFromAttestationType(AttestationType type)
  {
    switch (type) {
      case AttestationType::AWS:
        return "AWS";
      case AttestationType::AZURE:
        return "AZURE";
      case AttestationType::GCP:
        return "GCP";
      case AttestationType::OIDC:
        return "OIDC";
    }

    return "UNKNOWN";
  }

  inline boost::optional<AttestationType> attestationTypeFromString(const std::string& type)
  {
    if (type == "AWS")
      return AttestationType::AWS;
    if (type == "AZURE")
      return AttestationType::AZURE;
    if (type == "GCP")
      return AttestationType::GCP;
    if (type == "OIDC")
      return AttestationType::OIDC;

    return boost::none;
  }

  struct Attestation {
    AttestationType type;
    std::string credential;
    boost::optional<std::string> issuer;
    boost::optional<std::string> subject;

    static Attestation makeOidc(const std::string& token, const std::string& issuer, const std::string& subject) {
      return Attestation{AttestationType::OIDC, token, issuer, subject};
    }

    static Attestation makeAws(const std::string& credential) {
      return Attestation{AttestationType::AWS, credential, boost::none, boost::none};
    }

    static Attestation makeAzure(const std::string& credential, const std::string& issuer, const std::string& subject) {
      return Attestation{AttestationType::AZURE, credential, issuer, subject};
    }

    static Attestation makeGcp(const std::string& credential, const std::string& issuer, const std::string& subject) {
      return Attestation{AttestationType::GCP, credential, issuer, subject};
    }
  };

  class IHttpClient;
  namespace AwsUtils {
    class ISdkWrapper;
  }

  struct AttestationConfig {
    boost::optional<AttestationType> type;
    boost::optional<std::string> token;
    boost::optional<std::string> snowflakeEntraResource;
    boost::optional<std::string> workloadIdentityImpersonationPath;
    boost::optional<std::string> audience;
    IHttpClient* httpClient = NULL;
    AwsUtils::ISdkWrapper* awsSdkWrapper = NULL;

    // Raw SF_CON_WIF_HOST value, exactly as supplied by the caller (either a
    // bare hostname or a full base URL). Provider code must not use this
    // directly -- use getWifHostForAws() / getWifHostForGcp() instead, which
    // normalize it to the format each provider's URL-building code expects.
    boost::optional<std::string> wifHost;

    SF_STATUS configureWIFAttestation(SF_CONNECT* conn);

    std::string getAudience() const {
      return audience.value_or(SF_SNOWFLAKE_WIF_AUDIENCE);
    }

    std::string getWifHost() const {
      return wifHost.value_or("");
    }

    // Normalized bare hostname (no scheme/path) for AWS STS requests.
    std::string getWifHostForAws() const;

    // Normalized full base URL (scheme + host + path prefix) for GCP IAM
    // credentials requests.
    std::string getWifHostForGcp() const;
  };

  boost::optional<Attestation> createAttestation(AttestationConfig& config);
}

}

#endif //SNOWFLAKECLIENT_CSPATESTATIONS_HPP
