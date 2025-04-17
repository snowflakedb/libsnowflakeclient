#include "ClientQueryContextCache.hpp"
#include "../logger/SFLogger.hpp"
#include "query_context_cache.h"

// wrapper functions for C
extern "C"
{

  sf_bool is_qcc_enabled(SF_CONNECT *conn)
  {
    if (!conn || conn->qcc_disable)
    {
      return SF_BOOLEAN_FALSE;
    }
    return SF_BOOLEAN_TRUE;
  }

  void qcc_initialize(SF_CONNECT *conn)
  {
    if (!is_qcc_enabled(conn) || conn->qcc)
    {
      return;
    }

    conn->qcc = (void *)new Snowflake::Client::ClientQueryContextCache(conn->qcc_capacity);
  }

  void qcc_set_capacity(SF_CONNECT *conn, uint64 capacity)
  {
    if (!is_qcc_enabled(conn))
    {
      return;
    }
    if (!conn->qcc)
    {
      qcc_initialize(conn);
    }
    static_cast<Snowflake::Client::ClientQueryContextCache *>(conn->qcc)->setCapacity(capacity);
  }

  cJSON *qcc_serialize(SF_CONNECT *conn)
  {
    if (!is_qcc_enabled(conn) || !conn->qcc)
    {
      return NULL;
    }

    return static_cast<Snowflake::Client::ClientQueryContextCache *>(conn->qcc)->serializeQueryContext();
  }

  void qcc_deserialize(SF_CONNECT *conn, cJSON *query_context)
  {
    if (!is_qcc_enabled(conn))
    {
      return;
    }
    if (!conn->qcc)
    {
      qcc_initialize(conn);
    }

    static_cast<Snowflake::Client::ClientQueryContextCache *>(conn->qcc)->deserializeQueryContext(query_context);
  }

  void qcc_terminate(SF_CONNECT *conn)
  {
    if (!conn || !conn->qcc)
    {
      return;
    }

    delete static_cast<Snowflake::Client::ClientQueryContextCache *>(conn->qcc);
  }

} // extern "C"

namespace Snowflake
{
  namespace Client
  {

    // Public ======================================================================
    void ClientQueryContextCache::deserializeQueryContext(cJSON *data)
    {
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
      std::vector<QueryContextElement> elements;
      cJSON *entries = snowflake_cJSON_GetObjectItem(data, SF_QCC_ENTRIES_KEY);
      if ((entries) && (entries->type == cJSON_Array))
      {
        int arraySize = snowflake_cJSON_GetArraySize(entries);
        for (int i = 0; i < arraySize; i++)
        {
          QueryContextElement entry;
          if (deserializeQueryContextElement(snowflake_cJSON_GetArrayItem(entries, i), entry))
          {
            elements.push_back(entry);
          }
          else
          {
            CXX_LOG_TRACE("ClientQueryContextCache::deserializeQueryContext: "
                          "meets mismatch field type. Clear the QueryContextCache.");
            clearCache();
            return;
          }
        }
      }

      UpdateQueryContextCache(elements);
    }

    void ClientQueryContextCache::deserializeQueryContextReq(cJSON *data)
    {
      if ((!data) || (data->type != cJSON_Object))
      {
        // Clear the cache
        clearCache();
        return;
      }

      std::vector<QueryContextElement> elements;
      cJSON *entries = snowflake_cJSON_GetObjectItem(data, SF_QCC_ENTRIES_KEY);
      int arraySize = snowflake_cJSON_GetArraySize(entries);
      if ((entries) && (entries->type == cJSON_Array))
      {
        for (int i = 0; i < arraySize; i++)
        {
          QueryContextElement entry;
          if (deserializeQueryContextElementReq(snowflake_cJSON_GetArrayItem(entries, i), entry))
          {
            elements.push_back(entry);
          }
          else
          {
            CXX_LOG_TRACE("ClientQueryContextCache::deserializeQueryContextReq: "
                          "meets mismatch field type. Clear the QueryContextCache.");
            clearCache();
            return;
          }
        }
      }

      UpdateQueryContextCache(elements);
    }

