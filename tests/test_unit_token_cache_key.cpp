/*
 * File:   test_unit_token_cache_key.cpp
 *
 * Unit tests for the v2 token cache key: canonical-JSON serialization, the
 * cross-driver golden hash, URL/identifier normalization, dimension isolation
 * and input validation. These exercise only the key-derivation logic
 * (SecureStorage::convertTarget and the normalization helpers), so they run on
 * every platform regardless of the secure-storage backend.
 */

#include <string>

#include <boost/optional.hpp>

#include "snowflake/SecureStorage.hpp"

#include "utils/test_setup.h"

using namespace Snowflake::Client;

// Locked golden key — changing this hash is a breaking change for cross-driver token sharing.
static const char* GOLDEN_KEY =
    "SnowflakeTokenCache.v2.75ff2ad65a68afb402f125f62894697673c5ef3d863aba466d16b7a81053d1f4";

static SecureStorageKey golden_key()
{
  SecureStorageKey key;
  key.type = SecureStorageKeyType::DPOP_BUNDLED_ACCESS_TOKEN;
  key.idp = "LOGIN.MICROSOFTONLINE.COM:443/TENANT-ID/OAUTH2/V2.0";
  key.snowflake = "MYORG-MYACCOUNT.PRIVATELINK.SNOWFLAKECOMPUTING.COM";
  key.user = "\"FIRST LAST\"@LONG-CORPORATE-DOMAIN.EXAMPLE.COM";
  key.role = "\"ANALYST ROLE WITH SPACES\":NORTH_AMERICA:PROD:READONLY";
  return key;
}

void test_golden_hash(void **)
{
  auto result = SecureStorage::convertTarget(golden_key());
  assert_true(result.is_initialized());
  assert_string_equal(result.get().c_str(), GOLDEN_KEY);
}

void test_golden_hash_from_raw_inputs(void **)
{
  // Same key, but supplied with a scheme and mixed case; normalization must
  // reproduce the exact golden hash.
  SecureStorageKey key;
  key.type = SecureStorageKeyType::DPOP_BUNDLED_ACCESS_TOKEN;
  key.idp = "https://login.microsoftonline.com:443/tenant-id/oauth2/v2.0";
  key.snowflake = "https://myorg-myaccount.privatelink.snowflakecomputing.com";
  key.user = "\"FIRST LAST\"@long-corporate-domain.example.com";
  key.role = "\"ANALYST ROLE WITH SPACES\":north_america:prod:readonly";

  auto result = SecureStorage::convertTarget(key);
  assert_true(result.is_initialized());
  assert_string_equal(result.get().c_str(), GOLDEN_KEY);
}

void test_key_has_versioned_prefix(void **)
{
  auto result = SecureStorage::convertTarget(golden_key());
  assert_true(result.is_initialized());
  assert_true(result.get().rfind("SnowflakeTokenCache.v2.", 0) == 0);
}

void test_normalize_url(void **)
{
  assert_string_equal(
      normalizeUrl("https://login.microsoftonline.com:443/tenant-id/oauth2/v2.0").c_str(),
      "LOGIN.MICROSOFTONLINE.COM:443/TENANT-ID/OAUTH2/V2.0");

  // Bare host: scheme stripped, no trailing slash.
  assert_string_equal(
      normalizeUrl("https://myorg-myaccount.privatelink.snowflakecomputing.com").c_str(),
      "MYORG-MYACCOUNT.PRIVATELINK.SNOWFLAKECOMPUTING.COM");
  assert_string_equal(
      normalizeUrl("https://myorg-myaccount.snowflakecomputing.com/").c_str(),
      "MYORG-MYACCOUNT.SNOWFLAKECOMPUTING.COM");

  // Non-root path is preserved (and uppercased).
  assert_string_equal(
      normalizeUrl("https://account.snowflakecomputing.com/path/to/resource").c_str(),
      "ACCOUNT.SNOWFLAKECOMPUTING.COM/PATH/TO/RESOURCE");

  // Userinfo, query and fragment are dropped.
  assert_string_equal(
      normalizeUrl("https://user:pass@host.example.com/p?a=b#frag").c_str(),
      "HOST.EXAMPLE.COM/P");

  // Userinfo is stripped from the authority only.
  assert_string_equal(
      normalizeUrl("https://user:pass@host.example.com/path").c_str(),
      "HOST.EXAMPLE.COM/PATH");

  // An '@' after the authority is part of the path and must survive.
  assert_string_equal(
      normalizeUrl("https://host.example.com/oauth/@handle/token").c_str(),
      "HOST.EXAMPLE.COM/OAUTH/@HANDLE/TOKEN");

  // An '@' inside a dropped query string is never mistaken for userinfo.
  assert_string_equal(
      normalizeUrl("https://host.example.com/path?email=a@b.com").c_str(),
      "HOST.EXAMPLE.COM/PATH");
}

void test_normalize_identifier(void **)
{
  assert_string_equal(normalizeIdentifier("user@domain.com").c_str(), "USER@DOMAIN.COM");

  // Double-quoted segment preserved verbatim; the rest uppercased.
  assert_string_equal(
      normalizeIdentifier("\"First Last\"@domain.com").c_str(),
      "\"First Last\"@DOMAIN.COM");
  assert_string_equal(
      normalizeIdentifier("\"Analyst Role With Spaces\":north_america:prod:readonly").c_str(),
      "\"Analyst Role With Spaces\":NORTH_AMERICA:PROD:READONLY");

  // Already uppercase input is unchanged.
  assert_string_equal(normalizeIdentifier("USER@DOMAIN.COM").c_str(), "USER@DOMAIN.COM");
}

