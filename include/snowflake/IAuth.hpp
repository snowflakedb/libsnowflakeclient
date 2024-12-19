/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_IAUTH_HPP
#define SNOWFLAKECLIENT_IAUTH_HPP

#include <string>
#include <snowflake/SFURL.hpp>
#include "../../lib/snowflake_util.h"

#define AUTH_THROW(msg)      \
{                            \
  throw AuthException(msg);  \
}

namespace Snowflake
{
namespace Client
{
namespace IAuth
{
    /**
     * Authenticator
     */
    class IAuthenticator
    {
    public:

        IAuthenticator() : m_renewTimeout(0)
        {}

        virtual ~IAuthenticator()
        {}

        virtual void authenticate() = 0;

        virtual void updateDataMap(jsonObject_t& dataMap) = 0;

        // Retrieve authenticator renew timeout, return 0 if not available.
        // When the authenticator renew timeout is available, the connection should
        // renew the authentication (call renewDataMap) for each time the
        // authenticator specific timeout exceeded within the entire login timeout.
        int64 getAuthRenewTimeout()
        {
            return m_renewTimeout;
        }

        // Renew the autentication and update datamap.
        // The default behavior is to call authenticate() and updateDataMap().
        virtual void renewDataMap(jsonObject_t& dataMap);

    protected:
        int64 m_renewTimeout;
    };


    class IDPAuthenticator
    {
    public:
        IDPAuthenticator()
        {};

        virtual ~IDPAuthenticator()
        {};

        bool getIDPInfo();

        virtual SFURL getServerURLSync();
        /*
         * Get IdpInfo for OKTA and SAML 2.0 application
         */
        virtual bool curlPostCall(SFURL& url, const jsonObject_t& body, jsonObject_t& resp) = 0;
        virtual bool curlGetCall(SFURL& url, jsonObject_t& resp, bool parseJSON, std::string& raw_data, bool& isRetry) = 0;

        std::string m_errMsg = "";
    
    protected:
        std::string tokenURLStr;
        std::string ssoURLStr;
        //For EXTERNALBROSER in the future
        std::string proofKeyStr;

        //These fields should be definied in the child class.
        std::string m_authenticator;
        std::string m_account;
        std::string m_appID;
        std::string m_appVersion;
        std::string m_user;
        std::string m_port;
        std::string m_host;
        std::string m_protocol;
    };

    class IAuthenticatorOKTA : public IDPAuthenticator, public IAuthenticator
    {
    public:
        IAuthenticatorOKTA() {};

        virtual ~IAuthenticatorOKTA() {};

        virtual void authenticate();

        virtual void updateDataMap(jsonObject_t& dataMap);

        /**
         * Extract post back url from samel response. Input is in HTML format.
        */
        std::string extractPostBackUrlFromSamlResponse(std::string html);

    protected:
        //These fields should be definied in the child class.
        std::string m_password;
        bool m_disableSamlUrlCheck;
        int8 m_retriedCount;
        int64 m_retryTimeout;
        std::string oneTimeToken;
        std::string m_samlResponse;
    };
} // namespace Auth
} // namespace Client
} // namespace Snowflake

#endif //SNOWFLAKECLIENT_IIDP_AUTH_HPP