    cJSON *ClientQueryContextCache::serializeQueryContext()
    {
      std::vector<QueryContextElement> elements;
      GetQueryContextEntries(elements);

      cJSON *queryContext = snowflake_cJSON_CreateObject();
      cJSON *entries = snowflake_cJSON_CreateArray();
      for (auto itr = elements.begin(); itr != elements.end(); itr++)
      {
        cJSON *entry = snowflake_cJSON_CreateObject();
        snowflake_cJSON_AddUint64ToObject(entry, SF_QCC_ID_KEY, itr->id);
        snowflake_cJSON_AddUint64ToObject(entry, SF_QCC_PRIORITY_KEY, itr->priority);
        snowflake_cJSON_AddUint64ToObject(entry, SF_QCC_TIMESTAMP_KEY, itr->readTimestamp);
        cJSON *context = snowflake_cJSON_CreateObject();
        if (!itr->context.empty())
        {
          snowflake_cJSON_AddStringToObject(context, SF_QCC_CONTEXT_VALUE_KEY, itr->context.c_str());
        }
        snowflake_cJSON_AddItemToObject(entry, SF_QCC_CONTEXT_KEY, context);
        snowflake_cJSON_AddItemToArray(entries, entry);
      }

      snowflake_cJSON_AddItemToObject(queryContext, SF_QCC_ENTRIES_KEY, entries);

      return queryContext;
    }

    // Private =====================================================================
    bool ClientQueryContextCache::deserializeQueryContextElement(cJSON *entryNode,
                                                                 QueryContextElement &contextElement)
    {
      cJSON *id = snowflake_cJSON_GetObjectItem(entryNode, SF_QCC_ID_KEY);
      if (id && (id->type == cJSON_Number))
      {
        contextElement.id = snowflake_cJSON_GetUint64Value(id);
      }
      else
      {
        CXX_LOG_WARN("ClientQueryContextCache::deserializeQueryContextElement: `id` field is not integer type");
        return false;
      }

      cJSON *timestamp = snowflake_cJSON_GetObjectItem(entryNode, SF_QCC_TIMESTAMP_KEY);
      if (timestamp && (timestamp->type == cJSON_Number))
      {
        contextElement.readTimestamp = snowflake_cJSON_GetUint64Value(timestamp);
      }
      else
      {
        CXX_LOG_WARN("ClientQueryContextCache::deserializeQueryContextElement: `timestamp` field is not integer type");
        return false;
      }

      cJSON *priority = snowflake_cJSON_GetObjectItem(entryNode, SF_QCC_PRIORITY_KEY);
      if (priority && (priority->type == cJSON_Number))
      {
        contextElement.priority = snowflake_cJSON_GetUint64Value(priority);
      }
      else
      {
        CXX_LOG_WARN("ClientQueryContextCache::deserializeQueryContextElement: `priority` field is not integer type");
        return false;
      }

      cJSON *context = snowflake_cJSON_GetObjectItem(entryNode, SF_QCC_CONTEXT_KEY);
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
        CXX_LOG_WARN("ClientQueryContextCache::deserializeQueryContextElement: `context` field is not string type");
        return false;
      }

      return true;
    }

    bool ClientQueryContextCache::deserializeQueryContextElementReq(cJSON *entryNode,
                                                                    QueryContextElement &contextElement)
    {
      cJSON *id = snowflake_cJSON_GetObjectItem(entryNode, SF_QCC_ID_KEY);
      if (id && (id->type == cJSON_Number))
      {
        contextElement.id = snowflake_cJSON_GetUint64Value(id);
      }
      else
      {
        CXX_LOG_WARN("ClientQueryContextCache::deserializeQueryContextElement: `id` field is not integer type");
        return false;
      }

      cJSON *timestamp = snowflake_cJSON_GetObjectItem(entryNode, SF_QCC_TIMESTAMP_KEY);
      if (timestamp && (timestamp->type == cJSON_Number))
      {
        contextElement.readTimestamp = snowflake_cJSON_GetUint64Value(timestamp);
      }
      else
      {
        CXX_LOG_WARN("ClientQueryContextCache::deserializeQueryContextElement: `timestamp` field is not integer type");
        return false;
      }

      cJSON *priority = snowflake_cJSON_GetObjectItem(entryNode, SF_QCC_PRIORITY_KEY);
      if (priority && (priority->type == cJSON_Number))
      {
        contextElement.priority = snowflake_cJSON_GetUint64Value(priority);
      }
      else
      {
        CXX_LOG_WARN("ClientQueryContextCache::deserializeQueryContextElement: `priority` field is not integer type");
        return false;
      }

      cJSON *context = snowflake_cJSON_GetObjectItem(entryNode, SF_QCC_CONTEXT_KEY);
      if (!context || context->type == cJSON_NULL)
      {
        return true;
      }
      if (context->type != cJSON_Object)
      {
        return false;
      }

      cJSON *contextValue = snowflake_cJSON_GetObjectItem(context, SF_QCC_CONTEXT_VALUE_KEY);
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

  } // namespace Client
} // namespace Snowflake
