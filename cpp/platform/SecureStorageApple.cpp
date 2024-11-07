/*
 * File:   SecureStorageApple.cpp *
 * Copyright (c) 2013-2020 Snowflake Computing
 */

#include <string.h>
#include "Snowflake.h"
#include "Logger.hpp"
#include "CoreFoundation/CoreFoundation.h"
#include "Security/Security.h"

#include "SecureStorageApple.hpp"

#define MAX_TOKEN_LEN 1024
#define DRIVER_NAME "SNOWFLAKE_ODBC_DRIVER"
#define COLON_CHAR_LENGTH 1
#define NULL_CHAR_LENGTH 1

using namespace Simba;

namespace sf
{

  void SecureStorageImpl::convertTarget(const char *host,
                                        const char *username,
                                        const char *credType,
                                        char *targetname,
                                        size_t max_len)
  {
    simba_strcat(targetname, max_len, host);
    simba_strcat(targetname, max_len, ":");
    simba_strcat(targetname, max_len, username);
    simba_strcat(targetname, max_len, ":");
    simba_strcat(targetname, max_len, DRIVER_NAME);
    simba_strcat(targetname, max_len, ":");
    simba_strcat(targetname, max_len, credType);
  }

  SECURE_STORAGE_STATUS SecureStorageImpl::storeToken(const char *host,
                                                      const char *username,
                                                      const char *credType,
                                                      const char *token)
  {
    /*
     * More on OS X Types can be read here:
     * https://developer.apple.com/documentation/corefoundation?language=objc
     */
    CFTypeRef keys[4];
    CFTypeRef values[4];
    /*
     * Concatenate Host & Username in the format:
     * `host:username:drivername:credType'
     * the target_max_len hence becomes :
     * strlen(host)+strlen(username)+strlen(drivername)+strlen(credType)+ (3 * strlen(':'))
     * Add 1 to accommodate the NULL character.
     */
    size_t target_max_len = strlen(host)+strlen(username)+strlen(DRIVER_NAME)+
            strlen(credType) + (3*COLON_CHAR_LENGTH) + NULL_CHAR_LENGTH;
    char *targetname = new char[target_max_len];
    convertTarget(host, username, credType, targetname, target_max_len);


    keys[0] = kSecClass;
    keys[1] = kSecAttrServer;
    keys[2] = kSecAttrAccount;
    keys[3] = kSecValueData;

    values[0] = kSecClassInternetPassword;
    values[1] = CFStringCreateWithCString(kCFAllocatorDefault, targetname, kCFStringEncodingUTF8);
    values[2] = CFStringCreateWithCString(kCFAllocatorDefault, username, kCFStringEncodingUTF8);
    values[3] = CFStringCreateWithCString(kCFAllocatorDefault, token, kCFStringEncodingUTF8);

    CFDictionaryRef query;
    query = CFDictionaryCreate(kCFAllocatorDefault, (const void**) keys, (const void**) values, 4, NULL, NULL);

    OSStatus result = SecItemAdd(query, NULL);

    if (result == errSecDuplicateItem)
    {
      SF_DEBUG_LOG("sf", "SecureStorageApple", "storeToken",
                   "Token already exists, updatingi%s", "");
      return updateToken(host, username, credType, token);
    }
    else if (result == errSecSuccess)
    {
      SF_DEBUG_LOG("sf", "SecureStorageApple", "storeToken",
                   "Successfully stored secure token%s", "");
      return SUCCESS;
    }
    else
    {
      SF_ERROR_LOG("sf", "SecureStorageApple", "storeToken",
                   "Failed to store secure token%s", "");
      return ERROR;
    }
  }

