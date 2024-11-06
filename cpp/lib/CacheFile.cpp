//
// Created by Jakub Szczerbinski on 14.10.24.
//

#include <fstream>
#include <string>
#include <boost/filesystem.hpp>

#include "picojson.h"

#include "CacheFile.hpp"
#include "CredentialCache.hpp"
#include "snowflake/platform.h"
#include "../logger/SFLogger.hpp"

#if defined(__linux__) || defined(__APPLE__)
#include <sys/stat.h>
#endif

namespace Snowflake {

namespace Client {

  const char* CREDENTIAL_FILE_NAME = "temporary_credential.json";

  const std::vector<std::string> CACHE_ROOT_ENV_VARS =
  {
      "SF_TEMPORARY_CREDENTIAL_CACHE_DIR",
      "HOME",
      "TMP"
  };

  const std::vector<std::string> CACHE_DIR_NAMES =
  {
      ".cache",
      "snowflake"
  };


  bool mkdirIfNotExists(const char *dir)
  {
    int result = sf_mkdir(dir);
    if (result == 0)
    {
      CXX_LOG_DEBUG("Created %s directory.", dir);
      return true;
    }

    if (errno == EEXIST)
    {
      CXX_LOG_TRACE("Directory already exists %s directory.", dir);
      return true;
    }

    CXX_LOG_ERROR("Failed to create %s directory. Error: %d", dir, errno);
    return false;

  }

  boost::optional<std::string> findCacheDirRoot() {
    for (auto const &envVar: CACHE_ROOT_ENV_VARS)
    {
      char *root = getenv(envVar.c_str());
      if (root != nullptr)
      {
        return std::string(root);
      }
    }
    return {};
  }

  boost::optional<std::string> getCredentialFilePath()
  {
    const auto cacheDirRootOpt = findCacheDirRoot();
    if (!cacheDirRootOpt)
    {
      return {};
    }
    const std::string &cacheDirRoot = cacheDirRootOpt.value();

    if (!mkdirIfNotExists(cacheDirRoot.c_str()))
    {
      return {};
    }

    std::string cacheDir = cacheDirRoot;
    for (const auto &name: CACHE_DIR_NAMES)
    {
      cacheDir += PATH_SEP + name;
      if (!mkdirIfNotExists(cacheDir.c_str()))
      {
        return {};
      }
    }
    return cacheDir + PATH_SEP + CREDENTIAL_FILE_NAME;
  };

  std::string credItemStr(const CredentialKey &key)
  {
    return key.host + ":" + key.user + ":SNOWFLAKE-ODBC-DRIVER:" + credTypeToString(key.type);
  }

  void ensureObject(picojson::value &val)
  {
    if (!val.is<picojson::object>())
    {
      val = picojson::value(picojson::object());
    }
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

    if (!ensurePermissions(path, 0700))
    {
      return "Cannot ensure correct permissions on a file";
    }

    cacheFile << result.serialize(true);
    return {};
  }

  void cacheFileUpdate(picojson::value &cache, const CredentialKey &key, const std::string &credential)
  {
    ensureObject(cache);
    picojson::object &obj = cache.get<picojson::object>();
    auto [accountCacheIt, inserted] = obj.emplace(key.account, picojson::value(picojson::object()));

    ensureObject(accountCacheIt->second);
    accountCacheIt->second.get<picojson::object>().emplace(credItemStr(key), credential);
  }

  void cacheFileRemove(picojson::value &cache, const CredentialKey &key)
  {
    ensureObject(cache);
    picojson::object &cacheObj = cache.get<picojson::object>();

    auto accountCacheIt = cacheObj.find(key.account);
    if (accountCacheIt == cacheObj.end())
    {
      return;
    }

    ensureObject(accountCacheIt->second);
    picojson::object &accountCacheObj = accountCacheIt->second.get<picojson::object>();
    accountCacheObj.erase(credItemStr(key));
    if (accountCacheObj.empty())
    {
      cacheObj.erase(accountCacheIt);
    }
  }

  boost::optional<std::string> cacheFileGet(picojson::value &cache, const CredentialKey &key) {
    ensureObject(cache);
    picojson::object &cacheObj = cache.get<picojson::object>();

    auto accountCacheIt = cacheObj.find(key.account);
    if (accountCacheIt == cacheObj.end())
    {
      return {};
    }

    ensureObject(accountCacheIt->second);
    picojson::object &accountCacheObj = accountCacheIt->second.get<picojson::object>();
    auto it = accountCacheObj.find(credItemStr(key));

    if (it == accountCacheObj.end())
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
