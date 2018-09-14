/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_IJWT_HPP
#define SNOWFLAKECLIENT_IJWT_HPP

#include <string>
#include <openssl/ossl_typ.h>
#include <memory>

namespace Snowflake
{
namespace Client
{
namespace Jwt
{

/**
 * Type of algorithms
 */
enum class AlgorithmType
{
  HS256, HS384, HS512,
  RS256, RS384, RS512,
  ES256, ES384, ES512,
  UNKNOWN,
};

struct JwtException : public std::exception
{
  JwtException(const std::string &message) : message_(message) {}
  const char *what() noexcept
  {
    return message_.c_str();
  }

  std::string message_;
};


class IClaimSet
{

public:
  virtual ~IClaimSet() = default;

  static IClaimSet *buildClaimSet();

  static IClaimSet *parseClaimset(const std::string &text);

  /**
   * Check if the claim set contains a specific key
   */
  virtual bool containsClaim(const std::string &key) = 0;

  /**
   * Add the key and a string value to the claim set
   * Would replace the old one if the key exists
   * @param key
   * @param value
   */
  virtual void addClaim(const std::string &key, const std::string &value) = 0;

  /**
   * Add the key and a long value to the claim set
   * Would replace the old one if the key exists
   * @param key
   * @param value
   */
  virtual void addClaim(const std::string &key, long number) = 0;

  /**
   * Get a claim from the claim set in string type
   */
  virtual std::string getClaimInString(const std::string &key) = 0;

  /**
   * Get a claim from the claim set in long type
   */
  virtual long getClaimInLong(const std::string &key) = 0;

  /**
   * Serialize the claim set to base64url encoded format
   */
  virtual std::string serialize() = 0;

  /**
   * Remove a claim from the claim set with specified key
   */
  virtual void removeClaim(const std::string &key) = 0;

};

class IHeader
{
public:
  virtual ~IHeader() = default;

  static IHeader *buildHeader();

  static IHeader *parseHeader(const std::string &text);

  /**
   * Set the algorithm's type
   * @param type
   */
  virtual void setAlgorithm(AlgorithmType type) = 0;

  /**
   * Get the algorithm type of the header
   * @return the algorithm type
   */
  virtual AlgorithmType getAlgorithmType() = 0;

  /**
   * Serialize the header
   * @return serialized string in base64urlencoded
   */
  virtual std::string serialize() = 0;
};


class IJwt
{
  typedef std::shared_ptr<IClaimSet> ClaimSetPtr;
  typedef std::shared_ptr<IHeader> HeaderPtr;

public:
  static IJwt *buildIJwt();

  static IJwt *buildIJwt(const std::string &text);

  IJwt() = default;

  virtual ~IJwt() {}

  virtual std::string serialize(EVP_PKEY *key) = 0;

  virtual bool verify(EVP_PKEY *key) = 0;

  virtual void setClaimSet(ClaimSetPtr claim_set) = 0;

  virtual ClaimSetPtr getClaimSet() = 0;

  virtual void setHeader(HeaderPtr header) = 0;

  virtual HeaderPtr getHeader() = 0;
};

} // namespace Jwt
} // namespace Client
} // namespace Snowflake

#endif //SNOWFLAKECLIENT_IJWT_HPP
