/*
 * File: SecureStorageImpl.cpp
 */

#include "snowflake/SecureStorage.hpp"

#include <sstream>

#include "../util/Sha256.hpp"
#include "../logger/SFLogger.hpp"

namespace Snowflake
{

  namespace Client
  {

    boost::optional<std::string> SecureStorage::convertTarget(const SecureStorageKey &key)
    {
      if (key.host.empty() || key.user.empty())
      {
        CXX_LOG_ERROR("Cannot convert secure storage key to string, user='%s' or host='%s' empty", key.host.c_str(), key.host.c_str());
        return {};
      }
      std::stringstream ss;
      ss << key.host << ":" << key.user << ":" << keyTypeToString(key.type);
      auto plain_text = ss.str();
      auto sha = sha256(ss.str());
      if (!sha)
      {
        CXX_LOG_ERROR("Cannot generate sha256 hash of secure storage key = '%s'", plain_text.c_str());
        return {};
      }
      return sha;
    }

  }

}
