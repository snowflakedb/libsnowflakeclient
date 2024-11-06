/*
 * File:   SecureStorage.cpp *
 * Copyright (c) 2013-2020 Snowflake Computing
 */

#include "../logger/SFLogger.hpp"
#include "SecureStorageImpl.hpp"

#include "SecureStorage.hpp"

namespace Snowflake {

namespace Client {

  using Snowflake::Client::SFLogger;

  bool SecureStorage::storeToken(const std::string& host,
                                 const std::string& username,
                                 const std::string& credType,
                                 const std::string& token)
  {
    SecureStorageImpl secStorage;
    if (secStorage.storeToken(host, username, credType, token) != SecureStorageStatus::Success)
    {
      CXX_LOG_ERROR("Failed to store secure token");
      return false;
    }

    CXX_LOG_DEBUG("Successfully stored secure token");
    return true;
  }

  boost::optional<std::string> SecureStorage::retrieveToken(const std::string& host,
                                                          const std::string& username,
                                                          const std::string& credType)
  {
    SecureStorageImpl secStorage;
    std::string result;
    if (secStorage.retrieveToken(host, username, credType, result) != SecureStorageStatus::Success)
    {
      CXX_LOG_ERROR("Failed to retrieve secure token");
      return {};
    }

    CXX_LOG_DEBUG("Successfully retrieved secure token");
    return result;
  }

  bool SecureStorage::updateToken(const std::string& host,
                                  const std::string& username,
                                  const std::string& credType,
                                  const std::string& token)
  {
    SecureStorageImpl secStorage;
    if ( secStorage.updateToken(host, username, credType, token) != SecureStorageStatus::Success)
    {
      CXX_LOG_ERROR("Failed to update secure token");
      return false;
    }

    CXX_LOG_DEBUG("Successfully updated secure token");
    return true;
  }

  bool SecureStorage::removeToken(const std::string& host,
                                  const std::string& username,
                                  const std::string& credType)
  {
    SecureStorageImpl secStorage;
    if ( secStorage.removeToken(host, username, credType) != SecureStorageStatus::Success)
    {
      CXX_LOG_ERROR("Failed to remove secure token");
      return false;
    }
    CXX_LOG_DEBUG("Successfully removed secure token");
    return true;
  }

}

}
