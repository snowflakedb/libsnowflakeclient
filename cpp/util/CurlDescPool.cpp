/*
 * Copyright (c) 2024 Snowflake Computing, Inc. All rights reserved.
 */

#include <utility>
#include "snowflake/CurlDescPool.hpp"
#include "curl_desc_pool.h"
#include "../logger/SFLogger.hpp"

// wrapper functions for C
extern "C" {
  using namespace Snowflake;
  using namespace Snowflake::Client;

  struct CURL_DESC_S {
    CurlDescPool::SubPool* sub_pool;
    std::unique_ptr<CurlDesc> curl_desc;
  };

  void* get_curl_desc_from_pool(const char* url, const char* proxy, const char* no_proxy)
  {
    CURL_DESC_S* ret = new CURL_DESC_S;
    if (!ret)
    {
      CXX_LOG_ERROR("get_curl_desc_from_pool: memory allocation failed.");
      return NULL;
    }

    Snowflake::Client::SFURL sfurl;
    try
    {
      sfurl.parse(url);
      if (proxy)
      {
        Snowflake::Client::Util::Proxy proxy_setting(proxy);
        if (no_proxy)
        {
          proxy_setting.setNoProxy(no_proxy);
        }
        sfurl.setProxy(proxy_setting);
      }
      // get pool of curl descriptors
      ClientCurlDescPool& curlDescPool = ClientCurlDescPool::getInstance();

      // get sub pool for our URL class
      ret->sub_pool = &curlDescPool.getSubPool(sfurl);
      if (!ret->sub_pool)
      {
        CXX_LOG_ERROR("get_curl_desc_from_pool: failed on getting subpool");
        delete ret;
        return NULL;
      }

      // now allocate and get ref to a descriptor
      ret->sub_pool->newCurlDesc(ret->curl_desc);
      if (!ret->curl_desc)
      {
        CXX_LOG_ERROR("get_curl_desc_from_pool: failed on getting curl desc");
        delete ret;
        return NULL;
      }
    }
    catch (std::exception& e)
    {
      CXX_LOG_ERROR("get_curl_desc_from_pool: failed with exception %s",
                    e.what());
      delete ret;
      return NULL;
    }

    return ret;
  }

  CURL* get_curl_from_desc(void* curl_desc)
  {
    if (!curl_desc || !((CURL_DESC_S*)curl_desc)->curl_desc)
    {
      CXX_LOG_ERROR("get_curl_from_desc: invalid curl desc");
      return NULL;
    }

    return ((CURL_DESC_S*)curl_desc)->curl_desc->getCurl();
  }

  void free_curl_desc(void* curl_desc)
  {
    if (!curl_desc || !((CURL_DESC_S*)curl_desc)->sub_pool ||
        !((CURL_DESC_S*)curl_desc)->curl_desc)
    {
      CXX_LOG_ERROR("free_curl_desc: invalid curl desc");
      return;
    }

    ((CURL_DESC_S*)curl_desc)->sub_pool->freeCurlDesc(((CURL_DESC_S*)curl_desc)->curl_desc);
    delete (CURL_DESC_S*)curl_desc;
  }
} // extern "C"

namespace Snowflake
{
namespace Client
{
  /**
   * Constructor
   */
  CurlDescPool::CurlDescPool()
    : m_init(false)
    , m_curlShared(nullptr)
  {
    // initialize service
    this->init();
  }

