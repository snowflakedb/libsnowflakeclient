
#include <snowflake/WifAttestation.hpp>
#include "GcpAttestation.hpp"
#include "AzureAttestation.hpp"
#include "AwsAttestation.hpp"
#include "logger/SFLogger.hpp"
#include "jwt/Jwt.hpp"

namespace Snowflake {
  namespace Client {
    namespace {

      boost::optional<Attestation> createOIDCAttestation(const AttestationConfig& config) {
        if (!config.token) {
          CXX_LOG_WARN("No token provided for OIDC attestation.");
          return {};
        }

        Jwt::JWTObject jwt(config.token.get());
        auto claimSet = jwt.getClaimSet();
        std::string issuer = claimSet->getClaimInString("iss");
        std::string subject = claimSet->getClaimInString("sub");
        if (issuer.empty() || subject.empty()) {
          CXX_LOG_ERROR("No issuer or subject found in OIDC JWT.");
          return boost::none;
        }

        return Attestation{AttestationType::OIDC, config.token.get(), issuer, subject};
      }

      using AttestationProvider = std::function<boost::optional<Attestation>(AttestationConfig&)>;
      const std::map<AttestationType, AttestationProvider> attestationProviders = {
          {AttestationType::AWS, createAwsAttestation},
          {AttestationType::AZURE, createAzureAttestation},
          {AttestationType::GCP, createGcpAttestation},
          {AttestationType::OIDC, createOIDCAttestation},
      };

      inline boost::optional<Attestation> createAutodetectAttestation(AttestationConfig& config) {
        CXX_LOG_INFO("Detecting attestation type");
        for (const auto& typeProvider : attestationProviders) {
          auto type = typeProvider.first;
          auto provider = typeProvider.second;
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

    const std::unique_ptr<IHttpClient> defaultHttpClient = std::unique_ptr<IHttpClient>(IHttpClient::createSimple(defaultHttpClientConfig));
  }
}