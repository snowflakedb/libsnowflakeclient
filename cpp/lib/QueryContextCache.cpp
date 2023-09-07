/*
* File:   QueryContextCache.cpp
* Author: harryx
*
* Copyright (c) 2023 Snowflake Computing
*
* Created on August 31, 2023, 4:05 PM
*/

#include "QueryContextCache.hpp"
#include "../logger/SFLogger.hpp"

// wrapper functions for C
extern "C" {

  void qcc_initialize(SF_CONNECT * conn)
  {
    if (!conn || conn->qcc_disable || conn->qcc)
    {
      return;
    }

    conn->qcc = (void *) new Snowflake::Client::QueryContextCache(conn->qcc_capacity);
  }

  void qcc_set_capacity(SF_CONNECT * conn, uint64 capacity)
  {
    if (!conn || conn->qcc_disable)
    {
      return;
    }
    if (!conn->qcc)
    {
      qcc_initialize(conn);
    }
    static_cast<Snowflake::Client::QueryContextCache *>(conn->qcc)->setCapacity(capacity);
  }

  cJSON* qcc_serialize(SF_CONNECT * conn)
  {
    if (!conn || conn->qcc_disable || !conn->qcc)
    {
      return NULL;
    }

    return static_cast<Snowflake::Client::QueryContextCache *>(conn->qcc)->serializeQueryContext();
  }

  void qcc_deserialize(SF_CONNECT * conn, cJSON* query_context)
  {
    if (!conn || conn->qcc_disable)
    {
      return;
    }
    if (!conn->qcc)
    {
      qcc_initialize(conn);
    }

    static_cast<Snowflake::Client::QueryContextCache *>(conn->qcc)->deserializeQueryContext(query_context);
  }

  void qcc_terminate(SF_CONNECT * conn)
  {
    if (!conn || !conn->qcc)
    {
      return;
    }

    delete static_cast<Snowflake::Client::QueryContextCache *>(conn->qcc);
  }

} // extern "C"