  /**
   * Initialization (or reinitialization). This is called after forking a job
   * so that we can start clean
   */
  void CurlDescPool::init()
  {
    CURLcode curlCode;
    CURLSHcode curlShareCode;

    // cleanup first if we need to re-initialize
    if (m_init)
    {
      // already initialized, simply destroy all existing connection
      if (!m_subPools.empty())
        m_subPools.clear();
    }
    else
    {
      // first ensure that curl is globally initialized
      if ((curlCode = curl_global_init(CURL_GLOBAL_ALL)) != CURLE_OK)
      {
        CXX_LOG_ERROR("CurlDescPool::init(): curl_global_init() failed, curlCode=%d, %s",
                      curlCode, curl_easy_strerror(curlCode));       
      }

      // now initialize the shared descriptor
      if ((m_curlShared = curl_share_init()) == (CURLSH *)0)
      {
        CXX_LOG_ERROR("CurlDescPool::init(): curl_share_init() failed, unknown error, null returned.")
      }

      // set lock function
      if ((curlShareCode = curl_share_setopt(m_curlShared, CURLSHOPT_LOCKFUNC,
                                   CurlDescPool::curlShareLock)) != CURLSHE_OK)
      {
        CXX_LOG_ERROR("CurlDescPool::init(): curl_share_setopt(CURLSHOPT_LOCKFUNC) failed, curlCode=%d, %s",
                      curlShareCode, curl_share_strerror(curlShareCode));       
      }

      // set unlock function
      if ((curlShareCode = curl_share_setopt(m_curlShared, CURLSHOPT_UNLOCKFUNC,
                                  CurlDescPool::curlShareUnlock)) != CURLSHE_OK)
      {
        CXX_LOG_ERROR("CurlDescPool::init(): curl_share_setopt(CURLSHOPT_UNLOCKFUNC) failed, curlCode=%d, %s",
                      curlShareCode, curl_share_strerror(curlShareCode));       
      }

      // ask for DNS cache to be shared
      if ((curlShareCode = curl_share_setopt(m_curlShared, CURLSHOPT_SHARE,
                                             CURL_LOCK_DATA_DNS)) != CURLSHE_OK)
      {
        CXX_LOG_ERROR("CurlDescPool::init(): curl_share_setopt(CURLSHOPT_SHARE/DNS) failed, curlCode=%d, %s",
                      curlShareCode, curl_share_strerror(curlShareCode));       
      }

      // pass the pointer to the descriptor pool as context of the callbacks
      if ((curlShareCode = curl_share_setopt(m_curlShared, CURLSHOPT_USERDATA,
                                                     (void *)this)) != CURLSHE_OK)
      {
        CXX_LOG_ERROR("CurlDescPool::init(): curl_share_setopt(CURLSHOPT_USERDATA) failed, curlCode=%d, %s",
                      curlShareCode, curl_share_strerror(curlShareCode));       
      }

      // we are initialized
      m_init = true;
    }

  }

  /**
   * Destructor, destroy all sub-pools, destroy shared descriptor and terminate
   */
  CurlDescPool::~CurlDescPool()
  {
    // cleanup if initialization was performed
    if (m_init)
    {
      // destroy all curl descriptors
      m_subPools.clear();

      // then destroy share descriptor if any
      if (m_curlShared)
      {
        curl_share_cleanup(m_curlShared);
        m_curlShared = (CURLSH *)0;
      }

      // finally end curl
      curl_global_cleanup();

      // done
      m_init = false;
    }
  }

  /**
   * Callback function called by curl to lock the shared descriptor
   *
   * @param handle
   *   handle for which this function is called
   *
   * @param data
   *   data being accessed
   *
   * @param access
   *   access type
   *
   * @param ctx
   *   our context, pointer to this singleton instance
   */
  void CurlDescPool::curlShareLock(CURL *handle, curl_lock_data data,
                                   curl_lock_access access, void *ctx)
  {
    CurlDescPool &curlDescPool = *((CurlDescPool *)ctx);

    if (data == CURL_LOCK_DATA_DNS)
    {
      curlDescPool.m_lockSharedDns.lock();
    }
    else if (data == CURL_LOCK_DATA_SSL_SESSION)
    {
      curlDescPool.m_lockSharedSsl.lock();
    }
    else if (data == CURL_LOCK_DATA_SHARE)
    {
      curlDescPool.m_lockSharedShare.lock();
    }
  }

  /**
   * Callback function for curl to unlock the shared descriptor
   *
   * @param handle
   *   handle for which this function is called
   *
   * @param data
   *   data being accessed
   *
   * @param ctx
   *   our context, pointer to this singleton instance
   */
  void CurlDescPool::curlShareUnlock(CURL *handle, curl_lock_data data,
                                     void *ctx)
  {
    CurlDescPool &curlDescPool = *((CurlDescPool *)ctx);

    if (data == CURL_LOCK_DATA_DNS)
    {
      curlDescPool.m_lockSharedDns.unlock();
    }
    else if (data == CURL_LOCK_DATA_SSL_SESSION)
    {
      curlDescPool.m_lockSharedSsl.unlock();
    }
    else if (data == CURL_LOCK_DATA_SHARE)
    {
      curlDescPool.m_lockSharedShare.unlock();
    }
  }

