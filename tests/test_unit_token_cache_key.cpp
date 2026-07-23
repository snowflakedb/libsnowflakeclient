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

// Vector A — OAuth (DpopBundledAccessToken) golden key.
// Locked: changing this hash is a breaking change for cross-driver token sharing.
static const char* GOLDEN_KEY_OAUTH =
    "SnowflakeTokenCache.v2.DpopBundledAccessToken.741b6d66d252666d6821bfd19e0151511cf4efdaaeba2b3c87673aa4de6d2c0b";

// Vector B — MFA golden key.
// Locked: changing this hash is a breaking change for cross-driver token sharing.
static const char* GOLDEN_KEY_MFA =
    "SnowflakeTokenCache.v2.MfaToken.10c5dde84bb8f584c0df06ea826d418c4f580e08f9db10187c0cb5e2a732a0d6";

// Pre-normalized Vector A inputs (no scheme prefix — normalizeUrl is a no-op on
// already-stripped, already-lowercase strings). The quoted values are returned
// verbatim by normalizeIdentifier because they contain double-quote characters.
static SecureStorageKey golden_key_oauth()
{
  SecureStorageKey key;
  key.type = SecureStorageKeyType::DPOP_BUNDLED_ACCESS_TOKEN;
  key.idp       = "login.microsoftonline.com:443/tenant-id/oauth2/v2.0";
  key.snowflake = "myorg-myaccount.privatelink.snowflakecomputing.com";
  key.user      = "\"First Last\"@long-corporate-domain.example.com";
  key.role      = "\"Analyst Role With Spaces\":north_america:prod:readonly";
  return key;
}

void test_golden_hash(void **)
{
  auto result = SecureStorage::convertTarget(golden_key_oauth());
  assert_true(result.is_initialized());
  assert_string_equal(result.get().c_str(), GOLDEN_KEY_OAUTH);
}

void test_golden_hash_from_raw_inputs(void **)
{
  // Same key, but supplied with a scheme and mixed case; normalization must
  // reproduce the exact golden hash. Quoted segments use mixed case to verify
  // that normalizeIdentifier preserves them verbatim.
  SecureStorageKey key;
  key.type = SecureStorageKeyType::DPOP_BUNDLED_ACCESS_TOKEN;
  key.idp       = "https://login.microsoftonline.com:443/tenant-id/oauth2/v2.0";
  key.snowflake = "https://myorg-myaccount.privatelink.snowflakecomputing.com";
  key.user      = "\"First Last\"@long-corporate-domain.example.com";
  key.role      = "\"Analyst Role With Spaces\":north_america:prod:readonly";

  auto result = SecureStorage::convertTarget(key);
  assert_true(result.is_initialized());
  assert_string_equal(result.get().c_str(), GOLDEN_KEY_OAUTH);
}

// Vector B — MFA golden hash (2-field keyData: snowflake + username only).
void test_golden_hash_mfa(void **)
{
  SecureStorageKey key;
  key.type      = SecureStorageKeyType::MFA_TOKEN;
  key.snowflake = "https://myorg-myaccount.privatelink.snowflakecomputing.com";
  key.user      = "\"First Last\"@long-corporate-domain.example.com";
  // idp and role are absent for MFA flows — not passed to convertTarget.

  auto result = SecureStorage::convertTarget(key);
  assert_true(result.is_initialized());
  assert_string_equal(result.get().c_str(), GOLDEN_KEY_MFA);
}

void test_key_has_versioned_prefix(void **)
{
  auto result = SecureStorage::convertTarget(golden_key_oauth());
  assert_true(result.is_initialized());
  // Key format: SnowflakeTokenCache.v2.<TokenType>.<hash>
  assert_true(result.get().rfind("SnowflakeTokenCache.v2.DpopBundledAccessToken.", 0) == 0);
}

