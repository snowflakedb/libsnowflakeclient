
#ifndef SNOWFLAKECLIENT_GCPATTESTATION_HPP
#define SNOWFLAKECLIENT_GCPATTESTATION_HPP

#include "snowflake/HttpClient.hpp"
#include "snowflake/WifAttestation.hpp"

namespace Snowflake {
  namespace Client {
    boost::optional<Attestation> createGcpAttestation(AttestationConfig& config);
  }
}

#endif //SNOWFLAKECLIENT_GCPATTESTATION_HPP
