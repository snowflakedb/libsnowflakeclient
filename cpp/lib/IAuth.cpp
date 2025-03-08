/*
 * Copyright (c) 2025 Snowflake Computing, Inc. All rights reserved.
 */

#include <string>
#include "cJSON.h"
#include "../include/snowflake/entities.hpp"
#include "../logger/SFLogger.hpp"
#include "snowflake/IAuth.hpp"
#include "../../lib/authenticator.h"
#include "client_int.h"

namespace Snowflake
{
namespace Client
{
    namespace IAuth
    {
        using namespace picojson;

        const char* AuthErrorHandler::getErrorMessage()
        {
            return m_errMsg.c_str();
        }

        bool AuthErrorHandler::isError()
        {
            return !m_errMsg.empty();
        }

        void IAuthenticator::renewDataMap(jsonObject_t& dataMap)
        {
            authenticate();
            updateDataMap(dataMap);
        }

        bool IDPAuthenticator::getIDPInfo(jsonObject_t& dataMap)
        {
            bool ret = true;
            SFURL connectURL = getServerURLSync().path(AUTHENTICATOR_URL);
            dataMap["ACCOUNT_NAME"] = value(m_account);
            dataMap["AUTHENTICATOR"] = value(m_authenticator);
            dataMap["LOGIN_NAME"] = value(m_user);
            dataMap["PORT"] = value(m_port);
            dataMap["PROTOCOL"] = value(m_protocol);

            jsonObject_t authnData, respData;
            authnData["data"] = value(dataMap);

            if (curlPostCall(connectURL, authnData, respData))
            {
                jsonObject_t& data = respData["data"].get<jsonObject_t>();
                ssoURLStr = data["ssoUrl"].get<std::string>();

                if (getAuthenticatorType(m_authenticator.c_str()) == AUTH_OKTA) {
                    tokenURLStr = data["tokenUrl"].get<std::string>();
                }

                if (getAuthenticatorType(m_authenticator.c_str()) == AUTH_EXTERNALBROWSER) {
                    proofKey = data["proofKey"].get<std::string>();
                }
            }
            else {
                CXX_LOG_INFO("sf", "Connection", "getIdpInfo",
                    "Fail to get authenticator info");
                m_errMsg = "Fail to get authenticator info";
                ret = false;
            }
            return ret;
        }

        SFURL IDPAuthenticator::getServerURLSync()
        {
            return SFURL::getServerURLSync(m_protocol, m_host, m_port);
        }

        void IAuthenticatorOKTA::authenticate() 
        {
            // 1. get authenticator info
            jsonObject_t dataMap;
            dataMap["CLIENT_APP_ID"] = value(m_appID);
            dataMap["CLIENT_APP_VERSION"] = value(m_appVersion);
            if (!m_idp->getIDPInfo(dataMap)) {
                return;
            }

            // 2. verify ssoUrl and tokenUrl contains same prefix
            if (!urlHasSamePrefix(m_idp->tokenURLStr, m_idp->m_authenticator))

            {
                CXX_LOG_ERROR("sf", "AuthenticatorOKTA", "authenticate",
                    "The specified authenticator is not supported, "
                    "authenticator=%s, token url=%s, sso url=%s",
                    m_idp->m_authenticator.c_str(), m_idp->tokenURLStr.c_str(), m_idp->ssoURLStr.c_str());
                m_errMsg = "SFAuthenticatorVerificationFailed: ssoUrl or tokenUrl does not contains same prefix with the authenticator";
                return;
            }

            // 3. get one time token from okta
            while (true)
            {
                SFURL tokenURL = SFURL::parse(m_idp->tokenURLStr);

                jsonObject_t dataMap, respData;
                dataMap["username"] = picojson::value(m_idp->m_user);
                dataMap["password"] = picojson::value(m_password);

                if (!m_idp->curlPostCall(tokenURL, dataMap, respData))
                {
                    CXX_LOG_WARN("sf", "AuthenticatorOKTA", "getOneTimeToken",
                        "Fail to get one time token response, response body=%s",
                        picojson::value(respData).serialize().c_str());
                    return;
                }

                oneTimeToken = respData.find("sessionToken") != respData.end() ?
                    respData["sessionToken"].get<std::string>() : 
                    respData["cookieToken"].get<std::string>();

                // 4. get SAML response
                jsonObject_t resp;
                bool isRetry = false;
                SFURL sso_url = SFURL::parse(m_idp->ssoURLStr);
                sso_url.addQueryParam("onetimetoken", oneTimeToken);
                if (!m_idp->curlGetCall(sso_url, resp, false, m_samlResponse, isRetry))
                {
                    if (isRetry)
                    {
                        CXX_LOG_TRACE("sf", "Connection", "Connect",
                            "Retry on getting SAML response with one time token renewed for %d times "
                            "with updated retryTimeout = %d",
                            m_idp->m_retriedCount, m_idp->m_retryTimeout);
                        continue;
                    }
                    return;
                }
                break;
            }

            // 5. Validate post_back_url matches Snowflake URL
            std::string post_back_url = extractPostBackUrlFromSamlResponse(m_samlResponse);
            std::string server_url = m_idp->getServerURLSync().toString();

            if ((!m_disableSamlUrlCheck) &&
                (!urlHasSamePrefix(post_back_url, server_url)))
            {
                CXX_LOG_ERROR("sf","AuthenticatorOKTA", "authenticate",
                    "The specified authenticator and destination URL in "
                    "Saml Assertion did not "
                    "match, expected=%s, post back=%s",
                    server_url.c_str(),
                    post_back_url.c_str());
                m_errMsg = "SFSamlResponseVerificationFailed";
            }
        }

        void IAuthenticatorOKTA::updateDataMap(jsonObject_t& dataMap)
        {
            dataMap["RAW_SAML_RESPONSE"] = picojson::value(m_samlResponse);
        }

        std::string IAuthenticatorOKTA::extractPostBackUrlFromSamlResponse(std::string html)
        {
            std::size_t form_start = html.find("<form");
            std::size_t post_back_start = html.find("action=\"", form_start);
            post_back_start += 8;
            std::size_t post_back_end = html.find("\"", post_back_start);

            std::string post_back_url = html.substr(post_back_start,
                post_back_end - post_back_start);
            CXX_LOG_TRACE("sf", "AuthenticatorOKTA",
                "extractPostBackUrlFromSamlResponse",
                "Post back url before unescape: %s", post_back_url.c_str());
            char unescaped_url[200];
            decode_html_entities_utf8(unescaped_url, post_back_url.c_str());
            CXX_LOG_TRACE("sf", "AuthenticatorOKTA",
                "extractPostBackUrlFromSamlResponse",
                "Post back url after unescape: %s", unescaped_url);
            return std::string(unescaped_url);
        }
    }// namespace IAuth
} // namespace Client
} // namespace Snowflake
