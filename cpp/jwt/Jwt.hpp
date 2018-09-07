/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_JWT_HPP
#define SNOWFLAKECLIENT_JWT_HPP

#include <unordered_map>
#include <memory>
#include "Signer.hpp"
#include "ClaimSet.hpp"
#include "Header.hpp"

namespace Snowflake
{
namespace Jwt
{

class JWTObject
{
  typedef std::shared_ptr<IClaimSet> ClaimSetPtr;
  typedef std::shared_ptr<IHeader> HeaderPtr;

public:
  /**
   * Marshalize a plain jwt token text to JWTObject data
   * @param input
   */
  explicit JWTObject(std::string &input);

  /**
   * Set the claim set
   * @param claim_set
   */
  inline void setClaimSet(ClaimSetPtr claim_set)
  {
    this->claim_set_ = std::move(claim_set);
  }

  /**
   * Get the claim set
   * @return
   */
  inline const ClaimSetPtr getClaimSet()
  {
    return claim_set_;
  }

  /**
   * Set the header
   * @param header
   */
  inline void setHeader(HeaderPtr header)
  {
    this->header_ = std::move(header);
  }

  /**
   * Get the header
   * @return
   */
  inline const HeaderPtr getHeader()
  {
    return header_;
  }

  /**
   * Verify that the secret meets with the overall message
   * Note: It is the user's responsibility to make sure the header, claim set
   *       and the secret's value are properly set already unless the object is
   *       parsed from some serialized text
   * @param key
   * @return true if the verification pass
   */
  inline bool verify(EVP_PKEY *key);

  /**
   * Serialize the JWT token to a string, the secret field would be set
   * @param key
   * @return
   */
  std::string serialize(EVP_PKEY *key);

private:
  HeaderPtr header_;
  ClaimSetPtr claim_set_;
  std::string secret_;
};

} // namespace Jwt
} // namespace Snowflake


#endif //SNOWFLAKECLIENT_JWT_HPP
