/*
 * Copyright (c) 2025 Snowflake Computing, Inc. All rights reserved.
 */

#include <string>
#include <regex>
#ifdef _WIN32
#include <WS2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include "cJSON.h"
#include "../include/snowflake/entities.hpp"
#include "../logger/SFLogger.hpp"
#include "snowflake/IAuth.hpp"
#include "../../lib/authenticator.h"

#include <openssl/rand.h>
#include "../include/snowflake/entities.hpp"
#include "../logger/SFLogger.hpp"

#ifdef __APPLE__
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>
#endif

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
            SFURL connectURL = getServerURLSync().path("/session/authenticator-request");
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
                CXX_LOG_INFO("sf::IDPAuthenticator::getIDPInfo::Fail to get authenticator info.");
                m_errMsg = "Fail to get authenticator info.";
                ret = false;
            }
            return ret;
        }

        SFURL IDPAuthenticator::getServerURLSync()
        {
            SFURL url = SFURL().scheme(m_protocol)
                .host(m_host)
                .port(m_port);

            return url;
        }

        int IAuthenticatorExternalBrowser::getPort()
        {
            return m_authWebServer->getPort();
        }

        /**
         * Authenticate user by external browser
         */
        void IAuthenticatorExternalBrowser::authenticate()
        {
#ifdef _WIN32
            m_authWinSock = AuthWinSock();
            if (m_authWinSock.isError())
            {
                return;
            }
#endif
            m_authWebServer->start();
            if (m_authWebServer->isError())
            {
                return;
            }

            std::map<std::string, std::string> out;
            getLoginUrl(out, m_authWebServer->getPort());
            if (isError())
            {
                return;
            }

            startWebBrowser(out[std::string("LOGIN_URL")]);
            if (isError())
            {
                return;
            }
            m_proofKey = out[std::string("PROOF_KEY")];

            m_authWebServer->setTimeout(m_browser_response_timeout);
            m_authWebServer->startAccept();
            if (m_authWebServer->isError())
            {
                return;
            }
            // accept SAML token
            while (m_authWebServer->receive())
            {
                // nop
            }

            m_authWebServer->stop();
            if (m_authWebServer->isError())
            {
                return;
            }

            m_token = m_authWebServer->getSAMLToken();
            m_consentCacheIdToken = m_authWebServer->isConsentCacheIdToken();
        }

        /**
         * Update data map to get final authentication.
         * @param dataMap data map for the final authentication
         */
        void IAuthenticatorExternalBrowser::updateDataMap(jsonObject_t& dataMap)
        {
            dataMap["PROOF_KEY"] = picojson::value(m_proofKey);
            dataMap["TOKEN"] = picojson::value(m_token);
            dataMap["AUTHENTICATOR"] = picojson::value(SF_AUTHENTICATOR_EXTERNAL_BROWSER);
        }

        /**
         * Get Login URL for multiple SAML
         * @param out the login URL
         * @param port port number listening to get SAML token
         */
        void IAuthenticatorExternalBrowser::getLoginUrl(
            std::map<std::string, std::string>& out, int port)
        {
            if (m_disable_console_login)
            {
                jsonObject_t dataMap;
                dataMap["BROWSER_MODE_REDIRECT_PORT"] = picojson::value((double)port);
                if (!m_idp->getIDPInfo(dataMap)) {
                    return;
                }
                out[std::string("LOGIN_URL")] = m_idp->ssoURLStr;
                out[std::string("PROOF_KEY")] = m_idp->proofKey;
            }
            else
            {
                std::string proofKey = generateProofKey();
                SFURL connectURL = m_idp->getServerURLSync().path("/console/login");
                connectURL.addQueryParam("login_name", m_idp->m_user);
                connectURL.addQueryParam("browser_mode_redirect_port", std::to_string(m_authWebServer->getPort()));
                connectURL.addQueryParam("proof_key", proofKey);

                m_idp->ssoURLStr = connectURL.toString();
                out[std::string("LOGIN_URL")] = m_idp->ssoURLStr;
                out[std::string("PROOF_KEY")] = proofKey;
            }
            CXX_LOG_DEBUG("sf::IAuthenticatorExternalBrowser::getLoginUrl::SSO URL: %s.", m_idp->ssoURLStr.c_str());
        }

        std::string IAuthenticatorExternalBrowser::generateProofKey()
        {
            std::vector<char> randomness(32);
            RAND_bytes(reinterpret_cast<unsigned char*>(randomness.data()), randomness.size());
            return Base64::encodePadding(randomness);
        }

        /**
         * Start web browser so that the user can type IdP user and password
         * @param ssoUrl SSO URL
         */
        void IAuthenticatorExternalBrowser::startWebBrowser(std::string ssoUrl)
        {
            // Validate the SSO URL to mitigate OS command injection vulnerability.
            // Semicolon (;) and single quote (') are not allowed.
            char regexStr[] = "^http(s?)\\:\\/\\/[0-9a-zA-Z]([-.\\w]*[0-9a-zA-Z@:])*(:(0-9)*)*(\\/?)([a-zA-Z0-9\\-\\.\\?\\,\\&\\(\\)\\/\\\\\\+&%\\$#_=@]*)?$";
            if (!std::regex_match(ssoUrl, std::regex(regexStr)))
            {
                CXX_LOG_ERROR("sf::IAuthenticatorExternalBrowser::startWebBrowser::Failed to start web browser.Invalid SSO URL.");
                m_errMsg = "SFAuthWebBrowserFailed: Invalid SSO URL.";
            }

            std::cout << "Initiating login request with your identity provider. A "
                "browser window should have opened for you to complete the "
                "login. If you can't see it, check existing browser windows, "
                "or your OS settings. Press CTRL+C to abort and try again..." << "\n";
            // need double quotes to reserve ampasand characters
            std::stringstream urlBuf;
#ifdef __APPLE__
            openURL(ssoUrl);
#elif _WIN32
            HINSTANCE ret = ShellExecuteA(NULL, "open", ssoUrl.c_str(), NULL, NULL,
                SW_SHOWNORMAL);
            if (ret > (HINSTANCE)32)
            {
                // success
                return;
            }
            CXX_LOG_ERROR("sf::IAuthenticatorExternalBrowser::startWebBrowser::Failed to start web browser. err: %d.", (int)(unsigned long long)ret);
            m_errMsg = "SFAuthWebBrowserFailed: Failed to start web browser.";

#else
            // use fork to avoid using system() call and prevent command injection
            char* argv[3];
            pid_t child_pid;
            int child_status;
            argv[0] = const_cast<char*>("xdg-open");
            argv[1] = const_cast<char*>(ssoUrl.c_str());
            argv[2] = NULL;

            child_pid = fork();
            if (child_pid < 0)
            {
                // fork failed
                CXX_LOG_ERROR("sf::IAuthenticatorExternalBrowser::startWebBrowser::Failed to start web browser on fork.");
                m_errMsg = "SFAuthWebBrowserFailed: Failed to start web browser on fork.";
            }
            else if (child_pid == 0)
            {
                // This is done by the child process.
                execvp(argv[0], argv);
                /* If execvp returns, it must have failed. */
                exit(-1); // Do nothing as we are in child process
            }
            else
            {
                // This is run by the parent. Wait for the child to terminate.
                if (waitpid(child_pid, &child_status, 0) < 0)
                {
                    CXX_LOG_ERROR("sf::IAuthenticatorExternalBrowser::startWebBrowser::Failed to start web browser on waitpid.");
                    m_errMsg = "SFAuthWebBrowserFailed: Failed to start web browser on waitpid.";
                }

                if (WIFEXITED(child_status)) {
                    const int es = WEXITSTATUS(child_status);
                    if (es != 0)
                    {
                        CXX_LOG_ERROR("sf::IAuthenticatorExternalBrowser::startWebBrowser::Failed to start web browser. xdg-open returned %d.", es);
                        m_errMsg = "SFAuthWebBrowserFailed: Failed to start web browser. xdg-open returned.";
                    }
                }
            }
#endif
        }

