#include <iterator>
#include <algorithm>
#include <memory>
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <util/Base64.hpp>
#include "utils/TestSetup.hpp"
#include "utils/test_setup.h"

using Snowflake::Client::Util::Base64;

/**
 * Interface of Base64 encoding/decoding test case
 */
struct Base64TestcaseI
{
public:
  virtual std::vector<char> getDecodeVec() = 0;


  virtual std::string getEncodeStr() = 0;

  virtual std::string getEncodeUrlStr()
  {
    auto encode = this->getEncodeStr();
    std::replace(encode.begin(), encode.end(), '+', '-');
    std::replace(encode.begin(), encode.end(), '/', '_');
    return encode;
  }

  virtual std::string getEncodeStrNoPadding() = 0;

  virtual std::string getEncodeUrlStrNoPadding() = 0;

protected:
  Base64TestcaseI() = default;
  virtual ~Base64TestcaseI() {};
};

/**
 * Helper template for base 64 test case
 */
struct AbstractBase64TestCase : Base64TestcaseI
{

public:
   virtual std::string getEncodeUrlStr() final
  {
    auto encode = this->getEncodeStr();
    std::replace(encode.begin(), encode.end(), '+', '-');
    std::replace(encode.begin(), encode.end(), '/', '_');
    return encode;
  }

  virtual std::string getEncodeStrNoPadding() final
  {
    return truncatePadding(this->getEncodeStr());
  }

  virtual std::string getEncodeUrlStrNoPadding() final
  {
    auto encode = this->getEncodeStrNoPadding();
    std::replace(encode.begin(), encode.end(), '+', '-');
    std::replace(encode.begin(), encode.end(), '/', '_');
    return encode;
  }
protected:

  virtual ~AbstractBase64TestCase() {};


  static std::string truncatePadding(const std::string &origin)
  {
    if (origin.length() == 0)
    {
      return origin;
    }
    size_t end = origin.length() - 1;
    while (origin[end] == '=') end--;
    return std::string(origin.begin(), origin.begin() + end + 1);
  }

  AbstractBase64TestCase() = default;

};

/**
 * Implementation of Base64 test case using string as representation
 */
struct Base64TestcaseStr : AbstractBase64TestCase
{
  inline std::vector<char> getDecodeVec()
  {
    return std::vector<char>(decode_.begin(), decode_.end());
  }

