
#ifndef SNOWFLAKECLIENT_AZUREATTESTATION_HPP
#define SNOWFLAKECLIENT_AZUREATTESTATION_HPP

#include <snowflake/WifAttestation.hpp>

namespace Snowflake {
  namespace Client {

    struct AzureManagedIdentityConfig {
      static boost::optional<AzureManagedIdentityConfig> fromEnv();

      std::string header;
      boost::url endpoint;
      boost::optional<std::string> clientId;
    };

    struct AzureAttestationConfig
    {
      static boost::optional<AzureAttestationConfig> fromConfig(const AttestationConfig& config);

      boost::url getRequestURL() const;
      std::map<std::string, std::string> getRequestHeaders() const;

      std::string snowflakeEntraResource;
      boost::optional<AzureManagedIdentityConfig> managedIdentity;
    };

    boost::optional<Attestation> createAzureAttestation(AttestationConfig& config);
  }
}

#endif //SNOWFLAKECLIENT_AZUREATTESTATION_HPP
