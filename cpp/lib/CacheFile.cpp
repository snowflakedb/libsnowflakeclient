/*
 * File:   CacheFile.cpp *
 * Copyright (c) 2025 Snowflake Computing
 */

#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <boost/filesystem.hpp>

#include "picojson.h"

#include "CacheFile.hpp"
#include "snowflake/platform.h"
#include "../logger/SFLogger.hpp"
#include "../util/Sha256.hpp"
#include "snowflake/SecureStorage.hpp"

#if defined(__linux__) || defined(__APPLE__)
#include <sys/stat.h>
#endif

namespace Snowflake {

namespace Client {
  const char* CREDENTIAL_FILE_NAME = "credential_cache_v1.json";

  bool mkdirIfNotExists(const std::string& dir)
  {
    int result = sf_mkdir(dir.c_str());
    if (result == 0)
    {
      CXX_LOG_DEBUG("Created %s directory.", dir.c_str());
      return true;
    }

    if (errno == EEXIST)
    {
      CXX_LOG_TRACE("Directory %s already exists.", dir.c_str());
      return true;
    }

    CXX_LOG_ERROR("Failed to create %s directory. Error: %d", dir.c_str(), errno);
    return false;

  }

  boost::optional<std::string> getEnv(const std::string& envVar)
  {
    char *root = getenv(envVar.c_str());

    if (root == nullptr)
    {
      return {};
    }

    return std::string(root);
  }

  boost::optional<std::string> getCacheDir(const std::string& envVar, const std::vector<std::string>& subPathSegments)
  {
#ifdef __linux__
    auto envVarValueOpt = getEnv(envVar);
    if (!envVarValueOpt)
    {
      return {};
    }

    const std::string& envVarValue = envVarValueOpt.get();

    struct stat s = {};
    int err = stat(envVarValue.c_str(), &s);

    if (err != 0)
    {
      CXX_LOG_INFO("Failed to stat %s=%s, errno=%d. Skipping it in cache file location lookup.", envVar.c_str(), envVarValue.c_str(), errno);
      return {};
    }

    if (!S_ISDIR(s.st_mode))
    {
      CXX_LOG_INFO("%s=%s is not a directory. Skipping it in cache file location lookup.", envVar.c_str(), envVarValue.c_str());
      return {};
    }

    auto cacheDir = envVarValue;
    for (const auto& segment: subPathSegments)
    {
      cacheDir.append(PATH_SEP + segment);
      if (!mkdirIfNotExists(cacheDir))
      {
        CXX_LOG_INFO("Could not create cache dir=%s. Skipping it in cache file location lookup.", cacheDir.c_str());
        return {};
      }
    }

    if (!subPathSegments.empty())
    {
      err = stat(cacheDir.c_str(), &s);
      if (err != 0)
      {
        CXX_LOG_INFO("Failed to stat %s, errno=%d. Skipping it in cache file location lookup.", cacheDir.c_str(), errno);
        return {};
      }
    }

    if (s.st_uid != geteuid())
    {
      CXX_LOG_INFO("%s=%s is not owned by current user. Skipping it in cache file location lookup.", envVar.c_str(), envVarValue.c_str());
      return {};
    }

    unsigned permissions = s.st_mode & 0777;
    if (permissions != 0700)
    {
      CXX_LOG_INFO("Incorrect permissions=%o for cache dir %s. Changing permissions to 700.", permissions, cacheDir.c_str())
      if (chmod(cacheDir.c_str(), 0700) != 0)
      {
        CXX_LOG_WARN("Failed to change permissions for a cache dir %s, errno=%d. Skipping it in cache file location lookup.", cacheDir.c_str(), errno);
        return {};
      }
    }
    return cacheDir;
#else
    CXX_LOG_FATAL("Using NOOP implementation. This function is implemented only for linux.");
    return {};
#endif
  }

