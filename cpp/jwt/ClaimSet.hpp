/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_CLAIMSET_HPP
#define SNOWFLAKECLIENT_CLAIMSET_HPP

#include <cJSON.h>
#include <string>
#include <memory>
#include <set>
#include <functional>
#include "Util.hpp"
#include "../util/Base64.hpp"
#include "snowflake/IJwt.hpp"

namespace Snowflake
{
namespace Client
{
namespace Jwt
{
/**
 * The ClaimSet implementation using CJSON as underlying data structure
 */
class CJSONClaimSet : public IClaimSet
{
public:
  /**
   * Constructor of an empty ClaimSet
   */
  CJSONClaimSet()
  {
    this->json_root_ = {snowflake_cJSON_CreateObject(),
                        CJSONOperation::cJSONDeleter};
    if (this->json_root_ == nullptr)
    {
      throw std::bad_alloc();
    }
  }

  /**
   * Constructor of an claim set given the base64encoded text
   * @param text
   */
  explicit CJSONClaimSet(const std::string &text)
  {
    this->json_root_ = {CJSONOperation::parse(Client::Util::Base64::decodeURLNoPadding(text)),
                        CJSONOperation::cJSONDeleter};
  }

  inline bool containsClaim(const std::string &key) override
  {
    return snowflake_cJSON_HasObjectItem(json_root_.get(), key.c_str());
  }

  inline void addClaim(const std::string &key, const std::string &value) override
  {
    cJSON *item = snowflake_cJSON_CreateString(value.c_str());
    return CJSONOperation::addOrReplaceJSON(json_root_.get(), key, item);
  }

  inline void addClaim(const std::string &key, long number) override
  {
    double d_num = number;
    cJSON *item = snowflake_cJSON_CreateNumber(d_num);
    return CJSONOperation::addOrReplaceJSON(json_root_.get(), key, item);
  }

  inline std::string getClaimInString(const std::string &key) override
  {
    cJSON *value = snowflake_cJSON_GetObjectItemCaseSensitive(this->json_root_.get(), key.c_str());
    if (!value || (value->type != cJSON_String)) return "";
    return std::string(value->valuestring);
  }

  inline long getClaimInLong(const std::string &key) override
  {
    cJSON *value = snowflake_cJSON_GetObjectItemCaseSensitive(this->json_root_.get(), key.c_str());
    if (!value || (value->type != cJSON_Number)) return 0;
    return (long)value->valuedouble;
  }

  inline double getClaimInDouble(const std::string &key) override
  {
    cJSON *value = snowflake_cJSON_GetObjectItemCaseSensitive(this->json_root_.get(), key.c_str());
    if (!value || (value->type != cJSON_Number)) return 0;
    return value->valuedouble;
  }

  inline std::string serialize(bool format=true) override
  {
    if (format)
    {
      return Client::Util::Base64::encodeURLNoPadding(CJSONOperation::serialize(json_root_.get()));
    }
    else
    {
      return Client::Util::Base64::encodeURLNoPadding(CJSONOperation::serializeUnformatted(json_root_.get()));
    }
  }

  inline void removeClaim(const std::string &key) override
  {
    snowflake_cJSON_DeleteItemFromObject(this->json_root_.get(), key.c_str());
  }

private:
  std::unique_ptr<cJSON, std::function<void(cJSON *)>> json_root_;

};

} // namespace Jwt
} // namespace Client
} // namespace Snowflake

#endif //SNOWFLAKECLIENT_CLAIMSET_HPP
