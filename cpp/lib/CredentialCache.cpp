/*
 * File:   CredentialCache.cpp *
 * Copyright (c) 2025 Snowflake Computing
 */

#include <cstring>
#include <fstream>
#include <sstream>
#include <map>

#include "picojson.h"

#include "snowflake/mfa_token_cache.h"

#include "CredentialCache.hpp"
#include "../platform/SecureStorage.hpp"
#include "CacheFile.hpp"
#include "../logger/SFLogger.hpp"
#include "../platform/FileLock.hpp"

namespace Snowflake {

namespace Client {

  class FileCredentialCache : public CredentialCache {
  public:

    boost::optional<std::string> get(const CredentialKey& key) override
    {
      auto pathOpt = getCredentialFilePath();
      if (!pathOpt)
      {
        CXX_LOG_ERROR("Cannot get path to credential file.")
        return {};
      }
      const std::string path = pathOpt.value();

      FileLock lock(path);
      if (!lock.isLocked())
      {
        CXX_LOG_ERROR("Failed to get token. Could not acquire file lock(path=%s)", lock.getPath().c_str());
      }

      picojson::value contents;

      std::string error = readFile(path, contents);
      if (!error.empty()) {
        CXX_LOG_WARN("Failed to read file(path=%s). Error: %s", path.c_str(), error.c_str());
        contents = picojson::value(picojson::object());
      }

      return cacheFileGet(contents, key);
    }

    bool save(const CredentialKey& key, const std::string& credential) override
    {
      auto pathOpt = getCredentialFilePath();
      if (!pathOpt)
      {
        CXX_LOG_ERROR("Cannot get path to credential file.")
        return {};
      }

      const std::string path = pathOpt.value();

      FileLock lock(path);
      if (!lock.isLocked())
      {
        CXX_LOG_ERROR("Failed to save token. Could not acquire file lock(path=%s)", lock.getPath().c_str());
      }

      picojson::value contents;
      std::string error = readFile(path, contents);
      if (!error.empty()) {
        CXX_LOG_WARN("Failed to read file(path=%s). Error: %s", path.c_str(), error.c_str())
        contents = picojson::value(picojson::object());
      }

      cacheFileUpdate(contents, key, credential);

      error = writeFile(path, contents);
      if (!error.empty()) {
        CXX_LOG_ERROR("Failed to write file(path=%s). Error: %s", path.c_str(), error.c_str());
        return false;
      }

      return true;
    }

    bool remove(const CredentialKey& key) override
    {
      auto pathOpt = getCredentialFilePath();
      if (!pathOpt)
      {
        CXX_LOG_ERROR("Cannot get path to credential file.")
        return {};
      }
      const std::string path = pathOpt.value();

      FileLock lock(path);
      if (!lock.isLocked())
      {
        CXX_LOG_ERROR("Failed to delete token. Could not acquire file lock(path=%s)", lock.getPath().c_str());
      }

      picojson::value contents;
      std::string error = readFile(path, contents);
      if (!error.empty())
      {
        CXX_LOG_WARN("Failed to read file(path=%s). Error: %s", path.c_str(), error.c_str())
        contents = picojson::value(picojson::object());
      }

      cacheFileRemove(contents, key);

      error = writeFile(path, contents);
      if (!error.empty()) {
        CXX_LOG_ERROR("Failed to write file(path=%s). Error: %s", path.c_str(), error.c_str());
        return false;
      }
      return true;
    }

    ~FileCredentialCache() override = default;
  };

  class SecureStorageCredentialCache : public CredentialCache {
  public:
    boost::optional<std::string> get(const CredentialKey &key) override {
      return storage.retrieveToken(key.host, key.user, credTypeToString(key.type));
    }

    bool save(const CredentialKey &key, const std::string &credential) override {
      return storage.storeToken(key.host, key.user, credTypeToString(key.type), credential);
    }

    bool remove(const CredentialKey &key) override {
      return storage.removeToken(key.host, key.user, credTypeToString(key.type));
    }

    ~SecureStorageCredentialCache() override = default;

  private:
    SecureStorage storage;
  };

  CredentialCache *CredentialCache::make() {
#if defined(_WIN32) || defined(__APPLE__)
    return new SecureStorageCredentialCache();
#else
    return new FileCredentialCache();
#endif
  }

}

}


#ifdef __cplusplus
extern "C" {
  using namespace Snowflake::Client;
#endif

cred_cache_ptr cred_cache_init() {
    return CredentialCache::make();
}

char* cred_cache_get_credential(cred_cache_ptr tc, const char* host, const char* user, CredentialType type)
{
  Snowflake::Client::CredentialKey key = { host, user, type };
  auto tokenOpt = reinterpret_cast<Snowflake::Client::CredentialCache *>(tc)->get(key);
  if (!tokenOpt)
  {
      return nullptr;
  }
  size_t result_size = tokenOpt->size() + 1;
  char* result = new char[result_size];
  strncpy(result, tokenOpt->c_str(), result_size);
  return result;
}

void cred_cache_free_credential(char* cred) {
    delete[] cred;
}

void cred_cache_save_credential(cred_cache_ptr tc, const char* host, const char* user, CredentialType type, const char *cred)
{
  Snowflake::Client::CredentialKey key = { host, user, type };
  reinterpret_cast<Snowflake::Client::CredentialCache *>(tc)->save(key, std::string(cred));
}

void cred_cache_remove_credential(cred_cache_ptr tc, const char* host, const char* user, CredentialType type)
{
  Snowflake::Client::CredentialKey key = { host, user, type };
  reinterpret_cast<Snowflake::Client::CredentialCache *>(tc)->remove(key);
}

void cred_cache_term(cred_cache_ptr tc) {
    delete reinterpret_cast<Snowflake::Client::CredentialCache*>(tc);
}

#ifdef __cplusplus
};
#endif

