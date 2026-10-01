#include <string>
#include <vector>
#include <regex>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <memory>
#ifdef _WIN32
#include <WS2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include "../include/snowflake/entities.hpp"
#include "../logger/SFLogger.hpp"
#include "snowflake/IAuth.hpp"
#include "../../lib/authenticator.h"
#include <openssl/rand.h>
#include "../lib/client_int.h"

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

        static bool equalsIgnoreCase(const std::string& left, const std::string& right)
        {
            return left.size() == right.size() &&
                std::equal(
                    left.begin(),
                    left.end(),
                    right.begin(),
                    [](char lhs, char rhs)
                    {
                        return std::tolower(static_cast<unsigned char>(lhs)) ==
                            std::tolower(static_cast<unsigned char>(rhs));
                    });
        }

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
                CXX_LOG_DEBUG("sf::IDPAuthenticator::getIDPInfo::Fail to get authenticator info.");
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

        IAuthenticatorExternalBrowser::IAuthenticatorExternalBrowser(IAuthWebServer* authWebServer, IDPAuthenticator* idp, IAuthenticationWebBrowserRunner* webBrowserRunner) :
            m_authWebServer(authWebServer),
            m_webBrowserRunner(webBrowserRunner != nullptr ? webBrowserRunner : IAuthenticationWebBrowserRunner::getInstance()),
            m_idp(idp)
        {
            if (!m_authWebServer)
            {
                m_authWebServer.reset(new AuthWebServer());
            }
            AuthWebServer* authServer = dynamic_cast<AuthWebServer*>(m_authWebServer.get());
            if (authServer && m_idp)
            {
                authServer->setExpectedOrigin(m_idp->getServerURLSync());
            }
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
            try {
                m_authWebServer->start();
                std::map<std::string, std::string> out;
                getLoginUrl(out, m_authWebServer->getPort());
                startWebBrowser(out[std::string("LOGIN_URL")]);
                m_proofKey = out[std::string("PROOF_KEY")];

                m_authWebServer->setTimeout(m_browser_response_timeout);
                m_authWebServer->startAccept();
                while (m_authWebServer->receive())
                {
                    m_authWebServer->startAccept();
                }
                m_authWebServer->stop();
            }
            catch (const AuthException& e) {
                try {
                    m_authWebServer->stop();
                }
                catch (const AuthException& ex) {
                    CXX_LOG_WARN("sf::IAuthenticatorExternalBrowser::authenticate::Failed to stop auth web server: %s.", e.cause().c_str());
                    m_errMsg = ex.cause();
                    return;
                }
                m_errMsg = e.cause();
                return;
            }
            
            m_token = m_authWebServer->getToken();
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
                connectURL.addQueryParam("login_name", m_user);
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
                throw AuthException("sf::IAuthenticatorExternalBrowser::Error. Invalid SSO URL.");
            }

            if (m_webBrowserRunner == nullptr)
            {
                CXX_LOG_ERROR("sf::IAuthenticatorExternalBrowser::startWebBrowser::Failed to start web browser. Unable to open SSO URL.");
                throw AuthException("sf::IAuthenticatorExternalBrowser::Error. Unable to open SSO URL.");
            }

            m_webBrowserRunner->startWebBrowser(ssoUrl);
        }

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

            CXX_LOG_DEBUG("sf::AuthWinSock::constructor::Winsock %s.%s DLL was found", std::to_string(LOBYTE(wsaData.wVersion)).c_str(), std::to_string(HIBYTE(wsaData.wVersion)).c_str());
        }

        AuthWinSock::~AuthWinSock()
        {
            WSACleanup();
        }
