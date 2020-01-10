/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <vector>
#include <functional>
#include "Signer.hpp"
#include "Util.hpp"
#include "../util/Base64.hpp"

namespace Snowflake
{
namespace Client
{
namespace Jwt
{
std::map<std::string, AlgorithmType>
  AlgorithmTypeMapper::reverse_map_ = {
  {HS_256,  AlgorithmType::HS256},
  {HS_384,  AlgorithmType::HS384},
  {HS_512,  AlgorithmType::HS512},
  {RS_256,  AlgorithmType::RS256},
  {RS_384,  AlgorithmType::RS384},
  {RS_512,  AlgorithmType::RS512},
  {ES_256,  AlgorithmType::ES256},
  {ES_384,  AlgorithmType::ES384},
  {ES_512,  AlgorithmType::ES512},
  {unknown, AlgorithmType::UNKNOWN},
};

std::string AlgorithmTypeMapper::toString(AlgorithmType type)
{
  switch (type)
  {
    case AlgorithmType::HS256:
      return HS_256;
    case AlgorithmType::HS384:
      return HS_384;
    case AlgorithmType::HS512:
      return HS_512;
    case AlgorithmType::RS256:
      return RS_256;
    case AlgorithmType::RS384:
      return RS_384;
    case AlgorithmType::RS512:
      return RS_512;
    case AlgorithmType::ES256:
      return ES_256;
    case AlgorithmType::ES384:
      return ES_384;
    case AlgorithmType::ES512:
      return ES_512;
    case AlgorithmType::UNKNOWN:
    default:
      return unknown;
  }
}

ISigner *ISigner::buildSigner(Snowflake::Client::Jwt::AlgorithmType type)
{
  switch (type)
  {
    case AlgorithmType::RS256:
      return new RSASigner<RS256>();
    case AlgorithmType::RS384:
      return new RSASigner<RS384>();
    case AlgorithmType::RS512:
      return new RSASigner<RS512>();
    default:
      // TODO Implement the other signer class when needed
      throw JwtException("Algorithm type not implemented");
  }
}

template<typename Hash>
std::string RSASigner<Hash>::sign(EVP_PKEY *key, const std::string &msg)
{
  /* Create the Message Digest Context */
  std::unique_ptr<EVP_MD_CTX, std::function<void(EVP_MD_CTX *)>> mdctx(
    EVP_MD_CTX_create(), EVP_MD_CTXDeleter);

  if (mdctx == nullptr) return "";

  /* Initialise the DigestSign operation */
  if (1 != EVP_DigestSignInit(mdctx.get(), nullptr, Hash{}(), nullptr, key)) return "";

  /* Call update with the message */
  if (1 != EVP_DigestSignUpdate(mdctx.get(), msg.c_str(), msg.length())) return "";

  /* Finalise the DigestSign operation */
  /* First call EVP_DigestSignFinal with a NULL sig parameter to obtain the length of the
   * signature. Length is returned in slen */
  size_t slen;
  if (1 != EVP_DigestSignFinal(mdctx.get(), nullptr, &slen)) return "";
  std::vector<char> buf(slen);
  /* Obtain the signature */
  if (1 != EVP_DigestSignFinal(mdctx.get(), (unsigned char *) buf.data(), &slen)) return "";

  /* Success */
  buf.resize(slen);
  return Util::Base64::encodeURLNoPadding(buf);
}

template<typename Hash>
bool
RSASigner<Hash>::verify(EVP_PKEY *key, const std::string &msg, const std::string &sig)
{
  /* Create the Message Digest Context */
  std::unique_ptr<EVP_MD_CTX, std::function<void(EVP_MD_CTX *)>> mdctx(
    EVP_MD_CTX_create(), EVP_MD_CTXDeleter);

  /* Initialize `key` with a public key */
  if (1 != EVP_DigestVerifyInit(mdctx.get(), nullptr, Hash{}(), nullptr, key))
  {
    return false;
  }

  /* Initialize `key` with a public key */
  if (1 != EVP_DigestVerifyUpdate(mdctx.get(), msg.c_str(), msg.length()))
  {
    return false;
  }

  auto sig_decode = Util::Base64::decodeURLNoPadding(sig);

  return (1 == EVP_DigestVerifyFinal(mdctx.get(), (unsigned char *) sig_decode.data(), sig_decode.size()));
}

} // namespace Jwt
} // namespace Client
} // namespace Snowflake