  boost::optional<std::string> getSfTemporaryCacheDir()
  {
    return getCacheDir("SF_TEMPORARY_CREDENTIAL_CACHE_DIR", {});
  }

  boost::optional<std::string> getXdgCacheDir()
  {
    return getCacheDir("XDG_CACHE_HOME", {"snowflake"});
  }

  boost::optional<std::string> getHomeCacheDir()
  {
    return getCacheDir("HOME", {".cache", "snowflake"});
  }

  boost::optional<std::string> getCredentialFilePath()
  {
    std::vector<std::function<boost::optional<std::string>()>> lookupFunctions =
    {
        getSfTemporaryCacheDir,
#ifdef __linux__
        getXdgCacheDir,
#endif
        getHomeCacheDir
    };

    for (const auto& lf: lookupFunctions) {
      boost::optional<std::string> directory = lf();
      if (directory)
      {
        auto path = directory.get() + PATH_SEP + CREDENTIAL_FILE_NAME;
        CXX_LOG_TRACE("Successfully found credential file path=%s", path.c_str());
        return path;
      }
    }

    return {};
  };

  boost::optional<std::string> credItemStr(const SecureStorageKey &key)
  {
    std::string plainTextKey = key.host + ":" + key.user + ":"  + keyTypeToString(key.type);
    return sha256(plainTextKey);
  }

  void ensureObject(picojson::value &val)
  {
    if (!val.is<picojson::object>())
    {
      val = picojson::value(picojson::object());
    }
  }

  picojson::object& getTokens(picojson::value& cache)
  {
    ensureObject(cache);
    auto &obj = cache.get<picojson::object>();
    auto pair = obj.emplace("tokens", picojson::value(picojson::object()));
    auto& tokens = pair.first->second;
    ensureObject(tokens);
    return tokens.get<picojson::object>();
  }


  std::string readFile(const std::string &path, picojson::value &result) {
    if (!boost::filesystem::exists(path))
    {
      result = picojson::value(picojson::object());
      return {};
    }

    std::ifstream cacheFile(path);
    if (!cacheFile.is_open())
    {
      return "Failed to open the file(path=" + path + ")";
    }

    std::string error = picojson::parse(result, cacheFile);
    if (!error.empty())
    {
      return "Failed to parse the file: " + error;
    }
    return {};
  }

#if defined(__linux__) || defined(__APPLE__)
  bool ensurePermissions(const std::string& path, mode_t mode)
  {
    if (chmod(path.c_str(), mode) == -1)
    {
      CXX_LOG_ERROR("Cannot ensure permissions. chmod(%s, %o) failed with errno=%d", path.c_str(), mode, errno);
      return false;
    }

    return true;
  }
#else
  bool ensurePermissions(const std::string& path, unsigned mode)
  {
    CXX_LOG_ERROR("Cannot ensure permissions on current platform");
    return false;
  }
#endif

  std::string writeFile(const std::string &path, const picojson::value &result) {
    std::ofstream cacheFile(path, std::ios_base::trunc);
    if (!cacheFile.is_open())
    {
      return "Failed to open the file";
    }

    if (!ensurePermissions(path, 0600))
    {
      return "Cannot ensure correct permissions on a file";
    }

    cacheFile << result.serialize(true);
    return {};
  }

  void cacheFileUpdate(picojson::value &cache, const std::string &key, const std::string &credential)
  {
    picojson::object& tokens = getTokens(cache);
    tokens.emplace(key, credential);
  }

  void cacheFileRemove(picojson::value &cache, const std::string &key)
  {
    picojson::object& tokens = getTokens(cache);
    tokens.erase(key);
  }

  boost::optional<std::string> cacheFileGet(picojson::value &cache, const std::string &key) {
    picojson::object& tokens = getTokens(cache);
    auto it = tokens.find(key);

    if (it == tokens.end())
    {
      return {};
    }

    if (!it->second.is<std::string>())
    {
      return {};
    }

    return it->second.get<std::string>();
  }

}

}
