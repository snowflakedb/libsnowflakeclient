/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include "Proxy.hpp"

Snowflake::Client::Util::Proxy::Proxy(const std::string &proxy_str)
{
    stringToProxyParts(proxy_str);
}

Snowflake::Client::Util::Proxy::Proxy(std::string &user, std::string &pwd,
  std::string &machine, unsigned port, Snowflake::Client::Util::Proxy::Protocol scheme) :
  m_user(user), m_pwd(pwd), m_machine(machine), m_port(port), m_protocol(scheme) {}


void Snowflake::Client::Util::Proxy::stringToProxyParts(const std::string &proxy)
{
    std::string protocol;
    std::size_t found, pos;

    // Return on an empty input proxy string
    if (proxy.empty()) {
        return;
    }

    // Proxy is in the form: "[protocol://][user:password@]machine[:port]"
    pos = 0;
    found = proxy.find("://");
    if (found != std::string::npos) {
        // constant value for '://'
        protocol = proxy.substr(pos, found - pos);
        m_protocol = !protocol.compare("http") ? Snowflake::Client::Util::Proxy::Protocol::HTTP :
                                               Snowflake::Client::Util::Proxy::Protocol::HTTPS;
        // Set port with default value based on protocol and update
        // below if user specifies port in proxy string
        m_port = m_protocol == Snowflake::Client::Util::Proxy::Protocol::HTTP ? 80 : 443;
        pos = found + 3;
    } else {
        m_protocol = Snowflake::Client::Util::Proxy::Protocol::HTTP;
        m_port = 80;
    }

    found = proxy.find('@', pos);
    // If username and password exist, set them
    if (found != std::string::npos) {
        size_t colon_pos = proxy.find(':', pos);
        m_user = std::string(proxy.substr(pos, colon_pos - pos));
        pos = colon_pos + 1;
        m_pwd = std::string(proxy.substr(pos, found - pos));
        pos = found + 1;
    }

    found = proxy.find(':', pos);
    if (found != std::string::npos) {
        // port exists, set machine and port
        m_machine = std::string(proxy.substr(pos, found - pos));
        pos = found + 1;
        m_port = (unsigned int) strtoul(proxy.substr(pos, (proxy.length()) - pos).c_str(), nullptr, 10);
    } else {
        // just set machine
        m_machine = std::string(proxy.substr(pos, (proxy.length()) - pos));
    }
}

void Snowflake::Client::Util::Proxy::clearPwd() {
    m_pwd.clear();
}

void Snowflake::Client::Util::Proxy::setProxyFromEnv() {
    std::string proxy;

    // Get proxy string and set scheme
    if (std::getenv("all_proxy")) {
        proxy = std::getenv("all_proxy");
    } else if (std::getenv("https_proxy")) {
        proxy = std::getenv("https_proxy");
    } else if (std::getenv("http_proxy")) {
        proxy = std::getenv("http_proxy");
    } else {
        return;
    }

    stringToProxyParts(proxy);
}