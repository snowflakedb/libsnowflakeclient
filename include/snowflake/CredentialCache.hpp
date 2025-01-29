/*
 * File:   CredentialCache.cpp *
 * Copyright (c) 2025 Snowflake Computing
 */

#ifndef SNOWFLAKECLIENT_CREDENTIALCACHE_HPP
#define SNOWFLAKECLIENT_CREDENTIALCACHE_HPP

#include <string>
#include <memory>
#include <boost/optional.hpp>

#include "snowflake/mfa_token_cache.h"

namespace Snowflake {

namespace Client {

  struct CredentialKey {
    std::string host;
    std::string user;
    CredentialType type;
  };

  class CredentialCache {
  public:
    static CredentialCache *make();

    virtual boost::optional<std::string> get(const CredentialKey &key) = 0;

    virtual bool save(const CredentialKey &key, const std::string &credential) = 0;

    virtual bool remove(const CredentialKey &key) = 0;

    virtual ~CredentialCache() = default;
  };

  inline std::string credTypeToString(CredentialType type) {
    switch (type) {
      case CredentialType::MFA_TOKEN:
        return "MFA_TOKEN";
      case CredentialType::SSO_TOKEN:
        return "SSO_TOKEN";
      case CredentialType::OAUTH_REFRESH_TOKEN:
        return "OAUTH_REFRESH_TOKEN";
      case CredentialType::OAUTH_ACCESS_TOKEN:
        return "OAUTH_ACCESS_TOKEN";
      default:
        return "UNKNOWN";
    }
  }

}

}

#endif //SNOWFLAKECLIENT_CREDENTIALCACHE_HPP
