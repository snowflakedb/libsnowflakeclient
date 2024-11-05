//
// Created by Jakub Szczerbinski on 05.11.24.
//
#include "lib/CredentialCache.hpp"
#include "utils/test_setup.h"

void test_credential_cache(void **unused)
{
  std::unique_ptr<sf::CredentialCache> cache{sf::CredentialCache::make()};
  sf::CredentialKey key { "account", "host", "user", CredentialType::MFA_TOKEN };

  std::string token = "example_token";
  assert_true(cache->save(key, token));
  assert_true(cache->get(key).value() == token);

  assert_true(cache->remove(key));
  assert_false(cache->get(key).has_value());
}

int main(void) {
  /* Testing only file based credential cache */
#ifndef __linux__
  return 0;
#endif
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_credential_cache),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
