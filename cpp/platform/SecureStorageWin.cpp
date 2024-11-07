/*
 * File:   SecureStorage.cpp *
 * Copyright (c) 2013-2020 Snowflake Computing
 */

#include "Logger.hpp"
#include "windows.h"
#include "wincred.h"

#include "SecureStorageWin.hpp"
#include <vector>

#define MAX_TOKEN_LEN 1024
#define DRIVER_NAME "SNOWFLAKE_ODBC_DRIVER"
#define COLON_CHAR_LENGTH 1
#define NULL_CHAR_LENGTH 1

namespace sf
{

  void SecureStorageImpl::convertTarget(const char *host,
                                        const char *username,
                                        const char *credType,
                                        wchar_t *target_wcs,
                                        unsigned int max_len)
  {
    std::vector<char> buf(max_len);
    char* targetname(&buf[0]);
    size_t converted_len = 0;

    targetname[0] = '\0';
    strncat_s(targetname, max_len, host, strlen(host));
    strncat_s(targetname, max_len, ":", COLON_CHAR_LENGTH);
    strncat_s(targetname, max_len, username, strlen(username));
    strncat_s(targetname, max_len, ":", COLON_CHAR_LENGTH);
    strncat_s(targetname, max_len, DRIVER_NAME, strlen(DRIVER_NAME));
    strncat_s(targetname, max_len, ":", COLON_CHAR_LENGTH);
    strncat_s(targetname, max_len, credType, strlen(credType));

    mbstowcs_s(&converted_len, target_wcs, max_len, targetname, max_len);
  }

  SECURE_STORAGE_STATUS SecureStorageImpl::storeToken(const char *host,
                                                      const char *username,
                                                      const char *credType,
                                                      const char *token)
  {
    unsigned int max_len = strlen(host) + strlen(username) + strlen(DRIVER_NAME) +
            strlen(credType) + (3 * COLON_CHAR_LENGTH) + NULL_CHAR_LENGTH;
    std::vector<wchar_t> buf(max_len);
    wchar_t* target_wcs(buf.data());
    CREDENTIALW creds = { 0 };

    convertTarget(host, username, credType, target_wcs, max_len);

    creds.TargetName = target_wcs;
    creds.CredentialBlobSize = strlen(token);
    creds.CredentialBlob = (LPBYTE)token;
    creds.Persist = CRED_PERSIST_LOCAL_MACHINE;
    creds.Type = CRED_TYPE_GENERIC;

    if (!CredWriteW(&creds, 0))
    {
      SF_INFO_LOG("sf", "SecureStorageWin", "storeToken",
                  "Failed to store token.", "");
      return FAILURE;
    }
    else
    {
      SF_DEBUG_LOG("sf","SecureStorageWin","storeToken",
                   "Successfulyy stored id token","");
      return SUCCESS;
    }
  }

  SECURE_STORAGE_STATUS SecureStorageImpl::retrieveToken(const char *host,
                                                         const char *username,
                                                         const char *credType,
                                                         char *token,
                                                         size_t *token_len)
  {
    unsigned int max_len = strlen(host) + strlen(username) + strlen(DRIVER_NAME) +
                           strlen(credType) + (3 * COLON_CHAR_LENGTH) + NULL_CHAR_LENGTH;
    std::vector<wchar_t> buf(max_len);
    wchar_t* target_wcs(buf.data());
    PCREDENTIALW retcreds = { 0 };
    DWORD blobSize = 0;

    SF_DEBUG_LOG("sf", "SecureStorageWin", "retrieveToken",
                 "Inside retrieveToken implementation", "");

    convertTarget(host, username, credType, target_wcs, max_len);

    if (!CredReadW(target_wcs, CRED_TYPE_GENERIC, 0, &retcreds))
    {
      SF_INFO_LOG("sf", "SecureStorageWin", "retrieveToken",
                  "Failed to read target or could not find it", "");
      return FAILURE;
    }
    else
    {
      SF_DEBUG_LOG("sf", "SecureStorageWin", "retrieveToken",
                   "Read the token now copying it", "");

      blobSize = retcreds->CredentialBlobSize;
      if (!blobSize)
      {
         return FAILURE;
      }
      strncpy_s(token, MAX_TOKEN_LEN, (char *)retcreds->CredentialBlob, size_t(blobSize)+1);
      SF_DEBUG_LOG("sf", "SecureStorageWin", "retrieveToken",
                   "Read the token, copied it will return now.", "");
    }

    *token_len = size_t(blobSize);
    CredFree(retcreds);
    return SUCCESS;
  }

  SECURE_STORAGE_STATUS SecureStorageImpl::updateToken(const char *host,
                                                       const char *username,
                                                       const char *credType,
                                                       const char *token)
  {
    return storeToken(host, username, credType, token);
  }

  SECURE_STORAGE_STATUS SecureStorageImpl::removeToken(const char *host,
                                                       const char *username,
                                                       const char *credType)
  {
    unsigned int max_len = strlen(host) + strlen(username) + strlen(DRIVER_NAME) +
                           strlen(credType) + (3 * COLON_CHAR_LENGTH) + NULL_CHAR_LENGTH;
    std::vector<wchar_t> buf(max_len);
    wchar_t* target_wcs(buf.data());
    convertTarget(host, username, credType, target_wcs, max_len);

    if (!CredDeleteW(target_wcs, CRED_TYPE_GENERIC, 0))
    {
      return FAILURE;
    }
    else
    {
      return SUCCESS;
    }
  }
}
