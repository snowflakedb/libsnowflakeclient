#include "snowflake/QueryContextCache.hpp"
#include "../logger/SFLogger.hpp"

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

    void QueryContextCache::UpdateQueryContextCache(const std::vector<QueryContextElement> &entries)
    {
      std::lock_guard<std::recursive_mutex> guard(m_mutex);
      // Log existing cache entries
      logCacheEntries();

      for (auto entry = entries.begin(); entry != entries.end(); entry++)
      {
        merge(entry->id, entry->readTimestamp, entry->priority, entry->context);
      }
      // after merging all entries, sync the internal priority map to priority map. Because of priority swicth from GS side,
      // there could be priority key conflict if we directly operating on the priorityMap during a round of merge.
      syncPriorityMap();

      // After merging all entries, truncate to capacity
      checkCacheCapacity();

      // Log existing cache entries
      logCacheEntries();
    }

    size_t QueryContextCache::GetQueryContextEntries(std::vector<QueryContextElement> &entries)
    {
      std::lock_guard<std::recursive_mutex> guard(m_mutex);

      // Log existing cache entries
      logCacheEntries();

      entries.clear();

      for (auto itr = m_cacheSet.begin(); itr != m_cacheSet.end(); itr++)
      {
        entries.emplace_back(*itr);
      }

      return entries.size();
    }

    void QueryContextCache::merge(uint64 id, uint64 readTimestamp,
                                  uint64 priority, const std::string &context)
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
      if (m_cacheSet.size() > m_capacity)
      {
        // remove elements based on priority

        while (m_cacheSet.size() > m_capacity)
        {
          QueryContextElement qce = *(m_cacheSet.rbegin());
          removeQCE(qce);
        }
      }

      CXX_LOG_TRACE("QueryContextCache::checkCacheCapacity returns. cacheSet size %d cache capacity %d",
                    m_cacheSet.size(), m_capacity);
    }

    size_t QueryContextCache::getElements(std::vector<uint64> &ids,
                                          std::vector<uint64> &readTimestamps,
                                          std::vector<uint64> &priorities,
                                          std::vector<std::string> &contexts)
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
    void QueryContextCache::addQCE(const QueryContextElement &qce)
    {
      m_idMap[qce.id] = qce;
      // In a round of merge operations, we should save the new priority->qce mapping in an additional map
      // and sync `newPriorityMap` to `priorityMap` at the end of a for loop of `merge` operations
      m_newPriorityMap[qce.priority] = qce;
      m_cacheSet.insert(qce);
    }

    void QueryContextCache::removeQCE(const QueryContextElement &qce)
    {
      m_cacheSet.erase(qce);
      m_priorityMap.erase(qce.priority);
      m_idMap.erase(qce.id);
    }

    void QueryContextCache::updateQCE(const QueryContextElement &qce,
                                      uint64 timestamp,
                                      const std::string &context)
    {
      QueryContextElement qceUpdate = qce;
      qceUpdate.readTimestamp = timestamp;
      qceUpdate.context = context;
      m_cacheSet.erase(qce);
      m_cacheSet.insert(qceUpdate);
      m_idMap[qce.id] = qceUpdate;
      m_priorityMap[qce.priority] = qceUpdate;
    }

    void QueryContextCache::replaceQCE(const QueryContextElement &oldQCE,
                                       const QueryContextElement &newQCE)
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
