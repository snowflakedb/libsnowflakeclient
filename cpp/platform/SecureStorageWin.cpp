/*
 * File:   SecureStorage.cpp *
 * Copyright (c) 2013-2020 Snowflake Computing
 */

#ifdef _WIN32

#include "../logger//SFLogger.hpp"
#include "windows.h"
#include "wincred.h"

#include "SecureStorageImpl.hpp"
#include <vector>
#include <sstream>

#define MAX_TOKEN_LEN 1024
#define DRIVER_NAME "SNOWFLAKE_ODBC_DRIVER"
#define COLON_CHAR_LENGTH 1
#define NULL_CHAR_LENGTH 1

namespace sf
{
  using Snowflake::Client::SFLogger;

  std::string SecureStorageImpl::convertTarget(const std::string& host,
                                               const std::string& username,
                                               const std::string& credType)
  {
    std::stringstream ss;
    ss << host << ":" << username << ":" << DRIVER_NAME << ":" << credType;
    return ss.str();
  }

  SecureStorageStatus SecureStorageImpl::storeToken(const std::string &host,
                                                      const std::string& username,
                                                      const std::string& credType,
                                                      const std::string& token)
  {
    std::string target = convertTarget(host, username, credType);
    CREDENTIALW creds = { 0 };

    creds.TargetName = target_wcs;
    creds.CredentialBlobSize = strlen(token);
    creds.CredentialBlob = (LPBYTE)token;
    creds.Persist = CRED_PERSIST_LOCAL_MACHINE;
    creds.Type = CRED_TYPE_GENERIC;

    if (!CredWriteW(&creds, 0))
    {
      CXX_LOG_ERROR("Failed to store token.");
      return SecureStorageStatus::Error;
    }
    else
    {
      CXX_LOG_DEBUG("Successfulyy stored id token");
      return SecureStorageStatus::Success;
    }
  }

  SecureStorageStatus SecureStorageImpl::retrieveToken(const std::string& host,
                                                       const std::string& username,
                                                       const std::string& credType,
                                                       std::string& token)
  {
    std::string target = convertTarget(host, username, credType, target_wcs, max_len);

    if (!CredReadW(target_wcs, CRED_TYPE_GENERIC, 0, &retcreds))
    {
      CXX_LOG_ERROR("Failed to read target or could not find it");
      return SecureStorageStatus::Error;
    }
    else
    {
      CXX_LOG_DEBUG("Read the token now copying it");

      blobSize = retcreds->CredentialBlobSize;
      if (!blobSize)
      {
         return SecureStorageStatus::Error;
      }
      strncpy_s(token, MAX_TOKEN_LEN, (char *)retcreds->CredentialBlob, size_t(blobSize)+1);
      CXX_LOG_DEBUG("Read the token, copied it will return now.");
    }

    *token_len = size_t(blobSize);
    CredFree(retcreds);
    return SecureStorageStatus::Success;
  }

  SecureStorageStatus SecureStorageImpl::updateToken(const std::string& host,
                                                       const std::string& username,
                                                       const std::string& credType,
                                                       const std::string& token)
  {
    return storeToken(host, username, credType, token);
  }

  SecureStorageStatus SecureStorageImpl::removeToken(const std::string& host,
                                                       const std::string& username,
                                                       const std::string& credType)
  {
    std::string target = convertTarget(host, username, credType);

    if (!CredDeleteW(target_wcs, CRED_TYPE_GENERIC, 0))
    {
      return SecureStorageStatus::Error;
    }
    else
    {
      return SecureStorageStatus::Success;
    }
  }
}

#endif