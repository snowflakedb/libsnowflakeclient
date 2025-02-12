/*
 * File: SecureStorageImpl.cpp
 * Copyright (c)  2025 Snowflake Computing
 */

#include "snowflake/SecureStorage.hpp"

#include <sstream>

#include "../util/Sha256.hpp"

namespace Snowflake {

namespace Client {

std::string SecureStorage::convertTarget(const SecureStorageKey& key)
  {
    std::stringstream ss;
    ss << key.host << ":" << key.user << ":" << keyTypeToString(key.type);
    auto plain_text = ss.str();
    auto sha = sha256(ss.str());
    return sha.get();
  }

}

}


