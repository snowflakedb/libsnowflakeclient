/*
 * File:   SecureStorage.cpp *
 * Copyright (c) 2013-2020 Snowflake Computing
 */

#include "../logger/SFLogger.hpp"
#ifdef __APPLE__
#include "SecureStorageImpl.hpp"
#elif _WIN32
#include "SecureStorageWin.hpp"
#else
#include "SecureStorageUnSup.hpp"
#endif

#include "SecureStorage.hpp"

namespace sf
{

  bool SecureStorage::storeToken(const std::string& host,
                                 const std::string& username,
                                 const std::string& credType,
                                 const std::string& token)
  {
    SecureStorageImpl secStorage;
    if (secStorage.storeToken(host, username, credType, token) != SUCCESS)
    {
      log_error("Failed to store secure token%s", "");
      return false;
    }

    log_debug("Successfully stored secure token%s", "");
    return true;
  }

  std::optional<std::string> SecureStorage::retrieveToken(const std::string& host,
                                                          const std::string& username,
                                                          const std::string& credType)
  {
    SecureStorageImpl secStorage;
    std::string result;
    if (secStorage.retrieveToken(host, username, credType, result) != SUCCESS)
    {
      log_error("Failed to retrieve secure token%s", "");
      return {};
    }

    log_debug("Successfully retrieved secure tokeni%s", "");
    return result;
  }

  bool SecureStorage::updateToken(const std::string& host,
                                  const std::string& username,
                                  const std::string& credType,
                                  const std::string& token)
  {
    SecureStorageImpl secStorage;
    if ( secStorage.updateToken(host, username, credType, token) != SUCCESS)
    {
      log_error("Failed to update secure token%s", "");
      return false;
    }

    log_debug("Successfully updated secure token%s", "");
    return true;
  }

  bool SecureStorage::removeToken(const std::string& host,
                                  const std::string& username,
                                  const std::string& credType)
  {
    SecureStorageImpl secStorage;
    if ( secStorage.removeToken(host, username, credType) != SUCCESS)
    {
      log_error("Failed to remove secure token%s", "");
      return false;
    }
    log_debug("Successfully removed secure token%s", "");
    return true;
  }

}
