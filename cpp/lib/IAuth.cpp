/*
 * Copyright (c) 2024 Snowflake Computing, Inc. All rights reserved.
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


#ifdef __APPLE__
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>
#endif

#include "../../lib/authenticator.h"
#include <openssl/rand.h>


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
                CXX_LOG_INFO("sf", "Connection", "getIdpInfo",
                    "Fail to get authenticator info");
                m_errMsg = "Fail to get authenticator info";
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

        void IAuthenticatorOKTA::authenticate()
        {
            // 1. get authenticator info
            jsonObject_t dataMap;
            dataMap["CLIENT_APP_ID"] = value(m_appID);
            dataMap["CLIENT_APP_VERSION"] = value(m_appVersion);
            if (!m_idp->getIDPInfo(dataMap)) {
                return;
            }
            CXX_LOG_INFO("Okta 1step Error. ErrorMessage: %s", m_errMsg.c_str());

            // 2. verify ssoUrl and tokenUrl contains same prefix
            if (!urlHasSamePrefix(m_idp->tokenURLStr, m_idp->m_authenticator))
            {
                CXX_LOG_INFO("Okta 2step Error. ErrorMessage: %s", m_errMsg.c_str());

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

                oneTimeToken = respData["sessionToken"].get<std::string>();
                if (oneTimeToken.empty()) {
                    oneTimeToken = respData["cookieToken"].get<std::string>();
                }
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
            CXX_LOG_INFO("Okta 4step Error. ErrorMessage: %s", m_errMsg.c_str());

            // 5. Validate post_back_url matches Snowflake URL
            std::string post_back_url = extractPostBackUrlFromSamlResponse(m_samlResponse);
            std::string server_url = m_idp->getServerURLSync().toString();
            if ((!m_disableSamlUrlCheck) &&
                (!urlHasSamePrefix(post_back_url, server_url)))
            {
                CXX_LOG_ERROR("sf", "AuthenticatorOKTA", "authenticate",
                    "The specified authenticator and destination URL in "
                    "Saml Assertion did not "
                    "match, expected=%s, post back=%s",
                    server_url.c_str(),
                    post_back_url.c_str());
                m_errMsg = "SFSamlResponseVerificationFailed";
                CXX_LOG_INFO("Okta 5step Error. ErrorMessage: %s", m_errMsg.c_str());
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
            CXX_LOG_DEBUG("sf", "AuthenticatorExternalBrowser", "getSS  OUrl",
                "SSO URL: %s", m_idp->ssoURLStr.c_str());
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
                CXX_LOG_ERROR("sf", "AuthenticatorExternalBrowser", "startWebBrowser",
                    "Failed to start web browser. %s", "Invalid SSO URL.");
                m_errMsg = "SFAuthWebBrowserFailed: Invalid SSO URL";
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
            CXX_LOG_ERROR("sf", "AuthenticatorExternalBrowser", "startWebBrowser",
                "Failed to start web browser. err: %d", (int)(unsigned long long)ret);
            m_errMsg = "SFAuthWebBrowserFailed: Failed to start web browser";

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
                CXX_LOG_ERROR("sf", "AuthenticatorExternalBrowser", "startWebBrowser",
                    "Failed to start web browser on fork.");
                m_errMsg = "SFAuthWebBrowserFailed: Failed to start web browser on fork";
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
                    CXX_LOG_ERROR("sf", "AuthenticatorExternalBrowser", "startWebBrowser",
                        "Failed to start web browser on waitpid.");
                    m_errMsg = "SFAuthWebBrowserFailed: Failed to start web browser on waitpid";
                }

                if (WIFEXITED(child_status)) {
                    const int es = WEXITSTATUS(child_status);
                    if (es != 0)
                    {
                        CXX_LOG_ERROR("sf", "AuthenticatorExternalBrowser", "startWebBrowser",
                            "Failed to start web browser. xdg-open returned %d", es);
                        m_errMsg = "SFAuthWebBrowserFailed: Failed to start web browser. xdg-open returned";
                    }
                }
            }
#endif
        }

#ifdef __APPLE__
        void AuthenticatorExternalBrowser::openURL(const std::string& url_str) {
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
                CXX_LOG_ERROR(
                    "sf", "AuthWinSock", "constructor",
                    "Failed to call WSAStartup: %d", err);
                m_errMsg = "SFAuthWebBrowserFailed: Failed to call WSAStartup";
            }
            if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
            {
                /* Tell the user that we could not find a usable */
                /* WinSock DLL.                                  */
                WSACleanup();
                CXX_LOG_ERROR(
                    "sf", "AuthWinSock", "constructor",
                    "Could not find a usable version of Winsock.dll.%s", "");
                m_errMsg = "SFAuthWebBrowserFailed: Could not find a usable version of Winsock.dll";
            }

            CXX_LOG_INFO("sf", "AuthWinSock",
                "constructor",
                "Winsock %s.%s DLL was found",
                std::to_string(LOBYTE(wsaData.wVersion)).c_str(),
                std::to_string(HIBYTE(wsaData.wVersion)).c_str());
        }

        AuthWinSock::~AuthWinSock()
        {
            WSACleanup();
        }
