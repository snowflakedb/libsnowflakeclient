/*
 * File:   SecureStorageWin.cpp *
 */

#ifdef _WIN32

#include "snowflake/SecureStorage.hpp"

#include <vector>
#include <sstream>
#include <algorithm>
#include <iterator>

#include <windows.h>
#include <wincred.h>

#include "../logger/SFLogger.hpp"

#define MAX_TOKEN_LEN 1024
#define DRIVER_NAME "SNOWFLAKE_ODBC_DRIVER"
#define COLON_CHAR_LENGTH 1
#define NULL_CHAR_LENGTH 1

namespace Snowflake
{

namespace Client
{

  SecureStorageStatus SecureStorage::storeToken(const SecureStorageKey& key, const std::string& token)
  {
    auto targetOpt = convertTarget(key);
    if (!targetOpt)
    {
      CXX_LOG_ERROR("Cannot store token. Failed to convert key to string.");
      return SecureStorageStatus::Error;
    }
    std::string target = targetOpt.get();

    std::wstring wide_target = std::wstring(target.begin(), target.end());

    CREDENTIALW creds = { 0 };
    creds.TargetName = (LPWSTR)wide_target.data();
    creds.CredentialBlobSize = token.size();
    creds.CredentialBlob = (LPBYTE)token.data();
    creds.Persist = CRED_PERSIST_LOCAL_MACHINE;
    creds.Type = CRED_TYPE_GENERIC;

    if (!CredWriteW(&creds, 0))
    {
      CXX_LOG_ERROR("Failed to store token.");
      return SecureStorageStatus::Error;
    }
    else
    {
      CXX_LOG_DEBUG("Successfully stored token");
      return SecureStorageStatus::Success;
    }
  }

  SecureStorageStatus SecureStorage::retrieveToken(const SecureStorageKey& key, std::string& token)
  {
    auto targetOpt = convertTarget(key);
    if (!targetOpt)
    {
      CXX_LOG_ERROR("Cannot retrieve token. Failed to convert key to string.");
      return SecureStorageStatus::Error;
    }
    std::string target = targetOpt.get();

    std::wstring wide_target = std::wstring(target.begin(), target.end());
    PCREDENTIALW retcreds = nullptr;

    if (!CredReadW(wide_target.data(), CRED_TYPE_GENERIC, 0, &retcreds))
    {
      CXX_LOG_ERROR("Failed to read target or could not find it");
      return SecureStorageStatus::Error;
    }

    CXX_LOG_DEBUG("Read the token now copying it");

    DWORD blobSize = retcreds->CredentialBlobSize;
    if (!blobSize)
    {
        return SecureStorageStatus::Error;
    }

    token = "";
    std::copy(
        retcreds->CredentialBlob, 
        retcreds->CredentialBlob + blobSize,
        std::back_insert_iterator<std::string>(token)
    );

    CXX_LOG_DEBUG("Successfully retrieved token.");
  
    CredFree(retcreds);
    return SecureStorageStatus::Success;
  }

  SecureStorageStatus SecureStorage::removeToken(const SecureStorageKey& key)
  {
    auto targetOpt = convertTarget(key);
    if (!targetOpt)
    {
      CXX_LOG_ERROR("Cannot remove token. Failed to convert key to string.");
      return SecureStorageStatus::Error;
    }
    std::string target = targetOpt.get();

    std::wstring wide_target = std::wstring(target.begin(), target.end());

    if (!CredDeleteW(wide_target.data(), CRED_TYPE_GENERIC, 0))
    {
      return SecureStorageStatus::Error;
    }

    CXX_LOG_DEBUG("Successfully removed id token");
    return SecureStorageStatus::Success;
  }
}

}

#endif