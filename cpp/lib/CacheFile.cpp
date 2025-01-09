#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <boost/filesystem.hpp>

#include "picojson.h"

#include "CacheFile.hpp"
#include "CredentialCache.hpp"
#include "snowflake/platform.h"
#include "../logger/SFLogger.hpp"
#include "openssl/sha.h"
#include "openssl/types.h"
#include "openssl/evp.h"

#if defined(__linux__) || defined(__APPLE__)
#include <sys/stat.h>
#endif

namespace Snowflake {

namespace Client {
  const char* CREDENTIAL_FILE_NAME = "credential_cache_v1.json";

  const std::vector<std::string> CACHE_ROOT_ENV_VARS =
  {
      "SF_TEMPORARY_CREDENTIAL_CACHE_DIR",
      "HOME",
      "TMP"
  };

  const std::vector<std::string> CACHE_DIR_PATH =
  {
      ".cache",
      "snowflake"
  };


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

  boost::optional<std::string> getSfTemporaryCacheDir()
  {
    auto SF_TEMPORARY_CREDENTIAL_CACHE_DIR_OPT = getEnv("SF_TEMPORARY_CREDENTIAL_CACHE_DIR");
    if (!SF_TEMPORARY_CREDENTIAL_CACHE_DIR_OPT)
    {
      return {};
    }

    const std::string& SF_TEMPORARY_CREDENTIAL_CACHE_DIR = SF_TEMPORARY_CREDENTIAL_CACHE_DIR_OPT.get();

    struct stat s = {};
    int err = stat(SF_TEMPORARY_CREDENTIAL_CACHE_DIR.c_str(), &s);

    if (err != 0)
    {
      CXX_LOG_INFO("Failed to stat SF_TEMPORARY_CREDENTIAL_CACHE_DIR=%s, errno=%d. Skipping it in cache file location lookup.", SF_TEMPORARY_CREDENTIAL_CACHE_DIR.c_str(), errno);
      return {};
    }

    if (!S_ISDIR(s.st_mode))
    {
      CXX_LOG_INFO("SF_TEMPORARY_CREDENTIAL_CACHE_DIR=%s is not a directory. Skipping it in cache file location lookup.", SF_TEMPORARY_CREDENTIAL_CACHE_DIR.c_str());
      return {};
    }

    if (s.st_uid != geteuid())
    {
      CXX_LOG_INFO("SF_TEMPORARY_CREDENTIAL_CACHE_DIR=%s is not owned by current user. Skipping it in cache file location lookup.", SF_TEMPORARY_CREDENTIAL_CACHE_DIR.c_str());
      return {};
    }

    unsigned permissions = s.st_mode & 0777;
    if (permissions != 0700)
    {
      CXX_LOG_INFO("Incorrect permissions=%o for SF_TEMPORARY_CREDENTIAL_CACHE_DIR=%s. Changing permissions to 700.", permissions, SF_TEMPORARY_CREDENTIAL_CACHE_DIR.c_str())
      if (chmod(SF_TEMPORARY_CREDENTIAL_CACHE_DIR.c_str(), 0700) != 0)
      {
        CXX_LOG_WARN("Failed to change permissions for a file SF_TEMPORARY_CREDENTIAL_CACHE_DIR=%s, errno=%d. Skipping it in cache file location lookup.", SF_TEMPORARY_CREDENTIAL_CACHE_DIR.c_str(), errno);
        return {};
      }
    }

    return SF_TEMPORARY_CREDENTIAL_CACHE_DIR;
  }

  boost::optional<std::string> getXdgCacheDir()
  {
    auto XDG_CACHE_HOME_OPT = getEnv("XDG_CACHE_HOME");
    if (!XDG_CACHE_HOME_OPT)
    {
      return {};
    }
    const std::string& XDG_CACHE_HOME = XDG_CACHE_HOME_OPT.get();

    struct stat s = {};
    int err = stat(XDG_CACHE_HOME.c_str(), &s);

    if (err != 0)
    {
      CXX_LOG_INFO("Failed to stat XDG_CACHE_HOME=%s, errno=%d. Skipping it in cache file location lookup.", XDG_CACHE_HOME.c_str(), errno);
      return {};
    }

    if (!S_ISDIR(s.st_mode))
    {
      CXX_LOG_INFO("XDG_CACHE_HOME=%s is not a directory. Skipping it in cache file location lookup.", XDG_CACHE_HOME.c_str());
      return {};
    }

    auto cacheDir = XDG_CACHE_HOME + PATH_SEP + "snowflake";
    if (!mkdirIfNotExists(cacheDir))
    {
      CXX_LOG_INFO("Could not create 'snowflake' directory in XDG_CACHE_HOME=%s. Skipping it in cache file location lookup.", XDG_CACHE_HOME.c_str());
      return {};
    }

    if (chmod(cacheDir.c_str(), 700) != 0)
    {
      CXX_LOG_INFO("Failed to set permissions on directory=%s, errno=%d. Skipping it in cache file location lookup.", XDG_CACHE_HOME.c_str(), errno);
      return {};
    }

    return cacheDir;
  }

