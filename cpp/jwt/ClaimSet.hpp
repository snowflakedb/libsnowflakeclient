/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_CLAIMSET_HPP
#define SNOWFLAKECLIENT_CLAIMSET_HPP

#include <cJSON.h>
#include <string>
#include <memory>
#include <set>
#include <chrono>
#include "Util.hpp"
#include "JwtException.hpp"

namespace Snowflake
{
namespace Jwt
{
class IClaimSet
{

public:
  using sysClock = std::chrono::system_clock;
  using sysTimeDuration = sysClock::duration;

  virtual ~IClaimSet() = default;

  static IClaimSet *buildClaimSet();

  static IClaimSet *parseClaimset(const std::string &text);

  /**
   * Check if the claim set contains a specific key
   */
  virtual bool containsClaim(std::string &key) = 0;

  /**
   * Add the key and a string value to the claim set
   * Would replace the old one if the key exists
   * @param key
   * @param value
   */
  virtual void addClaim(std::string &key, std::string &value) = 0;

  /**
   * Add the key and a long value to the claim set
   * Would replace the old one if the key exists
   * @param key
   * @param value
   */
  virtual void addClaim(std::string &key, long number) = 0;

  /**
   * Get a claim from the claim set in string type
   */
  virtual std::string getClaimInString(std::string &key) = 0;

  /**
   * Get a claim from the claim set in long type
   */
  virtual long getClaimInLong(std::string &key) = 0;

  /**
   * Serialize the claim set to base64url encoded format
   */
  virtual std::string serialize() = 0;

  /**
   * Remove a claim from the claim set with specified key
   */
  virtual void removeClaim(std::string &key) = 0;

private:
  inline long timeDurationToSecond(sysTimeDuration &td)
  {
    return td.count() * sysClock::period::num / sysClock::period::den;
  }
};

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
    this->json_root_ = {snowflake_cJSON_CreateObject(), CJSONOperation::cJSONDeleter};
    if (this->json_root_ == nullptr)
    {
      throw JwtMemoryAllocationFailure();
    }
  }

  /**
   * Constructor of an claim set given the base64encoded text
   * @param text
   */
  explicit CJSONClaimSet(const std::string &text)
  {
    this->json_root_ = {CJSONOperation::parse(text), CJSONOperation::cJSONDeleter};
  }

  inline bool containsClaim(std::string &key) override
  {
    return snowflake_cJSON_HasObjectItem(json_root_.get(), key.c_str());
  }

  inline void addClaim(std::string &key, std::string &value) override
  {
    cJSON *item = snowflake_cJSON_CreateString(value.c_str());
    return CJSONOperation::addOrReplaceJSON(json_root_.get(), key, item);
  }

  inline void addClaim(std::string &key, long number) override
  {
    double d_num = number;
    cJSON *item = snowflake_cJSON_CreateNumber(d_num);
    return CJSONOperation::addOrReplaceJSON(json_root_.get(), key, item);
  }

  inline std::string getClaimInString(std::string &key) override
  {
    cJSON *value = snowflake_cJSON_GetObjectItemCaseSensitive(this->json_root_.get(), key.c_str());
    if (!value || (value->type != cJSON_String)) return "";
    return std::string(value->string);
  }

  inline long getClaimInLong(std::string &key) override
  {
    cJSON *value = snowflake_cJSON_GetObjectItemCaseSensitive(this->json_root_.get(), key.c_str());
    if (!value || (value->type != cJSON_String)) return 0;
    return (long)value->valuedouble;
  }

  inline std::string serialize() override
  {
    return CJSONOperation::serialize(json_root_.get());
  }

  inline void removeClaim(std::string &key) override
  {
    snowflake_cJSON_DeleteItemFromObject(this->json_root_.get(), key.c_str());
  }

private:

  std::unique_ptr<cJSON, void (*)(cJSON *)> json_root_;

};

} // namespace Jwt
} // namespace Snowflake

#endif //SNOWFLAKECLIENT_CLAIMSET_HPP
