/*
 * File:   mfa_token_cache.cpp
 * Copyright (c) 2025 Snowflake Computing
 */

#include <boost/filesystem.hpp>

#include "lib/CredentialCache.hpp"
#include "utils/test_setup.h"

class EnvOverride
{
public:
  EnvOverride(std::string envVar_, const std::string& newValue_)
    : envVar{std::move(envVar_)}
    , oldValue{}
  {
    char buf[BUF_SIZE];
    char* oldValueCstr = sf_getenv_s(envVar.c_str(), buf, BUF_SIZE);
    if (oldValueCstr != nullptr)
    {
      oldValue = oldValueCstr;
    }
    sf_setenv(envVar.c_str(), newValue_.c_str());
  }

  ~EnvOverride()
  {
    if (oldValue)
    {
      sf_setenv(envVar.c_str(), oldValue->c_str());
    }
    else
    {
      sf_unsetenv(envVar.c_str());
    }
  }

private:
  constexpr static size_t BUF_SIZE = 1024;
  std::string envVar;
  boost::optional<std::string> oldValue;
};

void remove_cache_file()
{
  boost::system::error_code ec;
  boost::filesystem::remove("./credential_cache_v1.json", ec);
  assert_true(!ec);
}

void test_credential_cache_simple(void **)
{
  EnvOverride override("SF_TEMPORARY_CREDENTIAL_CACHE_DIR", ".");
  std::unique_ptr<Snowflake::Client::CredentialCache> cache{Snowflake::Client::CredentialCache::make()};
  Snowflake::Client::CredentialKey key { "host", "user", CredentialType::MFA_TOKEN };

  std::string token = "example_token";
  assert_true(cache->save(key, token));
  assert_true(cache->get(key).value() == token);

  assert_true(cache->remove(key));
  assert_false(cache->get(key).has_value());
}

void test_credential_cache_two_keys(void **)
{
  remove_cache_file();
  sf_setenv("SF_TEMPORARY_CREDENTIAL_CACHE_DIR", ".");
  std::unique_ptr<Snowflake::Client::CredentialCache> cache{Snowflake::Client::CredentialCache::make()};
  Snowflake::Client::CredentialKey key1 { "host", "user1", CredentialType::MFA_TOKEN };
  Snowflake::Client::CredentialKey key2 { "host", "user2", CredentialType::MFA_TOKEN };
  std::string token1 = "example_token";
  std::string token2 = "example_token";

  assert_true(cache->save(key1, token1));
  assert_true(cache->get(key1).value() == token1);

  assert_true(cache->save(key2, token2));
  assert_true(cache->get(key2).value() == token2);

  assert_true(cache->remove(key1));
  assert_false(cache->get(key1).has_value());
  assert_true(cache->get(key2).has_value());

  assert_true(cache->remove(key2));
  assert_false(cache->get(key1).has_value());
  assert_false(cache->get(key2).has_value());
}

int main(void) {
  /* Testing only file based credential cache, available on linux */
#ifndef __linux__
  return 0;
#endif
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_credential_cache_simple),
      cmocka_unit_test(test_credential_cache_two_keys),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
