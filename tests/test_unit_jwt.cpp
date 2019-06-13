/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include "utils/TestSetup.hpp"
#include "utils/test_setup.h"
#include <jwt/Jwt.hpp>
#include "openssl/rsa.h"
#include <openssl/pem.h>

using Snowflake::Client::Jwt::IHeader;
using Snowflake::Client::Jwt::IClaimSet;
using Snowflake::Client::Jwt::AlgorithmType;
using Snowflake::Client::Jwt::JWTObject;
using IHeaderUptr = std::unique_ptr<IHeader>;
using IClaimSetUptr = std::unique_ptr<IClaimSet>;
using IHeaderSptr = std::shared_ptr<IHeader>;
using IClaimSetSptr = std::shared_ptr<IClaimSet>;

static EVP_PKEY *generate_key()
{
  EVP_PKEY *key = nullptr;

  std::unique_ptr<EVP_PKEY_CTX, std::function<void(EVP_PKEY_CTX *)>>
    kct{EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr), [](EVP_PKEY_CTX *ctx) { EVP_PKEY_CTX_free(ctx); }};

  if (EVP_PKEY_keygen_init(kct.get()) <= 0) return nullptr;

  if (EVP_PKEY_CTX_set_rsa_keygen_bits(kct.get(), 2048) <= 0) return nullptr;

  if (EVP_PKEY_keygen(kct.get(), &key) <= 0) return nullptr;

  return key;
}

static EVP_PKEY *extract_pub_key(EVP_PKEY *key_pair)
{
  std::unique_ptr<BIO, std::function<void(BIO *)>> mem
    {BIO_new(BIO_s_mem()), [](BIO *bio) { BIO_free(bio); }};

  if (mem == nullptr) return nullptr;

  if (!PEM_write_bio_PUBKEY(mem.get(), key_pair)) return nullptr;

  EVP_PKEY *pub_key = PEM_read_bio_PUBKEY(mem.get(), nullptr, nullptr, nullptr);

  return pub_key;
}

void test_header(void **)
{
  IHeaderUptr header(IHeader::buildHeader());
  header->setAlgorithm(AlgorithmType::RS256);

  assert_true(header->getAlgorithmType() == AlgorithmType::RS256);

  std::string header_str = header->serialize();

  IHeaderUptr header_cpy(IHeader::parseHeader(header_str));

  assert_true(header_str.back() != '=');

  assert_true(header->getAlgorithmType() == header_cpy->getAlgorithmType());
}

void test_claim_set(void **)
{
  // Basic get/set tests
  // Numbers
  IClaimSetUptr claim_set(IClaimSet::buildClaimSet());
  assert_false(claim_set->containsClaim("number"));
  claim_set->addClaim("number", 200L);
  assert_true(claim_set->containsClaim("number"));
  assert_int_equal(claim_set->getClaimInLong("number"), 200L);
  claim_set->addClaim("number", 300);
  assert_int_equal(claim_set->getClaimInLong("number"), 300L);

  // Strings
  claim_set->addClaim("string", "value");
  assert_true(claim_set->containsClaim("string"));
  assert_string_equal(claim_set->getClaimInString("string").c_str(), "value");
  claim_set->removeClaim("string");
  assert_false(claim_set->containsClaim("string"));
  claim_set->addClaim("string", "new_value");
  assert_string_equal(claim_set->getClaimInString("string").c_str(), "new_value");

  // Serialization
  std::string claim_set_str = claim_set->serialize();
  IClaimSetUptr claim_set_cpy(IClaimSet::parseClaimset(claim_set_str));

  assert_string_equal(claim_set->getClaimInString("string").c_str(),
                      claim_set_cpy->getClaimInString("string").c_str());

  assert_int_equal(claim_set->getClaimInLong("number"),
                   claim_set_cpy->getClaimInLong("number"));

}

void test_sign_verify(void **)
{
  JWTObject jwt;

  IHeaderSptr header(IHeader::buildHeader());
  IClaimSetSptr claim_set(IClaimSet::buildClaimSet());
  header->setAlgorithm(AlgorithmType::RS256);
  claim_set->addClaim("string", "value");
  claim_set->addClaim("number", 200L);

  jwt.setHeader(header);
  jwt.setClaimSet(claim_set);

  std::unique_ptr<EVP_PKEY, std::function<void(EVP_PKEY *)>> key
    {generate_key(), [](EVP_PKEY *k) { EVP_PKEY_free(k); }};

  if (key == nullptr)
  {
    assert_true(false);
    return;
  }

  std::unique_ptr<EVP_PKEY, std::function<void(EVP_PKEY *)>> pub_key
    {extract_pub_key(key.get()), [](EVP_PKEY *k) { EVP_PKEY_free(k); }};

  std::string result = jwt.serialize(key.get());

  assert_string_not_equal(result.c_str(), "");

  assert_true(jwt.verify(pub_key.get(), true));
}

int main()
{
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_claim_set),
    cmocka_unit_test(test_header),
    cmocka_unit_test(test_sign_verify),
  };
  return cmocka_run_group_tests(tests, nullptr, nullptr);
}
