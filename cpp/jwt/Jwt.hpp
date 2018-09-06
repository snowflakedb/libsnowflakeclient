/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_JWT_HPP
#define SNOWFLAKECLIENT_JWT_HPP

#include <unordered_map>
#include <memory>
#include "Signer.hpp"
#include "ClaimSet.hpp"

namespace Snowflake
{
namespace Jwt
{

#define JWT_TYPE "TOKEN"


class JWTObject
{
  typedef std::shared_ptr<ClaimSet> ClaimSetPtr;
  typedef std::shared_ptr<Signer> SignerPtr;

public:

  static JWTObject *Parse(char *str);

  inline void setClaimSet(ClaimSetPtr claim_set)
  {
    this->claim_set_ = std::move(claim_set);
  }

  inline ClaimSetPtr getClaimSet()
  {
    return claim_set_;
  }

  inline void setAlgorithm(AlgorithmType type)
  {
    signer_ = SignerPtr(Signer::BuildSigner(type));
  };

  inline bool verify(SignerPtr signer);

private:
  ClaimSetPtr claim_set_;
  SignerPtr signer_;
  std::string serialized_data;
};

} // namespace Jwt
} // namespace Snowflake


#endif //SNOWFLAKECLIENT_JWT_HPP
