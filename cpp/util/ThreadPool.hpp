//
// Created by hyu on 3/19/18.
//

#ifndef SNOWFLAKECLIENT_THREADPOOL_HPP
#define SNOWFLAKECLIENT_THREADPOOL_HPP

#include <pthread.h>
#include <vector>
#include <deque>

namespace Snowflake
{
namespace Client
{
namespace Util
{

class Task
{
public:
//  Task(TCLass::* obj_fn_ptr); // pass an object method pointer
  Task(void (*fn_ptr)(void *), void *arg); // pass a free function pointer
  ~Task();

  void operator()();

  void run();

private:
//  TClass* _obj_fn_ptr;
  void (*m_fn_ptr)(void *);

  void *m_arg;
};

enum PoolState
{
  STARTED, STOPPED
};

class ThreadPool
{
public:
  ThreadPool();

  ThreadPool(int pool_size);

  ~ThreadPool();

  int initialize_threadpool();

  int destroy_threadpool();

  void *execute_thread();

  int add_task(Task *task);

private:
  int m_pool_size;
  pthread_mutex_t m_task_mutex;
  pthread_cond_t m_task_cond_var;
  std::vector<pthread_t> m_threads; // storage for threads
  std::deque<Task *> m_tasks;
  volatile int m_pool_state;
};


}
}
}

#endif //SNOWFLAKECLIENT_THREADPOOL_HPP
