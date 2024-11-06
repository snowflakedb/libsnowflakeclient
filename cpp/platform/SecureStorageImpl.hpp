/*
 * File:   SecureStorageApple.hpp *
 * Copyright (c) 2013-2020 Snowflake Computing
 */

#ifndef PROJECT_SECURESTORAGE_IMPL_H
#define PROJECT_SECURESTORAGE_IMPL_H

#include <string>

enum class SecureStorageStatus
{
  NotFound,
  Error,
  Success,
  Unsupported
};

namespace Snowflake {

namespace Client {
  /**
   * Class SecureStorage
   */
  class SecureStorageImpl
  {

    static std::string convertTarget(const std::string& host,
                                     const std::string& username,
                                     const std::string& credType);

  public:

    /**
     * storeToken
     *
     * API to secure store credential in Apple Keychain
     *
     * @param host - snowflake host url
     * @param username - snowflake user name
     * @param credType - type of snowflake credential to be stored
     * @param cred - credential to be secured
     *
     * @return ERROR / SUCCESS
     */
    SecureStorageStatus storeToken(const std::string& host,
                                   const std::string& username,
                                   const std::string& credType,
                                   const std::string& cred);

    /**
     * retrieveToken
     *
     * API to retrieve credential from Apple Key Chain
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
    SecureStorageStatus retrieveToken(const std::string& host,
                                      const std::string& username,
                                      const std::string& credType,
                                      std::string& cred);

    /**
     * updateToken
     *
     * API to update an existing credential in the keychain.
     *
     * @param host - snowflake host url associated
     * with the credential
     * @param username - snowflake username assoicated with
     * the credential
     * @param credType - type of snowflake credential to be updated
     * @param cred - credential to be stored in the keychain.
     * @return ERROR / SUCCESS
     */
    SecureStorageStatus updateToken(const std::string& host,
                                    const std::string& username,
                                    const std::string& credType,
                                    const std::string& cred);

    /**
     * remove
     *
     * API to remove a credential from the keychain.
     *
     * @param host - snowflake host url associated
     * with the credential
     * @param username - snowflake username assoicated with
     * the credential
     * @param credType - type of credential to be removed.
     *
     * @return ERROR / SUCCESS
     */
    SecureStorageStatus removeToken(const std::string& host,
                                    const std::string& username,
                                    const std::string& credType);
  };

}

}

#endif //PROJECT_SECURESTORAGE_IMPL_H
