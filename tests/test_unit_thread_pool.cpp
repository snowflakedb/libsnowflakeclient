/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "utils/test_setup.h"
#include "util/ThreadPool.hpp"
#include <thread>

void test_thread_pool(void **unused)
{
  Snowflake::Client::Util::ThreadPool tp(4);

  for (int i=0; i<10; i++)
  {
    tp.AddJob([=]{
      std::this_thread::sleep_for(std::chrono::seconds(4));
    });
  }

  tp.WaitAll();
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_thread_pool),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  return ret;
}
