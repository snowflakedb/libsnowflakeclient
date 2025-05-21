#include "snowflake/SFURL.hpp"
#include "snowflake/SF_CRTFunctionSafe.h"

namespace Snowflake
{
namespace Client
{

/// Functions of QueryParams
SFURL::QueryParams::QueryParams(const SFURL::QueryParams &params, bool &cacheValid, std::string &cacheURL)
:m_cacheValid(&cacheValid), m_cacheURL(&cacheURL)
{
  m_list = params.m_list;
  for (auto iter = m_list.begin(); iter != m_list.end(); iter++)
  {
    m_map[iter->m_key] = iter;
  }
}

void SFURL::QueryParams::parse(size_t &i)
{
  enum ParseState
  {
    KEY, VALUE
  };

  ParseState s = KEY;
  std::string key;
  size_t pre = i;
  for (; i < m_cacheURL->length(); i++)
  {
    char c = (*m_cacheURL)[i];
    switch (s) {
      case KEY:
        if (c == '=')
        {
          key = m_cacheURL->substr(pre, i - pre);
          pre = i + 1;
          s = VALUE;
        }
        break;

      case VALUE:
      {
        if (c == '&' || c == '#')
        {
          std::string value = m_cacheURL->substr(pre, i - pre);
          addQueryParam(key, value, pre);
          pre = i + 1; // skip the current & or #

          if (c == '#')
          {
            return;
          }

          s = KEY;
        }
        break;
      }
    } // switch
  } // for loop

  if (s != VALUE)
  {
    throw SFURLParseError("error parsing Url [***]: empty value");
  }

  std::string value = m_cacheURL->substr(pre, i - pre);
  this->addQueryParam(key, value, pre);
}

const std::string &SFURL::QueryParams::getQueryParam(const std::string &key) const
{
  auto iter = m_map.find(key);
  if (iter == m_map.cend())
  {
    return empty;
  }

  return iter->second->m_value;
}

void SFURL::QueryParams::addQueryParam(const std::string &paramName, const std::string &paramValue, size_t index)
{
  auto iter = m_map.find(paramName);
  if (iter == m_map.end())
  {
    *m_cacheValid = false;
    auto node_iter = m_list.emplace(m_list.begin(), paramName, paramValue, index);
    m_map[paramName] = node_iter;
    return;
  }

  renewQueryParam(iter->second, paramValue);
}

void SFURL::QueryParams::renewQueryParam(
  std::list<SFURL::QueryParams::QueryParamNode>::iterator &elem,
  const std::string &newValue)
{
  if (!(*m_cacheValid) || elem->m_value.length() != newValue.length())
  {
    elem->m_value = newValue;
    *m_cacheValid = false;
    return;
  }

  // cache is valid and paramValue is as same length, we can directly replace on the cached url
  elem->m_value = newValue;
  size_t startIndex = elem->m_index;
  m_cacheURL->replace(startIndex, elem->m_value.length(), elem->m_value);
}

std::string SFURL::QueryParams::encodeParam(const std::string &srcParam)
{
  std::string encoded;
  char buf[5];
  buf[0] = '%';

  // encode all special characters
  for (size_t pos = 0; pos < srcParam.length(); pos++)
  {
    // if unreserved, put as is
    unsigned char car = srcParam[pos];
    if ((car >= '0' && car <= '9') ||
      (car >= 'A' && car <= 'Z') ||
      (car >= 'a' && car <= 'z') ||
      (car == '-' || car == '_' || car == '.' || car == '~' ||
        car == '*' || car == '/'))
    {
      encoded.push_back(car);
    }
    else if (car == ' ')
    {
      encoded.push_back('+');
    }
    else
    {
      sf_sprintf(&buf[1], sizeof(buf) - 1, "%.2X", car);
      encoded.append(buf);
    }
  }
  return encoded;
}

/// Functions of SFURL
SFURL::SFURL()
: m_cacheValid(false), m_params(m_cacheValid, m_cacheURL)
, m_proxyEnabled(false) {}

SFURL::SFURL(const SFURL &copy)
: m_cacheURL(copy.m_cacheURL), m_cacheValid(copy.m_cacheValid)
, m_scheme(copy.m_scheme)
, m_userinfo(copy.m_userinfo), m_host(copy.m_host), m_port(copy.m_port)
, m_path(copy.path())
, m_params(copy.m_params, m_cacheValid, m_cacheURL)
, m_fragment(copy.m_fragment)
, m_proxy(copy.m_proxy)
, m_proxyEnabled(copy.m_proxyEnabled)
{}

SFURL::SFURL(std::string& protocol, std::string& host, std::string& port)
    : m_scheme(protocol), m_host(host), m_port(port),
      m_cacheValid(false), m_params(m_cacheValid, m_cacheURL)
    , m_proxyEnabled(false)
     {}

SFURL &SFURL::operator= (const SFURL &copy)
{
  if (this == &copy)
  {
    return *this;
  }

  m_cacheURL = copy.m_cacheURL;
  m_cacheValid = copy.m_cacheValid;

  m_scheme = copy.m_scheme;
  m_userinfo = copy.m_userinfo;
  m_host = copy.m_host;
  m_port = copy.m_port;
  m_path = copy.m_path;

  m_params = QueryParams(copy.m_params, m_cacheValid, m_cacheURL);

  m_fragment = copy.m_fragment;
  m_proxy = copy.m_proxy;
  m_proxyEnabled = copy.m_proxyEnabled;
  return *this;
}

SFURL SFURL::parse(const std::string &url)
{
  SFURL sfurl;
  sfurl.m_cacheURL = url;
  sfurl.m_cacheValid = true;
  enum ParseState
  {
    SCHEME, SCHEME_END, AUTH_OR_PATH, AUTHORITY, PATH, QUERY, FRAGMENT
  };

  ParseState s = SCHEME;
  size_t pre = 0;
  for (size_t i = 0; i < url.length(); i++)
  {
    char c = url[i];
    switch(s)
    {
      case SCHEME:
        if (c == ':')
        {
          sfurl.m_scheme = url.substr(pre, i - pre);
          s = SCHEME_END;
        }
        break;

      case SCHEME_END:
        if (url.substr(i, 2) != "//")
        {
          throw SFURLParseError("error scheme ending for"
                                    " Url [***]: empty value");
        }
        s = AUTH_OR_PATH;
        break;

      case AUTH_OR_PATH:
        pre = i + 1;  // skip the last '/' for "://"
        if (url.substr(i, 2) == "//")
        {
          s = PATH;
        }
        else
        {
          s = AUTHORITY;
        }
        break;

      case AUTHORITY:
      {
        parseAuthority(sfurl, i);
        // already reach the end of string;
        if (i == url.length())
        {
          return sfurl;
        }

        // end with one of # / or ?
        s = url[i] == '#' ? FRAGMENT : (url[i] == '/' ? PATH : QUERY);
        pre = url[i] == '/' ? i : (i + 1);  // path should include '/'
        break;
      }

      case PATH:
        if (i == url.length() - 1)
        {
          sfurl.m_path = url.substr(pre, i - pre + 1);
          return sfurl;
        }

        else if (c == '?' || c == '#')
        {
          sfurl.m_path = url.substr(pre, i - pre);
          pre = i + 1;  // skip the '?' or '#
          s = (c == '?' ? QUERY : FRAGMENT);
        }
        break;

      case QUERY:
        sfurl.m_params.parse(i);
        sfurl.m_cacheValid = true;
        if (i == url.length())
        {
          return sfurl;
        }

        // end with one of #
        s = FRAGMENT;
        pre = i + 1;  // skip the #

        break;

      case FRAGMENT:
        if (i == url.length() - 1)
        {
          sfurl.m_fragment = url.substr(pre, i - pre + 1);
        }
        break;
    } // switch
  } // for
  return sfurl;
}

void SFURL::parseAuthority(SFURL &sfurl, size_t &i)
{
  enum ParseState
  {
    USERINFO_OR_HOST, HOST, PORT
  };

  ParseState s = USERINFO_OR_HOST;
  size_t pre = i;
  for (; i < sfurl.m_cacheURL.length(); i++)
  {
    char c = sfurl.m_cacheURL[i];
    switch (s)
    {
      case USERINFO_OR_HOST:
        if (c == '@')
        {
          sfurl.m_userinfo = sfurl.m_cacheURL.substr(pre, i - pre);
          pre = i + 1;  // skip the @
          s = HOST;
        }
        else if (c == ':' || c == '/' || c == '?')
        {
          sfurl.m_host = sfurl.m_cacheURL.substr(pre, i - pre);

          if (c == '/' || c == '?')
          {
            return;
          }

          pre = i + 1;
          s = PORT;
        }
        break;

      case HOST:
        if (c == ':' || c == '/' || c == '#' || c == '?')
        {
          sfurl.m_host = sfurl.m_cacheURL.substr(pre, i - pre);

          if (c == '/' || c == '#' || c == '?')
          {
            return;
          }

          pre = i + 1;
          s = PORT;
        }
        break;

      case PORT:
        if (c == '/' || c == '#' || c == '?')
        {
          sfurl.m_port = sfurl.m_cacheURL.substr(pre, i - pre);
          return;
        }
        else if (c < '0' || c > '9')
        {
            throw SFURLParseError("Error parsing port from"
                                      " Url [***]");
        }

        break;
    }
  }

  switch (s) {
    case USERINFO_OR_HOST:
    case HOST:
      sfurl.m_host = sfurl.m_cacheURL.substr(pre, i - pre + 1);
      break;
    case PORT:
      sfurl.m_port = sfurl.m_cacheURL.substr(pre, i - pre + 1);
      return;
  }
}

std::string SFURL::authority() const
{
  if (m_host.empty())
  {
    return "";
  }

  std::stringstream ss;
  if (!m_userinfo.empty())
  {
    ss << m_userinfo << '@';
  }

  ss << m_host;

  if (!m_port.empty())
  {
    ss << ':' << m_port;
  }

  return ss.str();
}

std::string SFURL::toString()
{
  if (m_cacheValid)
  {
    return m_cacheURL;
  }

  m_cacheURL.clear();

  m_cacheURL.append(m_scheme + "://");

  if (!m_host.empty())
  {
    if (!m_userinfo.empty())
    {
      m_cacheURL.append(m_userinfo + "@");
    }

    m_cacheURL.append(m_host);

    if (!m_port.empty())
    {
      m_cacheURL.append(":" + m_port);
    }
  }

  if (!m_path.empty())
  {
    m_cacheURL.append(m_path);
  }

  m_params.flushStr();

  if (!m_fragment.empty())
  {
    m_cacheURL.append("#" + m_fragment);
  }

  m_cacheValid = true;
  return m_cacheURL;
}
}
}
