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
  EnvOverride(const EnvOverride&) = delete;
  void operator=(const EnvOverride&) = delete;
  EnvOverride(std::string envVar_, const boost::optional<std::string>& newValue_)
    : envVar{std::move(envVar_)}
    , oldValue{}
  {
    char buf[BUF_SIZE];
    char* oldValueCstr = sf_getenv_s(envVar.c_str(), buf, BUF_SIZE);
    if (oldValueCstr != nullptr)
    {
      oldValue = oldValueCstr;
    }
    if (newValue_)
    {
      sf_setenv(envVar.c_str(), newValue_.get().c_str());
    }
    else
    {
      sf_unsetenv(envVar.c_str());
    }
  }

  EnvOverride(std::string envVar_, const std::string& newValue_)
    : EnvOverride(std::move(envVar_), boost::make_optional(newValue_)) {}

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

constexpr const char* CACHE_FILENAME = "credential_cache_v1.json";

void remove_file_if_exists(const std::string& path)
{
  boost::system::error_code ec;
  boost::filesystem::remove_all(path, ec);
  assert_true(!ec);
}

void assert_permissions(const std::string& path, boost::filesystem::perms permissions)
{
  boost::filesystem::file_status s = boost::filesystem::status( path );
  assert_true((s.permissions() & boost::filesystem::perms_mask) == permissions);
}

void test_credential_cache_simple(void **)
{
  remove_file_if_exists(CACHE_FILENAME);
  EnvOverride override("SF_TEMPORARY_CREDENTIAL_CACHE_DIR", ".");
  std::unique_ptr<Snowflake::Client::CredentialCache> cache{Snowflake::Client::CredentialCache::make()};
  Snowflake::Client::CredentialKey key { "host", "user", CredentialType::MFA_TOKEN };

  std::string token = "example_token";
  assert_true(cache->save(key, token));
  assert_true(cache->get(key).value() == token);
  assert_permissions(CACHE_FILENAME, boost::filesystem::owner_read | boost::filesystem::owner_write);
  assert_permissions(".", boost::filesystem::owner_all);

  assert_true(cache->remove(key));
  assert_false(cache->get(key).has_value());
}

void test_credential_cache_two_keys(void **)
{
  remove_file_if_exists(CACHE_FILENAME);
  EnvOverride override("SF_TEMPORARY_CREDENTIAL_CACHE_DIR", ".");
  std::unique_ptr<Snowflake::Client::CredentialCache> cache{Snowflake::Client::CredentialCache::make()};
  Snowflake::Client::CredentialKey key1 { "host", "user1", CredentialType::MFA_TOKEN };
  Snowflake::Client::CredentialKey key2 { "host", "user2", CredentialType::MFA_TOKEN };
  std::string token1 = "example_token";
  std::string token2 = "example_token";

  assert_true(cache->save(key1, token1));
  assert_true(cache->get(key1).value() == token1);
  assert_permissions(CACHE_FILENAME, boost::filesystem::owner_read | boost::filesystem::owner_write);
  assert_permissions(".", boost::filesystem::owner_all);

  assert_true(cache->save(key2, token2));
  assert_true(cache->get(key2).value() == token2);

  assert_true(cache->remove(key1));
  assert_false(cache->get(key1).has_value());
  assert_true(cache->get(key2).has_value());

  assert_true(cache->remove(key2));
  assert_false(cache->get(key1).has_value());
  assert_false(cache->get(key2).has_value());
}

void test_credential_cache_home_dir(void **)
{
  boost::filesystem::remove_all("home");
  boost::filesystem::create_directory("home");
  EnvOverride override1("XDG_CACHE_HOME", boost::none);
  EnvOverride override2("HOME", "home");
  std::unique_ptr<Snowflake::Client::CredentialCache> cache{Snowflake::Client::CredentialCache::make()};
  Snowflake::Client::CredentialKey key { "host", "user", CredentialType::MFA_TOKEN };

  std::string token = "example_token";
  assert_true(cache->save(key, token));
  assert_true(cache->get(key).value() == token);

  assert_true(boost::filesystem::exists(std::string(".cache/snowflake/") + CACHE_FILENAME));
  assert_permissions(".cache/snowflake", boost::filesystem::owner_all);
  assert_permissions(std::string(".cache/snowflake/") + CACHE_FILENAME, boost::filesystem::owner_read | boost::filesystem::owner_write);
}

void test_credential_cache_xdg_cache_home(void **)
{
  boost::filesystem::remove_all("cache_dir");
  boost::filesystem::create_directory("cache_dir");
  EnvOverride override("XDG_CACHE_HOME", "cache_dir");
  std::unique_ptr<Snowflake::Client::CredentialCache> cache{Snowflake::Client::CredentialCache::make()};
  Snowflake::Client::CredentialKey key { "host", "user", CredentialType::MFA_TOKEN };

  std::string token = "example_token";
  assert_true(cache->save(key, token));
  assert_true(cache->get(key).value() == token);

  assert_true(boost::filesystem::exists(std::string("cache_dir/snowflake/") + CACHE_FILENAME));
  assert_permissions("cache_dir/snowflake", boost::filesystem::owner_all);
  assert_permissions(std::string("cache_dir/snowflake/") + CACHE_FILENAME, boost::filesystem::owner_read | boost::filesystem::owner_write);
}

int main(void) {
  /* Testing only file based credential cache, available on linux */
#ifndef __linux__
  return 0;
#endif
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_credential_cache_simple),
      cmocka_unit_test(test_credential_cache_two_keys),
      cmocka_unit_test(test_credential_cache_xdg_cache_home),
      cmocka_unit_test(test_credential_cache_home_dir),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
