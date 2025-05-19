
#ifndef SNOWFLAKECLIENT_AZUREATTESTATION_HPP
#define SNOWFLAKECLIENT_AZUREATTESTATION_HPP

#include <snowflake/WifAttestation.hpp>

namespace Snowflake {
  namespace Client {

    struct AzureFunctionsManagedIdentityConfig {
      static boost::optional<AzureFunctionsManagedIdentityConfig> fromEnv();

      std::string header;
      boost::urls::url endpoint;
      boost::optional<std::string> clientId;
    };

    struct AzureAttestationConfig
    {
      static boost::optional<AzureAttestationConfig> fromConfig(const AttestationConfig& config);

      boost::urls::url getRequestURL() const;
      std::map<std::string, std::string> getRequestHeaders() const;

      std::string snowflakeEntraResource;
      boost::optional<AzureFunctionsManagedIdentityConfig> managedIdentity;
    };

    boost::optional<Attestation> createAzureAttestation(AttestationConfig& config);
  }
}

#endif //SNOWFLAKECLIENT_AZUREATTESTATION_HPP
