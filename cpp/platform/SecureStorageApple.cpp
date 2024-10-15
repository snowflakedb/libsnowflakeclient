/*
 * File:   SecureStorageApple.cpp *
 * Copyright (c) 2013-2020 Snowflake Computing
 */

#ifdef __APPLE__

#include "SecureStorageImpl.hpp"

#include "../logger/SFLogger.hpp"

#include "CoreFoundation/CoreFoundation.h"
#include "Security/Security.h"

#include <string>
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

  SECURE_STORAGE_STATUS SecureStorageImpl::storeToken(const std::string& host,
                                                      const std::string& username,
                                                      const std::string& credType,
                                                      const std::string& cred)
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
    std::string target = convertTarget(host, username, credType);

    keys[0] = kSecClass;
    keys[1] = kSecAttrServer;
    keys[2] = kSecAttrAccount;
    keys[3] = kSecValueData;

    values[0] = kSecClassInternetPassword;
    values[1] = CFStringCreateWithCString(kCFAllocatorDefault, target.c_str(), kCFStringEncodingUTF8);
    values[2] = CFStringCreateWithCString(kCFAllocatorDefault, username.c_str(), kCFStringEncodingUTF8);
    values[3] = CFStringCreateWithCString(kCFAllocatorDefault, cred.c_str(), kCFStringEncodingUTF8);

    CFDictionaryRef query;
    query = CFDictionaryCreate(kCFAllocatorDefault, (const void**) keys, (const void**) values, 4, NULL, NULL);

    OSStatus result = SecItemAdd(query, NULL);

    if (result == errSecDuplicateItem)
    {
      CXX_LOG_DEBUG("Token already exists, updating");
      return updateToken(host, username, credType, cred);
    }

    if (result != errSecSuccess)
    {
      CXX_LOG_ERROR("Failed to store secure token");
      return ERROR;
    }

    CXX_LOG_DEBUG("Successfully stored secure token");
    return SUCCESS;
  }

  SECURE_STORAGE_STATUS SecureStorageImpl::retrieveToken(const std::string& host,
                                                         const std::string& username,
                                                         const std::string& credType,
                                                         std::string& cred)
  {
    OSStatus result = errSecSuccess;
    SecKeychainItemRef pitem = NULL;
    UInt32 plength = 0;
    char *pdata = NULL;
    std::string target = convertTarget(host, username, credType);

    /*
     * https://developer.apple.com/documentation/security/1397763-seckeychainfindinternetpassword?language=objc
     */
    result = SecKeychainFindInternetPassword(NULL, target.size(),
                                             target.c_str(), 0, NULL,
                                             username.size(),
                                             username.c_str(), 0, NULL, 0,
                                             kSecProtocolTypeAny,
                                             kSecAuthenticationTypeAny,
                                             &plength, (void **)&pdata,
                                             &pitem);

    if (result == errSecItemNotFound)
    {
      cred = "";
      CXX_LOG_ERROR("Failed to retrieve secure token - %s", "Token Not Found");
      return NOT_FOUND;
    }

    if (result == errSecSuccess)
    {
      cred = "";
      CXX_LOG_ERROR("Failed to retrieve secure token");
      return ERROR;
    }

    if (plength > MAX_TOKEN_LEN)
    {
      CXX_LOG_ERROR("Failed to retrieve secure token - %s", "Stored token length greater than max allowed length");
      return ERROR;
    }

    CXX_LOG_DEBUG("Successfully retrieved token");
    cred = std::string(pdata, plength);
    return SUCCESS;
  }

  SECURE_STORAGE_STATUS SecureStorageImpl::updateToken(const std::string& host,
                                                       const std::string& username,
                                                       const std::string& credType,
                                                       const std::string& token)
  {
    OSStatus result = errSecSuccess;
    CFTypeRef keys[4];
    CFTypeRef values[4];
    std::string target = convertTarget(host, username, credType);

    keys[0] = kSecClass;
    keys[1] = kSecAttrServer;
    keys[2] = kSecAttrAccount;
    keys[3] = kSecMatchLimit;

    values[0] = kSecClassInternetPassword;
    values[1] = CFStringCreateWithCString(kCFAllocatorDefault, target.c_str(), kCFStringEncodingUTF8);
    values[2] = CFStringCreateWithCString(kCFAllocatorDefault, username.c_str(), kCFStringEncodingUTF8);
    values[3] = kSecMatchLimitOne;

    CFDictionaryRef extract_query = CFDictionaryCreate(kCFAllocatorDefault, (const void **)keys,
                                                       (const void **)values, 4, NULL, NULL);
    result = SecItemDelete(extract_query);

    if (result != errSecSuccess && result != errSecItemNotFound)
    {
      CXX_LOG_ERROR("Failed to update secure token");
      return ERROR;
    }

    return storeToken(host, username, credType, token);
  }

  SECURE_STORAGE_STATUS SecureStorageImpl::removeToken(const std::string& host,
                                                       const std::string& username,
                                                       const std::string& credType)
  {
    OSStatus result = errSecSuccess;
    CFTypeRef keys[4];
    CFTypeRef values[4];
    std::string target = convertTarget(host, username, credType);

    keys[0] = kSecClass;
    keys[1] = kSecAttrServer;
    keys[2] = kSecAttrAccount;
    keys[3] = kSecMatchLimit;

    values[0] = kSecClassInternetPassword;
    values[1] = CFStringCreateWithCString(kCFAllocatorDefault, target.c_str(), kCFStringEncodingUTF8);
    values[2] = CFStringCreateWithCString(kCFAllocatorDefault, username.c_str(), kCFStringEncodingUTF8);
    values[3] = kSecMatchLimitOne;

    CFDictionaryRef extract_query = CFDictionaryCreate(kCFAllocatorDefault, (const void **)keys,
                                                       (const void **)values, 4, NULL, NULL);
    result = SecItemDelete(extract_query);
    if (result != errSecSuccess && result != errSecItemNotFound)
    {
      CXX_LOG_ERROR("Failed to remove secure token");
      return ERROR;
    }

    CXX_LOG_DEBUG("Successfully removed secure token");
    return SUCCESS;
  }
}

#endif
