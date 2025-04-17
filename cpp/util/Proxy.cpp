

#include "snowflake/Proxy.hpp"
#include "snowflake/SF_CRTFunctionSafe.h"
#include "snowflake/platform.h"

namespace
{
    char *get_env_or(char *outbuf, size_t bufsize, const char *name)
    {
        return sf_getenv_s(name, outbuf, bufsize);
    }
    template <typename... Args>
    char *get_env_or(char *outbuf, size_t bufsize, const char *prime, Args... fallback)
    {
        char *value = get_env_or(outbuf, bufsize, prime);
        return value ? value : get_env_or(outbuf, bufsize, fallback...);
    }
}

Snowflake::Client::Util::Proxy::Proxy(const std::string &proxy_str)
{
    stringToProxyParts(proxy_str);
}

Snowflake::Client::Util::Proxy::Proxy(std::string &user, std::string &pwd,
                                      std::string &machine, unsigned port, Snowflake::Client::Util::Proxy::Protocol scheme) : m_user(user), m_pwd(pwd), m_machine(machine), m_port(port), m_protocol(scheme) {}

void Snowflake::Client::Util::Proxy::stringToProxyParts(const std::string &proxy)
{
    std::string protocol;
    std::size_t found, pos;

    // Return on an empty input proxy string
    if (proxy.empty())
    {
        return;
    }

    // Proxy is in the form: "[protocol://][user:password@]machine[:port]"
    pos = 0;
    found = proxy.find("://");
    if (found != std::string::npos)
    {
        // constant value for '://'
        protocol = proxy.substr(pos, found - pos);
        m_protocol = !protocol.compare("http") ? Snowflake::Client::Util::Proxy::Protocol::HTTP : Snowflake::Client::Util::Proxy::Protocol::HTTPS;
        // Set port with default value based on protocol and update
        // below if user specifies port in proxy string
        m_port = m_protocol == Snowflake::Client::Util::Proxy::Protocol::HTTP ? 80 : 443;
        pos = found + 3;
    }
    else
    {
        m_protocol = Snowflake::Client::Util::Proxy::Protocol::HTTP;
        m_port = 80;
    }

    found = proxy.find('@', pos);
    // If username and password exist, set them
    if (found != std::string::npos)
    {
        size_t colon_pos = proxy.find(':', pos);
        m_user = std::string(proxy.substr(pos, colon_pos - pos));
        pos = colon_pos + 1;
        m_pwd = std::string(proxy.substr(pos, found - pos));
        pos = found + 1;
    }

    found = proxy.find(':', pos);
    if (found != std::string::npos)
    {
        // port exists, set machine and port
        m_machine = std::string(proxy.substr(pos, found - pos));
        pos = found + 1;
        m_port = (unsigned int)strtoul(proxy.substr(pos, (proxy.length()) - pos).c_str(), nullptr, 10);
    }
    else
    {
        // just set machine
        m_machine = std::string(proxy.substr(pos, (proxy.length()) - pos));
    }
}

void Snowflake::Client::Util::Proxy::clearPwd()
{
    m_pwd.clear();
}

void Snowflake::Client::Util::Proxy::setProxyFromEnv()
{
    char valbuf[1024];
    char *env_value = get_env_or(valbuf, sizeof(valbuf), "all_proxy", "https_proxy", "http_proxy");
    if (env_value != nullptr)
    {
        std::string proxy(env_value);
        stringToProxyParts(proxy);
    }

    // Get noproxy string
    env_value = get_env_or(valbuf, sizeof(valbuf), "no_proxy", "NO_PROXY");
    if (env_value != nullptr)
    {
        m_noProxy = env_value;
    }
}

std::string Snowflake::Client::Util::Proxy::getHost() const
{
    if (m_machine.empty())
    {
        return "";
    }
    std::string host;
    if (Protocol::HTTPS == m_protocol)
    {
        host = "https://";
    }
    else if (Protocol::HTTP == m_protocol)
    {
        host = "http://";
    }
    else
    {
        return "";
    }
    host += m_machine;

    return host;
}