#endif

        IAuthenticatorOKTA::IAuthenticatorOKTA(IDPAuthenticator* idp) : 
            m_idp(idp){}

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
                //dataMap["username"] = picojson::value(m_idp->m_user);
                dataMap["username"] = picojson::value(m_user);
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
                      CXX_LOG_DEBUG("sf::IAuthenticatorOKTA::authenticate::Retry on getting SAML response with one time token renewed for %d times with updated retryTimeout = %d.",
                            m_idp->m_retriedCount, m_idp->m_retryTimeout);
                        continue;
                    }
                    CXX_LOG_ERROR("sf::IAuthenticatorOKTA::authenticate::Fail to get SAML response, response body=%s.",
                      picojson::value(resp).serialize().c_str());
                    return;
                }
                break;
            }

            // 5. Validate post_back_url matches Snowflake URL
            std::string post_back_url = extractPostBackUrlFromSamlResponse(m_samlResponse);
            if (post_back_url.empty())
            {
                CXX_LOG_ERROR("sf::IAuthenticatorOKTA::authenticate::Missing or malformed SAML post-back URL in IdP response.");
                m_errMsg = "SFSamlResponseVerificationFailed.";
                return;
            }
            std::string server_url = m_idp->getServerURLSync().toString();

            if ((!m_disableSamlUrlCheck) &&
                (!urlHasSamePrefix(post_back_url, server_url)))
            {
                CXX_LOG_ERROR("sf::IAuthenticatorOKTA::authenticate::The specified authenticator and destination URL in Saml Assertion did not match, expected=%s, post back=%s.",
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
            const std::size_t form_start = html.find("<form");
            if (form_start == std::string::npos)
            {
                CXX_LOG_DEBUG("sf::IAuthenticatorOKTA::extractPostBackUrlFromSamlResponse::No form element in IdP response.");
                return {};
            }

            const std::size_t action_pos = html.find("action=\"", form_start);
            if (action_pos == std::string::npos)
            {
                CXX_LOG_DEBUG("sf::IAuthenticatorOKTA::extractPostBackUrlFromSamlResponse::No action attribute in IdP response.");
                return {};
            }

            const std::size_t post_back_start = action_pos + 8;
            const std::size_t post_back_end = html.find("\"", post_back_start);
            if (post_back_end == std::string::npos || post_back_end <= post_back_start)
            {
                CXX_LOG_DEBUG("sf::IAuthenticatorOKTA::extractPostBackUrlFromSamlResponse::Malformed action attribute in IdP response.");
                return {};
            }

            std::string post_back_url = html.substr(post_back_start,
                post_back_end - post_back_start);
            CXX_LOG_DEBUG("sf::IAuthenticatorOKTA::extractPostBackUrlFromSamlResponse::Post back url before unescape: %s.", post_back_url.c_str());
            std::vector<char> unescaped_buf(post_back_url.size() + 1);
            decode_html_entities_utf8(unescaped_buf.data(), unescaped_buf.size(),
                post_back_url.c_str());
            CXX_LOG_DEBUG("sf::IAuthenticatorOKTA::extractPostBackUrlFromSamlResponse::Post back url after unescape: %s.", unescaped_buf.data());
            return std::string(unescaped_buf.data());
        }

        /**
         * Start http listener
         */
        void IAuthWebServer::start()
        {
            m_socket_desc_web_client = 0;
            m_socket_descriptor = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            /*TODO: On Windows, socket functions don't set errno and the error code
                    should be retrieved by WSAGetLastError(). Therefore for now the error
                    message won't be logged correctly on Windows.
                    Leave it for now since it won't affect the functionality.*/
            if ((int)m_socket_descriptor < 0)
            {
                CXX_LOG_ERROR("sf::%s::WebServer::start::Failed to start web server. Could not create a socket.  err: %s", m_className, strerror(errno));
                throw AuthException("sf::" + std::string(m_className) + "::WebServer::" + std::string(strerror(errno)));
            }

            struct sockaddr_in recv_server;
            memset((char*)&recv_server, 0, sizeof(struct sockaddr_in));
            recv_server.sin_family = AF_INET;
            recv_server.sin_port = htons(m_port); // ephemeral port
            CXX_LOG_INFO("HOST is %s", m_host.c_str());
            if (inet_pton(AF_INET, m_host.c_str(), &recv_server.sin_addr.s_addr) != 1)
            {
                CXX_LOG_ERROR(
                    "sf::%s::WebServer::start::Failed to start web server. Could not convert buffer to a network address. err: %s",
                    m_className, strerror(errno));
                throw AuthException("sf::" + std::string(m_className) + "::WebServer::" + std::string(strerror(errno)));
            }
            if (bind(m_socket_descriptor, (struct sockaddr*)&recv_server,
                sizeof(struct sockaddr_in)) < 0)
            {
                CXX_LOG_ERROR(
                    "sf::%s::WebServer::start::Failed to start web server. Could not bind a port. err: %s",
                    m_className, strerror(errno));
                throw AuthException("sf::" + std::string(m_className) + "::WebServer::" + std::string(strerror(errno)));
            }
            socklen_t length = sizeof(struct sockaddr_in);
            if (getsockname(m_socket_descriptor, (struct sockaddr*)&recv_server, &length) < 0) {
                CXX_LOG_ERROR(
                    "sf::%s::WebServer::start::Failed to get socket name. Could not get a port. err: %s",
                    m_className, strerror(errno));
                throw AuthException("sf::" + std::string(m_className) + "::WebServer::" + std::string(strerror(errno)));
            }
            m_real_port = ntohs(recv_server.sin_port);
            if (m_real_port != m_port) {
                CXX_LOG_TRACE("sf::%s::WebServer::start::Started on port: %d for %s:%d%s", m_className, m_real_port, m_host.c_str(), m_real_port, m_path.c_str());
            }
            if (listen(m_socket_descriptor, 5) < 0)
            {
                CXX_LOG_ERROR(
                    "sf::%s::WebServer::start::Failed to start web server. Could not listen a port. err: %s",
                    m_className, strerror(errno));
                throw AuthException("sf::" + std::string(m_className) + "::WebServer::" + std::string(strerror(errno)));
            }
            CXX_LOG_TRACE("sf::%s::WebServer::start::Web Server successfully started on %s:%d and path %s", m_className, m_host.c_str(), m_real_port, m_path.c_str());
        }

        void IAuthWebServer::stop()
        {
            CXX_LOG_TRACE("sf::%s::WebServer::stop::Stopping HTTP listener: %s:%d%s", m_className, m_host.c_str(), m_real_port, m_path.c_str());
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
                        "sf::%s::WebServer::stop::Failed close HTTP port err: %s",
                        m_className, strerror(errno));
                    throw AuthException("sf::" + std::string(m_className) + "::WebServer::" + std::string(strerror(errno)));
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
                        "sf::%s::WebServer::stop::Failed to stop web server. err: %s",
                        m_className, strerror(errno));
                    m_socket_descriptor = 0;
                    throw AuthException("sf::" + std::string(m_className) + "::WebServer::" + std::string(strerror(errno)));
                }
            }
            m_socket_descriptor = 0;
        }

        void IAuthWebServer::startAccept()
        {
            if ((int)m_socket_desc_web_client > 0)
            {
#ifndef _WIN32
                shutdown(m_socket_desc_web_client, SHUT_RDWR);
                close(m_socket_desc_web_client);
#else
                shutdown(m_socket_desc_web_client, SD_BOTH);
                closesocket(m_socket_desc_web_client);
#endif
                m_socket_desc_web_client = 0;
            }

            struct sockaddr_in client = { 0, 0, 0, 0 };
            socklen_t len = sizeof(client);

            fd_set fd;
            timeval timeout;
            FD_ZERO(&fd);
            FD_SET(m_socket_descriptor, &fd);
            if (m_timeout_deadline_set)
            {
                std::chrono::microseconds remaining =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        m_timeout_deadline - std::chrono::steady_clock::now());
                if (remaining.count() <= 0)
                {
                    throw AuthException(
                        "sf::" + std::string(m_className) +
                        "::WebServer::Auth browser timed out.");
                }
                timeout.tv_sec = (long)(remaining.count() / 1000000);
                timeout.tv_usec = (long)(remaining.count() % 1000000);
            }
            else
            {
                timeout.tv_sec = m_timeout;
                timeout.tv_usec = 0;
            }
            CXX_LOG_TRACE("sf::%s::WebServer::startAccept::select(m_socket_descriptor,...", m_className);
            int retVal = select(m_socket_descriptor + 1, &fd, NULL, NULL, &timeout);
            if (retVal > 0)
            {
                m_socket_desc_web_client = accept(
                    m_socket_descriptor, (struct sockaddr*)&client, &len);
                if ((int)m_socket_desc_web_client < 0)
                {
                    CXX_LOG_ERROR(
                        "sf::%s::WebServer::startAccept::Failed to receive token. Could not accept a request. error: %s",
                        m_className, strerror(errno));
                    throw AuthException("sf::" + std::string(m_className) + "::WebServer::" + std::string(strerror(errno)));
                }
            }
            else if (retVal == 0)
            {
                CXX_LOG_ERROR("sf::%s::WebServer::startAccept::Auth browser timed out. ", m_className);
                throw AuthException("sf::" + std::string(m_className) + "::WebServer::Auth browser timed out.");
            }
            else
            {
                CXX_LOG_ERROR(
                    "sf::%s::WebServer::startAccept::Failed to determine status of auth web server. err: %s",
                    m_className,strerror(errno));
                throw AuthException("sf::" + std::string(m_className) + "::WebServer::" + std::string(strerror(errno)));

            }
        }

        bool IAuthWebServer::receive()
        {
            bool is_options = false;
            std::unique_ptr<char[]> mesg(new char[SOCKET_BUFFER_SIZE]());
            char* reqline;
            char* rest_mesg;
            int recvlen;

            if ((recvlen = (int)recv(m_socket_desc_web_client, mesg.get(), SOCKET_BUFFER_SIZE, 0)) < 0)
            {
                CXX_LOG_ERROR("sf::%s::WebServer::receive::Failed to receive SAML token. Could not receive a request.", m_className);
                throw AuthException("sf::" + std::string(m_className) + "::WebServer::Failed to receive SAML token. Could not receive a request.");
            }
            reqline = sf_strtok(mesg.get(), " \t\n", &rest_mesg);
            if (strncmp(reqline, "GET\0", 4) == 0)
            {
                parseAndRespondGetRequest(&rest_mesg);
            }
            else if (strncmp(reqline, "POST\0", 5) == 0)
            {
                parseAndRespondPostRequest(std::string(rest_mesg, (unsigned long)recvlen));
            }
            else if (strncmp(reqline, "OPTIONS\0", 8) == 0)
            {
                is_options = parseAndRespondOptionsRequest(std::string(rest_mesg, (unsigned long)recvlen));
            }
            else
            {
                CXX_LOG_ERROR("sf::%s::WebServer::receive::Failed to receive SAML token. Could not get HTTP request. err: %s.", m_className, reqline);
                throw AuthException("sf::" + std::string(m_className) + "::WebServer::Not HTTP request");
            }
            return is_options;
        }

        void IAuthWebServer::respond(std::string errorCode, std::string message)
        {
            std::stringstream buf;
            buf << "HTTP/1.0 " << errorCode << "\r\n"
                << "Content-Type: text/html" << "\r\n"
                << "Content-Length: " << message.length() << "\r\n\r\n"
                << message;
            send(m_socket_desc_web_client, buf.str().c_str(), (int)buf.str().length(), 0);
            buf.clear();
        }

        void IAuthWebServer::fail(std::string httpError, std::string errMessage, std::string failureResponse)
        {
            CXX_LOG_ERROR("sf::%s::WebServer::fail %s", m_className, errMessage.c_str());
            respond(httpError, failureResponse.empty() ? errMessage : failureResponse); // unless error message is html-escaped it shouldn't be part of response visible in the browser
            throw AuthException(errMessage);
        }

        /**
         * Get port number listening
         * @return port number
         */
        int IAuthWebServer::getPort()
        {
            return m_real_port;
        }

        std::string IAuthWebServer::getToken()
        {
            return m_token;
        }

        /**
         * Set the timeout for the web server.
         */
        void IAuthWebServer::setTimeout(int timeout)
        {
            m_timeout = timeout;
            m_timeout_deadline =
                std::chrono::steady_clock::now() + std::chrono::seconds(timeout);
            m_timeout_deadline_set = true;
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

        /**
* Constructor for AuthWebServer
*/
        AuthWebServer::AuthWebServer() :
            m_consent_cache_id_token(true)
        {
            m_className = "AuthenticatorExternalBrowser";
        }

        /**
         * Destructor for AuthWebServer
         */
        AuthWebServer::~AuthWebServer()
        {
            // nop
        }

        void AuthWebServer::setExpectedOrigin(const SFURL& expectedOrigin)
        {
            m_expected_origin = expectedOrigin;
        }

        bool AuthWebServer::receive()
        {
            std::unique_ptr<char[]> message(new char[SOCKET_BUFFER_SIZE]());
            int received = (int)recv(
                m_socket_desc_web_client,
                message.get(),
                SOCKET_BUFFER_SIZE,
                0);
            if (received < 0)
            {
                throw AuthException(
                    "sf::" + std::string(m_className) +
                    "::WebServer::Failed to receive SAML token. Could not receive a request.");
            }
            if (received == 0)
            {
                return true;
            }

            std::string request(message.get(), (unsigned long)received);
            size_t methodEnd = request.find_first_of(" \t\r\n");
            std::string method = request.substr(0, methodEnd);
            if (!requestOriginAllowed(method, request))
            {
                return true;
            }

            char* restMessage;
            char* requestLine = sf_strtok(message.get(), " \t\n", &restMessage);
            bool continueListening = false;
            if (strncmp(requestLine, "GET\0", 4) == 0)
            {
                parseAndRespondGetRequest(&restMessage);
                continueListening = m_token.empty();
            }
            else if (strncmp(requestLine, "POST\0", 5) == 0)
            {
                parseAndRespondPostRequest(request);
                continueListening = m_token.empty();
            }
            else if (strncmp(requestLine, "OPTIONS\0", 8) == 0)
            {
                continueListening = parseAndRespondOptionsRequest(request);
            }
            else
            {
                return true;
            }
            return continueListening;
        }

        int AuthWebServer::start(std::string host, int port, std::string path)
        {
            SF_UNUSED(host);
            SF_UNUSED(port);
            SF_UNUSED(path);
            return 0;
        };

        bool AuthWebServer::extractHeader(
            const std::string& request,
            const std::string& name,
            std::string& value)
        {
            size_t headerEnd = request.find("\r\n\r\n");
            size_t lfHeaderEnd = request.find("\n\n");
            if (headerEnd == std::string::npos ||
                (lfHeaderEnd != std::string::npos && lfHeaderEnd < headerEnd))
            {
                headerEnd = lfHeaderEnd;
            }
            const std::string headers = request.substr(0, headerEnd);
            std::istringstream lines(headers);
            std::string line;
            while (std::getline(lines, line))
            {
                size_t separator = line.find(':');
                if (separator == std::string::npos)
                {
                    continue;
                }

                std::string key = line.substr(0, separator);
                std::string candidate = line.substr(separator + 1);
                auto trimWhitespace = [](std::string& input)
                {
                    input.erase(
                        input.begin(),
                        std::find_if(
                            input.begin(),
                            input.end(),
                            [](char c) { return !std::isspace(static_cast<unsigned char>(c)); }));
                    input.erase(
                        std::find_if(
                            input.rbegin(),
                            input.rend(),
                            [](char c) { return !std::isspace(static_cast<unsigned char>(c)); }).base(),
                        input.end());
                };
                trimWhitespace(key);
                trimWhitespace(candidate);
                if (equalsIgnoreCase(key, name))
                {
                    value = candidate;
                    return true;
                }
            }
            return false;
        }

        std::string AuthWebServer::extractBody(const std::string& request)
        {
            size_t separator = request.find("\r\n\r\n");
            size_t separatorLength = 4;
            size_t lfSeparator = request.find("\n\n");
            if (separator == std::string::npos ||
                (lfSeparator != std::string::npos && lfSeparator < separator))
            {
                separator = lfSeparator;
                separatorLength = 2;
            }
            return separator == std::string::npos
                ? std::string()
                : request.substr(separator + separatorLength);
        }

        /**
         * Split a serialized origin into scheme, host and port.
         * A serialized origin carries no userinfo, path, query or fragment, so a
         * value that holds any of them is not an origin and is rejected.
         */
        static bool parseSerializedOrigin(
            const std::string& origin,
            std::string& scheme,
            std::string& host,
            std::string& port)
        {
            const std::string schemeSeparator = "://";
            size_t schemeEnd = origin.find(schemeSeparator);
            if (schemeEnd == std::string::npos)
            {
                return false;
            }

            scheme = origin.substr(0, schemeEnd);
            if (!equalsIgnoreCase(scheme, "https") && !equalsIgnoreCase(scheme, "http"))
            {
                return false;
            }

            std::string authority = origin.substr(schemeEnd + schemeSeparator.length());
            if (!authority.empty() && authority[authority.length() - 1] == '/')
            {
                authority.erase(authority.length() - 1);
            }
            if (authority.empty() || authority.find_first_of("@/?#") != std::string::npos)
            {
                return false;
            }

            size_t portSeparator = authority.find(':');
            host = authority.substr(0, portSeparator);
            port = portSeparator == std::string::npos
                ? std::string()
                : authority.substr(portSeparator + 1);
            if (host.empty())
            {
                return false;
            }
            if (portSeparator != std::string::npos &&
                (port.empty() ||
                    port.find_first_not_of("0123456789") != std::string::npos))
            {
                return false;
            }
            return true;
        }

        static std::string effectiveOriginPort(const std::string& scheme, const std::string& port)
        {
            if (!port.empty())
            {
                return port;
            }
            if (equalsIgnoreCase(scheme, "https"))
            {
                return std::string("443");
            }
            if (equalsIgnoreCase(scheme, "http"))
            {
                return std::string("80");
            }
            return std::string();
        }

        bool AuthWebServer::originMatchesExpected(const std::string& origin) const
        {
            std::string scheme, host, port;
            if (!parseSerializedOrigin(origin, scheme, host, port))
            {
                return false;
            }

            return equalsIgnoreCase(scheme, m_expected_origin.scheme()) &&
                equalsIgnoreCase(host, m_expected_origin.host()) &&
                effectiveOriginPort(scheme, port) ==
                    effectiveOriginPort(m_expected_origin.scheme(), m_expected_origin.port());
        }

        bool AuthWebServer::requestOriginAllowed(
            const std::string& method,
            const std::string& request)
        {
            m_origin.clear();
            bool isGet = equalsIgnoreCase(method, "GET");
            if (!isGet &&
                !equalsIgnoreCase(method, "POST") &&
                !equalsIgnoreCase(method, "OPTIONS"))
            {
                return false;
            }

            std::string origin;
            bool hasOrigin = extractHeader(request, "Origin", origin);
            if (isGet && (!hasOrigin || equalsIgnoreCase(origin, "null")))
            {
                return true;
            }
            if (!hasOrigin || !originMatchesExpected(origin))
            {
                return false;
            }
            m_origin = origin;
            return true;
        }

        bool AuthWebServer::isCorsPostPreflight(const std::string& request)
        {
            std::string method;
            if (!extractHeader(request, "Access-Control-Request-Method", method) ||
                !equalsIgnoreCase(method, "POST"))
            {
                return false;
            }

            std::string headers;
            if (!extractHeader(request, "Access-Control-Request-Headers", headers))
            {
                return true;
            }
            std::istringstream requestedHeaders(headers);
            std::string header;
            while (std::getline(requestedHeaders, header, ','))
            {
                trim(header, ' ');
                if (header.empty())
                {
                    continue;
                }
                if (!equalsIgnoreCase(header, "Content-Type"))
                {
                    return false;
                }
            }
            return true;
        }


        void AuthWebServer::parseAndRespondPostRequest(std::string response)
        {
            std::string payload = extractBody(response);
            if (payload.empty())
            {
                CXX_LOG_ERROR("sf::AuthWebServer::parseAndRespondPostRequest:No token parameter is found %s.", response.c_str());
                return;
            }

            std::string::const_iterator first = std::find_if(
                payload.begin(),
                payload.end(),
                [](char c) { return !std::isspace(static_cast<unsigned char>(c)); });
            if (first == payload.end() || *first != '{')
            {
                respond(payload);
            }
            else
            {
                jsonValue_t json;
                std::string err;
                picojson::parse(json, payload.begin(), payload.end(), &err);
                if (!err.empty())
                {
                    CXX_LOG_ERROR("sf::AuthWebServer::parseAndRespondPostRequest:Error in parsing JSON : % s, err : % s.", payload.c_str(), err.c_str());
                    return;
                }
                respondJson(json);
            }
        }

        bool AuthWebServer::parseAndRespondOptionsRequest(std::string response)
        {
            if (!isCorsPostPreflight(response))
            {
                return true;
            }
            std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            );
            char current_timestamp[50];
            std::time_t t = (time_t)ms.count() / 1000;
            std::tm tms;
            strftime(current_timestamp, sizeof(current_timestamp), "%a, %d %b %Y %H:%M:%S GMT", sf_gmtime(&t, &tms));

            std::stringstream buf;
            buf << "HTTP/1.0 " << HTTP_OK << "\r\n"
                << "Date: " << current_timestamp << "\r\n"
                << "Access-Control-Allow-Methods: POST" << "\r\n"
                << "Access-Control-Allow-Headers: Content-Type" << "\r\n"
                << "Access-Control-Max-Age: 86400" << "\r\n"
                << "Access-Control-Allow-Origin: " << m_origin << "\r\n"
                << "\r\n\r\n";
            send(m_socket_desc_web_client, buf.str().c_str(), (int)buf.str().length(), 0);
            buf.clear();
            return true;
        }

        void AuthWebServer::parseAndRespondGetRequest(char** rest_mesg)
        {
            char* path = sf_strtok(NULL, " \t", rest_mesg);
            char* protocol = sf_strtok(NULL, " \t\n", rest_mesg);
            if (path == nullptr || protocol == nullptr)
            {
                CXX_LOG_ERROR("sf::AuthWebServer::parseAndRespondGetRequest:No token parameter is found.");
                return;
            }
            if (strncmp(protocol, "HTTP/1.0", 8) != 0 &&
                strncmp(protocol, "HTTP/1.1", 8) != 0)
            {
                CXX_LOG_ERROR("sf::AuthWebServer::parseAndRespondGetRequest::Not HTTP request.");
                fail(HTTP_BAD_REQUEST, "sf::AuthWebServer::parseAndRespondGetRequest::Not HTTP request", failureMessage);
            }

            if (strncmp(path, "/?", 2) != 0)
            {
                CXX_LOG_ERROR("sf::AuthWebServer::parseAndRespondGetRequest:No token parameter is found.");
                return;
            }
            respond(std::string(&path[2]));
        }

        void AuthWebServer::respondJson(picojson::value& json)
        {
            if (!json.is<picojson::object>())
            {
                return;
            }
            jsonObject_t& obj = json.get<picojson::object>();
            if (!obj["token"].is<std::string>() || obj["token"].get<std::string>().empty())
            {
                return;
            }
            m_token = obj["token"].get<std::string>();
            if (obj["consent"].is<bool>())
            {
                m_consent_cache_id_token = obj["consent"].get<bool>();
            }

            jsonObject_t payloadBody;
            payloadBody["consent"] = picojson::value(m_consent_cache_id_token);
            auto payloadBodyString = picojson::value(payloadBody).serialize();

            respondSuccess(payloadBodyString);
        }

        void AuthWebServer::respond(std::string queryParameters)
        {
            auto params = splitQuery(queryParameters);
            for (auto& p : params)
            {
                if (p.first == "token" && !p.second.empty())
                {
                    m_token = p.second;
                    break;
                }
            }
            if (m_token.empty())
            {
                return;
            }

            respondSuccess(successMessage);
        }

        void AuthWebServer::respondSuccess(const std::string& body)
        {
            std::stringstream buf;
            buf << "HTTP/1.0 " << HTTP_OK << "\r\n"
                << "Content-Type: text/html" << "\r\n"
                << "Content-Length: " << body.length() << "\r\n";
            if (!m_origin.empty())
            {
                buf << "Access-Control-Allow-Origin: " << m_origin << "\r\n";
            }
            buf << "\r\n" << body;
            send(m_socket_desc_web_client, buf.str().c_str(), (int)buf.str().length(), 0);
        }

        std::string AuthWebServer::unquote(std::string src)
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

        std::vector<std::pair<std::string, std::string>> AuthWebServer::splitQuery(std::string query)
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

        bool AuthWebServer::isConsentCacheIdToken()
        {
            return m_consent_cache_id_token;
        }


    }// namespace IAuth
} // namespace Client
} // namespace Snowflake
