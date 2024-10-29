/*
 * File:   SecureStorageUnSup.cpp *
 * Copyright (c) 2013-2020 Snowflake Computing
 */

#if !(defined(__APPLE__) || defined(_WIN32))

#include "SecureStorageImpl.hpp"

#include <string>

namespace sf
{

  SECURE_STORAGE_STATUS SecureStorageImpl::storeToken(const std::string& host,
                                                      const std::string& username,
                                                      const std::string& credType,
                                                      const std::string& token)
  {
    return UNSUPPORTED;
  }

  SECURE_STORAGE_STATUS SecureStorageImpl::retrieveToken(const std::string& host,
                                                         const std::string& username,
                                                         const std::string& credType,
                                                         std::string& token)
  {
    return UNSUPPORTED;
  }

  SECURE_STORAGE_STATUS SecureStorageImpl::updateToken(const std::string& host,
                                                       const std::string& username,
                                                       const std::string& credType,
                                                       const std::string& token)
  {
    return UNSUPPORTED;
  }

  SECURE_STORAGE_STATUS SecureStorageImpl::removeToken(const std::string& host,
                                                       const std::string& username,
                                                       const std::string& credType)
  {
    return UNSUPPORTED;
  }
}

#endif
