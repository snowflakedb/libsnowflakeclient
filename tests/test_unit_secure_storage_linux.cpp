/*
 * File:   test_unit_secure_storage.cpp
 */

#include <iostream>

#include <boost/optional.hpp>
#include <boost/filesystem.hpp>

#include "snowflake/SecureStorage.hpp"
#include "linux/CacheFile.hpp"

#include "utils/test_setup.h"
#include "utils/EnvOverride.hpp"

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
  boost::filesystem::remove_all("sf_cache_dir");
  mkdir("sf_cache_dir", 0700);
  EnvOverride override("SF_TEMPORARY_CREDENTIAL_CACHE_DIR", "sf_cache_dir");
  SecureStorage ss;
  SecureStorageKey key { "host", "user", SecureStorageKeyType::MFA_TOKEN };

  std::string token = "example_token";
  std::string retrievedToken;
  assert_true(ss.storeToken(key, token) == SecureStorageStatus::Success);
  assert_true(ss.retrieveToken(key, retrievedToken) == SecureStorageStatus::Success);
  assert_true(retrievedToken == token);
  assert_permissions(std::string("sf_cache_dir/") + CACHE_FILENAME, boost::filesystem::owner_read | boost::filesystem::owner_write);
  assert_permissions("sf_cache_dir", boost::filesystem::owner_all);

  assert_true(ss.removeToken(key) == SecureStorageStatus::Success);
  assert_true(ss.retrieveToken(key, retrievedToken) == SecureStorageStatus::NotFound);
}

void test_secure_storage_malformed_cache(void **)
{
  boost::filesystem::remove_all("sf_cache_dir");
  mkdir("sf_cache_dir", 0700);
  EnvOverride override("SF_TEMPORARY_CREDENTIAL_CACHE_DIR", "sf_cache_dir");
  SecureStorage ss;
  SecureStorageKey key { "host", "user", SecureStorageKeyType::MFA_TOKEN };

  std::string token = "example_token";
  std::string retrievedToken;
  assert_true(ss.storeToken(key, token) == SecureStorageStatus::Success);
  assert_true(ss.retrieveToken(key, retrievedToken) == SecureStorageStatus::Success);
  assert_true(retrievedToken == token);
  assert_permissions(std::string("sf_cache_dir/") + CACHE_FILENAME, boost::filesystem::owner_read | boost::filesystem::owner_write);
  assert_permissions("sf_cache_dir", boost::filesystem::owner_all);

  {
    std::ofstream fs(std::string("sf_cache_dir/") + CACHE_FILENAME, std::ios_base::trunc);
    assert_true(fs.is_open());
    fs << "[]";
  }
  assert_true(ss.retrieveToken(key, retrievedToken) == SecureStorageStatus::NotFound);

  assert_true(ss.storeToken(key, token) == SecureStorageStatus::Success);
  assert_true(ss.retrieveToken(key, retrievedToken) == SecureStorageStatus::Success);
  assert_true(retrievedToken == token);

  {
    std::ofstream fs(std::string("sf_cache_dir/") + CACHE_FILENAME, std::ios_base::trunc);
    assert_true(fs.is_open());
    fs << "{]";
  }
  assert_true(ss.retrieveToken(key, retrievedToken) == SecureStorageStatus::NotFound);

  assert_true(ss.storeToken(key, token) == SecureStorageStatus::Success);
  assert_true(ss.retrieveToken(key, retrievedToken) == SecureStorageStatus::Success);
  assert_true(retrievedToken == token);

  {
    std::ofstream fs(std::string("sf_cache_dir/") + CACHE_FILENAME, std::ios_base::trunc);
    assert_true(fs.is_open());
    fs << "{ \"random field\": []}";
  }
  assert_true(ss.retrieveToken(key, retrievedToken) == SecureStorageStatus::NotFound);

  assert_true(ss.storeToken(key, token) == SecureStorageStatus::Success);
  assert_true(ss.retrieveToken(key, retrievedToken) == SecureStorageStatus::Success);
  assert_true(retrievedToken == token);
}

