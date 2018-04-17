/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */
#include <sstream>
#include <crypto/Cryptor.hpp>
#include "utils/test_setup.h"
#include "crypto/CipherStreamBuf.hpp"

using Snowflake::Client::Crypto::CryptoIV;
using Snowflake::Client::Crypto::Cryptor;
using Snowflake::Client::Crypto::CryptoRandomDevice;
using Snowflake::Client::Crypto::CryptoOperation;
using Snowflake::Client::Crypto::CryptoKey;
using Snowflake::Client::Crypto::CipherStreamBuf;

void test_cipher_stream_core(int blockSize, const char * testData, int dataSize)
{
  fprintf(stderr, "-------------Start test---------\n");
  std::stringstream ss(testData);

  CryptoIV iv;
  Cryptor::generateIV(iv, CryptoRandomDevice::DEV_RANDOM);
  CryptoKey key;
  Cryptor::generateKey(key, 256, CryptoRandomDevice::DEV_RANDOM);

  CipherStreamBuf encryptBuf(ss.rdbuf(),
                             CryptoOperation::ENCRYPT, key, iv, blockSize);

  CipherStreamBuf decryptBuf(&encryptBuf,
                             CryptoOperation::DECRYPT, key, iv, blockSize);

  std::basic_iostream<char> decryptStream(&decryptBuf);

  char result[dataSize];
  decryptStream.read(result, dataSize);

  assert_memory_equal(testData, result, dataSize);
}

void test_cipher_stream_buf_zero(void **unused)
{
  const char * testData = "123456789123456789123456789\0";
  test_cipher_stream_core(16, testData, strlen(testData));
}

void test_cipher_stream_buf_one(void **unused)
{
  const char * testData = "123456789123456789123456789\0";
  test_cipher_stream_core(512, testData, strlen(testData));
}

void test_cipher_stream_buf_two(void **unused)
{
  const char * testData = "0123456789012345\0";
  test_cipher_stream_core(16, testData, strlen(testData));
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_cipher_stream_buf_zero),
    cmocka_unit_test(test_cipher_stream_buf_one),
    cmocka_unit_test(test_cipher_stream_buf_two),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  return ret;
}
