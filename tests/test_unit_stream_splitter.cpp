/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "util/StreamSplitter.hpp"
#include "utils/test_setup.h"
#include "util/ThreadPool.hpp"
#include <thread>
#include <cstring>
#include <sstream>

void test_byte_array_stream(void **unused)
{
  char data[] = "0123456789";
  Snowflake::Client::Util::ByteArrayStreamBuf buf(sizeof(data));
  memcpy(buf.getDataBuffer(), data, sizeof(data));

  std::basic_iostream<char> byteArrayStream(&buf);

  char result[sizeof(data)];

  byteArrayStream.read(result, sizeof(data));

  assert_memory_equal(result, data, sizeof(data));
}

void test_stream_splitter(void **unused)
{
  const int splitParts = 3;
  const int partSize = 4;

  std::stringstream ss("012345678901");
  Snowflake::Client::Util::StreamSplitter splitter(&ss, splitParts, partSize);
  Snowflake::Client::Util::ThreadPool tp(4);

  char result[splitParts][partSize];
  for (int i=0; i<splitParts; i++)
  {
    tp.AddJob([&, i]
              {
                  Snowflake::Client::Util::ByteArrayStreamBuf *buf =
                    splitter.getNextSplitPart();

                  std::basic_iostream<char> byteStreamArray(buf);
                  byteStreamArray.read(result[i], partSize);
                  splitter.markDone(buf);
              });
  }

  tp.WaitAll();

  assert_memory_equal(result[0], "0123", partSize);
  assert_memory_equal(result[1], "4567", partSize);
  assert_memory_equal(result[2], "8901", partSize);
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_byte_array_stream),
    cmocka_unit_test(test_stream_splitter)
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  return ret;
}
