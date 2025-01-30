/*
 * File:   test_unit_secure_storage.cpp
 * Copyright (c) 2025 Snowflake Computing
 */

#include <boost/filesystem.hpp>
#include <iostream>

#include "snowflake/SecureStorage.hpp"
#include "utils/test_setup.h"
#include <boost/optional.hpp>

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

using namespace Snowflake::Client;

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

void test_secure_storage_simple(void **)
{
  remove_file_if_exists(CACHE_FILENAME);
  EnvOverride override("SF_TEMPORARY_CREDENTIAL_CACHE_DIR", ".");
  SecureStorage ss;
  SecureStorageKey key { "host", "user", SecureStorageKeyType::MFA_TOKEN };

  std::string token = "example_token";
  std::string retrievedToken;
  assert_true(ss.storeToken(key, token) == SecureStorageStatus::Success);
  assert_true(ss.retrieveToken(key, retrievedToken) == SecureStorageStatus::Success);
  assert_true(retrievedToken == token);
  assert_permissions(CACHE_FILENAME, boost::filesystem::owner_read | boost::filesystem::owner_write);
  assert_permissions(".", boost::filesystem::owner_all);

  assert_true(ss.removeToken(key) == SecureStorageStatus::Success);
  assert_true(ss.retrieveToken(key, retrievedToken) == SecureStorageStatus::NotFound);
}

void test_secure_storage_two_keys(void **)
{
  remove_file_if_exists(CACHE_FILENAME);
  EnvOverride override("SF_TEMPORARY_CREDENTIAL_CACHE_DIR", ".");
  SecureStorage ss;
  SecureStorageKey key1 { "host", "user1", SecureStorageKeyType::MFA_TOKEN };
  SecureStorageKey key2 { "host", "user2", SecureStorageKeyType::MFA_TOKEN };
  std::string token1 = "example_token";
  std::string token2 = "example_token";
  std::string retrievedToken;

  assert_true(ss.storeToken(key1, token1) == SecureStorageStatus::Success);
  assert_true(ss.retrieveToken(key1, retrievedToken) == SecureStorageStatus::Success);
  assert_true(retrievedToken == token1);
  assert_permissions(CACHE_FILENAME, boost::filesystem::owner_read | boost::filesystem::owner_write);
  assert_permissions(".", boost::filesystem::owner_all);

  assert_true(ss.storeToken(key2, token2) == SecureStorageStatus::Success);
  assert_true(ss.retrieveToken(key2, retrievedToken) == SecureStorageStatus::Success);
  assert_true(retrievedToken == token2);

  assert_true(ss.removeToken(key1) == SecureStorageStatus::Success);
  assert_true(ss.retrieveToken(key1, retrievedToken) == SecureStorageStatus::NotFound);
  assert_true(ss.retrieveToken(key2, retrievedToken) == SecureStorageStatus::Success);
  assert_true(retrievedToken == token2);

  assert_true(ss.removeToken(key2) == SecureStorageStatus::Success);
  assert_true(ss.retrieveToken(key1, retrievedToken) == SecureStorageStatus::NotFound);
  assert_true(ss.retrieveToken(key2, retrievedToken) == SecureStorageStatus::NotFound);
}

void test_secure_storage_home_dir(void **)
{
  boost::filesystem::remove_all("home");
  boost::filesystem::create_directory("home");
  EnvOverride override1("XDG_CACHE_HOME", boost::none);
  EnvOverride override2("HOME", "home");
  SecureStorage ss;
  SecureStorageKey key { "host", "user", SecureStorageKeyType::MFA_TOKEN };

  std::string token = "example_token";
  std::string retrievedToken;
  assert_true(ss.storeToken(key, token) == SecureStorageStatus::Success);
  assert_true(ss.retrieveToken(key, retrievedToken) == SecureStorageStatus::Success);
  assert_true(retrievedToken == token);

  assert_true(boost::filesystem::exists(std::string("home/.cache/snowflake/") + CACHE_FILENAME));
  assert_permissions("home/.cache/snowflake", boost::filesystem::owner_all);
  assert_permissions(std::string("home/.cache/snowflake/") + CACHE_FILENAME, boost::filesystem::owner_read | boost::filesystem::owner_write);
}

void test_secure_storage_xdg_cache_home(void **)
{
  boost::filesystem::remove_all("cache_dir");
  boost::filesystem::create_directory("cache_dir");
  EnvOverride override("XDG_CACHE_HOME", "cache_dir");
  SecureStorage ss;
  SecureStorageKey key { "host", "user", SecureStorageKeyType::MFA_TOKEN };

  std::string token = "example_token";
  std::string retrievedToken;
  assert_true(ss.storeToken(key, token) == SecureStorageStatus::Success);
  assert_true(ss.retrieveToken(key, retrievedToken) == SecureStorageStatus::Success);
  assert_true(retrievedToken == token);

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
      cmocka_unit_test(test_secure_storage_simple),
      cmocka_unit_test(test_secure_storage_two_keys),
      cmocka_unit_test(test_secure_storage_xdg_cache_home),
      cmocka_unit_test(test_secure_storage_home_dir),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