  boost::optional<std::string> getHomeCacheDir()
  {
    auto HOME_OPT = getEnv("HOME");
    if (!HOME_OPT)
    {
      return {};
    }
    const std::string& HOME = HOME_OPT.get();

    struct stat s = {};
    int err = stat(HOME.c_str(), &s);

    if (err != 0)
    {
      CXX_LOG_INFO("Failed to stat HOME=%s, errno=%d. Skipping it in cache file location lookup.", HOME.c_str(), errno);
      return {};
    }

    if (!S_ISDIR(s.st_mode))
    {
      CXX_LOG_INFO("HOME=%s is not a directory. Skipping it in cache file location lookup.", HOME.c_str());
      return {};
    }

    std::vector<std::string> segments = {".cache", "snowflake"};

    auto cacheDir = HOME;
    for (const auto& segment: segments)
    {
      cacheDir.append(PATH_SEP + segment);
      if (!mkdirIfNotExists(cacheDir))
      {
        CXX_LOG_INFO("Could not create cache dir=%s in HOME=%s. Skipping it in cache file location lookup.", cacheDir.c_str(), HOME.c_str());
        return {};
      }
    }

    if (chmod(cacheDir.c_str(), 700) != 0)
    {
      CXX_LOG_INFO("Failed to set permissions on directory=%s, errno=%d. Skipping it in cache file location lookup.", cacheDir.c_str(), errno);
      return {};
    }

    return cacheDir;
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

  boost::optional<std::string> sha256(const std::string& str)
  {
    auto mdctx = std::unique_ptr<EVP_MD_CTX, std::function<void(EVP_MD_CTX*)>>(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (mdctx.get() == nullptr)
      return {};

    if(EVP_DigestInit_ex(mdctx.get(), EVP_sha256(), nullptr) != 1)
      return {};

    if(EVP_DigestUpdate(mdctx.get(), str.c_str(), str.length()) != 1)
      return {};

    std::vector<unsigned char> buf(EVP_MD_size(EVP_sha256()));
    unsigned int size = 0;
    if(EVP_DigestFinal_ex(mdctx.get(), buf.data(), &size) != 1)
      return {};

    std::stringstream ss;
    for (short b : buf)
    {
      ss << std::hex << std::setw(2) << std::setfill('0') << b;
    }
    return ss.str();
  }

  boost::optional<std::string> credItemStr(const CredentialKey &key)
  {
    std::string plainTextKey = key.host + ":" + key.user + ":" + credTypeToString(key.type);
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

    if (!ensurePermissions(path, 0700))
    {
      return "Cannot ensure correct permissions on a file";
    }

    cacheFile << result.serialize(true);
    return {};
  }

  void cacheFileUpdate(picojson::value &cache, const CredentialKey &key, const std::string &credential)
  {
    picojson::object& tokens = getTokens(cache);
    auto keyStrOpt = credItemStr(key);
    if (!keyStrOpt)
    {
      CXX_LOG_ERROR("Failed to create credential key, cache update failed.");
      return;
    }
    tokens.emplace(keyStrOpt.get(), credential);
  }

  void cacheFileRemove(picojson::value &cache, const CredentialKey &key)
  {
    picojson::object& tokens = getTokens(cache);
    auto keyStrOpt = credItemStr(key);
    if (!keyStrOpt)
    {
      CXX_LOG_ERROR("Failed to create credential key, cache remove failed.");
      return;
    }
    tokens.erase(keyStrOpt.get());
  }

  boost::optional<std::string> cacheFileGet(picojson::value &cache, const CredentialKey &key) {
    picojson::object& tokens = getTokens(cache);
    auto keyStrOpt = credItemStr(key);
    if (!keyStrOpt)
    {
      CXX_LOG_ERROR("Failed to create credential key, cache get failed.");
      return {};
    }
    auto it = tokens.find(keyStrOpt.get());

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
