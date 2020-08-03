/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */
#include <sstream>
#include <fstream>
#include <crypto/Cryptor.hpp>
#include "utils/test_setup.h"
#include "crypto/CipherStreamBuf.hpp"

using Snowflake::Client::Crypto::CryptoIV;
using Snowflake::Client::Crypto::Cryptor;
using Snowflake::Client::Crypto::CryptoRandomDevice;
using Snowflake::Client::Crypto::CryptoOperation;
using Snowflake::Client::Crypto::CryptoKey;
using Snowflake::Client::Crypto::CipherStreamBuf;
using Snowflake::Client::Crypto::CipherIOStream;

void test_cipher_stream_core(int blockSize, const char * testData, int dataSize, CryptoRandomDevice randDev)
{
  std::stringstream ss(testData);

  CryptoIV iv;
  Cryptor::generateIV(iv, randDev);
  CryptoKey key;
  Cryptor::generateKey(key, 256, randDev);

  CipherIOStream encryptedInputStream(ss, CryptoOperation::ENCRYPT, key, iv, blockSize);

  char encryptedResult[100];
  encryptedInputStream.read(encryptedResult, sizeof(encryptedResult));
  size_t resultSize = encryptedInputStream.gcount();

  std::stringstream outputString;

  CipherIOStream decryptedOutputStream(outputString, CryptoOperation::DECRYPT,
                                       key, iv, blockSize);

  decryptedOutputStream.write(encryptedResult, resultSize);
  decryptedOutputStream.flush();

  assert_memory_equal(testData, outputString.str().c_str(), dataSize);
}

void test_cipher_stream_buf_zero(void **unused)
{
  const char * testData = "123456789123456789123456789\0";
  test_cipher_stream_core(16, testData, strlen(testData), CryptoRandomDevice::DEV_RANDOM);
  test_cipher_stream_core(16, testData, strlen(testData), CryptoRandomDevice::DEV_URANDOM);
}

void test_cipher_stream_buf_one(void **unused)
{
  const char * testData = "123456789123456789123456789\0";
  test_cipher_stream_core(512, testData, strlen(testData), CryptoRandomDevice::DEV_RANDOM);
  test_cipher_stream_core(512, testData, strlen(testData), CryptoRandomDevice::DEV_URANDOM);
}

void test_cipher_stream_buf_two(void **unused)
{
  const char * testData = "0123456789012345\0";
  test_cipher_stream_core(16, testData, strlen(testData), CryptoRandomDevice::DEV_RANDOM);
  test_cipher_stream_core(16, testData, strlen(testData), CryptoRandomDevice::DEV_URANDOM);
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
