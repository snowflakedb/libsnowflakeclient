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

std::string Base64URLOpt::encodeNoPadding(const std::vector<char> &bytes)
{
  size_t buf_len = Client::Util::Base64::encodedLength(bytes.size());
  std::string buffer(buf_len, 0);
  size_t len = Client::Util::Base64::encodeUrl(bytes.data(), bytes.size(), (void *) buffer.data());

  // remove the end of padding
  while (buffer[len - 1] == '=') len--;

  return buffer.substr(0, len);
}

std::vector<char> Base64URLOpt::decodeNoPadding(const std::string &text)
{
  // add padding to the end
  size_t pad_len = (4 - text.length() % 4) % 4;
  std::string in_text = text + std::string(pad_len, '=');

  // Base 64 decode text
  size_t decode_len = Client::Util::Base64::decodedLength(in_text.length());
  std::vector<char> decoded(decode_len);
  decode_len = Client::Util::Base64::decodeUrl(in_text.c_str(), in_text.length(), decoded.data());
  if ((ssize_t) decode_len == -1) throw JwtParseFailure();

  decoded.resize(decode_len);
  return decoded;
}

}
}