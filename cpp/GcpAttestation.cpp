
#include "GcpAttestation.hpp"
#include "jwt/Jwt.hpp"
#include "snowflake/HttpClient.hpp"
#include "logger/SFLogger.hpp"

namespace Snowflake {
  namespace Client {

    constexpr const char* SNOWFLAKE_AUDIENCE = "snowflakecomputing.com";

    boost::optional<Attestation> createGcpAttestation(AttestationConfig& config)
    {
      auto url = boost::url("http://169.254.169.254/computeMetadata/v1/instance/service-accounts/default/identity");
      url.params().append({"audience", SNOWFLAKE_AUDIENCE});

      HttpRequest req {
          HttpRequest::Method::GET,
          url,
          {
              {"Metadata-Flavor", "Google"},
          }
      };

      auto responseOpt = config.httpClient->run(req);
      if (!responseOpt) {
        CXX_LOG_ERROR("No response from GCP metadata server.");
        return boost::none;
      }

      auto response = responseOpt.get();
      if (response.code != 200) {
        CXX_LOG_ERROR("GCP metadata server request was not successful.");
        return boost::none;
      }

      std::string jwtStr = response.getBody();
      if (jwtStr.empty()) {
        CXX_LOG_ERROR("No JWT found in GCP response.");
        return boost::none;
      }

      Jwt::JWTObject jwt(jwtStr);
      auto claimSet = jwt.getClaimSet();
      std::string issuer = claimSet->getClaimInString("iss");
      std::string subject = claimSet->getClaimInString("sub");
      if (issuer.empty() || subject.empty()) {
        CXX_LOG_ERROR("No issuer or subject found in GCP JWT.");
        return boost::none;
      }

      if (issuer != "https://accounts.google.com") {
        CXX_LOG_ERROR("Unexpected issuer in GCP JWT: %s", issuer.c_str());
        return boost::none;
      }

      return Attestation{AttestationType::GCP, jwtStr, issuer, subject};
    }
  }
}