void test_normalize_url(void **)
{
  assert_string_equal(
      normalizeUrl("https://login.microsoftonline.com:443/tenant-id/oauth2/v2.0").c_str(),
      "login.microsoftonline.com:443/tenant-id/oauth2/v2.0");

  // Bare host: scheme stripped, no trailing slash.
  assert_string_equal(
      normalizeUrl("https://myorg-myaccount.privatelink.snowflakecomputing.com").c_str(),
      "myorg-myaccount.privatelink.snowflakecomputing.com");
  assert_string_equal(
      normalizeUrl("https://myorg-myaccount.snowflakecomputing.com/").c_str(),
      "myorg-myaccount.snowflakecomputing.com");

  // Non-root path is preserved (and lowercased).
  assert_string_equal(
      normalizeUrl("https://account.snowflakecomputing.com/path/to/resource").c_str(),
      "account.snowflakecomputing.com/path/to/resource");

  // Userinfo, query and fragment are dropped.
  assert_string_equal(
      normalizeUrl("https://user:pass@host.example.com/p?a=b#frag").c_str(),
      "host.example.com/p");

  // Userinfo is stripped from the authority only.
  assert_string_equal(
      normalizeUrl("https://user:pass@host.example.com/path").c_str(),
      "host.example.com/path");

  // An '@' after the authority is part of the path and must survive.
  assert_string_equal(
      normalizeUrl("https://host.example.com/oauth/@handle/token").c_str(),
      "host.example.com/oauth/@handle/token");

  // An '@' inside a dropped query string is never mistaken for userinfo.
  assert_string_equal(
      normalizeUrl("https://host.example.com/path?email=a@b.com").c_str(),
      "host.example.com/path");
}

void test_normalize_identifier(void **)
{
  // No quotes: entire value is lowercased.
  assert_string_equal(normalizeIdentifier("user@domain.com").c_str(), "user@domain.com");
  assert_string_equal(normalizeIdentifier("USER@DOMAIN.COM").c_str(), "user@domain.com");
  assert_string_equal(normalizeIdentifier("ANALYST_ROLE").c_str(), "analyst_role");

  // Contains a double-quote anywhere: returned verbatim, unchanged.
  assert_string_equal(
      normalizeIdentifier("\"First Last\"@long-corporate-domain.example.com").c_str(),
      "\"First Last\"@long-corporate-domain.example.com");
  assert_string_equal(
      normalizeIdentifier("\"Analyst Role With Spaces\":north_america:prod:readonly").c_str(),
      "\"Analyst Role With Spaces\":north_america:prod:readonly");

  // Quote not at position 0 — still triggers the verbatim path.
  assert_string_equal(
      normalizeIdentifier("prefix-\"seg\"").c_str(),
      "prefix-\"seg\"");
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

void test_mfa_oauth_same_host_user_differ(void **)
{
  // MFA and OAuth keys for the same host/user must differ: they differ by
  // the token type in the prefix AND by field set (2 vs 4 fields).
  std::string mfa = keyString("IDP.EXAMPLE.COM", "ACCOUNT.SNOWFLAKECOMPUTING.COM", "USER", "",
                              SecureStorageKeyType::MFA_TOKEN);
  std::string oauth = keyString("IDP.EXAMPLE.COM", "ACCOUNT.SNOWFLAKECOMPUTING.COM", "USER", "",
                                SecureStorageKeyType::OAUTH_ACCESS_TOKEN);
  assert_true(mfa != oauth);
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
  // The (host, user, type) constructor maps to idp == snowflake == host.
  // For MFA flows idp is excluded from the key, so both constructors must
  // produce the same result.
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

void test_mfa_id_does_not_require_idp(void **)
{
  // For MFA and ID-token flows, idp is excluded from keyData.
  // An empty idp must not cause convertTarget to fail.
  SecureStorageKey key;
  key.type      = SecureStorageKeyType::ID_TOKEN;
  key.idp       = "";   // deliberately empty — not used for this flow
  key.snowflake = "host.example.com";
  key.user      = "user";
  assert_true(SecureStorage::convertTarget(key).is_initialized());
}

int main(void)
{
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_golden_hash),
      cmocka_unit_test(test_golden_hash_from_raw_inputs),
      cmocka_unit_test(test_golden_hash_mfa),
      cmocka_unit_test(test_key_has_versioned_prefix),
      cmocka_unit_test(test_normalize_url),
      cmocka_unit_test(test_normalize_identifier),
      cmocka_unit_test(test_different_snowflake_hosts_differ),
      cmocka_unit_test(test_same_idp_different_snowflake_differ),
      cmocka_unit_test(test_different_roles_differ),
      cmocka_unit_test(test_different_token_types_differ),
      cmocka_unit_test(test_mfa_oauth_same_host_user_differ),
      cmocka_unit_test(test_mfa_empty_role_is_stable),
      cmocka_unit_test(test_backward_compatible_ctor_sets_idp_snowflake),
      cmocka_unit_test(test_validation_rejects_empty_snowflake),
      cmocka_unit_test(test_validation_rejects_empty_user),
      cmocka_unit_test(test_mfa_id_does_not_require_idp),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
