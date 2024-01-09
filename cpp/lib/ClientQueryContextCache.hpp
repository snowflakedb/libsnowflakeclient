/*
 * Copyright (c) 2024 Snowflake Computing, Inc. All rights reserved.
 */

#pragma once
#ifndef CLIENT_QUERY_CONTEXT_CACHE_HPP
#define CLIENT_QUERY_CONTEXT_CACHE_HPP

#include <mutex>
#include <map>
#include <set>
#include <vector>
#include "cJSON.h"
#include "snowflake/client.h"
#include "snowflake/QueryContextCache.hpp"

namespace Snowflake
{
namespace Client
{

class ClientQueryContextCache : public QueryContextCache
{
public:
  // constructor
  ClientQueryContextCache(size_t capacity) : QueryContextCache(capacity) {}

  /**
  * @param data: the JSON value of QueryContext Object
  */
  void deserializeQueryContext(cJSON * data);

  cJSON * serializeQueryContext();

  /**
  * for test purpose only, deserialize from JSON value serialized from cache
  * for sending request
  * @param data: the JSON value of QueryContext Object
  */
  void deserializeQueryContextReq(cJSON * data);

private:
  bool deserializeQueryContextElement(cJSON * entryNode,
                                      QueryContextElement & contextElement);

  // for test purpose only, deserializeQueryContextElement from serialized
  // JSON value for request
  bool deserializeQueryContextElementReq(cJSON * entryNode,
                                         QueryContextElement & contextElement);
};

} // namespace Client
} // namespace Snowflake

#endif  // QUERY_CONTEXT_CACHE_HPP