#endif

        /**
  * Constructor for IAuthWebServer
  */
        IAuthWebServer::IAuthWebServer() : m_socket_descriptor(0),
            m_port(0),
            m_saml_token(""),
            m_consent_cache_id_token(true),
            m_origin(""),
            m_timeout(SF_BROWSER_RESPONSE_TIMEOUT)
        {
            // nop
        }

        /**
         * Destructor for IAuthWebServer
         */
        IAuthWebServer::~IAuthWebServer()
        {
            // nop
        }


        /**
         * Start web server that accepts SAML token from Snowflake
         */
        void IAuthWebServer::start()
        {
            m_socket_descriptor = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            // TODO: On Windows, socket functions don't set errno and the error code
            // should be retrieved by WSAGetLastError(). Therefore for now the error
            // message won't be logged correctly on Windows.
            // Leave it for now since it won't affect the functionality.
            if ((int)m_socket_descriptor < 0)
            {
                CXX_LOG_ERROR(
                    "sf", "IAuthWebServer", "start",
                    "Failed to start web server. Could not create a socket. err: %s",
                    "");
                m_errMsg = "SFAuthWebBrowserFailed: Failed to start web server. Could not create a socket.";
                return;
            }

            struct sockaddr_in recv_server;
            memset((char*)&recv_server, 0, sizeof(struct sockaddr_in));
            recv_server.sin_family = AF_INET;
            inet_pton(AF_INET, "127.0.0.1", &recv_server.sin_addr.s_addr);
            recv_server.sin_port = 0; // ephemeral port
            if (bind(m_socket_descriptor, (struct sockaddr*)&recv_server,
                sizeof(struct sockaddr_in)) < 0)
            {
                CXX_LOG_ERROR(
                    "sf", "IAuthWebServer", "start",
                    "Failed to start web server. Could not bind a port. err: %s", "");
                m_errMsg = "SFAuthWebBrowserFailed: Failed to start web server. Could not bind a port";
                return;
            }
            if (listen(m_socket_descriptor, 0) < 0)
            {
                CXX_LOG_ERROR(
                    "sf", "IAuthWebServer", "start",
                    "Failed to start web server. Could not listen a port. err: %s", "");
                m_errMsg = "SFAuthWebBrowserFailed: Failed to start web server. Could not listen a port";
                return;
            }
            socklen_t len = sizeof(recv_server);
            if (getsockname(m_socket_descriptor,
                (struct sockaddr*)&recv_server, &len) < 0)
            {
                CXX_LOG_ERROR(
                    "sf", "IAuthWebServer", "start",
                    "Failed to start web server. Could not get the port. err: %s", "");
                m_errMsg = "SFAuthWebBrowserFailed: Failed to start web server. Could not get the port.";
                return;
            }
            m_port = ntohs(recv_server.sin_port);
            CXX_LOG_INFO("sf", "IAuthWebServer", "start",
                "Web Server successfully started with port %d", m_port);
        }

        /**
         * Stop web server
         */
        void IAuthWebServer::stop()
        {
            if ((int)m_socket_desc_web_client > 0)
            {
#ifndef _WIN32
                shutdown(m_socket_desc_web_client, SHUT_RDWR);
                int ret = close(m_socket_desc_web_client);
#else
                shutdown(m_socket_desc_web_client, SD_BOTH);
                int ret = closesocket(m_socket_desc_web_client);
#endif
                if (ret < 0)
                {
                    CXX_LOG_ERROR(
                        "sf", "IAuthWebServer", "stop",
                        "Failed to accept the SAML token err: %s", "")
                        m_errMsg = "SFAuthWebBrowserFailed: Not HTTP request";
                    return;
                }
            }
            m_socket_desc_web_client = 0;

            if ((int)m_socket_descriptor > 0)
            {
#ifndef _WIN32
                int ret = close(m_socket_descriptor);
#else
                int ret = closesocket(m_socket_descriptor);
#endif
                if (ret < 0)
                {
                    CXX_LOG_ERROR(
                        "sf", "IAuthWebServer", "stop",
                        "Failed to stop web server. err: %s", "")
                        m_socket_descriptor = 0;
                    m_errMsg = "SFAuthWebBrowserFailed";
                    return;
                }
            }
            m_socket_descriptor = 0;
        }

        /**
         * Get port number listening
         * @return port number
         */
        int IAuthWebServer::getPort()
        {
            return m_port;
        }

        /**
         * Set the timeout for the web server.
         */
        void IAuthWebServer::setTimeout(int timeout)
        {
            m_timeout = timeout;
        }

        void IAuthWebServer::startAccept()
        {
            struct sockaddr_in client = { 0, 0, 0, 0 };
            memset((char*)&client, 0, sizeof(struct sockaddr_in));
            socklen_t len = sizeof(client);

            fd_set fd;
            timeval timeout;
            FD_ZERO(&fd);
            FD_SET(m_socket_descriptor, &fd);
            timeout.tv_sec = m_timeout;
            timeout.tv_usec = 0;
            int retVal = select(m_socket_descriptor + 1, &fd, NULL, NULL, &timeout);
            if (retVal > 0)
            {
                m_socket_desc_web_client = accept(
                    m_socket_descriptor, (struct sockaddr*)&client, &len);
                if ((int)m_socket_desc_web_client < 0)
                {
                    CXX_LOG_ERROR(
                        "sf", "IAuthWebServer", "startAccept",
                        "Failed to receive SAML token. Could not accept a request. err: %s", "");
                    m_errMsg = "Failed to receive SAML token. Could not accept a request.";
                }
            }
            else if (retVal == 0)
            {
                CXX_LOG_ERROR("sf", "IAuthWebServer", "startAccept", "Auth browser timed out");
                m_errMsg = "SFAuthWebBrowserFailed. Auth browser timed out.";

            }
            else
            {
                CXX_LOG_ERROR(
                    "sf", "IAuthWebServer", "startAccept",
                    "Failed to determine status of auth web server. err: %s", "");
                m_errMsg = "SFAuthWebBrowserFailed. Failed to determine status of auth web server..";
            }
        }

        bool IAuthWebServer::receive()
        {
            bool is_options = false;

            char mesg[20000];
            char* reqline;
            char* rest_mesg;
            int recvlen;
            memset((void*)mesg, (int)'\0', sizeof(mesg));
            if ((recvlen = (int)recv(m_socket_desc_web_client, mesg, sizeof(mesg), 0)) < 0)
            {
                CXX_LOG_ERROR(
                    "sf", "IAuthWebServer", "receive",
                    "Failed to receive SAML token. Could not receive a request.");
                m_errMsg = "SFAuthWebBrowserFailed: Failed to receive SAML token. Could not receive a request.";
            }
            reqline = sf_strtok(mesg, " \t\n", &rest_mesg);
            if (strncmp(reqline, "GET\0", 4) == 0)
            {
                parseAndRespondGetRequest(&rest_mesg);
            }
            else if (strncmp(reqline, "POST\0", 5) == 0)
            {
                parseAndRespondPostRequest(std::string(mesg, (unsigned long)recvlen));
            }
            else if (strncmp(reqline, "OPTIONS\0", 8) == 0)
            {
                parseAndRespondOptionsRequest(std::string(mesg, (unsigned long)recvlen));
                is_options = true;
            }
            else
            {
                CXX_LOG_ERROR(
                    "sf", "IAuthWebServer", "receive",
                    "Failed to receive SAML token. Could not get HTTP request. err: %s",
                    reqline);
                m_errMsg = "SFAuthWebBrowserFailed: Not HTTP request";
            }
            return is_options;
        }

        void IAuthWebServer::parseAndRespondPostRequest(std::string response)
        {
            auto ret = splitString(response, '\n');
            if (ret.empty())
            {
                CXX_LOG_ERROR(
                    "sf", "IAuthWebServer",
                    "parseAndRespondPostRequest", "No token parameter is found. %s",
                    response.c_str());
                send(m_socket_desc_web_client, "HTTP/1.0 400 Bad Request\n", 25, 0);
                return;
            }
            if (m_origin.empty())
            {
                respond(ret[ret.size() - 1]);
            }
            else
            {
                jsonValue_t json;
                std::string& payload = ret[ret.size() - 1];
                std::string err;
                picojson::parse(json, payload.begin(), payload.end(), &err);
                if (!err.empty())
                {
                    CXX_LOG_ERROR(
                        "sf",
                        "IAuthWebServer",
                        "parseAndRespondPostRequest",
                        "Error in parsing JSON: %s, err: %s", payload.c_str(), err.c_str());
                    return;
                }
                respondJson(json);
            }
        }

        void IAuthWebServer::parseAndRespondOptionsRequest(std::string response)
        {
            std::string requested_header;
            auto ret = splitString(response, '\n');
            if (ret.empty())
            {
                CXX_LOG_ERROR(
                    "sf", "IAuthWebServer",
                    "parseAndRespondOptionsRequest", "No token parameter is found. %s",
                    response.c_str());
                send(m_socket_desc_web_client, "HTTP/1.0 400 Bad Request\n", 25, 0);
                return;
            }

            for (auto const& value : ret)
            {
                if (value.find("Access-Control-Request-Method") != std::string::npos)
                {
                    auto v = value.substr(value.find(':') + 1);
                    trim(v, ' ');
                    if (v != "POST")
                    {
                        CXX_LOG_ERROR(
                            "sf", "IAuthWebServer",
                            "parseAndRespondOptionsRequest", "POST method is not requested. %s",
                            value.c_str());
                        send(m_socket_desc_web_client, "HTTP/1.0 400 Bad Request\n", 25, 0);
                        return;
                    }
                }
                else if (value.find("Access-Control-Request-Headers") != std::string::npos)
                {
                    requested_header = value.substr(value.find(':') + 1);
                    trim(requested_header, ' ');
                }
                else if (value.find("Origin") != std::string::npos)
                {
                    m_origin = value.substr(value.find(':') + 1);
                    trim(m_origin, ' ');
                }
            }
            if (requested_header.empty() || m_origin.empty())
            {
                CXX_LOG_ERROR(
                    "sf", "IAuthWebServer",
                    "parseAndRespondOptionsRequest",
                    "no Access-Control-Request-Headers or Origin header. %s",
                    response.c_str());
                send(m_socket_desc_web_client, "HTTP/1.0 400 Bad Request\n", 25, 0);
                return;
            }
            std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            );
            char current_timestamp[50];
            std::time_t t = (time_t)ms.count() / 1000;
            std::tm tms;
            strftime(current_timestamp, sizeof(current_timestamp), "%a, %d %b %Y %H:%M:%S GMT", sf_gmtime(&t, &tms));

            std::stringstream buf;
            buf << "HTTP/1.0 200 OK" << "\r\n"
                << "Date: " << current_timestamp << "\r\n"
                << "Access-Control-Allow-Methods: POST, GET" << "\r\n"
                << "Access-Control-Allow-Headers: " << requested_header << "\r\n"
                << "Access-Control-Max-Age: 86400" << "\r\n"
                << "Access-Control-Allow-Origin: " << m_origin << "\r\n"
                << "\r\n\r\n";
            send(m_socket_desc_web_client, buf.str().c_str(), (int)buf.str().length(), 0);
        }

        std::vector<std::string> IAuthWebServer::splitString(const std::string& s, char delimiter)
        {
            std::vector<std::string> tokens;
            std::string token;
            std::istringstream tokenStream(s);
            while (std::getline(tokenStream, token, delimiter))
            {
                tokens.push_back(token);
            }
            return tokens;
        }

        void IAuthWebServer::parseAndRespondGetRequest(char** rest_mesg)
        {
            char* path = sf_strtok(NULL, " \t", rest_mesg);
            char* protocol = sf_strtok(NULL, " \t\n", rest_mesg);
            if (strncmp(protocol, "HTTP/1.0", 8) != 0 &&
                strncmp(protocol, "HTTP/1.1", 8) != 0)
            {
                CXX_LOG_ERROR(
                    "sf", "IAuthWebServer", "receive", "Not HTTP request. %s", "");
                send(m_socket_desc_web_client, "HTTP/1.0 400 Bad Request\n", 25, 0);
                return;
            }

            if (strncmp(path, "/?", 2) != 0)
            {
                CXX_LOG_ERROR(
                    "sf", "IAuthWebServer", "receive", "No token parameter is found. %s",
                    path);
                send(m_socket_desc_web_client, "HTTP/1.0 400 Bad Request\n", 25, 0);
                return;
            }
            respond(std::string(&path[2]));
        }

        void IAuthWebServer::respondJson(picojson::value& json)
        {
            jsonObject_t& obj = json.get<picojson::object>();
            m_saml_token = obj["token"].get<std::string>();
            m_consent_cache_id_token = obj["consent"].get<bool>();

            jsonObject_t payloadBody;
            payloadBody["consent"] = picojson::value(m_consent_cache_id_token);
            auto payloadBodyString = picojson::value(payloadBody).serialize();

            std::stringstream buf;
            buf << "HTTP/1.0 200 OK" << "\r\n"
                << "Content-Type: text/html" << "\r\n"
                << "Content-Length: " << payloadBodyString.size() << "\r\n"
                << "Access-Control-Allow-Origin: " << m_origin << "\r\n"
                << "Vary: Accept-Encoding, Origin" << "\r\n"
                << "\r\n"
                << payloadBodyString;
            send(m_socket_desc_web_client, buf.str().c_str(), (int)buf.str().length(), 0);
        }

        void IAuthWebServer::respond(std::string queryParameters)
        {
            auto params = splitQuery(queryParameters);
            for (auto& p : params)
            {
                if (p.first == "token")
                {
                    m_saml_token = p.second;
                    break;
                }
            }
            const char* message =
                "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"/>\n"
                "<title>SAML Response for Snowflake</title></head>\n"
                "<body>\n"
                "Your identity was confirmed and propagated to Snowflake "
                "ODBC driver. You can close this window now and go back where you "
                "started from.\n"
                "</body></html>";
            std::stringstream buf;
            buf << "HTTP/1.0 200 OK" << "\r\n"
                << "Content-Type: text/html" << "\r\n"
                << "Content-Length: " << strlen(message) << "\r\n\r\n"
                << message;
            send(m_socket_desc_web_client, buf.str().c_str(), (int)buf.str().length(), 0);
        }

        std::string IAuthWebServer::unquote(std::string src)
        {
            std::string ret;
            char ch;
            int i, ii;
            for (i = 0; i < (int)src.length(); i++)
            {
                if (src[i] == '%')
                {
                    sf_sscanf(src.substr((unsigned long)(i + 1), 2).c_str(), "%x", &ii);
                    ch = static_cast<char>(ii);
                    ret += ch;
                    i = i + 2;
                }
                else
                {
                    ret += src[i];
                }
            }
            return ret;
        }

        std::vector<std::pair<std::string, std::string>> IAuthWebServer::splitQuery(std::string query)
        {
            std::vector<std::pair<std::string, std::string>> ret;
            std::string name;
            bool inValue = false;
            int prevPos = 0;
            int i;
            for (i = 0; i < (int)query.length(); ++i)
            {
                if (query[i] == '=' && !inValue) {
                    name = query.substr(prevPos, i - prevPos);
                    prevPos = i + 1;
                    inValue = true;
                }
                else if (query[i] == '&')
                {
                    ret.emplace_back(
                        std::make_pair(name, unquote(query.substr(prevPos, i - prevPos))));
                    name = "";
                    prevPos = i + 1;
                    inValue = false;
                }
            }
            if (!name.empty())
            {
                ret.emplace_back(
                    std::make_pair(name, unquote(query.substr(prevPos, i - prevPos))));
            }
            return ret;
        }

        std::string IAuthWebServer::getSAMLToken()
        {
            return m_saml_token;
        }

        bool IAuthWebServer::isConsentCacheIdToken()
        {
            return m_consent_cache_id_token;
        }
    }// namespace IAuth
} // namespace Client
} // namespace Snowflake
