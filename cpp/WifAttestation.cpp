#include "snowflake/AWSUtils.hpp"
#include "snowflake/WifAttestation.hpp"
#include "GcpAttestation.hpp"
#include "AzureAttestation.hpp"
#include "AwsAttestation.hpp"
#include "OIDCAttestation.hpp"
#include "logger/SFLogger.hpp"
#include "jwt/Jwt.hpp"

namespace Snowflake {
  namespace Client {
    namespace {

      using AttestationProvider = std::function<boost::optional<Attestation>(AttestationConfig&)>;
      const std::map<AttestationType, AttestationProvider> attestationProviders = {
          {AttestationType::AWS, createAwsAttestation},
          {AttestationType::AZURE, createAzureAttestation},
          {AttestationType::GCP, createGcpAttestation},
          {AttestationType::OIDC, createOIDCAttestation},
      };
    }

    boost::optional<Attestation> createAttestation(AttestationConfig& config) {
      if (config.httpClient == NULL)
        config.httpClient = IHttpClient::getInstance();
      if (config.awsSdkWrapper == NULL)
        config.awsSdkWrapper = AwsUtils::ISdkWrapper::getInstance();

      if (!config.type) {
        CXX_LOG_ERROR("Attestation type must be specified");
        return boost::none;
      }

      auto type = config.type.get();
      return attestationProviders.at(type)(config);
    }
  }
}