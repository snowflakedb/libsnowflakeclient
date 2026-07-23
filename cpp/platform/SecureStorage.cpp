/*
 * File: SecureStorageImpl.cpp
 */

#include "snowflake/SecureStorage.hpp"

#include <cctype>
#include <cstdio>
#include <string>

#include "../util/Sha256.hpp"
#include "../logger/SFLogger.hpp"

namespace Snowflake {

namespace Client {

namespace {

  // Appends `value` to `out` with standard JSON string escaping: `"` and `\`
  // are backslash-escaped, the usual short escapes are emitted for common
  // control characters, and any remaining control character is emitted as
  // \u00XX (lowercase hex). This matches the canonical serialization other
  // drivers produce, which is required for byte-exact cross-driver parity.
  void appendJsonEscaped(std::string& out, const std::string& value)
  {
    for (unsigned char c : value)
    {
      switch (c)
      {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b";  break;
        case '\f': out += "\\f";  break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
          if (c < 0x20)
          {
            char buf[7];
            std::snprintf(buf, sizeof(buf), "\\u%04x", c);
            out += buf;
          }
          else
          {
            out += static_cast<char>(c);
          }
      }
    }
  }

  // Serializes normalized OAuth key components into compact canonical JSON with
  // lexicographically sorted keys: idp, role, snowflake, username (4 fields).
  // token_type is NOT included; it belongs in the key prefix, not keyData.
  std::string canonicalJsonOAuth(const std::string& idp,
                                 const std::string& role,
                                 const std::string& snowflake,
                                 const std::string& username)
  {
    std::string json = "{";
    json += "\"idp\":\"";        appendJsonEscaped(json, idp);        json += "\",";
    json += "\"role\":\"";       appendJsonEscaped(json, role);       json += "\",";
    json += "\"snowflake\":\"";  appendJsonEscaped(json, snowflake);  json += "\",";
    json += "\"username\":\"";   appendJsonEscaped(json, username);   json += "\"";
    json += "}";
    return json;
  }

  // Serializes normalized MFA / ID-token key components into compact canonical
  // JSON with sorted keys: snowflake, username (2 fields).
  // idp and role are absent — they are not embedded in MFA/ID-token flows.
  std::string canonicalJsonMfaId(const std::string& snowflake,
                                  const std::string& username)
  {
    std::string json = "{";
    json += "\"snowflake\":\"";  appendJsonEscaped(json, snowflake);  json += "\",";
    json += "\"username\":\"";   appendJsonEscaped(json, username);   json += "\"";
    json += "}";
    return json;
  }

  // Returns true for OAuth flow types that require idp + role in keyData.
  bool isOAuthFlow(SecureStorageKeyType type)
  {
    return type == SecureStorageKeyType::OAUTH_ACCESS_TOKEN  ||
           type == SecureStorageKeyType::OAUTH_REFRESH_TOKEN ||
           type == SecureStorageKeyType::DPOP_BUNDLED_ACCESS_TOKEN;
  }

}

std::string normalizeUrl(const std::string& url)
  {
    // Strip the "scheme://" prefix from the raw string. The raw port is kept
    // verbatim so an explicitly-stated default port (e.g. :443) is preserved.
    std::string::size_type schemePos = url.find("://");
    std::string result = (schemePos == std::string::npos) ? url : url.substr(schemePos + 3);

    // Drop the query string and fragment, which never appear in cache keys.
    // This must happen before userinfo stripping so an '@' inside the query
    // is never mistaken for a userinfo delimiter.
    result = result.substr(0, result.find_first_of("?#"));

    // Strip optional userinfo ("user:pass@") from the authority only. The
    // authority ends at the first '/', so an '@' is a userinfo delimiter only
    // when it precedes that first slash; an '@' inside the path is preserved.
    std::string::size_type authorityEnd = result.find('/');
    std::string::size_type atPos = result.find('@');
    if (atPos != std::string::npos &&
        (authorityEnd == std::string::npos || atPos < authorityEnd))
    {
      result = result.substr(atPos + 1);
    }

    // Trim trailing slashes so bare-host URLs have no slash suffix.
    while (!result.empty() && result.back() == '/')
    {
      result.pop_back();
    }

    for (char& c : result)
    {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
  }

std::string normalizeIdentifier(const std::string& identifier)
  {
    // Quoted identifiers carry case-sensitive semantics in SQL — return them
    // verbatim, unchanged. Unquoted identifiers are case-insensitive, so
    // lowercasing produces a stable canonical form.
    if (identifier.find('"') != std::string::npos)
    {
      return identifier;
    }
    std::string result;
    result.reserve(identifier.size());
    for (char c : identifier)
    {
      result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
  }

boost::optional<std::string> SecureStorage::convertTarget(const SecureStorageKey& key)
  {
    const std::string snowflake = normalizeUrl(key.snowflake);
    const std::string username = normalizeIdentifier(key.user);
    const std::string tokenType = keyTypeToString(key.type);

    if (snowflake.empty())
    {
      CXX_LOG_ERROR("Cannot build secure storage key: snowflake URL is empty (host='%s').", key.host.c_str());
      return {};
    }
    if (username.empty())
    {
      CXX_LOG_ERROR("Cannot build secure storage key: username is empty (host='%s').", key.host.c_str());
      return {};
    }

    std::string json;
    if (isOAuthFlow(key.type))
    {
      // OAuth flows: 4-field keyData — idp, role, snowflake, username.
      const std::string idp = normalizeUrl(key.idp);
      const std::string role = normalizeIdentifier(key.role);
      json = canonicalJsonOAuth(idp, role, snowflake, username);
    }
    else
    {
      // MFA_TOKEN, ID_TOKEN: 2-field keyData — snowflake, username only.
      // idp and role are not part of these authentication flows.
      json = canonicalJsonMfaId(snowflake, username);
    }

    auto sha = sha256(json);
    if (!sha)
    {
      CXX_LOG_ERROR("Cannot generate sha256 hash of secure storage key (host='%s').", key.host.c_str());
      return {};
    }

    return std::string("SnowflakeTokenCache.v2.") + tokenType + "." + sha.get();
  }

}

}
