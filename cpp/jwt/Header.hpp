/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
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
#define UNSUPPORTED "NA"

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
   * Set Custom Header
   */
  inline void setCustomHeaderEntry(std::string header_type, std::string header_value) override
  {
     cJSON *item = snowflake_cJSON_CreateString(header_value.c_str());
     CJSONOperation::addOrReplaceJSON(this->json_root_.get(), header_type, item);
  }

  /**
   * See IHeader
   */
  AlgorithmType getAlgorithmType() override;

  /**
   * See IHeader
   */
  std::string getCustomHeaderEntry(const std::string header_type) override;

  /**
   * See IHeader
   */
  inline std::string serialize(bool format=true) override
  {
    if (format)
    {
      return Util::Base64::encodeURLNoPadding(CJSONOperation::serialize(this->json_root_.get()));
    }
    else
    {
      return Util::Base64::encodeURLNoPadding(CJSONOperation::serializeUnformatted(this->json_root_.get()));
    }
  }

private:
  std::unique_ptr<cJSON, std::function<void(cJSON *)>> json_root_;
};

} // namespace Jwt
} // namespace Client
} // namespace Snowflake

#endif //SNOWFLAKECLIENT_HEADER_HPP
