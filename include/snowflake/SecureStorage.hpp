/*
 * File:   SecureStorageApple.hpp *
 */

#ifndef PROJECT_SECURESTORAGE_HPP
#define PROJECT_SECURESTORAGE_HPP

#include <string>

#include "snowflake/secure_storage.h"
#include <boost/optional.hpp>

namespace Snowflake {

namespace Client {
  enum class SecureStorageStatus
  {
    NotFound,
    Error,
    Success,
    Unsupported
  };

  // Returns the canonical, uppercase token-type string used in the cache key.
  // These strings must match across all Snowflake drivers so that token cache
  // entries are interoperable.
  inline std::string keyTypeToString(SecureStorageKeyType type) {
    switch (type) {
      case SecureStorageKeyType::MFA_TOKEN:
        return "MFA_TOKEN";
      case SecureStorageKeyType::ID_TOKEN:
        return "ID_TOKEN";
      case SecureStorageKeyType::OAUTH_REFRESH_TOKEN:
        return "OAUTH_REFRESH_TOKEN";
      case SecureStorageKeyType::OAUTH_ACCESS_TOKEN:
        return "OAUTH_ACCESS_TOKEN";
      case SecureStorageKeyType::DPOP_BUNDLED_ACCESS_TOKEN:
        return "DPOP_BUNDLED_ACCESS_TOKEN";
      default:
        return "UNKNOWN";
    }
  }

  /**
   * Identifies a cached token across drivers.
   *
   * The cache key (see SecureStorage::convertTarget) is derived from the fields
   * below according to the flow type. `host` is kept for logging and
   * backward-compatible construction only; it is not part of the v2 key.
   * `idp`, `snowflake`, `user` and `role` are stored raw and normalized when
   * the key is built.
   *
   * OAuth flows (OAUTH_ACCESS_TOKEN, OAUTH_REFRESH_TOKEN,
   * DPOP_BUNDLED_ACCESS_TOKEN): all four dimensions — `idp`, `role`,
   * `snowflake`, `username` — are included in keyData.
   *
   * MFA and external-browser ID-token flows (MFA_TOKEN, ID_TOKEN): only
   * `snowflake` and `username` are included. Set `idp` and `role` to empty
   * strings; they are ignored by convertTarget for these flow types.
   */
  struct SecureStorageKey {
    std::string host;       // logging / backward-compat only, not part of the v2 key
    std::string user;       // raw username (normalized when the key is built)
    SecureStorageKeyType type;
    std::string idp;        // raw IdP / token-endpoint URL (normalized when built)
    std::string snowflake;  // raw Snowflake server URL (normalized when built)
    std::string role;       // raw role (normalized when built); empty for MFA

    SecureStorageKey() : type(SecureStorageKeyType::MFA_TOKEN) {}

    // Backward-compatible constructor for non-OAuth flows: idp == snowflake ==
    // host and no role. Existing callers that only know (host, user, type)
    // continue to produce a valid v2 key.
    SecureStorageKey(std::string host_, std::string user_, SecureStorageKeyType type_)
      : host(host_), user(std::move(user_)), type(type_),
        idp(host_), snowflake(std::move(host_)), role() {}

    // Full v2 constructor.
    SecureStorageKey(std::string host_, std::string user_, SecureStorageKeyType type_,
                     std::string idp_, std::string snowflake_, std::string role_)
      : host(std::move(host_)), user(std::move(user_)), type(type_),
        idp(std::move(idp_)), snowflake(std::move(snowflake_)), role(std::move(role_)) {}
  };

  /**
   * Normalizes a URL for use as a cache key component: strips the scheme, drops
   * the query string and fragment, strips any userinfo prefix from the authority
   * (an '@' inside the path is preserved), trims trailing slashes, and uppercases
   * the remainder (authority + optional :port + optional /path).
   */
  std::string normalizeUrl(const std::string& url);

  /**
   * Normalizes a Snowflake identifier for use as a cache key component:
   * uppercases every character outside double-quoted segments while preserving
   * the contents (and surrounding quotes) of double-quoted segments verbatim.
   */
  std::string normalizeIdentifier(const std::string& identifier);

  /**
   * Class SecureStorage
   */

  class SecureStorage
  {

  public:
    /**
     * Builds the versioned, uniformly-hashed cache key for `key`:
     *
     *   SnowflakeTokenCache.v2.<TOKEN_TYPE>.<lowercase sha256 hex of canonical JSON>
     *
     * The token type (e.g. MFA_TOKEN, OAUTH_ACCESS_TOKEN) is the third
     * dot-separated segment, allowing keystore tooling to identify token
     * classes without decoding the opaque hash.
     *
     * The canonical JSON (`keyData`) is compact, keys sorted lexicographically,
     * and is flow-dependent:
     *
     *   OAuth flows (OAUTH_ACCESS_TOKEN, OAUTH_REFRESH_TOKEN,
     *   DPOP_BUNDLED_ACCESS_TOKEN):
     *     {"idp":"…","role":"…","snowflake":"…","username":"…"}   // 4 fields
     *
     *   MFA and ID-token flows (MFA_TOKEN, ID_TOKEN):
     *     {"snowflake":"…","username":"…"}                        // 2 fields
     *
     * Values are normalized (see normalizeUrl / normalizeIdentifier) before
     * serialization. The returned string is used verbatim by every backend
     * (macOS Keychain, Windows Credential Manager, Linux JSON file); hashing
     * happens exactly once, here.
     *
     * Returns boost::none if the required `snowflake` or `user` components are
     * empty, or if hashing fails.
     */
    static boost::optional<std::string> convertTarget(const SecureStorageKey& key);

    /**
     * storeToken
     *
     * API to secure store credential
     *
     * @param key - credential key
     * @param cred - credential to be secured
     *
     * @return ERROR / SUCCESS
     */
    SecureStorageStatus storeToken(const SecureStorageKey& key,
                                   const std::string& cred);

    /**
     * retrieveToken
     *
     * API to retrieve credential
     *
     * @param key - credential key
     * @param cred - on succcess, retrieved credential will stored here
     * @return NOT_FOUND, ERROR, SUCCESS
     */
    SecureStorageStatus retrieveToken(const SecureStorageKey& key,
                                      std::string& cred);

    /**
     * remove
     *
     * API to remove a credential.
     *
     * @param key - credenetial key
     *
     * @return ERROR / SUCCESS
     */
    SecureStorageStatus removeToken(const SecureStorageKey& key);
  };

}

}

#endif //PROJECT_SECURESTORAGE_H
