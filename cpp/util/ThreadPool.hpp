/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_THREADPOOL_HPP
#define SNOWFLAKECLIENT_THREADPOOL_HPP

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include <vector>
#include <deque>
#include <functional>
#include <atomic>
#include <cstring>
#include "snowflake/platform.h"
#include "snowflake/SnowflakeTransferException.hpp"
#include "../logger/SFLogger.hpp"
#include "ByteArrayStreamBuf.hpp"

namespace Snowflake
{
namespace Client
{
namespace Util
{

/**
 * A naive implementation of thread pool
 */
class ThreadPool {
private:

  /// thread count
  const unsigned int threadCount;

  /// threads vector
  std::vector<SF_THREAD_HANDLE > threads;

  /// task queue
  std::deque<std::function<void(void)>> queue;

  /// threads not sleeping (initialize to thread count)
  unsigned int busyThreads;

  /// true if thread pool is going to be shutdown
  bool finished;

  /// condition variable that threads wait on task queue
  SF_CONDITION_HANDLE job_available_var;

  /// cv that main threads wait on all worker to finish task queue
  SF_CONDITION_HANDLE wait_var;

  /// queue mutex
  SF_CRITICAL_SECTION_HANDLE queue_mutex;

#ifdef _WIN32
  DWORD key;
#else
  /// key to get thread local object
  pthread_key_t key;
#endif

  struct ThreadCtx
  {
    ThreadPool *tp;
#ifdef _WIN32
    DWORD *key;
#else
    pthread_key_t *key;
#endif
    int threadIdx;
  };

  /**
   * Wrapper class that is passed to thread
   */
  static void *TaskWrapper(void *arg)
  {
    ThreadCtx* ctx = reinterpret_cast<ThreadCtx *>(arg);
#ifdef _WIN32
    LPVOID lpData = new int(ctx->threadIdx);
    TlsSetValue(*ctx->key, lpData);
#else
    pthread_setspecific(*ctx->key, &ctx->threadIdx);
#endif
    ctx->tp->execute_thread();
    delete ctx;
#ifdef _WIN32
    delete lpData;
#endif
    return nullptr;
  }

  /**
   *  Get the next job; pop the first item in the queue,
   *  otherwise wait for a signal from the main thread.
   */
  void execute_thread() {
    std::function<void(void)> res;
    while(true)
    {
      _critical_section_lock(&queue_mutex);

      busyThreads --;
      if (busyThreads == 0)
        _cond_signal(&wait_var);

      // Wait for a job if we don't have any.
      while (queue.empty() && !finished)
      {
        _cond_wait(&job_available_var, &queue_mutex);
      }

      // Get job from the queue
      if (finished)
      {
        _critical_section_unlock(&queue_mutex);
        break;
      }
      else
      {
        busyThreads ++;
        res = queue.front();
        queue.pop_front();
        _critical_section_unlock(&queue_mutex);
        res();
      }
    }
  }

public:
  ThreadPool(unsigned int threadNum)
    : finished( false )
    , threadCount (threadNum)
    , busyThreads (threadNum)
  {
    _critical_section_init(&queue_mutex);
    _cond_init(&job_available_var);
    _cond_init(&wait_var);

#ifdef _WIN32
    key = TlsAlloc();
    if (key == TLS_OUT_OF_INDEXES)
    {
      CXX_LOG_ERROR("Thread pool out of TLS index");
      throw SnowflakeTransferException(TransferError::INTERNAL_ERROR,
                                       "Out of TLS index in the thread pool");
    }
#else
    int err = pthread_key_create(&key, NULL);
    if (err)
    {
      CXX_LOG_ERROR("Thread pool creating key failed with error: %s", strerror(err));
      throw SnowflakeTransferException(TransferError::INTERNAL_ERROR,
                                       "Thread context fail to initialize");
    }
#endif

    for( unsigned i = 0; i < threadCount; ++i )
    {
      SF_THREAD_HANDLE tid;
      ThreadCtx * threadCtx = new ThreadCtx;
      threadCtx->key = &key;
      threadCtx->tp = this;
      threadCtx->threadIdx = i;

      _thread_init(&tid, TaskWrapper, (void *)threadCtx);
      threads.push_back(tid);
    }
  }

  int GetThreadIdx()
  {
#ifdef _WIN32
    return *(int *)TlsGetValue(key);
#else
    return *(int *)pthread_getspecific(key);
#endif
  }

  /**
   *  JoinAll on deconstruction
   */
  ~ThreadPool() {
    JoinAll();
#ifdef _WIN32
    if (key != TLS_OUT_OF_INDEXES)
    {
      TlsFree(key);
    }
#else
    pthread_key_delete(key);
#endif
    _critical_section_term(&queue_mutex);
    _cond_term(&job_available_var);
    _cond_term(&wait_var);
  }

  /**
   *  Add a new job to the pool. If there are no jobs in the queue,
   *  a thread is woken up to take the job. If all threads are busy,
   *  the job is added to the end of the queue.
   */
  void AddJob( std::function<void(void)> job ) {
    _critical_section_lock(&queue_mutex);
    queue.emplace_back( job );
    _cond_signal(&job_available_var);
    _critical_section_unlock(&queue_mutex);
  }

  /**
   *  Join with all threads. Block until all threads have completed.
   *  Params: WaitForAll: If true, will wait for the queue to empty
   *          before joining with threads. If false, will complete
   *          current jobs, then inform the threads to exit.
   *  The queue will be empty after this call, and the threads will
   *  be done. After invoking `ThreadPool::JoinAll`, the pool can no
   *  longer be used. If you need the pool to exist past completion
   *  of jobs, look to use `ThreadPool::WaitAll`.
   */
  void JoinAll() {
    _critical_section_lock(&queue_mutex);
    finished = true;
    _cond_broadcast(&job_available_var);
    _critical_section_unlock(&queue_mutex);

    for (auto &x : threads)
      _thread_join(x);
  }

  /**
   *  Wait for the pool to empty before continuing.
   *  This does not call `std::thread::join`, it only waits until
   *  all jobs have finshed executing.
   */
  void WaitAll() {
    _critical_section_lock(&queue_mutex);
    while(busyThreads > 0 || !queue.empty())
    {
      _cond_wait(&wait_var, &queue_mutex);
    }
    _critical_section_unlock(&queue_mutex);
  }
};


}
}
}

#endif //SNOWFLAKECLIENT_THREADPOOL_HPP
