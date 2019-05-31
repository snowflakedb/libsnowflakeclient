/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_JWT_HPP
#define SNOWFLAKECLIENT_JWT_HPP

#include <memory>
#include "Signer.hpp"
#include "ClaimSet.hpp"
#include "Header.hpp"
#include "snowflake/IJwt.hpp"

namespace Snowflake
{
namespace Client
{
namespace Jwt
{

class JWTObject : public IJwt
{
  typedef std::shared_ptr<IClaimSet> ClaimSetPtr;
  typedef std::shared_ptr<IHeader> HeaderPtr;

public:

  JWTObject();

  /**
   * Marshalize a plain jwt token text to JWTObject data
   * @param input
   */
  explicit JWTObject(const std::string &input);

  /**
   * Set the claim set
   * @param claim_set
   */
  inline void setClaimSet(ClaimSetPtr claim_set)
  {
    this->claim_set_ = claim_set;
  }

  /**
   * Get the claim set
   * @return
   */
  inline ClaimSetPtr getClaimSet()
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
  inline HeaderPtr getHeader()
  {
    return header_;
  }

  /**
   * Verify that the secret meets with the overall message
   * Note: It is the user's responsibility to make sure the header, claim set
   *       and the secret's value are properly set already unless the object is
   *       parsed from some serialized text
   * @param key
   * @param format - whether to format msg while verifying signature
   * @return true if the verification pass
   */
  bool verify(EVP_PKEY *key, bool format);

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
} // namespace Client
} // namespace Snowflake


#endif //SNOWFLAKECLIENT_JWT_HPP
