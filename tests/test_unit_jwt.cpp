/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "utils/TestSetup.hpp"
#include "utils/test_setup.h"
#include <jwt/Jwt.hpp>

using Snowflake::Jwt::IHeader;
using Snowflake::Jwt::IClaimSet;
using Snowflake::Jwt::AlgorithmType;
using IHeaderUptr = std::unique_ptr<IHeader>;
using IClaimSetUptr = std::unique_ptr<IClaimSet>;

void test_header(void **unused)
{
  IHeaderUptr header(IHeader::buildHeader());
  header->setAlgorithm(AlgorithmType::RS256);

  assert_true(header->getAlgorithmType() == AlgorithmType::RS256);

  std::string header_str = header->serialize();

  IHeaderUptr header_cpy(IHeader::parseHeader(header_str));

  assert_true(header_str.back() != '=');

  assert_true(header->getAlgorithmType() == header_cpy->getAlgorithmType());
}

void test_claim_set(void **unused)
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

void test_sign_verify(void **unused)
{

}

int main()
{
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_claim_set),
    cmocka_unit_test(test_header),
    cmocka_unit_test(test_sign_verify),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}