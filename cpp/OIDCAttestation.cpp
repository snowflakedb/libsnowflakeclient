#include "OIDCAttestation.hpp"
#include "logger/SFLogger.hpp"
#include "jwt/Jwt.hpp"


namespace Snowflake {
  namespace Client {
    boost::optional<Attestation> createOIDCAttestation(const AttestationConfig &config) {
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

      return Attestation::makeOidc(config.token.get(), issuer, subject);
    }
  }
}

