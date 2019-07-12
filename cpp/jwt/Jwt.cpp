/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include "Jwt.hpp"

namespace Snowflake
{
namespace Client
{
namespace Jwt
{

IJwt *IJwt::buildIJwt()
{
  return new JWTObject();
}

IJwt *IJwt::buildIJwt(const std::string &text)
{
  return new JWTObject(text);
}

JWTObject::JWTObject()
{
  this->header_ = HeaderPtr(IHeader::buildHeader());
  this->claim_set_ = ClaimSetPtr(IClaimSet::buildClaimSet());
}

JWTObject::JWTObject(const std::string &input)
{
  if (input.length() == 0) throw JwtException("Empty input string");

  std::string header;
  std::string claim_set;

  size_t pos;
  std::string remain;

  pos = input.find('.');
  if (pos == std::string::npos) throw JwtException("Fail to extract header");
  header = input.substr(0, pos);
  this->header_ = HeaderPtr(IHeader::parseHeader(header));

  remain = input.substr(pos + 1);
  pos = remain.find('.');
  if (pos == std::string::npos) throw JwtException("Fail to extract token");
  claim_set = remain.substr(0, pos);
  this->claim_set_ = ClaimSetPtr(IClaimSet::parseClaimset(claim_set));

  secret_ = remain.substr(pos + 1);
}

bool JWTObject::verify(EVP_PKEY *key, bool format)
{
  std::unique_ptr<ISigner> signer{ISigner::buildSigner(header_->getAlgorithmType())};
  std::string msg;

  if (signer == nullptr) return false;

  if (format)
  {
    msg = header_->serialize() + '.' + claim_set_->serialize();
  }
  else
  {
    msg = header_->serialize(false) + '.' + claim_set_->serialize(false);
  }
  return signer->verify(key, msg, secret_);
}

std::string JWTObject::serialize(EVP_PKEY *key)
{
  std::unique_ptr<ISigner> signer{ISigner::buildSigner(header_->getAlgorithmType())};
  std::string msg = header_->serialize() + '.' + claim_set_->serialize();
  this->secret_ = signer->sign(key, msg);
  return msg + '.' + this->secret_;
}

} // namespace Jwt
} // namespace Client
} // namespace Snowflake
