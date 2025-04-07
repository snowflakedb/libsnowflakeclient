
#ifndef SNOWFLAKECLIENT_AWSATTESTATION_HPP
#define SNOWFLAKECLIENT_AWSATTESTATION_HPP


#include "snowflake/WifAttestation.hpp"

namespace Snowflake {
  namespace Client {
    boost::optional<Attestation> createAwsAttestation(const AttestationConfig& config);
  }
}

#endif //SNOWFLAKECLIENT_AWSATTESTATION_HPP
