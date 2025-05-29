/*
 * File:   SecureStorageLinux.cpp *
 */

#ifdef __linux__

#include "snowflake/SecureStorage.hpp"

#include <cstring>
#include <map>
#include <string>

#include <picojson.h>

#include "../linux/CacheFile.hpp"
#include "../logger/SFLogger.hpp"
#include "../platform/FileLock.hpp"


namespace Snowflake {

namespace Client {

  SecureStorageStatus SecureStorage::storeToken(const SecureStorageKey& key, const std::string& token)
  {
    auto pathOpt = getCredentialFilePath();
    if (!pathOpt)
    {
      CXX_LOG_ERROR("Cannot get path to credential file.")
      return SecureStorageStatus::Error;
    }

    const std::string path = pathOpt.value();

    FileLock lock(path);
    if (!lock.isLocked())
    {
      CXX_LOG_ERROR("Failed to save token. Could not acquire file lock(path=%s)", lock.getPath().c_str());
      return SecureStorageStatus::Error;
    }

    picojson::value contents;
    std::string error = readFile(path, contents);
    if (!error.empty()) {
      CXX_LOG_WARN("Failed to read file(path=%s). Error: %s", path.c_str(), error.c_str())
      contents = picojson::value(picojson::object());
    }

    auto targetOpt = convertTarget(key);
    if (!targetOpt)
    {
      CXX_LOG_ERROR("Cannot store token. Failed to convert key to string.");
      return SecureStorageStatus::Error;
    }
    std::string& target = targetOpt.get();
    cacheFileUpdate(contents, target, token);

    error = writeFile(path, contents);
    if (!error.empty()) {
      CXX_LOG_ERROR("Failed to write file(path=%s). Error: %s", path.c_str(), error.c_str());
      return SecureStorageStatus::Error;
    }

    CXX_LOG_DEBUG("Successfully stored token.");
    return SecureStorageStatus::Success;
  }

  SecureStorageStatus SecureStorage::retrieveToken(const SecureStorageKey& key, std::string& token)
  {
    auto pathOpt = getCredentialFilePath();
    if (!pathOpt)
    {
      CXX_LOG_ERROR("Cannot get path to credential file.")
      return SecureStorageStatus::Error;
    }
    const std::string path = pathOpt.value();

    FileLock lock(path);
    if (!lock.isLocked())
    {
      CXX_LOG_ERROR("Failed to get token. Could not acquire file lock(path=%s)", lock.getPath().c_str());
      return SecureStorageStatus::Error;
    }

    picojson::value contents;

    std::string error = readFile(path, contents);
    if (!error.empty()) {
      CXX_LOG_WARN("Failed to read file(path=%s). Error: %s", path.c_str(), error.c_str());
      contents = picojson::value(picojson::object());
    }

    auto targetOpt = convertTarget(key);
    if (!targetOpt)
    {
      CXX_LOG_ERROR("Cannot retrieve token. Failed to convert key to string.");
      return SecureStorageStatus::Error;
    }
    std::string& target = targetOpt.get();
    auto tokenOpt = cacheFileGet(contents, target);
    if (!tokenOpt)
    {
      return SecureStorageStatus::NotFound;
    }
    token = tokenOpt.get();

    CXX_LOG_DEBUG("Successfully retrieved token.");
    return SecureStorageStatus::Success;
  }

  SecureStorageStatus SecureStorage::removeToken(const SecureStorageKey& key)
  {
    auto pathOpt = getCredentialFilePath();
    if (!pathOpt)
    {
      CXX_LOG_ERROR("Cannot get path to credential file.")
      return SecureStorageStatus::Error;
    }
    const std::string path = pathOpt.value();

    FileLock lock(path);
    if (!lock.isLocked())
    {
      CXX_LOG_ERROR("Failed to delete token. Could not acquire file lock(path=%s)", lock.getPath().c_str());
      return SecureStorageStatus::Error;
    }

    picojson::value contents;
    std::string error = readFile(path, contents);
    if (!error.empty())
    {
      CXX_LOG_WARN("Failed to read file(path=%s). Error: %s", path.c_str(), error.c_str())
      contents = picojson::value(picojson::object());
    }

    auto targetOpt = convertTarget(key);
    if (!targetOpt)
    {
      CXX_LOG_ERROR("Cannot remove token. Failed to convert key to string.");
      return SecureStorageStatus::Error;
    }
    std::string& target = targetOpt.get();
    cacheFileRemove(contents, target);

    error = writeFile(path, contents);
    if (!error.empty()) {
      CXX_LOG_ERROR("Failed to write file(path=%s). Error: %s", path.c_str(), error.c_str());
      return SecureStorageStatus::Error;
    }

    CXX_LOG_DEBUG("Successfully removed token.");
    return SecureStorageStatus::Success;
  }
}

}

#endif
