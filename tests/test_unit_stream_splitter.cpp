/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "util/ByteArrayStreamBuf.hpp"
#include "utils/test_setup.h"
#include "util/ThreadPool.hpp"
#include "crypto/Cryptor.hpp"
#include "crypto/CipherStreamBuf.hpp"
#include <thread>
#include <cstring>
#include <sstream>
#include <fstream>
#include <iostream>

using Snowflake::Client::Crypto::CryptoIV;
using Snowflake::Client::Crypto::Cryptor;
using Snowflake::Client::Crypto::CryptoRandomDevice;
using Snowflake::Client::Crypto::CryptoOperation;
using Snowflake::Client::Crypto::CryptoKey;
using Snowflake::Client::Crypto::CipherStreamBuf;
using Snowflake::Client::Crypto::CipherIOStream;
using Snowflake::Client::Util::ByteArrayStreamBuf;

void getDataDirectory(std::string& dataDir)
{
  const std::string current_file = __FILE__;
  std::string testsDir = current_file.substr(0, current_file.find_last_of('/'));
  dataDir = testsDir + "/data/";
}

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

/**
 * Test encrypt input stream and then split into single part,
 * then append those parts and decrypt the data.
 */
void test_stream_splitter_appender(void **unused)
{
  std::string inputStr = "123456789012345678901234567890";
  std::stringstream inputData(inputStr);

  const int partSize = 4;
  const int threadNum = 4;
  const int splitParts = 8;

  CryptoIV iv;
  Cryptor::generateIV(iv, CryptoRandomDevice::DEV_RANDOM);
  CryptoKey key;
  Cryptor::generateKey(key, 128, CryptoRandomDevice::DEV_RANDOM);
  CipherIOStream encryptedInputStream(inputData, CryptoOperation::ENCRYPT, key, iv, 128);

  std::stringstream outputStream;
  CipherIOStream decryptOutputStream(outputStream, CryptoOperation::DECRYPT, key, iv, 128);

  Snowflake::Client::Util::StreamSplitter splitter(&encryptedInputStream, threadNum, partSize);
  Snowflake::Client::Util::StreamAppender appender(&decryptOutputStream, splitParts,
                                                   threadNum, partSize);

  char encryptedParts[splitParts][partSize];

  Snowflake::Client::Util::ThreadPool tp(threadNum);
  for (int i=0; i<splitParts; i++)
  {
    tp.AddJob([&] ()
              {
                  int threadIdx = tp.GetThreadIdx();
                  int partId;
                  Snowflake::Client::Util::ByteArrayStreamBuf * srcBuf =
                    splitter.FillAndGetBuf(threadIdx, partId);

                  memcpy(encryptedParts[partId], srcBuf->getDataBuffer(),
                         srcBuf->getSize());
              });
  }
  tp.WaitAll();

  for (int i=0; i<splitParts; i++)
  {
    tp.AddJob([&, i] ()
              {
                  int threadIdx = tp.GetThreadIdx();
                  ByteArrayStreamBuf * dstBuf = appender.GetBuffer(threadIdx);

                  memcpy(dstBuf->getDataBuffer(), encryptedParts[i],
                    sizeof(encryptedParts[i]));
                  dstBuf->updateSize(sizeof(encryptedParts[i]));

                  appender.WritePartToOutputStream(threadIdx, i);
              });
  }
  tp.WaitAll();

  assert_string_equal(inputStr.c_str(), outputStream.str().c_str());
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_byte_array_stream),
    cmocka_unit_test(test_stream_splitter_appender),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  return ret;
}