static std::string keyString(const std::string& idp, const std::string& snowflake,
                             const std::string& user, const std::string& role,
                             SecureStorageKeyType type)
{
  SecureStorageKey key;
  key.type = type;
  key.idp = idp;
  key.snowflake = snowflake;
  key.user = user;
  key.role = role;
  auto result = SecureStorage::convertTarget(key);
  assert_true(result.is_initialized());
  return result.get();
}

void test_different_snowflake_hosts_differ(void **)
{
  std::string a = keyString("IDP.EXAMPLE.COM", "ACCOUNT1.SNOWFLAKECOMPUTING.COM", "USER", "",
                            SecureStorageKeyType::OAUTH_ACCESS_TOKEN);
  std::string b = keyString("IDP.EXAMPLE.COM", "ACCOUNT2.SNOWFLAKECOMPUTING.COM", "USER", "",
                            SecureStorageKeyType::OAUTH_ACCESS_TOKEN);
  assert_true(a != b);
}

void test_same_idp_different_snowflake_differ(void **)
{
  std::string a = keyString("IDP.SHARED.COM", "ORG-ACCOUNT1.SNOWFLAKECOMPUTING.COM", "USER", "",
                            SecureStorageKeyType::OAUTH_ACCESS_TOKEN);
  std::string b = keyString("IDP.SHARED.COM", "ORG-ACCOUNT2.SNOWFLAKECOMPUTING.COM", "USER", "",
                            SecureStorageKeyType::OAUTH_ACCESS_TOKEN);
  assert_true(a != b);
}

void test_different_roles_differ(void **)
{
  std::string a = keyString("IDP.EXAMPLE.COM", "ACCOUNT.SNOWFLAKECOMPUTING.COM", "USER", "ROLE_A",
                            SecureStorageKeyType::OAUTH_ACCESS_TOKEN);
  std::string b = keyString("IDP.EXAMPLE.COM", "ACCOUNT.SNOWFLAKECOMPUTING.COM", "USER", "ROLE_B",
                            SecureStorageKeyType::OAUTH_ACCESS_TOKEN);
  assert_true(a != b);
}

void test_different_token_types_differ(void **)
{
  std::string a = keyString("HOST.EXAMPLE.COM", "HOST.EXAMPLE.COM", "USER", "",
                            SecureStorageKeyType::ID_TOKEN);
  std::string b = keyString("HOST.EXAMPLE.COM", "HOST.EXAMPLE.COM", "USER", "",
                            SecureStorageKeyType::MFA_TOKEN);
  assert_true(a != b);
}

void test_mfa_empty_role_is_stable(void **)
{
  std::string a = keyString("ACCOUNT.SNOWFLAKECOMPUTING.COM", "ACCOUNT.SNOWFLAKECOMPUTING.COM",
                            "USER", "", SecureStorageKeyType::MFA_TOKEN);
  std::string b = keyString("ACCOUNT.SNOWFLAKECOMPUTING.COM", "ACCOUNT.SNOWFLAKECOMPUTING.COM",
                            "USER", "", SecureStorageKeyType::MFA_TOKEN);
  assert_true(a == b);
  assert_true(a.rfind("SnowflakeTokenCache.v2.", 0) == 0);
}

void test_backward_compatible_ctor_sets_idp_snowflake(void **)
{
  // The (host, user, type) constructor must map to idp == snowflake == host.
  SecureStorageKey compat("host.example.com", "user", SecureStorageKeyType::MFA_TOKEN);
  std::string viaCompat = SecureStorage::convertTarget(compat).get();
  std::string explicitKey = keyString("host.example.com", "host.example.com", "user", "",
                                      SecureStorageKeyType::MFA_TOKEN);
  assert_string_equal(viaCompat.c_str(), explicitKey.c_str());
}

void test_validation_rejects_empty_snowflake(void **)
{
  SecureStorageKey key;
  key.type = SecureStorageKeyType::ID_TOKEN;
  key.idp = "";
  key.snowflake = "";
  key.user = "user";
  assert_false(SecureStorage::convertTarget(key).is_initialized());
}

void test_validation_rejects_empty_user(void **)
{
  SecureStorageKey key;
  key.type = SecureStorageKeyType::ID_TOKEN;
  key.idp = "host.example.com";
  key.snowflake = "host.example.com";
  key.user = "";
  assert_false(SecureStorage::convertTarget(key).is_initialized());
}

void test_validation_rejects_empty_idp(void **)
{
  SecureStorageKey key;
  key.type = SecureStorageKeyType::ID_TOKEN;
  key.idp = "";
  key.snowflake = "host.example.com";
  key.user = "user";
  assert_false(SecureStorage::convertTarget(key).is_initialized());
}

int main(void)
{
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_golden_hash),
      cmocka_unit_test(test_golden_hash_from_raw_inputs),
      cmocka_unit_test(test_key_has_versioned_prefix),
      cmocka_unit_test(test_normalize_url),
      cmocka_unit_test(test_normalize_identifier),
      cmocka_unit_test(test_different_snowflake_hosts_differ),
      cmocka_unit_test(test_same_idp_different_snowflake_differ),
      cmocka_unit_test(test_different_roles_differ),
      cmocka_unit_test(test_different_token_types_differ),
      cmocka_unit_test(test_mfa_empty_role_is_stable),
      cmocka_unit_test(test_backward_compatible_ctor_sets_idp_snowflake),
      cmocka_unit_test(test_validation_rejects_empty_snowflake),
      cmocka_unit_test(test_validation_rejects_empty_user),
      cmocka_unit_test(test_validation_rejects_empty_idp),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
