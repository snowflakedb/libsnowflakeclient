//
// Created by Jakub Szczerbinski on 10.10.24.
//

#ifndef SNOWFLAKECLIENT_CREDENTIALCACHE_HPP
#define SNOWFLAKECLIENT_CREDENTIALCACHE_HPP

#include <string>
#include <memory>
#include <optional>

#include "snowflake/mfa_token_cache.h"

namespace sf {

  struct CredentialKey {
    std::string account;
    std::string host;
    std::string user;
    CredentialType type;
  };

  class CredentialCache {
  public:
    static CredentialCache *make();

    virtual std::optional<std::string> get(const CredentialKey& key) = 0;

    virtual bool save(const CredentialKey& key, const std::string& credential) = 0;

    virtual bool remove(const CredentialKey& key) = 0;

    virtual ~CredentialCache() = default;
  };

  inline std::string credTypeToString(CredentialType type) {
    switch (type) {
      case CredentialType::MFA_TOKEN:
        return "MFA_TOKEN";
      default:
        return "UNKNOWN";
    }
  }

};

#endif //SNOWFLAKECLIENT_CREDENTIALCACHE_HPP
