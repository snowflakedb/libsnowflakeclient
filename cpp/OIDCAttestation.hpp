
#ifndef SNOWFLAKECLIENT_OIDCATTESTATION_HPP
#define SNOWFLAKECLIENT_OIDCATTESTATION_HPP

#include <boost/optional.hpp>
#include "snowflake/WifAttestation.hpp"

namespace Snowflake {
  namespace Client {
    boost::optional<Attestation> createOIDCAttestation(const AttestationConfig &config);
  }
}


#endif //SNOWFLAKECLIENT_OIDCATTESTATION_HPP
