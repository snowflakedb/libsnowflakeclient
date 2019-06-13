/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include "Header.hpp"

namespace Snowflake
{
namespace Client
{
namespace Jwt
{

IHeader *IHeader::buildHeader()
{
  return new CJSONHeader();
}

IHeader *IHeader::parseHeader(const std::string &text)
{
  return new CJSONHeader(text);
}

CJSONHeader::CJSONHeader()
{
  json_root_ = {snowflake_cJSON_CreateObject(), CJSONOperation::cJSONDeleter};
  if (json_root_ == nullptr)
  {
    throw std::bad_alloc();
  }

  // Add the typ field to be token
  std::string default_token_type = DEFAULT_TOKEN_TYPE;
  cJSON *item = snowflake_cJSON_CreateString(default_token_type.c_str());
  if (!item)
  {
    throw std::bad_alloc();
  }
  CJSONOperation::addOrReplaceJSON(json_root_.get(), TOKEN_TYPE, item);
}

AlgorithmType CJSONHeader::getAlgorithmType()
{
  cJSON *item = snowflake_cJSON_GetObjectItem(json_root_.get(), ALGORITHM);
  if (!item || (item->type != cJSON_String)) return AlgorithmType::UNKNOWN;
  return AlgorithmTypeMapper::toAlgorithmType(std::string(item->valuestring));
}

std::string CJSONHeader::getCustomHeaderEntry(const std::string header_type)
{
  cJSON *item = snowflake_cJSON_GetObjectItem(json_root_.get(), header_type.c_str());
  if (!item || (item->type != cJSON_String)) return UNSUPPORTED;
  return std::string(item->valuestring);
}

}
} // namespace Client
}