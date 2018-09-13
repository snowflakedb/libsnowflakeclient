/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "Util.hpp"
#include "JwtException.hpp"
#include "../util/Base64.hpp"
#include <vector>
#include <memory>
#include <functional>

namespace Snowflake
{
namespace Client
{
namespace Jwt
{
std::vector<char> CJSONOperation::serialize(cJSON *root)
{
  std::unique_ptr<char, std::function<void(char *)>>
    json_str(snowflake_cJSON_Print(root), [](char *str) { snowflake_cJSON_free(str); });
  if (json_str == nullptr) throw JwtMemoryAllocationFailure();
  size_t json_str_len = strlen(json_str.get());

  std::vector<char> result(json_str.get(), json_str.get() + json_str_len + 1);

  return result;
}

cJSON *CJSONOperation::parse(const std::vector<char> &text)
{
  // Parse the decode string to object
  cJSON *root = snowflake_cJSON_Parse(text.data());
  if (root == nullptr) throw JwtParseFailure();

  return root;
}

void CJSONOperation::addOrReplaceJSON(cJSON *root, std::string key, cJSON *item)
{
  if (item == nullptr)
  {
    throw JwtMemoryAllocationFailure();
  }

  bool contains = snowflake_cJSON_HasObjectItem(root, key.c_str());

  if (!contains)
  {
    snowflake_cJSON_AddItemToObject(root, key.c_str(), item);
  }

  snowflake_cJSON_ReplaceItemInObject(root, key.c_str(), item);
}


}
} // namespace Client
}
