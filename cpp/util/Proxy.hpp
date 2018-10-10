/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_PROXY_HPP
#define SNOWFLAKECLIENT_PROXY_HPP

#include <cstddef>
#include <string>

namespace Snowflake
{
namespace Client
{
namespace Util
{
class Proxy
{
public:
    Proxy() = default;

    Proxy(const std::string &proxy_str);

    Proxy(std::string &user, std::string &pwd, std::string &machine, unsigned port, std::string &scheme);

    ~Proxy() = default;

    inline const std::string& getUser()
    {
        return this->m_user;
    }

    inline const std::string& getPwd()
    {
        return this->m_pwd;
    }

    inline const std::string& getMachine()
    {
        return this->m_machine;
    }

    inline unsigned getPort()
    {
        return this->m_port;
    }

    inline const std::string& getScheme()
    {
        return this->m_scheme;
    }

    void clearPwd();

    void setProxyFromEnv();

private:
    std::string m_user;
    std::string m_pwd;
    std::string m_machine;
    unsigned m_port = 0;
    std::string m_scheme;

    void stringToProxyParts(const std::string &proxy);
};
}
}
}

#endif //SNOWFLAKECLIENT_PROXY_HPP
