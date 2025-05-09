
#include <snowflake/WifAttestation.hpp>
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

      inline boost::optional<Attestation> createAutodetectAttestation(AttestationConfig& config) {
        AttestationType order[] = {AttestationType::OIDC, AttestationType::AWS, AttestationType::AZURE, AttestationType::GCP};
        CXX_LOG_INFO("Detecting attestation type");
        for (const auto& type: order) {
          auto provider = attestationProviders.at(type);
          CXX_LOG_INFO("Trying attestation type %s", stringFromAttestationType(type));
          auto attestation = provider(config);
          if (attestation) {
            CXX_LOG_INFO("Detected attestation type %s and performed attestation successfully", stringFromAttestationType(type));
            return attestation.get();
          }
        }

        CXX_LOG_ERROR("Failed to detect attestation type");
        return boost::none;
      }
    }

    boost::optional<Attestation> createAttestation(AttestationConfig& config) {
      if (!config.type) {
        auto result = createAutodetectAttestation(config);
        if (!result) {
          CXX_LOG_ERROR("Failed to create attestation for %s", stringFromAttestationType(config.type.get()));
        }
        return result;
      }

      auto type = config.type.get();
      return attestationProviders.at(type)(config);
    }
  }
}