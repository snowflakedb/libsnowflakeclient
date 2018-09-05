/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */
#include <numeric>
#include <iterator>
#include <algorithm>
#include <memory>
#include <iostream>
#include <fstream>
#include <util/Base64.hpp>
#include "utils/TestSetup.hpp"
#include "utils/test_setup.h"

using Snowflake::Client::Util::Base64;

/**
 * Interface of Base64 encoding/decoding test case
 */
struct Base64TestcaseI
{
  virtual const void *getEncode() = 0;
  virtual size_t getEncodeLen() = 0;
  virtual const void *getDecode() = 0;
  virtual size_t getDecodeLen() = 0;
};

/**
 * Implementation of Base64 test case using string as representation
 */
struct Base64TestcaseStr : Base64TestcaseI
{
  inline const void *getEncode()
  {
    return encode_.c_str();
  }

  inline size_t getEncodeLen()
  {
    return encode_.length();
  }

  inline const void *getDecode()
  {
    return decode_.c_str();
  }

  inline size_t getDecodeLen()
  {
    return decode_.length();
  }

  Base64TestcaseStr(std::string decode, std::string encode) :
    decode_(decode), encode_(encode) {}

private:
  std::string decode_;
  std::string encode_;

};

/**
 * Implementation of Base64 test cases that loads in files
 */
struct Base64TestcaseFile : Base64TestcaseI
{
  inline const void *getEncode()
  {
    return encode_.data();
  }

  inline size_t getEncodeLen() {
    return encode_.size();
  }

  inline const void *getDecode() {
    return decode_.data();
  }

  inline size_t getDecodeLen() {
    return decode_.size();
  }

  Base64TestcaseFile(std::string decode_file_name, std::string encode_file_name) {
    decode_ = readAllBytes(decode_file_name);
    encode_ = readAllBytes(encode_file_name);
  }

private:
  std::vector<char> decode_;
  std::vector<char> encode_;

  static std::vector<char> readAllBytes(std::string fileName)
  {
    std::string full_file_path = TestSetup::getDataDir();
    full_file_path += fileName;

    std::ifstream ifs(full_file_path, std::ios::binary);
    std::vector<char> result;
    result.reserve(1024);
    result.assign(std::istreambuf_iterator<char>(ifs),
                  std::istreambuf_iterator<char>());
    return result;
  }
};

/**
 * Test the encode/decode function of base 64 
 * @param unused
 */
void test_base64_coding(void **unused)
{
  char result[1024];

  std::shared_ptr<Base64TestcaseI> testcases[] = {
    std::make_shared<Base64TestcaseStr>("eoqwiroiqnweropiqnweorinqwoepir", "ZW9xd2lyb2lxbndlcm9waXFud2VvcmlucXdvZXBpcg=="),
    std::make_shared<Base64TestcaseStr>("32421bjbsaf", "MzI0MjFiamJzYWY="),
    std::make_shared<Base64TestcaseStr>("nfainwerq", "bmZhaW53ZXJx"),
    std::make_shared<Base64TestcaseStr>("Snowflake is fxxkin great!!!", "U25vd2ZsYWtlIGlzIGZ4eGtpbiBncmVhdCEhIQ=="),
    std::make_shared<Base64TestcaseStr>("asdfwerqewrsfasxc2312saDSFADF", "YXNkZndlcnFld3JzZmFzeGMyMzEyc2FEU0ZBREY="),
    std::make_shared<Base64TestcaseFile>("decode", "encode"),
  };

  for (auto &testcase : testcases) {
    const void *encode = testcase->getEncode();
    size_t encode_len = testcase->getEncodeLen();
    const void *decode = testcase->getDecode();
    size_t decode_len = testcase->getDecodeLen();

    // test encoding is correct
    size_t actualEncodeSize = Base64::encode(decode, decode_len, result);
    assert_int_equal(actualEncodeSize, encode_len);
    assert_memory_equal(result, encode, encode_len);

    // test decoding is correct
    size_t actualDecodeSize = Base64::decode(encode, encode_len, result);
    assert_int_equal(actualDecodeSize, decode_len);
    assert_memory_equal(result, decode, decode_len);
  }
}

/**
 * Test the encode/decode function of base 64 url
 * @param unused 
 */
void test_base64_url_coding(void **unused)
{
  char result[1024];

  std::shared_ptr<Base64TestcaseI> testcases[] = {
    std::make_shared<Base64TestcaseStr>("eoqwiroiqnweropiqnweorinqwoepir", "ZW9xd2lyb2lxbndlcm9waXFud2VvcmlucXdvZXBpcg=="),
    std::make_shared<Base64TestcaseStr>("32421bjbsaf", "MzI0MjFiamJzYWY="),
    std::make_shared<Base64TestcaseStr>("nfainwerq", "bmZhaW53ZXJx"),
    std::make_shared<Base64TestcaseStr>("Snowflake is fxxkin great!!!", "U25vd2ZsYWtlIGlzIGZ4eGtpbiBncmVhdCEhIQ=="),
    std::make_shared<Base64TestcaseStr>("asdfwerqewrsfasxc2312saDSFADF", "YXNkZndlcnFld3JzZmFzeGMyMzEyc2FEU0ZBREY="),
    std::make_shared<Base64TestcaseFile>("decode", "encode64"),
  };

  for (auto &testcase : testcases) {
    const void *encode = testcase->getEncode();
    size_t encode_len = testcase->getEncodeLen();
    const void *decode = testcase->getDecode();
    size_t decode_len = testcase->getDecodeLen();

    // test encoding is correct
    size_t actualEncodeSize = Base64::encodeUrl(decode, decode_len, result);
    assert_int_equal(actualEncodeSize, encode_len);
    assert_memory_equal(result, encode, encode_len);

    // test decoding is correct
    size_t actualDecodeSize = Base64::decodeUrl(encode, encode_len, result);
    assert_int_equal(actualDecodeSize, decode_len);
    assert_memory_equal(result, decode, decode_len);
  }
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_base64_coding),
    cmocka_unit_test(test_base64_url_coding),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
