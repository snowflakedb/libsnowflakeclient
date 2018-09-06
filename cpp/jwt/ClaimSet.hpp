//
// Created by tchen on 9/5/18.
//

#ifndef SNOWFLAKECLIENT_CLAIMSET_HPP
#define SNOWFLAKECLIENT_CLAIMSET_HPP

#include <cJSON.h>
#include <string>
#include <memory>
#include <set>
#include <chrono>

#define CLAIM_ISSUER "iss"
#define CLAIM_SUBJECT "sub"
#define CLAIM_AUDIENCE "aud"
#define CLAIM_EXPIRATION "exp"
#define CLAIM_NOT_BEFORE "nbf"
#define CLAIM_ISSUED_AT "iat"
#define CLAIM_JWT_ID "jti"

namespace Snowflake
{
namespace Jwt
{
class ClaimSet
{

public:

  using sysClock = std::chrono::system_clock;
  using sysTimeDuration = sysClock::duration;

  virtual bool containsClaim(std::string &key);


  virtual bool addClaim(std::string &key, std::string &value);


  virtual bool addClaim(std::string &key, long number);

  virtual std::string getClaimInString(std::string &key);

  virtual long getClaimInLong(std::string &key);

  virtual std::string serialize();

  virtual void removeClaim(std::string &key);

private:
  inline long timeDurationToSecond(sysTimeDuration &td)
  {
    return td.count() * sysClock::period::num / sysClock::period::den;
  }
};

class CJSONClaimSet : ClaimSet
{
public:
  /**
   * Constructor of an empty ClaimSet
   */
  CJSONClaimSet()
  {
    this->json_root_ = snowflake_cJSON_CreateObject();
    if (!this->json_root_) {
      // TODO throw an exception
    }
  }

  explicit CJSONClaimSet(std::string &text)
  {
    this->json_root_ = snowflake_cJSON_Parse(text.c_str());
    if (!this->json_root_) {
      // TODO throw an exception
    }
  }

  ~CJSONClaimSet()
  {
    if (this->json_root_) {
      snowflake_cJSON_Delete(this->json_root_);
    }
  }

  inline bool containsClaim(std::string &key) override
  {
    return snowflake_cJSON_HasObjectItem(json_root_, key.c_str());
  }

  inline bool addClaim(std::string &key, std::string &value) override
  {
    cJSON *item = snowflake_cJSON_CreateString(value.c_str());
    return addJSONClaim(key, item);
  }

  inline bool addClaim(std::string &key, long number) override
  {
    double d_num = number;
    cJSON *item = snowflake_cJSON_CreateNumber(d_num);
    return addJSONClaim(key, item);
  }

  inline std::string getClaimInString(std::string &key) override
  {
    cJSON *value = snowflake_cJSON_GetObjectItemCaseSensitive(this->json_root_, key.c_str());
    if (!value || (value->type != cJSON_String)) return "";
    return std::string(value->string);
  }

  inline long getClaimInLong(std::string &key) override
  {
    cJSON *value = snowflake_cJSON_GetObjectItemCaseSensitive(this->json_root_, key.c_str());
    if (!value || (value->type != cJSON_String)) return 0;
    return (long)value->valuedouble;
  }

  std::string serialize() override;

  inline void removeClaim(std::string &key) override
  {
    snowflake_cJSON_DeleteItemFromObject(this->json_root_, key.c_str());
  }

private:

  inline bool addJSONClaim(std::string key, cJSON *value);

  cJSON *json_root_;

};

}
}

#endif //SNOWFLAKECLIENT_CLAIMSET_HPP
