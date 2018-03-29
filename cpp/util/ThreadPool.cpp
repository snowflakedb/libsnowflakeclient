/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "ThreadPool.hpp"


namespace Snowflake
{
namespace Client
{
namespace Util
{

ThreadPool::ThreadPool() : m_pool_size(2)
{
}

ThreadPool::ThreadPool(int pool_size) : m_pool_size(pool_size)
{
  // init mutex
  pthread_mutex_init(&m_task_mutex, NULL);
  pthread_cond_init(&m_task_cond_var, NULL);
}

ThreadPool::~ThreadPool()
{
  // Release resources
  if (m_pool_state != PoolState::STOPPED)
  {
    destroy_threadpool();
  }
  pthread_mutex_destroy(&m_task_mutex);
  pthread_cond_destroy(&m_task_cond_var);
}

// We can't pass a member function to pthread_create.
// So created the wrapper function that calls the member function
// we want to run in the thread.
extern "C"
void *start_thread(void *arg)
{
  ThreadPool *tp = (ThreadPool *) arg;
  tp->execute_thread();
  return NULL;
}

int ThreadPool::initialize_threadpool()
{
  // TODO: COnsider lazy loading threads instead of creating all at once
  m_pool_state = PoolState::STARTED;
  int ret = -1;
  for (int i = 0; i < m_pool_size; i++)
  {
    pthread_t tid;
    ret = pthread_create(&tid, NULL, start_thread, (void *) this);
    if (ret != 0)
    {
      return -1;
    }
    m_threads.push_back(tid);
  }

  return 0;
}

int ThreadPool::destroy_threadpool()
{
  // Note: this is not for synchronization, its for thread communication!
  // destroy_threadpool() will only be called from the main thread, yet
  // the modified m_pool_state may not show up to other threads until its
  // modified in a lock!
  pthread_mutex_lock(&m_task_mutex);
  m_pool_state = PoolState::STOPPED;
  pthread_mutex_unlock(&m_task_mutex);
  pthread_cond_broadcast(&m_task_cond_var);

  int ret = -1;
  for (int i = 0; i < m_pool_size; i++)
  {
    void *result;
    ret = pthread_join(m_threads[i], &result);
    pthread_cond_broadcast(&m_task_cond_var);
  }
  return 0;
}

void *ThreadPool::execute_thread()
{
  Task *task = NULL;
  while (true)
  {
    // Try to pick a task
    pthread_mutex_lock(&m_task_mutex);

    // We need to put pthread_cond_wait in a loop for two reasons:
    // 1. There can be spurious wakeups (due to signal/ENITR)
    // 2. When mutex is released for waiting, another thread can be waken up
    //    from a signal/broadcast and that thread can mess up the condition.
    //    So when the current thread wakes up the condition may no longer be
    //    actually true!
    while (m_pool_state != PoolState::STOPPED && m_tasks.empty())
    {
      // Wait until there is a task in the queue
      // Unlock mutex while wait, then lock it back when signaled
      pthread_cond_wait(&m_task_cond_var, &m_task_mutex);
    }

    // If the thread was woken up to notify process shutdown, return from here
    if (m_pool_state == PoolState::STOPPED)
    {
      pthread_mutex_unlock(&m_task_mutex);
      pthread_exit(NULL);
    }

    task = m_tasks.front();
    m_tasks.pop_front();
    pthread_mutex_unlock(&m_task_mutex);

    // execute the task
    (*task)(); // could also do task->run(arg);
    //cout << "Done executing thread " << pthread_self() << endl;
    delete task;
  }
  return NULL;
}

int ThreadPool::add_task(Task *task)
{
  pthread_mutex_lock(&m_task_mutex);

  // TODO: put a limit on how many tasks can be added at most
  m_tasks.push_back(task);

  pthread_cond_signal(&m_task_cond_var);

  pthread_mutex_unlock(&m_task_mutex);

  return 0;
}

Task::Task(void (*fn_ptr)(void *), void *arg) : m_fn_ptr(fn_ptr),
                                                m_arg(arg)
{
}

Task::~Task()
{
}

void Task::operator()()
{
  (*m_fn_ptr)(m_arg);
  if (m_arg != NULL)
  {
    delete m_arg;
  }
}

void Task::run()
{
  (*m_fn_ptr)(m_arg);
}

}
}
}
