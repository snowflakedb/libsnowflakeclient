/*
 * File:   SecureStorageUnSup.cpp *
 * Copyright (c) 2013-2020 Snowflake Computing
 */

#if !(defined(__APPLE__) || defined(_WIN32))

#include "SecureStorageImpl.hpp"

#include <string>

namespace Snowflake {

namespace Client {

  SecureStorageStatus SecureStorageImpl::storeToken(const std::string &,
                                                    const std::string &,
                                                    const std::string &,
                                                    const std::string &)
  {
    return SecureStorageStatus::Unsupported;
  }

  SecureStorageStatus SecureStorageImpl::retrieveToken(const std::string &,
                                                       const std::string &,
                                                       const std::string &,
                                                       std::string &)
  {
    return SecureStorageStatus::Unsupported;
  }

  SecureStorageStatus SecureStorageImpl::updateToken(const std::string &,
                                                     const std::string &,
                                                     const std::string &,
                                                     const std::string &)
  {
    return SecureStorageStatus::Unsupported;
  }

  SecureStorageStatus SecureStorageImpl::removeToken(const std::string &,
                                                     const std::string &,
                                                     const std::string &)
  {
    return SecureStorageStatus::Unsupported;
  }
}

}

#endif
