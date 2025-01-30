/*
 * File: SecureStorageImpl.cpp
 * Copyright (c)  2025 Snowflake Computing
 */

#include "snowflake/SecureStorage.hpp"
#include "../util/Sha256.hpp"
#include <sstream>

namespace Snowflake {

namespace Client {

std::string SecureStorage::convertTarget(const SecureStorageKey& key)
  {
    std::stringstream ss;
    ss << key.host << ":" << key.user << ":" << keyTypeToString(key.type);
    auto plain_text = ss.str();
    auto sha = sha256(ss.str());

    if (sha) {
      return sha.get();
    }

    return plain_text;
  }

}

}


