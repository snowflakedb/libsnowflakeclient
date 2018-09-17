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
#include "../util/Base64.hpp"

#define ALGORITHM "alg"
#define TOKEN_TYPE "typ"
#define DEFAULT_TOKEN_TYPE "JWT"

namespace Snowflake
{
namespace Client
{
namespace Jwt
{

/**
 * CJSONHeader is an implementation that uses cJSON to store data
 */
class CJSONHeader : public IHeader
{
public:
  CJSONHeader();

  explicit CJSONHeader(const std::string &text)
  {
    this->json_root_ = {CJSONOperation::parse(Util::Base64::decodeURLNoPadding(text)),
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
    return Util::Base64::encodeURLNoPadding(CJSONOperation::serialize(this->json_root_.get()));
  }

private:
  std::unique_ptr<cJSON, std::function<void(cJSON *)>> json_root_;
};

} // namespace Jwt
} // namespace Client
} // namespace Snowflake

#endif //SNOWFLAKECLIENT_HEADER_HPP
