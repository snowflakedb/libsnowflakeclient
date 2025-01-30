#ifndef SNOWFLAKECLIENT_CACHEFILE_HPP
#define SNOWFLAKECLIENT_CACHEFILE_HPP

#include <string>
#include <fstream>
#include <boost/optional.hpp>

#include "picojson.h"

#include "snowflake/SecureStorage.hpp"

namespace Snowflake {

namespace Client {

  boost::optional<std::string> getCredentialFilePath();

  std::string readFile(const std::string &path, picojson::value &result);

  std::string writeFile(const std::string &path, const picojson::value &result);

  void cacheFileUpdate(picojson::value &cache, const SecureStorageKey &key, const std::string &credential);

  void cacheFileRemove(picojson::value &cache, const SecureStorageKey &key);

  boost::optional<std::string> cacheFileGet(picojson::value &cache, const SecureStorageKey &key);

}

}

#endif // SNOWFLAKECLIENT_CACHEFILE_HPP
