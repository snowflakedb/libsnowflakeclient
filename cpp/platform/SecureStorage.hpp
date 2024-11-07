/*
 * File:   SecureStorage.hpp *
 * Copyright (c) 2013-2020 Snowflake Computing
 */

#ifndef PROJECT_SECURESTORAGE_H
#define PROJECT_SECURESTORAGE_H

#include <stdlib.h>

namespace sf
{
  /**
   * Class SecureStorage
   */
  class SecureStorage
  {
  public:

    /**
     * storeToken
     *
     * API to secure store id_token in Apple Keychain
     *
     * @param host - snowflake host name
     * @param username - snowflake user name
     * @param token - id token to be secured
     * @param credType - type of snowflake temporary credential
     * @param cred - temporary credential to store
     * @return True / False
     */
    bool storeToken(const char *host,
                    const char *username,
                    const char *credType,
                    const char *cred);

    /**
     * retrieveToken
     *
     * API to retrieve token from Apple Key Chain
     *
     * @param host - snowflake host url associated
     * with the token
     * @param username - snowflake username associated with
     * the token
     * @param credType - type of credential to retrieve
     * @param token - on return , populated id credential extracted
     * from the keychain
     * @param tokenLen - on return, length of the credential retrieved
     * @return True / False
     */
    bool retrieveToken(const char *host,
                       const char *username,
                       const char *credType,
                       char *token,
                       size_t *tokenLen);

    /**
     * updateToken
     *
     * API to update an existing token in the keychain.
     *
     * @param host - snowflake host url associated
     * with the token
     * @param username - snowflake username assoicated with
     * the token
     * @param credType - type of credential to be updated
     * @param token - credential to be stored in the keychain.
     * @return True / False
     */
    bool updateToken(const char *host,
                     const char *username,
                     const char *credType,
                     const char *token);

    /**
     * removeToken
     *
     * API to remove a token from the keychain.
     *
     * @param host - snowflake host url associated
     * with the token
     * @param username - snowflake username assoicated with
     * the token
     * @param credType - type of credential to be removed
     * @return True / False
     */
    bool removeToken(const char *host,
                     const char *username,
                     const char *credType);
  };
}

#endif //PROJECT_SECURESTORAGE_H
