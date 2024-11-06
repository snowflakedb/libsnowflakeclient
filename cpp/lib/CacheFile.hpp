#ifndef SNOWFLAKECLIENT_CACHEFILE_HPP
#define SNOWFLAKECLIENT_CACHEFILE_HPP

#include <string>
#include <fstream>

#include "picojson.h"

#include "CredentialCache.hpp"

namespace Snowflake {

namespace Client {

  boost::optional<std::string> getCredentialFilePath();

  std::string readFile(const std::string &path, picojson::value &result);

  std::string writeFile(const std::string &path, const picojson::value &result);

  void cacheFileUpdate(picojson::value &cache, const CredentialKey &key, const std::string &credential);

  void cacheFileRemove(picojson::value &cache, const CredentialKey &key);

  boost::optional<std::string> cacheFileGet(picojson::value &cache, const CredentialKey &key);

}

}

#endif // SNOWFLAKECLIENT_CACHEFILE_HPP