namespace Snowflake
{
namespace Client
{

// Public ======================================================================
void QueryContextCache::clearCache()
{
  CXX_LOG_TRACE("QueryContextCache: clearCache called");

  std::lock_guard<std::recursive_mutex> guard(m_mutex);

  m_idMap.clear();
  m_priorityMap.clear();
  m_cacheSet.clear();
}

void QueryContextCache::setCapacity(size_t capacity)
{
  // check without locking first for performance reason
  if (m_capacity == capacity)
    return;

  std::lock_guard<std::recursive_mutex> guard(m_mutex);
  // check again after locking to verify
  if (m_capacity == capacity)
  {
    return;
  }
  m_capacity = capacity;
  checkCacheCapacity();
  logCacheEntries();
  CXX_LOG_TRACE("QueryContextCache::setCapacity: set capacity from %d to %d", m_capacity, capacity);
}

void QueryContextCache::deserializeQueryContext(cJSON * data)
{
  std::lock_guard<std::recursive_mutex> guard(m_mutex);
  // Log existing cache entries
  logCacheEntries();

  if ((!data) || (data->type != cJSON_Object))
  {
    // Clear the cache
    clearCache();
    return;
  }

  // Deserialize the entries. The first entry with priority is the main entry.
  // Save all entries into one list to simplify the logic. An example JSON is:
  // {
  //   "entries": [
  //    {
  //     "id": 0,
  //     "read_timestamp": 123456789,
  //     "priority": 0,
  //     "context": "base64 encoded context"
  //    },
  //     {
  //       "id": 1,
  //       "read_timestamp": 123456789,
  //       "priority": 1,
  //       "context": "base64 encoded context"
  //     },
  //     {
  //       "id": 2,
  //       "read_timestamp": 123456789,
  //       "priority": 2,
  //       "context": "base64 encoded context"
  //     }
  //   ]
  cJSON * entries = snowflake_cJSON_GetObjectItem(data, QCC_ENTRIES_KEY);
  if ((entries) && (entries->type == cJSON_Array))
  {
    int arraySize = snowflake_cJSON_GetArraySize(entries);
    for (int i = 0; i < arraySize; i++)
    {
      QueryContextElement entry;
      if (deserializeQueryContextElement(snowflake_cJSON_GetArrayItem(entries, i), entry))
      {
        merge(entry.id, entry.readTimestamp, entry.priority, entry.context);
      }
      else
      {
        CXX_LOG_TRACE("QueryContextCache::deserializeQueryContext: "
                      "meets mismatch field type. Clear the QueryContextCache.");
        clearCache();
        return;
      }
    }
    // after merging all entries, sync the internal priority map to priority map. Because of priority swicth from GS side,
    // there could be priority key conflict if we directly operating on the priorityMap during a round of merge.
    syncPriorityMap();
  }

  // After merging all entries, truncate to capacity
  checkCacheCapacity();

  // Log existing cache entries
  logCacheEntries();
}

void QueryContextCache::deserializeQueryContextReq(cJSON * data)
{
  std::lock_guard<std::recursive_mutex> guard(m_mutex);

  // Log existing cache entries
  logCacheEntries();

  if ((!data) || (data->type != cJSON_Object))
  {
    // Clear the cache
    clearCache();
    return;
  }

  cJSON * entries = snowflake_cJSON_GetObjectItem(data, QCC_ENTRIES_KEY);
  int arraySize = snowflake_cJSON_GetArraySize(entries);
  if ((entries) && (entries->type == cJSON_Array))
  {
    for (int i = 0; i < arraySize; i++)
    {
      QueryContextElement entry;
      if (deserializeQueryContextElementReq(snowflake_cJSON_GetArrayItem(entries, i), entry))
      {
        merge(entry.id, entry.readTimestamp, entry.priority, entry.context);
      }
      else
      {
        CXX_LOG_TRACE("QueryContextCache::deserializeQueryContextReq: "
          "meets mismatch field type. Clear the QueryContextCache.");
        clearCache();
        return;
      }
    }
    // after merging all entries, sync the internal priority map to priority map. Because of priority swicth from GS side,
    // there could be priority key conflict if we directly operating on the priorityMap during a round of merge.
    syncPriorityMap();
  }

  // After merging all entries, truncate to capacity
  checkCacheCapacity();

  // Log existing cache entries
  logCacheEntries();
}

cJSON *  QueryContextCache::serializeQueryContext()
{
  std::lock_guard<std::recursive_mutex> guard(m_mutex);

  // Log existing cache entries
  logCacheEntries();

  cJSON * queryContext = snowflake_cJSON_CreateObject();
  cJSON * entries = snowflake_cJSON_CreateArray();
  for (auto itr = m_cacheSet.begin(); itr != m_cacheSet.end(); itr++)
  {
    cJSON * entry = snowflake_cJSON_CreateObject();
    snowflake_cJSON_AddUint64ToObject(entry, QCC_ID_KEY, itr->id);
    snowflake_cJSON_AddUint64ToObject(entry, QCC_PRIORITY_KEY, itr->priority);
    snowflake_cJSON_AddUint64ToObject(entry, QCC_TIMESTAMP_KEY, itr->readTimestamp);
    cJSON * context = snowflake_cJSON_CreateObject();
    if (!itr->context.empty())
    {
      snowflake_cJSON_AddStringToObject(context, QCC_CONTEXT_VALUE_KEY, itr->context.c_str());
    }
    snowflake_cJSON_AddItemToObject(entry, QCC_CONTEXT_KEY, context);
    snowflake_cJSON_AddItemToArray(entries, entry);
  }

  snowflake_cJSON_AddItemToObject(queryContext, QCC_ENTRIES_KEY, entries);

  return queryContext;
}

void QueryContextCache::merge(uint64 id, uint64 readTimestamp,
                              uint64 priority, const std::string& context)
{
  if (m_idMap.find(id) != m_idMap.end())
  {
    CXX_LOG_TRACE("QueryContextCache: merge existing id: %lld with priority: %lld", id, priority)
    // ID found in the cache
    QueryContextElement qce = m_idMap[id];
    if (readTimestamp > qce.readTimestamp)
    {
      if (qce.priority == priority)
      {
        // Same priority, overwrite new data at same place
        updateQCE(qce, readTimestamp, context);
      }
      else
      {
        // Change in priority
        QueryContextElement newQCE(id, readTimestamp, priority, context);

        replaceQCE(qce, newQCE);
      } // new priority
    } // new data is recent
    else if (readTimestamp == qce.readTimestamp && qce.priority != priority)
    {
      // Same read timestamp but change in priority
      QueryContextElement newQCE(id, readTimestamp, priority, context);
      replaceQCE(qce, newQCE);
    }
  } // id found
  else
  {
    // new id
    if (m_priorityMap.find(priority) != m_priorityMap.end())
    {
      CXX_LOG_TRACE("QueryContextCache: merge existing priority: %lld with new id: %lld", priority, id)
      // Same priority with different id
      QueryContextElement qce = m_priorityMap[priority];
      // Replace with new data
      QueryContextElement newQCE(id, readTimestamp, priority, context);
      replaceQCE(qce, newQCE);
    }
    else
    {
      // new priority
      // Add new element in the cache
      CXX_LOG_TRACE("QueryContextCache: add new item id: %lld, priority: %lld", id, priority)
      QueryContextElement newQCE(id, readTimestamp, priority, context);
      addQCE(newQCE);
    }
  }
}

void QueryContextCache::syncPriorityMap()
{
  CXX_LOG_TRACE("QueryContextCache::syncPriorityMap called priorityMap size = %d, newPrioirtyMap size = %d",
               m_priorityMap.size(), m_newPriorityMap.size());
  m_priorityMap.insert(m_newPriorityMap.begin(), m_newPriorityMap.end());
  // clear the newPriorityMap for next round of QCC merge(a round consists of multiple entries)
  m_newPriorityMap.clear();
}

void QueryContextCache::checkCacheCapacity()
{
  CXX_LOG_TRACE("QueryContextCache::checkCacheCapacity called. cacheSet size %d cache capacity %d",
               m_cacheSet.size(), m_capacity);
  if (m_cacheSet.size() > m_capacity) {
    // remove elements based on priority

    while (m_cacheSet.size() > m_capacity) {
      QueryContextElement qce = *(m_cacheSet.rbegin());
      removeQCE(qce);
    }
  }

  CXX_LOG_TRACE("QueryContextCache::checkCacheCapacity returns. cacheSet size %d cache capacity %d",
               m_cacheSet.size(), m_capacity);
}

size_t QueryContextCache::getElements(std::vector<uint64>& ids,
                                      std::vector<uint64>& readTimestamps,
                                      std::vector<uint64>& priorities,
                                      std::vector<std::string>& contexts)
{
  size_t count = 0;
  ids.clear();
  readTimestamps.clear();
  priorities.clear();
  contexts.clear();
  for (auto itr = m_cacheSet.begin(); itr != m_cacheSet.end(); itr++)
  {
    ids.push_back(itr->id);
    readTimestamps.push_back(itr->readTimestamp);
    priorities.push_back(itr->priority);
    contexts.push_back(itr->context);
    count++;
  }

  return count;
}

// Private =====================================================================
bool QueryContextCache::deserializeQueryContextElement(cJSON * entryNode,
                                                       QueryContextElement & contextElement)
{
  cJSON * id = snowflake_cJSON_GetObjectItem(entryNode, QCC_ID_KEY);
  if (id && (id->type == cJSON_Number))
  {
    contextElement.id = snowflake_cJSON_GetUint64Value(id);
  }
  else
  {
    CXX_LOG_WARN("QueryContextCache::deserializeQueryContextElement: `id` field is not integer type");
    return false;
  }

  cJSON * timestamp = snowflake_cJSON_GetObjectItem(entryNode, QCC_TIMESTAMP_KEY);
  if (timestamp && (timestamp->type == cJSON_Number))
  {
    contextElement.readTimestamp = snowflake_cJSON_GetUint64Value(timestamp);
  }
  else
  {
    CXX_LOG_WARN("QueryContextCache::deserializeQueryContextElement: `timestamp` field is not integer type");
    return false;
  }

  cJSON * priority = snowflake_cJSON_GetObjectItem(entryNode, QCC_PRIORITY_KEY);
  if (priority && (priority->type == cJSON_Number))
  {
    contextElement.priority = snowflake_cJSON_GetUint64Value(priority);
  }
  else
  {
    CXX_LOG_WARN("QueryContextCache::deserializeQueryContextElement: `priority` field is not integer type");
    return false;
  }

  cJSON * context = snowflake_cJSON_GetObjectItem(entryNode, QCC_CONTEXT_KEY);
  if (!context || context->type == cJSON_NULL)
  {
    return true;
  }
  if (context->type == cJSON_String)
  {
    contextElement.context = snowflake_cJSON_GetStringValue(context);
  }
  else
  {
    CXX_LOG_WARN("QueryContextCache::deserializeQueryContextElement: `context` field is not string type");
    return false;
  }

  return true;
}

bool QueryContextCache::deserializeQueryContextElementReq(cJSON * entryNode,
                                                          QueryContextElement & contextElement)
{
  cJSON * id = snowflake_cJSON_GetObjectItem(entryNode, QCC_ID_KEY);
  if (id && (id->type == cJSON_Number))
  {
    contextElement.id = snowflake_cJSON_GetUint64Value(id);
  }
  else
  {
    CXX_LOG_WARN("QueryContextCache::deserializeQueryContextElement: `id` field is not integer type");
    return false;
  }

  cJSON * timestamp = snowflake_cJSON_GetObjectItem(entryNode, QCC_TIMESTAMP_KEY);
  if (timestamp && (timestamp->type == cJSON_Number))
  {
    contextElement.readTimestamp = snowflake_cJSON_GetUint64Value(timestamp);
  }
  else
  {
    CXX_LOG_WARN("QueryContextCache::deserializeQueryContextElement: `timestamp` field is not integer type");
    return false;
  }

  cJSON * priority = snowflake_cJSON_GetObjectItem(entryNode, QCC_PRIORITY_KEY);
  if (priority && (priority->type == cJSON_Number))
  {
    contextElement.priority = snowflake_cJSON_GetUint64Value(priority);
  }
  else
  {
    CXX_LOG_WARN("QueryContextCache::deserializeQueryContextElement: `priority` field is not integer type");
    return false;
  }

  cJSON * context = snowflake_cJSON_GetObjectItem(entryNode, QCC_CONTEXT_KEY);
  if (!context || context->type == cJSON_NULL)
  {
    return true;
  }
  if (context->type != cJSON_Object)
  {
    return false;
  }

  cJSON * contextValue = snowflake_cJSON_GetObjectItem(context, QCC_CONTEXT_VALUE_KEY);
  if (!contextValue || contextValue->type == cJSON_NULL)
  {
    return true;
  }

  if (contextValue->type == cJSON_String)
  {
    contextElement.context = snowflake_cJSON_GetStringValue(contextValue);
  }
  else
  {
    return false;
  }

  return true;
}

void QueryContextCache::addQCE(const QueryContextElement& qce)
{
  m_idMap[qce.id] = qce;
  // In a round of merge operations, we should save the new priority->qce mapping in an additional map
  // and sync `newPriorityMap` to `priorityMap` at the end of a for loop of `merge` operations
  m_newPriorityMap[qce.priority] = qce;
  m_cacheSet.insert(qce);
}

void QueryContextCache::removeQCE(const QueryContextElement& qce)
{
  m_cacheSet.erase(qce);
  m_priorityMap.erase(qce.priority);
  m_idMap.erase(qce.id);
}

void QueryContextCache::updateQCE(const QueryContextElement& qce,
                                  uint64 timestamp,
                                  const std::string& context)
{
  QueryContextElement qceUpdate = qce;
  qceUpdate.readTimestamp = timestamp;
  qceUpdate.context = context;
  m_cacheSet.erase(qce);
  m_cacheSet.insert(qceUpdate);
  m_idMap[qce.id] = qceUpdate;
  m_priorityMap[qce.priority] = qceUpdate;
}

void QueryContextCache::replaceQCE(const QueryContextElement& oldQCE,
                                   const QueryContextElement& newQCE)
{
  // Remove old element from the cache
  removeQCE(oldQCE);
  // Add new element in the cache
  addQCE(newQCE);
}

void QueryContextCache::logCacheEntries()
{
#ifdef _DEBUG
  for (auto itr = m_cacheSet.begin(); itr != m_cacheSet.end(); itr++)
  {
    CXX_LOG_TRACE("QueryContextCache: Cache Entry: id: %ld readTimestamp: %ld priority: %ld",
                 itr->id, itr->readTimestamp, itr->priority);
  }
#endif
}

} // namespace Client
} // namespace Snowflake
