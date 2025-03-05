/*
 * File:   SecureStorageApple.cpp *
 * Copyright (c) 2013-2025 Snowflake Computing
 */

#ifdef __APPLE__

#include "snowflake/SecureStorage.hpp"

#include <string>
#include <sstream>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include "../logger/SFLogger.hpp"

#define MAX_TOKEN_LEN 1024
#define COLON_CHAR_LENGTH 1
#define NULL_CHAR_LENGTH 1

namespace Snowflake
{

namespace Client
{

  using Snowflake::Client::SFLogger;

  SecureStorageStatus SecureStorage::storeToken(const SecureStorageKey& key, const std::string& cred)
  {
    auto targetOpt = convertTarget(key);
    if (!targetOpt)
    {
      CXX_LOG_ERROR("Cannot store token. Failed to convert key to string.");
      return SecureStorageStatus::Error;
    }
    std::string& target = targetOpt.get();
    bool first_try = true;

    do {
      CFTypeRef keys[4];
      CFTypeRef values[4];

      keys[0] = kSecClass;
      keys[1] = kSecAttrServer;
      keys[2] = kSecAttrAccount;
      keys[3] = kSecValueData;

      values[0] = kSecClassInternetPassword;
      values[1] = CFStringCreateWithCString(kCFAllocatorDefault, target.c_str(), kCFStringEncodingUTF8);
      values[2] = CFStringCreateWithCString(kCFAllocatorDefault, key.user.c_str(), kCFStringEncodingUTF8);
      values[3] = CFStringCreateWithCString(kCFAllocatorDefault, cred.c_str(), kCFStringEncodingUTF8);

      CFDictionaryRef query = CFDictionaryCreate(kCFAllocatorDefault, (const void **) keys, (const void **) values, 4, NULL, NULL);

      OSStatus result = SecItemAdd(query, NULL);

      if (first_try && result == errSecDuplicateItem) {
        CXX_LOG_DEBUG("Token already exists, updating");
        removeToken(key);
        first_try = false;
        continue;
      }

      if (result != errSecSuccess)
      {
        CXX_LOG_ERROR("Failed to store secure token");
        return SecureStorageStatus::Error;
      }

      CXX_LOG_DEBUG("Successfully stored secure token");
      return SecureStorageStatus::Success;
    }
    while(true);
  }

  SecureStorageStatus SecureStorage::retrieveToken(const SecureStorageKey& key, std::string& cred)
  {
    auto targetOpt = convertTarget(key);
    if (!targetOpt)
    {
      CXX_LOG_ERROR("Cannot retrieve token. Failed to convert key to string.");
      return SecureStorageStatus::Error;
    }
    std::string& target = targetOpt.get();

    CFTypeRef keys[5];
    keys[0] = kSecClass;
    keys[1] = kSecAttrServer;
    keys[2] = kSecAttrAccount;
    keys[3] = kSecReturnData;
    keys[4] = kSecReturnAttributes;

    CFTypeRef values[5];
    values[0] = kSecClassInternetPassword;
    values[1] = CFStringCreateWithCString(kCFAllocatorDefault, target.c_str(), kCFStringEncodingUTF8);
    values[2] = CFStringCreateWithCString(kCFAllocatorDefault, key.user.c_str(), kCFStringEncodingUTF8);
    values[3] = kCFBooleanTrue;
    values[4] = kCFBooleanTrue;

    CFDictionaryRef query = CFDictionaryCreate(kCFAllocatorDefault, (const void**) keys, (const void**) values, 5, NULL, NULL);
    CFDictionaryRef result;
    OSStatus status = SecItemCopyMatching(query, reinterpret_cast<CFTypeRef *>(&result));

    if (status == errSecItemNotFound)
    {
      cred = "";
      CXX_LOG_ERROR("Failed to retrieve secure token. Reason: token not found.");
      return SecureStorageStatus::NotFound;
    }

    if (status != errSecSuccess)
    {
      cred = "";
      CXX_LOG_ERROR("Failed to retrieve secure token");
      return SecureStorageStatus::Error;
    }

    CXX_LOG_DEBUG("Successfully retrieved token");

    auto val = reinterpret_cast<CFDataRef>(CFDictionaryGetValue(result, kSecValueData));
    cred = std::string(reinterpret_cast<const char*>(CFDataGetBytePtr(val)), CFDataGetLength(val));
    return SecureStorageStatus::Success;
  }

  SecureStorageStatus SecureStorage::removeToken(const SecureStorageKey& key)
  {
    CFTypeRef keys[4];
    CFTypeRef values[4];
    auto targetOpt = convertTarget(key);
    if (!targetOpt)
    {
      CXX_LOG_ERROR("Cannot remove token. Failed to convert key to string.");
      return SecureStorageStatus::Error;
    }
    std::string& target = targetOpt.get();

    keys[0] = kSecClass;
    keys[1] = kSecAttrServer;
    keys[2] = kSecAttrAccount;
    keys[3] = kSecMatchLimit;

    values[0] = kSecClassInternetPassword;
    values[1] = CFStringCreateWithCString(kCFAllocatorDefault, target.c_str(), kCFStringEncodingUTF8);
    values[2] = CFStringCreateWithCString(kCFAllocatorDefault, key.user.c_str(), kCFStringEncodingUTF8);
    values[3] = kSecMatchLimitOne;

    CFDictionaryRef extract_query = CFDictionaryCreate(kCFAllocatorDefault, (const void **)keys,
                                                       (const void **)values, 4, NULL, NULL);
    OSStatus result = SecItemDelete(extract_query);
    switch (result)
    {
      case errSecSuccess:
        CXX_LOG_DEBUG("Successfully removed secure token");
        break;
      case errSecItemNotFound:
        CXX_LOG_WARN("Failed to remove token: not found.")
        break;
      default:
        CXX_LOG_ERROR("Failed to remove secure token, %d", result);
        return SecureStorageStatus::Error;
    }

    return SecureStorageStatus::Success;
  }
}

}

#endif
