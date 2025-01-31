/*
 * File:   SecureStorageApple.hpp *
 * Copyright (c) 2013-2020 Snowflake Computing
 */

#ifndef PROJECT_SECURESTORAGE_IMPL_HPP
#define PROJECT_SECURESTORAGE_IMPL_HPP

#include <string>
#include "snowflake/secure_storage.h"


namespace Snowflake {

namespace Client {
  enum class SecureStorageStatus
  {
    NotFound,
    Error,
    Success,
    Unsupported
  };

  inline std::string keyTypeToString(SecureStorageKeyType type) {
    switch (type) {
      case SecureStorageKeyType::MFA_TOKEN:
        return "MFA_TOKEN";
      case SecureStorageKeyType::SSO_TOKEN:
        return "SSO_TOKEN";
      case SecureStorageKeyType::OAUTH_REFRESH_TOKEN:
        return "OAUTH_REFRESH_TOKEN";
      case SecureStorageKeyType::OAUTH_ACCESS_TOKEN:
        return "OAUTH_ACCESS_TOKEN";
      default:
        return "UNKNOWN";
    }
  }
  struct SecureStorageKey {
    std::string host;
    std::string user;
    SecureStorageKeyType type;
  };
  /**
   * Class SecureStorage
   */

  class SecureStorage
  {

  public:
    static std::string convertTarget(const SecureStorageKey& key);

    /**
     * storeToken
     *
     * API to secure store credential
     *
     * @param host - snowflake host url
     * @param username - snowflake user name
     * @param credType - type of snowflake credential to be stored
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
     * @param host - snowflake host url associated
     * with the credential
     * @param username - snowflake username associated with
     * the credential
     * @param cred - snowflake credential to be retrieved
     * @param cred - on return , populated credential extracted
     * from the keychain
     * @param credLen - on return, length of the credential retrieved
     * @return NOT_FOUND, ERROR, SUCCESS
     */
    SecureStorageStatus retrieveToken(const SecureStorageKey& key,
                                      std::string& cred);

    /**
     * remove
     *
     * API to remove a credential.
     *
     * @param host - snowflake host url associated
     * with the credential
     * @param username - snowflake username assoicated with
     * the credential
     * @param credType - type of credential to be removed.
     *
     * @return ERROR / SUCCESS
     */
    SecureStorageStatus removeToken(const SecureStorageKey& key);
  };

}

}

#endif //PROJECT_SECURESTORAGE_IMPL_H