  /**
   * Get sub-pool associated to the specified URL
   *
   * @param url
   *   url we are accessing
   */
  CurlDescPool::SubPool &CurlDescPool::getSubPool(const SFURL &url)
  {
    std::string endPointName = url.scheme() + "://" + url.authority();

    if (url.isProxyEnabled())
    {
      const Snowflake::Client::Util::Proxy & proxy = url.getProxy();
      if (proxy.getMachine().empty())
      {
        endPointName += " no proxy";
      }
      else
      {
        endPointName += " with Proxy: " + proxy.getHost() +
                        ", port: " + std::to_string(proxy.getPort()) +
                        ", no_proxy: " + proxy.getNoProxy();
      }
    }

    // iterator to lookup sub pools in our map
    SubPoolByName_t::iterator subPoolIterator;

    // see if sub pool exists, if not create it
    std::lock_guard<std::mutex> guard(m_lockPool);

    // find this sub pool
    subPoolIterator = m_subPools.find(endPointName);

    // sub-pool to return
    SubPool *subPool;

    // add it if it does not exists
    if (subPoolIterator == m_subPools.end())
    {
      std::unique_ptr<SubPool> subPoolPt(new SubPool(endPointName,
                                                     m_curlShared,
                                                     *this));
      subPool = subPoolPt.get();
      m_subPools[endPointName] = std::move(subPoolPt);
    }
    else
    {
      subPool = subPoolIterator->second.get();
    }

    // trace
    CXX_LOG_TRACE("CurlDescPool::getSubPool(): Getting subpool for endPoint=%s res=%p",
                  endPointName.c_str(), (void *)subPool);       

    // return sub pool
    return(*subPool);
  }

  /**
   * Constructor
   *
   * @param endPointName
   *   name of the server end point, derived from the URL
   *
   * @param (IN/NULL) curlShareDesc
   *   curl shared descriptor to use if non null
   *
   * @param parentPool
   *   reference to parent pool instance.
   */
  CurlDescPool::SubPool::SubPool(const std::string &endPointName,
                                 CURLSH *curlShareDesc,
                                 CurlDescPool& parentPool)
    : m_endPointName(endPointName)
    , m_curlShareDesc(curlShareDesc)
    , m_parentPool(parentPool)
  {
  }

  /**
   * Get new curl descriptor from the pool and populate unique_ptr with it
   *
   * @param curlDescPt
   *   empty unique pointer we will populate with a new pointer to a curl
   *   descriptor
   */
  void CurlDescPool::SubPool::newCurlDesc(std::unique_ptr<CurlDesc> &curlDescPt)
  {
    // ensure not already created
    if (!curlDescPt)
    {
      // get mutex and grab an entry from the subpool
      bool isEmpty;
      {
        std::lock_guard<std::mutex> mutexGuard(m_lockSubPool);

        // if pool empty, allocate new one
        isEmpty = m_curlDescPool.empty();
        if (isEmpty)
        {
          curlDescPt = std::move(m_parentPool.createCurlDesc(m_curlShareDesc));
        }
        else
        {
          // transfer element
          curlDescPt = std::move(m_curlDescPool.back());

          // remove the now empty unique pointer
          m_curlDescPool.pop_back();
        }
      }

      // trace
      if (isEmpty)
      {
        CXX_LOG_TRACE("CurlDescPool::newCurlDesc(): Allocate new curl descriptor %p(curl=%p) from subpool %p",
                      (void *)curlDescPt.get(),
                      (void *)(*curlDescPt).getCurl(), (void *)this);
      }
      else
      {
        CXX_LOG_TRACE("CurlDescPool::newCurlDesc(): Reusing curl descriptor %p(curl=%p) from subpool %p",
                      (void *)curlDescPt.get(),
                      (void *)(*curlDescPt).getCurl(), (void *)this);
      }
    }
    else
    {
      // trace
      CXX_LOG_TRACE("CurlDescPool::newCurlDesc(): Reusing curl descriptor %p(curl=%p) from subpool %p",
                    (void *)curlDescPt.get(),
                    (void *)(*curlDescPt).getCurl(), (void *)this);

      (*curlDescPt).reset(false);
    }
  }

  /**
   * Free curl descriptor back to the pool
   *
   * @param curlDescPt
   *   pointer to curl descriptor
   */
  void CurlDescPool::SubPool::freeCurlDesc(
                                          std::unique_ptr<CurlDesc> &curlDescPt)
  {
    // no-op if unique ptr is "null"
    if (curlDescPt)
    {
      // reset descriptor so that it could be reused next time around
      (*curlDescPt).reset();

      // trace
      CXX_LOG_TRACE("CurlDescPool::freeCurlDesc(): Free curl descriptor %p(curl=%p) back to subpool %p",
                    (void *)curlDescPt.get(),
                    (void *)(*curlDescPt).getCurl(), (void *)this);

      // free back to the pool
      std::lock_guard<std::mutex> mutexGuard(m_lockSubPool);

      // push back an empty unique ptr
      m_curlDescPool.emplace_back();

      // transfer descriptor to it
      m_curlDescPool.back() = std::move(curlDescPt);
    }
  }
}
}
