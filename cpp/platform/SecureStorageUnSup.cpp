/*
 * File:   SecureStorageUnSup.cpp *
 * Copyright (c) 2013-2020 Snowflake Computing
 */

#include "SecureStorageUnSup.hpp"

namespace sf
{

  SECURE_STORAGE_STATUS SecureStorageImpl::storeToken(const char *host,
                                                      const char *username,
                                                      const char *credType,
                                                      const char *token)
  {
    return UNSUPPORTED;
  }

  SECURE_STORAGE_STATUS SecureStorageImpl::retrieveToken(const char *host,
                                                         const char *username,
                                                         const char *credType,
                                                         char *token,
                                                         size_t *token_len)
  {
    return UNSUPPORTED;
  }

  SECURE_STORAGE_STATUS SecureStorageImpl::updateToken(const char *host,
                                                       const char *username,
                                                       const char *credType,
                                                       const char *token)
  {
    return UNSUPPORTED;
  }

  SECURE_STORAGE_STATUS SecureStorageImpl::removeToken(const char *host,
                                                       const char *username,
                                                       const char *credType)
  {
    return UNSUPPORTED;
  }
}
