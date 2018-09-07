/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_UTIL_HPP
#define SNOWFLAKECLIENT_UTIL_HPP


#include <string>
#include <cJSON.h>
#include <cstring>

namespace Snowflake
{
namespace Jwt
{


/**
 * Helper class with handy function to operate on cJSON objects
 */
class CJSONOperation
{
public:
  /**
   * serialize a cJSON object by base64url and remove padding at the end
   * @param root root of the cJSON object to be serialize
   * @return a base64URL coded cJSON object without padding
   */
  static std::string serialize(cJSON *root);

  /**
   * parse a base64url encoded string and construct a CJSON object
   */
  static cJSON *parse(const std::string &text);

  /**
   * Add an item below the cJSON root with key specified
   * @param root
   * @param key
   * @param item
   */
  static void addOrReplaceJSON(cJSON *root, std::string key, cJSON *item);

  /**
   * Deleter function wrapper
   * @param mdctx
   */
  static inline void cJSONDeleter(cJSON *item)
  {
    if (item) snowflake_cJSON_Delete(item);
  }
};

}
}


#endif //SNOWFLAKECLIENT_UTIL_HPP