  SECURE_STORAGE_STATUS SecureStorageImpl::retrieveToken(const char *host,
                                                         const char *username,
                                                         const char *credType,
                                                         char *token,
                                                         size_t *tokenLen)
  {
    OSStatus result = errSecSuccess;
    SecKeychainItemRef pitem = NULL;
    UInt32 plength = 0;
    char *pdata = NULL;
    size_t target_max_len = strlen(host)+strlen(username)+strlen(DRIVER_NAME)+
        strlen(credType) + (3*COLON_CHAR_LENGTH) + NULL_CHAR_LENGTH;
    char *targetname = new char[target_max_len];
    convertTarget(host, username, credType, targetname, target_max_len);

    if (tokenLen == NULL)
    {
      // tokenLen is expected to be Non NULL
      SF_ERROR_LOG("sf", "SecureStorageApple", "retrieveToken",
                   "Token len has to be allocated%s", "");
      return ERROR;
    }

    /*
     * https://developer.apple.com/documentation/security/1397763-seckeychainfindinternetpassword?language=objc
     */
    result = SecKeychainFindInternetPassword(NULL, (uint32)strlen(targetname),
                                             targetname, 0, NULL,
                                             (uint32) strlen(username),
                                             username, 0, NULL, 0,
                                             kSecProtocolTypeAny,
                                             kSecAuthenticationTypeAny,
                                             &plength, (void **)&pdata,
                                             &pitem);

    if (result == errSecItemNotFound)
    {
      *tokenLen = 0;
      token = NULL;
      SF_ERROR_LOG("sf", "SecureStorageApple", "storeToken",
                   "Failed to retrieve secure token - %s", "Token Not Found");
      return NOT_FOUND;
    }
    else if (result == errSecSuccess)
    {
      *tokenLen = plength;
      if (plength > MAX_TOKEN_LEN)
      {
        SF_ERROR_LOG("sf", "SecureStorageApple", "retrieveToken",
                     "Failed to retrieve secure token - %s", "Stored token length greater than max allowed length");
        return ERROR;
      }

      SF_DEBUG_LOG("sf", "SecureStorageApple", "retrieveToken",
                   "Successfully retrieved token%s", "");
      strlcpy(token, pdata, plength+1);
      return SUCCESS;
    }
    else
    {
      *tokenLen = 0;
      token = NULL;
      SF_ERROR_LOG("sf", "SecureStorageApple", "retrieveToken",
                   "Failed to retrieve secure token%s", "");
      return ERROR;
    }
  }

  SECURE_STORAGE_STATUS SecureStorageImpl::updateToken(const char *host,
                                                       const char *username,
                                                       const char *credType,
                                                       const char *token)
  {
    OSStatus result = errSecSuccess;
    CFTypeRef keys[4];
    CFTypeRef values[4];
    size_t target_max_len = strlen(host)+strlen(username)+strlen(DRIVER_NAME)+
                            strlen(credType) + (3*COLON_CHAR_LENGTH) + NULL_CHAR_LENGTH;
    char *targetname = new char[target_max_len];
    convertTarget(host, username, credType, targetname, target_max_len);

    keys[0] = kSecClass;
    keys[1] = kSecAttrServer;
    keys[2] = kSecAttrAccount;
    keys[3] = kSecMatchLimit;

    values[0] = kSecClassInternetPassword;
    values[1] = CFStringCreateWithCString(kCFAllocatorDefault, targetname, kCFStringEncodingUTF8);
    values[2] = CFStringCreateWithCString(kCFAllocatorDefault, username, kCFStringEncodingUTF8);
    values[3] = kSecMatchLimitOne;

    CFDictionaryRef extract_query = CFDictionaryCreate(kCFAllocatorDefault, (const void **)keys,
                                                       (const void **)values, 4, NULL, NULL);
    result = SecItemDelete(extract_query);

    if (result != errSecSuccess && result != errSecItemNotFound)
    {
      SF_ERROR_LOG("sf", "SecureStorageApple", "retrieveToken",
                   "Failed to update secure token%s", "");
      return ERROR;
    }

    return storeToken(host, username, credType, token);
  }

  SECURE_STORAGE_STATUS SecureStorageImpl::removeToken(const char *host,
                                                       const char *username,
                                                       const char *credType)
  {
    OSStatus result = errSecSuccess;
    CFTypeRef keys[4];
    CFTypeRef values[4];
    size_t target_max_len = strlen(host)+strlen(username)+strlen(DRIVER_NAME)+
                            strlen(credType) + (3*COLON_CHAR_LENGTH) + NULL_CHAR_LENGTH;
    char *targetname = new char[target_max_len];
    convertTarget(host, username, credType, targetname, target_max_len);

    keys[0] = kSecClass;
    keys[1] = kSecAttrServer;
    keys[2] = kSecAttrAccount;
    keys[3] = kSecMatchLimit;

    values[0] = kSecClassInternetPassword;
    values[1] = CFStringCreateWithCString(kCFAllocatorDefault, targetname, kCFStringEncodingUTF8);
    values[2] = CFStringCreateWithCString(kCFAllocatorDefault, username, kCFStringEncodingUTF8);
    values[3] = kSecMatchLimitOne;

    CFDictionaryRef extract_query = CFDictionaryCreate(kCFAllocatorDefault, (const void **)keys,
                                                       (const void **)values, 4, NULL, NULL);
    result = SecItemDelete(extract_query);
    if (result != errSecSuccess && result != errSecItemNotFound)
    {
      SF_ERROR_LOG("sf", "SecureStorageApple", "removeToken",
                   "Failed to remove secure token%s", "");
      return ERROR;
    }

    SF_DEBUG_LOG("sf", "SecureStorageApple", "removeToken",
                 "Successfully removed secure token%s", "");
    return SUCCESS;
  }
}