void test_secure_storage_two_keys(void **)
{
  boost::filesystem::remove_all("sf_cache_dir");
  mkdir("sf_cache_dir", 0700);
  EnvOverride override("SF_TEMPORARY_CREDENTIAL_CACHE_DIR", "sf_cache_dir");
  SecureStorage ss;
  SecureStorageKey key1 { "host", "user1", SecureStorageKeyType::MFA_TOKEN };
  SecureStorageKey key2 { "host", "user2", SecureStorageKeyType::MFA_TOKEN };
  std::string token1 = "example_token";
  std::string token2 = "example_token";
  std::string retrievedToken;

  assert_true(ss.storeToken(key1, token1) == SecureStorageStatus::Success);
  assert_true(ss.retrieveToken(key1, retrievedToken) == SecureStorageStatus::Success);
  assert_true(retrievedToken == token1);
  assert_permissions(std::string("sf_cache_dir/") + CACHE_FILENAME, boost::filesystem::owner_read | boost::filesystem::owner_write);
  assert_permissions("sf_cache_dir", boost::filesystem::owner_all);

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
  EnvOverride override1("SF_TEMPORARY_CREDENTIAL_CACHE_DIR", boost::none);
  EnvOverride override2("XDG_CACHE_HOME", "cache_dir");
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

void test_secure_storage_update_key(void **)
{
  boost::filesystem::remove_all("sf_cache_dir");
  mkdir("sf_cache_dir", 0700);
  EnvOverride override("SF_TEMPORARY_CREDENTIAL_CACHE_DIR", "sf_cache_dir");

  SecureStorage ss;
  SecureStorageKey key { "host", "user", SecureStorageKeyType::MFA_TOKEN };

  std::string token = "example_token";
  std::string newToken = "new_token";
  std::string retrievedToken;
  assert_true(ss.storeToken(key, token) == SecureStorageStatus::Success);
  assert_true(ss.retrieveToken(key, retrievedToken) == SecureStorageStatus::Success);
  assert_true(token == retrievedToken);
  assert_true(ss.storeToken(key, newToken) == SecureStorageStatus::Success);
  assert_true(ss.retrieveToken(key, retrievedToken) == SecureStorageStatus::Success);
  assert_true(newToken == retrievedToken);
}

void test_secure_storage_fails_to_lock(void **)
{
  boost::filesystem::remove_all("sf_cache_dir");
  mkdir("sf_cache_dir", 0700);
  EnvOverride override("SF_TEMPORARY_CREDENTIAL_CACHE_DIR", "sf_cache_dir");
  SecureStorage ss;
  SecureStorageKey key { "host", "user", SecureStorageKeyType::MFA_TOKEN };

  std::string token = "example_token";
  std::string retrievedToken;
  boost::filesystem::create_directory(std::string("sf_cache_dir/") + CACHE_FILENAME + ".lck");
  assert_true(ss.storeToken(key, token) == SecureStorageStatus::Error);
  assert_true(ss.retrieveToken(key, retrievedToken) == SecureStorageStatus::Error);
  assert_true(ss.removeToken(key) == SecureStorageStatus::Error);
}

void test_secure_storage_fails_to_find_cache_path(void **)
{
  EnvOverride override1("SF_TEMPORARY_CREDENTIAL_CACHE_DIR", boost::none);
  EnvOverride override2("XDG_CACHE_HOME", boost::none);
  EnvOverride override3("HOME", boost::none);
  SecureStorage ss;
  SecureStorageKey key { "host", "user", SecureStorageKeyType::MFA_TOKEN };

  std::string token = "example_token";
  std::string retrievedToken;
  assert_true(ss.storeToken(key, token) == SecureStorageStatus::Error);
  assert_true(ss.retrieveToken(key, retrievedToken) == SecureStorageStatus::Error);
  assert_true(ss.removeToken(key) == SecureStorageStatus::Error);
}

void test_secure_storage_c_api(void **)
{
  boost::filesystem::remove_all("sf_cache_dir");
  mkdir("sf_cache_dir", 0700);
  EnvOverride override("SF_TEMPORARY_CREDENTIAL_CACHE_DIR", "sf_cache_dir");
  SecureStorageKey key{"host", "user", SecureStorageKeyType::MFA_TOKEN};
  std::string token = "example_token";

  secure_storage_ptr ss = secure_storage_init();

  assert_true(secure_storage_save_credential(ss, key.host.c_str(), key.user.c_str(), key.type, token.c_str()));

  char* cred = secure_storage_get_credential(ss, key.host.c_str(), key.user.c_str(), key.type);
  assert_true(cred != nullptr);
  assert_true(strcmp(cred, "example_token") == 0);
  secure_storage_free_credential(cred);

  assert_true(secure_storage_remove_credential(ss, key.host.c_str(), key.user.c_str(), key.type));

  cred = secure_storage_get_credential(ss, key.host.c_str(), key.user.c_str(), key.type);
  assert_true(cred == nullptr);
  secure_storage_free_credential(cred);

  secure_storage_term(ss);
}

void test_get_cache_dir_bad_path(void **)
{
  EnvOverride override("ENV_VAR", "fakepath");
  assert_false(getCacheDir("ENV_VAR", {}).is_initialized());
}

void test_get_cache_dir_not_a_dir(void **)
{
  EnvOverride override("ENV_VAR", "file");
  std::ofstream file("file", std::ios_base::trunc);
  assert_true(file.is_open());
  file << "contents";
  file.close();
  assert_false(getCacheDir("ENV_VAR", {}).is_initialized());
  boost::filesystem::remove("file");
}

int main(void) {
  /* Testing only file based credential cache, available on linux */
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_secure_storage_simple),
      cmocka_unit_test(test_secure_storage_malformed_cache),
      cmocka_unit_test(test_secure_storage_two_keys),
      cmocka_unit_test(test_secure_storage_xdg_cache_home),
      cmocka_unit_test(test_secure_storage_home_dir),
      cmocka_unit_test(test_secure_storage_c_api),
      cmocka_unit_test(test_secure_storage_fails_to_lock),
      cmocka_unit_test(test_secure_storage_update_key),
      cmocka_unit_test(test_secure_storage_fails_to_find_cache_path),
      cmocka_unit_test(test_get_cache_dir_bad_path),
      cmocka_unit_test(test_get_cache_dir_not_a_dir)
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
