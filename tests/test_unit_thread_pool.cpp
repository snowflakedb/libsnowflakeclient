/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include "util/ByteArrayStreamBuf.hpp"
#include "utils/test_setup.h"
#include "util/ThreadPool.hpp"
#include <thread>

void test_thread_pool(void **unused)
{
  Snowflake::Client::Util::ThreadPool tp(4);
  const int threads = 10;

  int test_values[threads] = {0};
  int * it = test_values;
  for (int i=0; i<threads; i++)
  {
    tp.AddJob([=]{
      std::this_thread::sleep_for(std::chrono::seconds(4));
      *(it + i) = i;
    });
  }

  tp.WaitAll();

  for (int j=0; j<threads; j++)
  {
    assert_int_equal(j, test_values[j]);
  }

}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_thread_pool),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  return ret;
}
