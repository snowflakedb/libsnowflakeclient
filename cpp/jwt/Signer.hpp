/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_SIGNER_HPP
#define SNOWFLAKECLIENT_SIGNER_HPP

#define HS_256 "HS256"
#define HS_384 "HS384"
#define HS_512 "HS512"
#define RS_256 "RS256"
#define RS_384 "RS384"
#define RS_512 "RS512"
#define ES_256 "ES256"
#define ES_384 "ES384"
#define ES_512 "ES512"
#define unknown "UNKNOWN"

#include <memory>
#include <string>
#include <map>
#include "snowflake/IJwt.hpp"
#include "openssl/evp.h"

namespace Snowflake
{
namespace Client
{
namespace Jwt
{
#define ADD_RSA_ALGORITHM(hashType, hashFunc) \
  struct hashType \
  { \
    const EVP_MD *operator()() noexcept \
    { return hashFunc(); }\
  };

/**
 * AlgorithmTypeMapper maps between type and strings
 */
class AlgorithmTypeMapper
{
public:
  /**
   * Convert AlgorithmType to string
   * @param type
   * @return string representation of type
   */
  static std::string toString(AlgorithmType type);

  /**
   * Conver the string to AlgorithmType
   * @param type
   * @return AlgorithmType representation of type
   */
  inline static AlgorithmType toAlgorithmType(const std::string &type)
  {
    if (reverse_map_.count(type) == 0) return AlgorithmType::UNKNOWN;
    return reverse_map_[type];
  }

private:
  /**
   * Map between string and algorithm Type
   */
  static std::map<std::string, AlgorithmType> reverse_map_;
};

/**
 * Interface of a signer class
 */
class ISigner
{
public:

  virtual ~ISigner() = default;

  /**
   * Builder function of the signer class that would return an instance of specific signer
   * @param type AlgorithmType
   * @return Signer instance
   */
  static ISigner *buildSigner(AlgorithmType type);

  /**
   * Sign the msg with a key
   * @param key
   * @param msg
   * @return signed message or empty string when error happens
   */
  virtual std::string
  sign(EVP_PKEY *key, const std::string &msg) = 0;

  /**
   * Verify the token against message with the key specified
   * @param key
   * @param msg
   * @param sig
   * @return true if the verification pass
   */
  virtual bool
  verify(EVP_PKEY *key, const std::string &msg, const std::string &sig) = 0;


};

// Add the Hash function to RSA type
ADD_RSA_ALGORITHM(RS256, EVP_sha256);

ADD_RSA_ALGORITHM(RS384, EVP_sha384);

ADD_RSA_ALGORITHM(RS512, EVP_sha512);

/**
 * Signer for RSA
 * @tparam Hash function specified above
 */
template<typename Hash>
class RSASigner : public ISigner
{
  /**
   * See ISigner
   * @param key should be a RSA private key
   * @param msg
   * @return
   */
  std::string
  sign(EVP_PKEY *key, const std::string &msg) override;

  /**
   * See ISigner
   * @param key should be a RSA public key
   * @param msg
   * @param sig
   * @return
   */
  bool
  verify(EVP_PKEY *key, const std::string &msg, const std::string &sig) override;

private:
  /**
   * Deleter function wrapper
   * @param mdctx
   */
  static inline void EVP_MD_CTXDeleter(EVP_MD_CTX *mdctx)
  {
    if (mdctx) EVP_MD_CTX_destroy(mdctx);
  }
};
} // namespace Jwt
} // namespace Client
} // namespace Snowflake


#endif //SNOWFLAKECLIENT_SIGNER_HPP
