/*
 * File:   SecureStorageApple.hpp *
 * Copyright (c) 2013-2020 Snowflake Computing
 */

#ifndef PROJECT_SECURESTORAGE_APPLE_H
#define PROJECT_SECURESTORAGE_APPLE_H

#include <stdlib.h>

typedef enum
{
  NOT_FOUND,
  ERROR,
  SUCCESS,
  UNSUPPORTED
}SECURE_STORAGE_STATUS;

namespace sf
{
  /**
   * Class SecureStorage
   */
  class SecureStorageImpl
  {

    static void convertTarget(const char *host,
                              const char *username,
                              const char *credType,
                              char *targetname,
                              size_t max_len);

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
    SECURE_STORAGE_STATUS storeToken(const char *host,
                                     const char *username,
                                     const char *credType,
                                     const char *cred);

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
    SECURE_STORAGE_STATUS retrieveToken(const char *host,
                                        const char *username,
                                        const char *credType,
                                        char *cred,
                                        size_t *credLen);

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
    SECURE_STORAGE_STATUS updateToken(const char *host,
                                      const char *username,
                                      const char *credType,
                                      const char *cred);

    /**
     * removeToken
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
    SECURE_STORAGE_STATUS removeToken(const char *host,
                                      const char *username,
                                      const char *credType);
  };
}

#endif //PROJECT_SECURESTORAGE_APPLE_H
