/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_UTIL_HPP
#define SNOWFLAKECLIENT_UTIL_HPP


#include <string>
#include <cJSON.h>
#include <cstring>
#include <vector>

namespace Snowflake
{
namespace Client
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
  static std::vector<char> serialize(cJSON *root);

  /**
   * serialize a cJSON object by base64url and remove padding at the end
   * without formatting.
   * @param root root of the cJSON object to be serialize
   * @return a base64URL coded cJSON object without padding
   */
  static std::vector<char> serializeUnformatted(cJSON *root);

  /**
   * parse a base64url encoded string and construct a CJSON object
   */
  static cJSON *parse(const std::vector<char> &text);

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
} // namespace Client
}


#endif //SNOWFLAKECLIENT_UTIL_HPP
