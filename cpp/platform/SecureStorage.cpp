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

  // Serializes the (already-normalized) key components into the canonical JSON
  // document the cache key is hashed from: compact, no whitespace, keys in
  // lexicographic order (idp, role, snowflake, token_type, username).
  std::string canonicalJson(const std::string& idp,
                            const std::string& role,
                            const std::string& snowflake,
                            const std::string& tokenType,
                            const std::string& username)
  {
    std::string json = "{";
    json += "\"idp\":\"";        appendJsonEscaped(json, idp);        json += "\",";
    json += "\"role\":\"";       appendJsonEscaped(json, role);       json += "\",";
    json += "\"snowflake\":\"";  appendJsonEscaped(json, snowflake);  json += "\",";
    json += "\"token_type\":\""; appendJsonEscaped(json, tokenType);  json += "\",";
    json += "\"username\":\"";   appendJsonEscaped(json, username);   json += "\"";
    json += "}";
    return json;
  }

}

std::string normalizeUrl(const std::string& url)
  {
    // Strip the "scheme://" prefix from the raw string. The raw port is kept
    // verbatim so an explicitly-stated default port (e.g. :443) is preserved.
    std::string::size_type schemePos = url.find("://");
    std::string result = (schemePos == std::string::npos) ? url : url.substr(schemePos + 3);

    // Strip optional userinfo ("user:pass@").
    std::string::size_type atPos = result.find('@');
    if (atPos != std::string::npos)
    {
      result = result.substr(atPos + 1);
    }

    // Drop the query string and fragment, which never appear in cache keys.
    result = result.substr(0, result.find_first_of("?#"));

    // Trim a root-only trailing slash so bare-host URLs have no slash suffix.
    while (!result.empty() && result.back() == '/')
    {
      result.pop_back();
    }

    for (char& c : result)
    {
      c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return result;
  }

std::string normalizeIdentifier(const std::string& identifier)
  {
    std::string result;
    result.reserve(identifier.size());
    bool inQuotes = false;
    for (char c : identifier)
    {
      if (c == '"')
      {
        inQuotes = !inQuotes;
        result += c;
      }
      else if (inQuotes)
      {
        result += c;
      }
      else
      {
        result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      }
    }
    return result;
  }

boost::optional<std::string> SecureStorage::convertTarget(const SecureStorageKey& key)
  {
    const std::string idp = normalizeUrl(key.idp);
    const std::string snowflake = normalizeUrl(key.snowflake);
    const std::string role = normalizeIdentifier(key.role);
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

    const std::string json = canonicalJson(idp, role, snowflake, tokenType, username);
    auto sha = sha256(json);
    if (!sha)
    {
      CXX_LOG_ERROR("Cannot generate sha256 hash of secure storage key (host='%s').", key.host.c_str());
      return {};
    }

    return std::string("SnowflakeTokenCache.v2.") + sha.get();
  }

}

}
