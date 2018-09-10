/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_HEADER_HPP
#define SNOWFLAKECLIENT_HEADER_HPP

#include <string>
#include <cJSON.h>
#include <functional>
#include "Signer.hpp"
#include "Util.hpp"

#define ALGORITHM "alg"
#define TOKEN_TYPE "typ"
#define DEFAULT_TOKEN_TYPE "JWT"

namespace Snowflake
{
namespace Jwt
{

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

/**
 * CJSONHeader is an implementation that uses cJSON to store data
 */
class CJSONHeader : public IHeader
{
public:
  CJSONHeader();

  explicit CJSONHeader(const std::string &text)
  {
    this->json_root_ = {CJSONOperation::parse(Base64URLOpt::decodeNoPadding(text)),
                        CJSONOperation::cJSONDeleter};
  }

  /**
   * See IHeader
   * @throw JwtMemoryAllocationFailure
   */
  inline void setAlgorithm(AlgorithmType type) override
  {
    std::string type_str = AlgorithmTypeMapper::toString(type);
    cJSON *item = snowflake_cJSON_CreateString(type_str.c_str());
    CJSONOperation::addOrReplaceJSON(this->json_root_.get(), ALGORITHM, item);
  }

  /**
   * See IHeader
   */
  AlgorithmType getAlgorithmType() override;

  /**
   * See IHeader
   */
  inline std::string serialize() override
  {
    return Base64URLOpt::encodeNoPadding(CJSONOperation::serialize(this->json_root_.get()));
  }

private:
  std::unique_ptr<cJSON, std::function<void(cJSON *)>> json_root_;
};


} // namespace Jwt
} // namespace Snowflake

#endif //SNOWFLAKECLIENT_HEADER_HPP
