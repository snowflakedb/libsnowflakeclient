//
// Created by tchen on 9/5/18.
//

#include <vector>
#include "ClaimSet.hpp"
#include "../util/Base64.hpp"

namespace Snowflake
{
namespace Jwt
{

  bool CJSONClaimSet::addJSONClaim(std::string key, cJSON *item)
  {
    if (item == nullptr)  return false;

    if (!containsClaim(key))
    {
      snowflake_cJSON_AddItemToObject(this->json_root_, key.c_str(), item);
      return true;
    }

    snowflake_cJSON_ReplaceItemInObject(this->json_root_, key.c_str(), item);
    return true;
  }

  std::string CJSONClaimSet::serialize()
  {
    char *json_str = snowflake_cJSON_Print(this->json_root_);
    size_t json_str_len = strlen(json_str);
    size_t buf_len = Client::Util::Base64::encodedLength(json_str_len);
    std::vector<char> buffer(buf_len);
    size_t len = Client::Util::Base64::encodeUrl(json_str, json_str_len, buffer.data());
    
    return std::string(buffer.data(), len);
  }

}
}