/*
 * File: SecureStorageImpl.cpp
 * Copyright (c)  2025 Snowflake Computing
 */

#include "SecureStorageImpl.hpp"
#include "../util/Sha256.hpp"
#include <sstream>

namespace Snowflake {

namespace Client {

std::string SecureStorageImpl::convertTarget(const std::string& host,
                                   const std::string& username,
                                   const std::string& credType)
  {
    std::stringstream ss;
    ss << host << ":" << username << ":" << credType;
    auto plain_text = ss.str();
    auto sha = sha256(ss.str());

    if (sha) {
      return sha.get();
    }

    return plain_text;
  }

}

}


