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

    SF_STATUS AttestationConfig::configureWIFAttestation(SF_CONNECT* conn)
    {
        // Populate config from SF_CONNECT fields
        if (conn->wif_provider) 
        {
            auto typeOpt = attestationTypeFromString(conn->wif_provider);
            if (typeOpt) 
            {
                type = typeOpt;
                log_debug("Using explicit WIF provider: %s", conn->wif_provider);
            }
            else 
            {
                log_error("Invalid WIF provider specified: %s. Valid values: AWS, AZURE, GCP, OIDC", conn->wif_provider);
                return SF_STATUS_ERROR_GENERAL;
            }
        }
        else 
        {
            log_error("WIF provider is required but not specified");
            return SF_STATUS_ERROR_GENERAL;
        }

        if (conn->wif_token) 
        {
            token = std::string(conn->wif_token);
            log_debug("Using explicit WIF token");
        }

        if (conn->wif_azure_resource) 
        {
            snowflakeEntraResource = std::string(conn->wif_azure_resource);
            log_debug("Using Azure resource: %s", conn->wif_azure_resource);
        }

        // Pass workload identity impersonation path
        if (conn->workload_identity_impersonation_path)
        {
            workloadIdentityImpersonationPath = conn->workload_identity_impersonation_path;
        }

        if (conn->wif_audience)
        {
            audience = std::string(conn->wif_audience);
            log_debug("Using explicit WIF audience: %s", conn->wif_audience);
        }

        return SF_STATUS_SUCCESS;
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