  inline std::string getEncodeStr()
  {
    return encode_;
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
struct Base64TestcaseFile : AbstractBase64TestCase
{
  inline std::vector<char> getDecodeVec()
  {
    return decode_;
  }

  inline std::string getEncodeStr()
  {
    return std::string(encode_.begin(), encode_.end());
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

static std::shared_ptr<Base64TestcaseI> testcasesPadding[] = {
  std::make_shared<Base64TestcaseStr>("eoqwiroiqnweropiqnweorinqwoepir",
                                      "ZW9xd2lyb2lxbndlcm9waXFud2VvcmlucXdvZXBpcg=="),
  std::make_shared<Base64TestcaseStr>("32421bjbsaf", "MzI0MjFiamJzYWY="),
  std::make_shared<Base64TestcaseStr>("nfainwerq", "bmZhaW53ZXJx"),
  std::make_shared<Base64TestcaseStr>("Snowflake is fxxkin great!!!", "U25vd2ZsYWtlIGlzIGZ4eGtpbiBncmVhdCEhIQ=="),
  std::make_shared<Base64TestcaseStr>("asdfwerqewrsfasxc2312saDSFADF", "YXNkZndlcnFld3JzZmFzeGMyMzEyc2FEU0ZBREY="),
  std::make_shared<Base64TestcaseFile>("decode", "encode"),
  std::make_shared<Base64TestcaseFile>("decode2", "encode2"),
};

/**
 * Test the encode/decode function of base 64 
 * @param unused
 */
void test_base64_coding(void **unused)
{
  char result[1024];

  for (auto &testcase : testcasesPadding)
  {
    auto encodeExpect = testcase->getEncodeStr();
    auto decodeExpect = testcase->getDecodeVec();

    // test encoding is correct
    size_t actualEncodeSize = Base64::encode(decodeExpect.data(), decodeExpect.size(), result);
    assert_int_equal(actualEncodeSize, encodeExpect.size());
    assert_memory_equal(result, encodeExpect.data(), encodeExpect.size());

    // test decoding is correct
    size_t actualDecodeSize = Base64::decode(encodeExpect.data(), encodeExpect.size(), result, sizeof(result));
    assert_int_equal(actualDecodeSize, decodeExpect.size());
    assert_memory_equal(result, decodeExpect.data(), decodeExpect.size());
  }
}

/**
 * Verify that decode/decodeUrl refuse to write past the destination buffer.
 * @param unused
 */
void test_base64_decode_bounds(void **)
{
  // "AAAA" (4 base64 chars) decodes to exactly 3 bytes.
  const char encoded[] = "AAAAAAAAAAAA"; // 12 chars -> 9 decoded bytes
  // NOTE: do not name this 'small' - it is a predefined macro on Windows
  // (rpcndr.h defines `#define small char`).
  char dstBuf[4];

  // Fits exactly: 4 chars -> 3 bytes into a 3-byte buffer.
  assert_int_equal(Base64::decode(encoded, 4, dstBuf, 3), 3);
  // Would overflow: 12 chars -> 9 bytes into a 4-byte buffer -> rejected.
  assert_int_equal(Base64::decode(encoded, 12, dstBuf, sizeof(dstBuf)),
                   static_cast<size_t>(-1L));
  // Zero capacity always rejects non-empty output.
  assert_int_equal(Base64::decode(encoded, 4, dstBuf, 0),
                   static_cast<size_t>(-1L));
  // Same guarantees for the url-safe variant.
  assert_int_equal(Base64::decodeUrl(encoded, 12, dstBuf, sizeof(dstBuf)),
                   static_cast<size_t>(-1L));

  // Simulate the IV sink: a 16-byte IV buffer must reject an oversized value.
  char iv[16];
  // 512 base64 chars -> 384 decoded bytes; must not touch the 16-byte buffer.
  std::string oversized(512, 'A');
  assert_int_equal(Base64::decode(oversized.c_str(), oversized.size(), iv, sizeof(iv)),
                   static_cast<size_t>(-1L));
  // A correctly-sized 16-byte IV (24 base64 chars with padding) is accepted.
  const char ivEncoded[] = "AAAAAAAAAAAAAAAAAAAAAA==";
  assert_int_equal(Base64::decode(ivEncoded, sizeof(ivEncoded) - 1, iv, sizeof(iv)), 16);
}

/**
 * Test the encode/decode cpp style function of base 64
 * @param unused
 */
void test_base64_cpp_coding(void **)
{
  for (auto &testcase : testcasesPadding)
  {
    auto encodeExpect = testcase->getEncodeStr();
    auto decodeExpect = testcase->getDecodeVec();

    auto encodeActual = Base64::encodePadding(decodeExpect);
    auto decodeActual = Base64::decodePadding(encodeExpect);

    assert_string_equal(encodeActual.c_str(), encodeExpect.c_str());
    assert_int_equal(decodeActual.size(), decodeExpect.size());
    assert_memory_equal(decodeActual.data(), decodeExpect.data(), decodeActual.size());
  }
}

/**
 * Test the encode/decode cpp style function of base 64 url with no padding
 * @param unused
 */
void test_base64_url_cpp_coding(void **)
{

  for (auto &testcase : testcasesPadding)
  {
    auto encodeExpect = testcase->getEncodeUrlStrNoPadding();
    auto decodeExpect = testcase->getDecodeVec();

    auto encodeActual = Base64::encodeURLNoPadding(decodeExpect);
    auto decodeActual = Base64::decodeURLNoPadding(encodeExpect);

    if (encodeExpect == "sWdnRusMQR3rcW-Cw29jw2RPd8rzQ-e19DexvZV_Ov848rmXaAssU5I-cTYM6CF9jlBzoiE9DrWQKl4FFQysrO_9hruBY9VJlZl0zmrCyIk-W6-sSgDLqtWTrWMmGkS6iT-eRV-Phv_f3aiin01Kx8UB7fP47jjd-KOvRrAH_f1Ui_DPKnKW1igtO9BLlD034ozKtCTkP4kh3ZXQSgN3Po1U97tGkEBNoSMZzmrs0Q8mzUy0AJ7zSEfO7cui1RjB0Iej_xuC_8oagKEoVjjJNPQidDAkZpHh2rMObrOS8CxQwF4ea3mgLOuhlq983xSBnUnu12vwWHE5WBQDrOlx_g==")
    {
      std::ofstream output_file("./example.txt");
      for (int i = 0; i < decodeActual.size(); i++)
        output_file << decodeActual[i];
    }

//    assert_string_equal(encodeActual.c_str(), encodeExpect.c_str());
//    assert_int_equal(decodeActual.size(), decodeExpect.size());
//    assert_memory_equal(decodeActual.data(), decodeExpect.data(), decodeActual.size());
  }
}

/**
 * Test the encode/decode function of base 64 url
 * @param unused 
 */
void test_base64_url_coding(void **unused)
{
  char result[1024];

  for (auto &testcase : testcasesPadding)
  {
    auto encodeExpect = testcase->getEncodeUrlStr();
    auto decodeExpect = testcase->getDecodeVec();

    // test encoding is correct
    size_t actualEncodeSize = Base64::encodeUrl(decodeExpect.data(), decodeExpect.size(), result);
    assert_int_equal(actualEncodeSize, encodeExpect.size());
    assert_memory_equal(result, encodeExpect.data(), encodeExpect.size());

    // test decoding is correct
    size_t actualDecodeSize = Base64::decodeUrl(encodeExpect.data(), encodeExpect.size(), result, sizeof(result));
    assert_int_equal(actualDecodeSize, decodeExpect.size());
    assert_memory_equal(result, decodeExpect.data(), decodeExpect.size());
  }
}

int main(void) {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_base64_coding),
    cmocka_unit_test(test_base64_decode_bounds),
    cmocka_unit_test(test_base64_cpp_coding),
    cmocka_unit_test(test_base64_url_coding),
    cmocka_unit_test(test_base64_url_cpp_coding),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
