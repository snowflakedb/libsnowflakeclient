/*
 * File:   SecureStorage.cpp *
 * Copyright (c) 2013-2020 Snowflake Computing
 */

#include "Logger.hpp"
#ifdef __APPLE__
#include "SecureStorageApple.hpp"
#elif _WIN32
#include "SecureStorageWin.hpp"
#else
#include "SecureStorageUnSup.hpp"
#endif

#include "SecureStorage.hpp"

namespace sf
{

  bool SecureStorage::storeToken(const char *host,
                                 const char *username,
                                 const char *credType,
                                 const char *token)
  {
    SecureStorageImpl secStorage;
    if (secStorage.storeToken(host, username, credType, token) != SUCCESS)
    {
      SF_ERROR_LOG("sf", "SecureStorage", "storeToken",
                   "Failed to store secure token%s", "");
      return false;
    }

    SF_DEBUG_LOG("sf", "SecureStorage", "storeToken",
                 "Successfully stored secure token%s", "");
    return true;
  }

  bool SecureStorage::retrieveToken(const char *host,
                                    const char *username,
                                    const char *credType,
                                    char *token,
                                    size_t *tokenLen)
  {
    SecureStorageImpl secStorage;
    if (secStorage.retrieveToken(host, username, credType, token, tokenLen) != SUCCESS)
    {
      SF_ERROR_LOG("sf", "SecureStorage", "retrieveToken",
                   "Failed to retrieve secure token%s", "");
      return false;
    }

    SF_DEBUG_LOG("sf", "SecureStorage", "storeToken",
                 "Successfully retrieved secure tokeni%s", "");
    return true;
  }

  bool SecureStorage::updateToken(const char *host,
                                  const char *username,
                                  const char *credType,
                                  const char *token)
  {
    SecureStorageImpl secStorage;
    if ( secStorage.updateToken(host, username, credType, token) != SUCCESS)
    {
      SF_ERROR_LOG("sf", "SecureStorage", "retrieveToken",
                   "Failed to update secure token%s", "");
      return false;
    }

    SF_DEBUG_LOG("sf", "SecureStorage", "updateToken",
                 "Successfully updated secure token%s", "");
    return true;
  }

  bool SecureStorage::removeToken(const char *host,
                                  const char *username,
                                  const char *credType)
  {
    SecureStorageImpl secStorage;
    if ( secStorage.removeToken(host, username, credType) != SUCCESS)
    {
      SF_ERROR_LOG("sf", "SecureStorage", "removeToken",
                   "Failed to remove secure token%s", "");
      return false;
    }
    SF_DEBUG_LOG("sf", "SecureStorage", "removeToken",
                 "Successfully removed secure token%s", "");
    return true;
  }
}