#ifdef __APPLE__
        void IAuthenticatorExternalBrowser::openURL(const std::string& url_str) {
            CFURLRef url = CFURLCreateWithBytes(
                NULL,                        // allocator
                (UInt8*)url_str.c_str(),     // URLBytes
                url_str.length(),            // length
                kCFStringEncodingASCII,      // encoding
                NULL                         // baseURL
            );
            LSOpenCFURLRef(url, 0);
            CFRelease(url);
        }
#endif

#ifdef _WIN32
        AuthWinSock::AuthWinSock()
        {
            WORD wVersionRequested;
            WSADATA wsaData;
            int err;
            wVersionRequested = MAKEWORD(2, 2);
            err = WSAStartup(wVersionRequested, &wsaData);
            if (err != 0)
            {
                CXX_LOG_ERROR("sf::AuthWinSock::constructor::Failed to call WSAStartup: %d.", err);
                m_errMsg = "SFAuthWebBrowserFailed: Failed to call WSAStartup.";
            }
            if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
            {
                /* Tell the user that we could not find a usable */
                /* WinSock DLL.                                  */
                WSACleanup();
                CXX_LOG_ERROR("sf::AuthWinSock::constructor::Could not find a usable version of Winsock.dll");
                m_errMsg = "SFAuthWebBrowserFailed: Could not find a usable version of Winsock.dll.";
            }

            CXX_LOG_INFO("sf::AuthWinSock::constructor::Winsock %s.%s DLL was found", std::to_string(LOBYTE(wsaData.wVersion)).c_str(), std::to_string(HIBYTE(wsaData.wVersion)).c_str());
        }

        AuthWinSock::~AuthWinSock()
        {
            WSACleanup();
        }
#endif

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
                CXX_LOG_ERROR("sf::IAuthenticatorOKTA::authenticate::The specified authenticator is not supported, authenticator=%s, token url=%s, sso url=%s.",
                    m_idp->m_authenticator.c_str(), m_idp->tokenURLStr.c_str(), m_idp->ssoURLStr.c_str());
                m_errMsg = "SFAuthenticatorVerificationFailed: ssoUrl or tokenUrl does not contains same prefix with the authenticator.";
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
                    CXX_LOG_WARN("sf::IAuthenticatorOKTA::authenticate::Fail to get one time token response, response body=%s.",
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
                        CXX_LOG_TRACE("sf::IAuthenticatorOKTA::authenticate::Retry on getting SAML response with one time token renewed for %d times with updated retryTimeout = %d.",
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
                CXX_LOG_ERROR("sf","IAuthenticatorOKTA::authenticate::The specified authenticator and destination URL in Saml Assertion did not match, expected=%s, post back=%s.",
                    server_url.c_str(),
                    post_back_url.c_str());
                m_errMsg = "SFSamlResponseVerificationFailed.";
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
            CXX_LOG_TRACE("sf::IAuthenticatorOKTA::extractPostBackUrlFromSamlResponse::Post back url before unescape: %s.", post_back_url.c_str());
            char unescaped_url[200];
            decode_html_entities_utf8(unescaped_url, post_back_url.c_str());
            CXX_LOG_TRACE("sf::IAuthenticatorOKTA::extractPostBackUrlFromSamlResponse::Post back url after unescape: %s.", unescaped_url);
            return std::string(unescaped_url);
        }
    }// namespace IAuth
} // namespace Client
} // namespace Snowflake
