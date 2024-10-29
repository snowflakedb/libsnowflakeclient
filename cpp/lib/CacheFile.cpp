//
// Created by Jakub Szczerbinski on 14.10.24.
//

#include <fstream>
#include <string>
#include <boost/filesystem.hpp>

#include "picojson.h"

#include "CacheFile.hpp"
#include "CredentialCache.hpp"

namespace sf {

  std::string credItemStr(const CredentialKey &key) {
    return key.host + ":" + key.user + ":SNOWFLAKE-ODBC-DRIVER:" + credTypeToString(key.type);
  }

  void ensureObject(picojson::value &val) {
    if (!val.is<picojson::object>()) {
      val = picojson::value(picojson::object());
    }
  }

  std::string readFile(const std::string &path, picojson::value &result) {
    if (!boost::filesystem::exists(path)) {
      result = picojson::value(picojson::object());
      return {};
    }

    std::ifstream cacheFile(path);
    if (!cacheFile.is_open()) {
      result = picojson::value(picojson::object());
      return "Failed to open the file(path=" + path + ")";
    }

    std::string error = picojson::parse(result, cacheFile);
    if (!error.empty()) {
      return "Failed to parse the file: " + error;
    }
    return {};
  }

  std::string writeFile(const std::string &path, const picojson::value &result) {
    std::ofstream cacheFile(path, std::ios_base::trunc);
    if (!cacheFile.is_open()) {
      return "Failed to open the file";
    }

    cacheFile << result.serialize(true);
    return {};
  }

  void cacheFileUpdate(picojson::value &cache, const CredentialKey &key, const std::string &credential) {
    ensureObject(cache);
    picojson::object &obj = cache.get<picojson::object>();
    auto [accountCacheIt, inserted] = obj.emplace(key.account, picojson::value(picojson::object()));

    ensureObject(accountCacheIt->second);
    accountCacheIt->second.get<picojson::object>().emplace(credItemStr(key), credential);
  }

  void cacheFileRemove(picojson::value &cache, const CredentialKey &key) {
    ensureObject(cache);
    picojson::object &cacheObj = cache.get<picojson::object>();

    auto accountCacheIt = cacheObj.find(key.account);
    if (accountCacheIt == cacheObj.end()) {
      return;
    }

    ensureObject(accountCacheIt->second);
    picojson::object &accountCacheObj = accountCacheIt->second.get<picojson::object>();
    accountCacheObj.erase(credItemStr(key));
    if (accountCacheObj.empty()) {
      cacheObj.erase(accountCacheIt);
    }
  }

  std::optional<std::string> cacheFileGet(picojson::value &cache, const CredentialKey &key) {
    ensureObject(cache);
    picojson::object &cacheObj = cache.get<picojson::object>();

    auto accountCacheIt = cacheObj.find(key.account);
    if (accountCacheIt == cacheObj.end()) {
      return {};
    }

    ensureObject(accountCacheIt->second);
    picojson::object &accountCacheObj = accountCacheIt->second.get<picojson::object>();
    auto it = accountCacheObj.find(credItemStr(key));

    if (it == accountCacheObj.end()) {
      return {};
    }

    if (!it->second.is<std::string>()) {
      return {};
    }

    return it->second.get<std::string>();
  }

}
