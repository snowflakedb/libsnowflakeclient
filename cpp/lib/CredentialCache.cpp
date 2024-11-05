//
// Created by Jakub Szczerbinski on 10.10.24.
//

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

namespace sf {

  using namespace Snowflake::Client;

  class FileCredentialCache : public CredentialCache {
  public:
    explicit FileCredentialCache(std::string _path)
      : path{std::move(_path)}
    {}

    std::optional<std::string> get(const CredentialKey& key) override
    {
      FileLock lock(path);
      if (!lock.isLocked())
      {
        CXX_LOG_ERROR("Failed to get token. Could not acquire file lock(path=%s)", lock.getPath().c_str());
      }

      picojson::value contents;

      std::string error = readFile(path, contents);
      if (!error.empty()) {
        CXX_LOG_ERROR("Failed to read file(path=%s). Error: %s", path.c_str(), error.c_str())
        return {};
      }

      return cacheFileGet(contents, key);
    }

    bool save(const CredentialKey& key, const std::string& credential) override
    {
      FileLock lock(path);
      if (!lock.isLocked())
      {
        CXX_LOG_ERROR("Failed to save token. Could not acquire file lock(path=%s)", lock.getPath().c_str());
      }

      picojson::value contents;
      std::string error = readFile(path, contents);
      if (!error.empty()) {
        CXX_LOG_ERROR("Failed to read file(path=%s). Error: %s", path.c_str(), error.c_str())
        return false;
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
      FileLock lock(path);
      if (!lock.isLocked()) {
        CXX_LOG_ERROR("Failed to delete token. Could not acquire file lock(path=%s)", lock.getPath().c_str());
      }

      picojson::value contents;
      std::string error = readFile(path, contents);
      if (!error.empty()) {
        CXX_LOG_ERROR("Failed to read file(path=%s). Error: %s", path.c_str(), error.c_str())
        return false;
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

  private:
    std::string path;
  };

  class SecureStorageCredentialCache : public CredentialCache {
  public:
    std::optional<std::string> get(const CredentialKey& key) override
    {
      return storage.retrieveToken(key.host, key.user, credTypeToString(key.type));
    }

    bool save(const CredentialKey& key, const std::string& credential) override
    {
      return storage.storeToken(key.host, key.user, credTypeToString(key.type), credential);
    }

    bool remove(const CredentialKey& key) override
    {
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
    return new FileCredentialCache(std::string("file.txt"));
#endif
  }

};

#ifdef __cplusplus
extern "C" {
#endif

cred_cache_ptr cred_cache_init() {
    return sf::CredentialCache::make();
}

char* cred_cache_get_credential(cred_cache_ptr tc, const char* account, const char* host, const char* user, CredentialType type)
{
  sf::CredentialKey key = { account, host, user, type };
  auto tokenOpt = reinterpret_cast<sf::CredentialCache *>(tc)->get(key);
  if (!tokenOpt) {
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

void cred_cache_save_credential(cred_cache_ptr tc, const char* account, const char* host, const char* user, CredentialType type, const char *cred)
{
  sf::CredentialKey key = { account, host, user, type };
  reinterpret_cast<sf::CredentialCache *>(tc)->save(key, std::string(cred));
}

void cred_cache_remove_credential(cred_cache_ptr tc, const char* account, const char* host, const char* user, CredentialType type)
{
  sf::CredentialKey key = { account, host, user, type };
  reinterpret_cast<sf::CredentialCache *>(tc)->remove(key);
}

void cred_cache_term(cred_cache_ptr tc) {
    delete reinterpret_cast<sf::CredentialCache*>(tc);
}

#ifdef __cplusplus
};
#endif

