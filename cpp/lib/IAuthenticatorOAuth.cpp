#ifdef _WIN32
#include <WS2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cctype>
#include <future>
#include <regex>
#include <string>

#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <connection.h>

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#endif

#ifdef __linux__
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include "snowflake/IBase64.hpp"
#include "AuthenticationChallengeProvider.hpp"
#include "AuthenticationWebBrowserRunner.hpp"
#include "../include/snowflake/entities.hpp"
#include "../logger/SFLogger.hpp"
#include "snowflake/IAuth.hpp"

#include "curl_desc_pool.h"
#include "cJSON.h"
#include "memory.h"
#include "error.h"
#include <errno.h>

namespace Snowflake::Client
{
    using Snowflake::Client::SFURL;
    namespace IAuth {
        const std::string IAuthenticatorOAuth::S_LOCALHOST = "127.0.0.1";
        const std::string IAuthenticatorOAuth::S_LOCALHOST_URL = "http://" + IAuthenticatorOAuth::S_LOCALHOST;
        const std::string IAuthenticatorOAuth::S_OAUTH_DEFAULT_AUTHORIZATION_URL_POSTFIX = "/oauth/authorize";
        const std::string IAuthenticatorOAuth::S_OAUTH_DEFAULT_TOKEN_URL_POSTFIX = "/oauth/token-request";

        struct AuthorizationCodeRequest
        {
            SFURL authorizationEndpoint;
            std::string authorizationScope;
            std::string clientId;
            std::string clientSecret;
            SFURL redirectCallbackUrl;
            std::string codeChallenge;
            std::string state;
            std::string codeChallengeMethod;

            std::string authorizationCodeUrl()
            {
                std::string url = authorizationEndpoint.toString();
                url.append("?client_id=").append(UrlEncode(clientId))
                    .append("&response_type=").append("code")
                    .append("&redirect_uri=").append(UrlEncode(redirectCallbackUrl.toString()))
                    .append("&scope=").append(UrlEncode(authorizationScope))
                    .append("&code_challenge=").append(UrlEncode(codeChallenge))
                    .append("&code_challenge_method=").append(codeChallengeMethod)
                    .append("&state=").append(UrlEncode(state));
                return url;
            }
        };

        struct AuthorizationCodeResponse
        {
            std::string authorizationCode;
            bool success;
            std::string errorMessage;

            static AuthorizationCodeResponse succeeded(const std::string& code)
            {
                return { code, true, "" };
            }

            static AuthorizationCodeResponse failed(const std::string& error)
            {
                return { "", false, error };
            }
        };

        struct AccessTokenRequest
        {
            SFURL tokenRequestUri;
            std::string authorizationCode;
            std::string authorizationScope;
            std::string clientId;
            std::string clientSecret;
            std::string codeVerifier;
            SFURL redirectUri;
            std::string state;
            bool singleUseRefreshTokens;
        };

        struct AccessTokenResponse
        {
            std::string accessToken;
            std::string refreshToken;
            bool success;
            std::string errorMessage;

            static AccessTokenResponse succeeded(const std::string& token, const std::string& refreshToken)
            {
                return { token, refreshToken, true, "" };
            }

            static AccessTokenResponse failed(const std::string& error)
            {
                return { ":", ":", false, error };
            }
        };

        struct RefreshAccessTokenRequest
        {
            std::string clientId;
            std::string clientSecret;
            SFURL tokenEndpoint;
            std::string refreshToken;
            std::string scope;
        };

        IAuthenticatorOAuth::IAuthenticatorOAuth(
            IAuthWebServer* authWebServer,
            IAuthenticationWebBrowserRunner* webBrowserRunner)
            : m_authWebServer(authWebServer != nullptr ? authWebServer : new OAuthTokenListenerWebServer()),
            m_webBrowserRunner(webBrowserRunner == nullptr ? IAuthenticationWebBrowserRunner::getInstance() : webBrowserRunner) {
        }

        void IAuthenticatorOAuth::authenticate()
        {
            // already has an access token
            if (!m_token.empty()) {
                return;
            }

#ifdef _WIN32
            AuthWinSock authWinSock;
#endif
            try {
                // try to get a new access token using the existing refresh token
                if (refreshAccessTokenFlow())
                    return;

                // do full authorization pipeline
                switch (m_oauthFlow)
                {
                case AUTH_OAUTH_AUTHORIZATION_CODE:
                    authorizationCodeFlow();
                    break;
                case AUTH_OAUTH_CLIENT_CREDENTIALS:
                    clientCredentialsFlow();
                    break;
                default:
                    m_errMsg = "Unsupported OAuth flow type";
                }
            }
            catch (std::exception& e) {
                m_errMsg = e.what();
            }
        }

        void IAuthenticatorOAuth::authorizationCodeFlow()
        {
            CXX_LOG_TRACE("sf::AuthenticatorOAuth::authorizationCodeFlow::OAuth authorization code flow started");

            std::string codeVerifier = m_challengeProvider->generateCodeVerifier();
            std::string codeChallenge = m_challengeProvider->generateCodeChallenge(codeVerifier);
            std::string state = m_challengeProvider->generateState();

            CXX_LOG_TRACE("sf::AuthenticatorOAuth::authorizationCodeFlow::createAuthorizationCodeRequest %s", maskOAuthSecret(m_authEndpoint).c_str())
                AuthorizationCodeRequest authCodeRequest = { m_authEndpoint,
                                                            m_authScope,
                                                            m_clientId,
                                                            m_clientSecret,
                                                            m_redirectUri,
                                                            codeChallenge,
                                                            state,
                                                            m_challengeProvider->codeChallengeMethod() };
            CXX_LOG_DEBUG("sf::AuthenticatorOAuth::authorizationCodeFlow::executeAuthorizationCodeRequest %s",
                maskOAuthSecret(authCodeRequest.authorizationCodeUrl()).c_str());
            AuthorizationCodeResponse authCodeResponse = executeAuthorizationCodeRequest(authCodeRequest);
            handleInvalidResponse(authCodeResponse.success, std::string("oauth authorization code request failure ") + authCodeResponse.errorMessage);

            CXX_LOG_TRACE("sf::AuthenticatorOAuth::authorizationCodeFlow::createAccessTokenRequest %s", maskOAuthSecret(m_tokenEndpoint).c_str())
                AccessTokenRequest accessRequest = { m_tokenEndpoint,
                                                    authCodeResponse.authorizationCode,
                                                    m_authScope,
                                                    m_clientId,
                                                    m_clientSecret,
                                                    codeVerifier,
                                                    m_redirectUri,
                                                    state,
                                                    m_singleUseRefreshTokens };
            CXX_LOG_DEBUG("sf::AuthenticatorOAuth::authorizationCodeFlow::executeAccessTokenRequest %s", maskOAuthSecret(m_tokenEndpoint).c_str());
            AccessTokenResponse accessResponse = executeAccessTokenRequest(accessRequest);
            handleInvalidResponse(accessResponse.success, std::string("oauth access token request failure ") + accessResponse.errorMessage);

            CXX_LOG_DEBUG("sf::AuthenticatorOAuth::authorizationCodeFlow::Successfully acquired access token: %s", maskOAuthSecret(accessResponse.accessToken).c_str());
            resetTokens(accessResponse.accessToken, accessResponse.refreshToken);
        }

        void IAuthenticatorOAuth::clientCredentialsFlow()
        {
            CXX_LOG_TRACE("sf::AuthenticatorOAuth::clientCredentialsFlow::OAuth client credentials flow started")
                CXX_LOG_TRACE("sf::AuthenticatorOAuth::clientCredentialsFlow::createAccessTokenRequest %s", maskOAuthSecret(m_tokenEndpoint).c_str());
            AccessTokenRequest accessRequest = { m_tokenEndpoint,
                                                "",
                                                m_authScope,
                                                m_clientId,
                                                m_clientSecret,
                                                "",
                                               SFURL::parse(""),
                                                "",
                                                false };
            CXX_LOG_DEBUG("sf::AuthenticatorOAuth::clientCredentialsFlow::executeAccessTokenRequest %s", maskOAuthSecret(m_tokenEndpoint).c_str());
            AccessTokenResponse accessResponse = executeAccessTokenRequest(accessRequest);
            handleInvalidResponse(accessResponse.success, std::string("oauth access token request failure"));

            CXX_LOG_DEBUG("sf::AuthenticatorOAuth::clientCredentialsFlow::Successfully acquired access token: %s", maskOAuthSecret(accessResponse.accessToken).c_str());
            resetTokens(accessResponse.accessToken, accessResponse.refreshToken);
        }

        std::string IAuthenticatorOAuth::oauthWebServerTask(IAuthWebServer* authWebServer, const SFURL& redirectUrl, const std::string& state, const int browserResponseTimeout) {
            SF_UNUSED(redirectUrl);

            authWebServer->setTimeout(browserResponseTimeout);
            try {
                authWebServer->startAccept(state);
                authWebServer->receive();
                authWebServer->stop();
            }
            catch (...)
            {
                CXX_LOG_WARN("sf::AuthenticatorOAuth::oauthWebServerTask::Problem during HTTP listen.");
                authWebServer->stop();
                throw;
            }
            CXX_LOG_TRACE("sf::AuthenticatorOAuth::oauthWebServerTask::Token: %s", maskOAuthSecret(authWebServer->getToken()).c_str());
            return authWebServer->getToken();
        }

        std::future<std::string> IAuthenticatorOAuth::asyncStartOAuthWebserver(const AuthorizationCodeRequest& authorizationCodeRequest) const {
            SFURL redirectUrl = authorizationCodeRequest.redirectCallbackUrl;

            return std::async(std::launch::async, oauthWebServerTask,
                m_authWebServer.get(),
                redirectUrl,
                authorizationCodeRequest.state,
                m_browserResponseTimeout);
        }

        bool IAuthenticatorOAuth::refreshAccessTokenFlow() {
            if (m_oauth_refresh_token.empty()) {
                CXX_LOG_DEBUG("sf::AuthenticatorOAuth::refreshAccessTokenFlow::Refresh token is empty, a complete flow is required");
                return false;
            }
            CXX_LOG_TRACE("sf::AuthenticatorOAuth::refreshAccessTokenFlow::OAuth refresh access token flow started");

            RefreshAccessTokenRequest request{
              m_clientId,
              m_clientSecret,
              m_tokenEndpoint,
              m_oauth_refresh_token,
              m_authScope
            };

            CXX_LOG_DEBUG("sf::AuthenticatorOAuth::clientCredentialsFlow::executeRefreshAccessTokenRequest")
                AccessTokenResponse response = executeRefreshAccessTokenRequest(request);
            if (!response.success) {
                CXX_LOG_ERROR("sf::AuthenticatorOAuth::refreshAccessTokenFlow::OAuth refresh access token failed: %s",
                    response.errorMessage.c_str());
            }
            else {
                CXX_LOG_DEBUG("sf::AuthenticatorOAuth::clientCredentialsFlow::token refresh completed");
                resetTokens(std::move(response.accessToken), std::move(response.refreshToken));
            }
            return response.success;
        }

        AuthorizationCodeResponse IAuthenticatorOAuth::executeAuthorizationCodeRequest(AuthorizationCodeRequest& authorizationCodeRequest)
        {
            SFURL redirectUrl = authorizationCodeRequest.redirectCallbackUrl;
            m_authWebServer->setTimeout(m_browserResponseTimeout);
            int portUsed = m_authWebServer->start(redirectUrl.host(), std::atoi(redirectUrl.port().c_str()), redirectUrl.path());
            refreshDynamicRedirectUri(portUsed, authorizationCodeRequest);
            std::string token;
            std::future<std::string> tokenFuture = asyncStartOAuthWebserver(authorizationCodeRequest);

            CXX_LOG_TRACE("sf::AuthenticatorOAuth::executeAuthorizationCodeRequest::------------------------");
            CXX_LOG_TRACE("sf::AuthenticatorOAuth::executeAuthorizationCodeRequest::GET %s", maskOAuthSecret(
                authorizationCodeRequest.authorizationCodeUrl()).c_str());
            CXX_LOG_TRACE("sf::AuthenticatorOAuth::executeAuthorizationCodeRequest::------------------------");

            startWebBrowser(authorizationCodeRequest.authorizationCodeUrl());
            try
            {
                token = tokenFuture.get();
            }
            catch (const Snowflake::Client::IAuth::AuthException& e)
            {
                m_authWebServer->stop();
                CXX_LOG_ERROR("sf::AuthenticatorOAuth::executeAuthorizationCodeRequest::Error while trying to get authorization code: %s", e.what());
                return AuthorizationCodeResponse::failed(e.cause());
            }

            CXX_LOG_TRACE("sf::AuthenticatorOAuth::executeAuthorizationCodeRequest::Received token: %s", maskOAuthSecret(token).c_str());
            return AuthorizationCodeResponse::succeeded(token);
        }

        AccessTokenResponse IAuthenticatorOAuth::executeAccessTokenRequest(AccessTokenRequest& request)
        {
            SFURL tokenURL = request.tokenRequestUri;
            CXX_LOG_TRACE("sf::AuthenticatorOAuth::executeAccessTokenRequest::token request: %s",
                maskOAuthSecret(tokenURL).c_str());

            // body
            std::string body = createAccessTokenRequestBody(request);
            jsonObject_t resp;

            CXX_LOG_TRACE("sf::AuthenticatorOAuth::executeAccessTokenRequest::------------------------");
            CXX_LOG_TRACE("sf::AuthenticatorOAuth::executeAccessTokenRequest::POST %s",
                tokenURL.toString().c_str());

            CXX_LOG_TRACE("sf::AuthenticatorOAuth::executeAccessTokenRequest");
            CXX_LOG_TRACE("sf::AuthenticatorOAuth::executeAccessTokenRequest::%s", maskOAuthSecret(body).c_str());
            CXX_LOG_TRACE("sf::AuthenticatorOAuth::executeAccessTokenRequest::------------------------");

            if (!executeRestRequest(tokenURL, body, resp)) {
                CXX_LOG_ERROR("sf::AuthenticatorOAuth::executeAccessTokenRequest::OAuth error: %s", m_errMsg);
                return AccessTokenResponse::failed(std::string("Invalid Identity Provider response: ") + m_errMsg);
            }

            CXX_LOG_TRACE("sf::AuthenticatorOAuth::executeAccessTokenRequest::------------------------");
            CXX_LOG_TRACE("sf::AuthenticatorOAuth::executeAccessTokenRequest::Body: %s", maskOAuthSecret(picojson::value(resp).serialize()).c_str());
            CXX_LOG_TRACE("sf::AuthenticatorOAuth::executeAccessTokenRequest::------------------------")

                std::string accessToken = resp.find("access_token") != resp.end() ? resp["access_token"].get<std::string>() : "";
            std::string refreshToken = resp.find("refresh_token") != resp.end() ? resp["refresh_token"].get<std::string>() : "";


            if (accessToken.empty()) {
                return AccessTokenResponse::failed("Invalid Identity Provider response: access token not provided");
            }

            return AccessTokenResponse::succeeded(accessToken, refreshToken);
        }

        AccessTokenResponse IAuthenticatorOAuth::executeRefreshAccessTokenRequest(
            RefreshAccessTokenRequest& request)
        {
            CXX_LOG_TRACE("sf::AuthenticatorOAuth::executeRefreshAccessTokenRequest::token request: %s",
                maskOAuthSecret(request.tokenEndpoint).c_str());

            // body
            std::string body = createRefreshAccessTokenRequestBody(request);
            jsonObject_t resp;

            CXX_LOG_TRACE("sf::AuthenticatorOAuth::executeRefreshAccessTokenRequest::----------------------");
            CXX_LOG_TRACE("sf::AuthenticatorOAuth::executeRefreshAccessTokenRequest::POST %s",
                request.tokenEndpoint.toString().c_str());
            CXX_LOG_TRACE("sf::AuthenticatorOAuth::executeRefreshAccessTokenRequest::%s",
                maskOAuthSecret(body).c_str());
            CXX_LOG_TRACE("sf::AuthenticatorOAuth::executeRefreshAccessTokenRequest::----------------------");

            if (!executeRestRequest(request.tokenEndpoint, body, resp)) {
                CXX_LOG_ERROR("sf::AuthenticatorOAuth::executeRefreshAccessTokenRequest::OAuth error: %s", m_errMsg.c_str());
                return AccessTokenResponse::failed(std::string("Invalid Identity Provider response: ") + m_errMsg);
            }

            std::string accessToken = resp.find("access_token") != resp.end() ? resp["access_token"].get<std::string>() : "";
            std::string refreshToken = resp.find("refresh_token") != resp.end() ? resp["refresh_token"].get<std::string>() : "";


            if (accessToken.empty()) {
                return AccessTokenResponse::failed("Invalid Identity Provider response: access token not provided");
            }

            return AccessTokenResponse::succeeded(accessToken, refreshToken);
        }

        std::string IAuthenticatorOAuth::createRefreshAccessTokenRequestBody(const RefreshAccessTokenRequest& request) {
            std::string requestBody = "grant_type=refresh_token&refresh_token=" + request.refreshToken;
            if (!request.scope.empty()) {
                requestBody.append("&scope=");
                requestBody.append(UrlEncode(request.scope));
            }
            return requestBody;
        }

        std::string IAuthenticatorOAuth::createAccessTokenRequestBody(AccessTokenRequest& request)
        {
            std::string requestBody = "";
            std::string grantType; // authorization_code, client_credentials, later: refresh_token

            if (m_oauthFlow == AUTH_OAUTH_AUTHORIZATION_CODE) {
                grantType = "authorization_code";
            }
            else {
                grantType = "client_credentials";
            }

            requestBody.append("grant_type=");
            requestBody.append(UrlEncode(grantType));

            if (!request.authorizationCode.empty())
            {
                requestBody.append("&code=");
                requestBody.append(UrlEncode(request.authorizationCode));
            }

            if (m_oauthFlow == AUTH_OAUTH_AUTHORIZATION_CODE)
            {
                requestBody.append("&redirect_uri=");
                requestBody.append(UrlEncode(request.redirectUri.toString()));
            }

            if (!request.codeVerifier.empty())
            {
                requestBody.append("&code_verifier=");
                requestBody.append(UrlEncode(request.codeVerifier));
            }

            requestBody.append("&scope=");
            requestBody.append(UrlEncode(request.authorizationScope));

            if (request.singleUseRefreshTokens)
            {
                requestBody.append("&enable_single_use_refresh_tokens=true");
            }

            return requestBody;
        }

        void IAuthenticatorOAuth::startWebBrowser(const std::string& ssoUrl)
        {
            std::cout << "Initiating login request with your identity provider. A "
                "browser window should have opened for you to complete the "
                "login. If you can't see it, check existing browser windows, "
                "or your OS settings. Press CTRL+C to abort and try again..." << "\n";
            // need double quotes to reserve ampasand characters
            std::stringstream urlBuf;

            if (m_webBrowserRunner == nullptr)
            {
                CXX_LOG_ERROR("sf::AuthenticatorOAuth::startWebBrowser::Failed to start web browser. Unable to open SSO URL.");
                throw AuthException("SFOAuthError. Unable to open SSO URL.");
            }

            m_webBrowserRunner->startWebBrowser(ssoUrl);
        }


        void IAuthenticatorOAuth::refreshDynamicRedirectUri(int portUsed, AuthorizationCodeRequest& authorizationCodeRequest) {
            if (m_redirectUriDynamicDefault) {
                m_redirectUri = SFURL::parse(S_LOCALHOST_URL + ":" + std::to_string(portUsed));
                authorizationCodeRequest.redirectCallbackUrl = m_redirectUri;
            }
        }

        void IAuthenticatorOAuth::updateDataMap(jsonObject_t& dataMap)
        {
            dataMap["AUTHENTICATOR"] = picojson::value(SF_AUTHENTICATOR_OAUTH);
            jsonObject_t& clientEnvironment = dataMap["CLIENT_ENVIRONMENT"].get<jsonObject_t>();
            clientEnvironment["OAUTH_TYPE"] = picojson::value(oauthAuthorizationFlow());

            if (!m_token.empty())
            {
                CXX_LOG_TRACE("sf::AuthenticatorOAuth::updateDataMap::Passing token to Snowflake authentication: %s",
                    maskOAuthSecret(m_token).c_str());
                dataMap["TOKEN"] = picojson::value(m_token);
            }
            else
                CXX_LOG_ERROR("sf::AuthenticatorOAuth::updateDataMap::Token not provided!");
        }

        std::string IAuthenticatorOAuth::oauthAuthorizationFlow()
        {
            if (m_oauthFlow == AUTH_OAUTH_AUTHORIZATION_CODE) {
                return SF_AUTHENTICATOR_OAUTH_AUTHORIZATION_CODE;
            }
            else {
                return SF_AUTHENTICATOR_OAUTH_CLIENT_CREDENTIALS;
            }
        }

        void IAuthenticatorOAuth::handleInvalidResponse(bool success, const std::string& errorMessage)
        {
            if (!success)
            {
                CXX_LOG_ERROR(
                    "sf::AuthenticatorOAuth::handleInvalidResponse::Failure during authentication: %s",
                    errorMessage.c_str());
                throw AuthException("OAuth authentication failure: " + errorMessage);
            }
        }

        //    ////////////////////////////////////////////////////////
        //     WEB SRV
        //    ////////////////////////////////////////////////////////
        //
        //    TODO: SNOW-2452931: This Websever have redundant code with AuthWebServer for the external browser. 
        //     This code needs to be refactored.
        //
        OAuthTokenListenerWebServer::OAuthTokenListenerWebServer() : m_socket_descriptor(0),
            m_socket_desc_web_client(0),
            m_port(0),
            m_real_port(0),
            m_token(""),
            m_origin(""),
            m_timeout(SF_BROWSER_RESPONSE_TIMEOUT) {
        }

        /**
         * Start http listener on given port and path that accepts token (and optionally refresh token) from Snowflake or external IdP
         */
        int OAuthTokenListenerWebServer::start(std::string host, int port, std::string path)
        {
            m_host = std::move(host);
            m_path = std::move(path);
            m_port = std::move(port);
            m_real_port = std::move(port);
            if (sf_strncasecmp("localhost", m_host.c_str(), 9) == 0) {
                m_host = IAuthenticatorOAuth::S_LOCALHOST;
            }

            if (port == 0) {
                CXX_LOG_TRACE("sf::OAuthTokenListenerWebServer::start::Trying to start HTTP listener on: %s%s, port will be randomly chosen",
                    m_host.c_str(), path.c_str());
            }
            else {
                CXX_LOG_TRACE("sf::OAuthTokenListenerWebServer::start::Trying to start HTTP listener on: %s:%d%s",
                    m_host.c_str(), port, path.c_str());
            }
            start();
            return m_real_port;
        }

        /**
         * Start http listener
         */
        void OAuthTokenListenerWebServer::start()
        {
            m_socket_desc_web_client = 0;
            m_socket_descriptor = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            /*               TODO: On Windows, socket functions don't set errno and the error code
                           should be retrieved by WSAGetLastError(). Therefore for now the error
                           message won't be logged correctly on Windows.
                           Leave it for now since it won't affect the functionality.*/
            if ((int)m_socket_descriptor < 0)
            {
                CXX_LOG_ERROR("sf::AuthWebServer::start::Failed to start web server. Could not create a socket.  err: %s", strerror(errno));
                throw AuthException("SFOAuthError: " + std::string(strerror(errno)));
            }

            struct sockaddr_in recv_server;
            memset((char*)&recv_server, 0, sizeof(struct sockaddr_in));
            recv_server.sin_family = AF_INET;
            recv_server.sin_port = htons(m_port); // ephemeral port
            CXX_LOG_INFO("HOST is %s", m_host.c_str())
                if (inet_pton(AF_INET, m_host.c_str(), &recv_server.sin_addr.s_addr) != 1)
                {
                    CXX_LOG_ERROR(
                        "sf::OAuthTokenListenerWebServer::start::Failed to start web server. Could not convert buffer to a network address. err: %s",
                        strerror(errno));
                    throw AuthException("SFOAuthError: " + std::string(strerror(errno)));

                }
            if (bind(m_socket_descriptor, (struct sockaddr*)&recv_server,
                sizeof(struct sockaddr_in)) < 0)
            {
                CXX_LOG_ERROR(
                    "sf::OAuthTokenListenerWebServer::start::Failed to start web server. Could not bind a port. err: %s",
                    strerror(errno));
                throw AuthException("SFOAuthError: " + std::string(strerror(errno)));
            }
            socklen_t length = sizeof(struct sockaddr_in);
            if (getsockname(m_socket_descriptor, (struct sockaddr*)&recv_server, &length) < 0) {
                CXX_LOG_ERROR(
                    "sf::OAuthTokenListenerWebServer::start::Failed to get socket name. Could not get a port. err: %s",
                    strerror(errno));
                throw AuthException("SFOAuthError: " + std::string(strerror(errno)));
            }
            m_real_port = ntohs(recv_server.sin_port);
            if (m_real_port != m_port) {
                CXX_LOG_TRACE("sf::OAuthTokenListenerWebServer::start::Started on port: %d for %s:%d%s", m_real_port, m_host.c_str(), m_real_port, m_path.c_str());
            }
            if (listen(m_socket_descriptor, 0) < 0)
            {
                CXX_LOG_ERROR(
                    "sf::OAuthTokenListenerWebServer::start::Failed to start web server. Could not listen a port. err: %s",
                    strerror(errno));
                throw AuthException("SFOAuthError: " + std::string(strerror(errno)));

            }
            CXX_LOG_TRACE("sf::OAuthTokenListenerWebServer::start::Web Server successfully started on %s:%d and path %s", m_host.c_str(), m_real_port, m_path.c_str())
        }

        void OAuthTokenListenerWebServer::stop()
        {
            CXX_LOG_TRACE("sf::OAuthTokenListenerWebServer::stop::Stopping HTTP listener: %s:%d%s", m_host.c_str(), m_real_port, m_path.c_str())
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
                            "sf::OAuthTokenListenerWebServer::stop::Failed close HTTP port err: %s",
                            strerror(errno));
                        throw AuthException("SFOAuthError: " + std::string(strerror(errno)));
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
                        "sf::OAuthTokenListenerWebServer::stop::Failed to stop web server. err: %s",
                        strerror(errno));
                    m_socket_descriptor = 0;
                    throw AuthException("SFOAuthError: " + std::string(strerror(errno)));
                }
            }
            m_socket_descriptor = 0;
        }

        /**
         * Get port number listening
         * @return port number
         */
        int OAuthTokenListenerWebServer::getPort()
        {
            return m_real_port;
        }

        /**
         * Set the timeout for the web server.
         */
        void OAuthTokenListenerWebServer::setTimeout(int timeout)
        {
            m_timeout = timeout;
        }

        void OAuthTokenListenerWebServer::startAccept(std::string state)
        {
            m_state = state;
            startAccept();
        }

        void OAuthTokenListenerWebServer::startAccept()
        {
            struct sockaddr_in client = { 0, 0, 0, {} };
            memset((char*)&client, 0, sizeof(struct sockaddr_in));
            socklen_t len = sizeof(client);

            fd_set fd;
            timeval timeout;
            FD_ZERO(&fd);
            FD_SET(m_socket_descriptor, &fd);
            timeout.tv_sec = m_timeout;
            timeout.tv_usec = 0;
            CXX_LOG_TRACE("sf::OAuthTokenListenerWebServer::startAccept::select(m_socket_descriptor,...");
            int retVal = select(m_socket_descriptor + 1, &fd, NULL, NULL, &timeout);
            if (retVal > 0)
            {
                m_socket_desc_web_client = accept(
                    m_socket_descriptor, (struct sockaddr*)&client, &len);
                if ((int)m_socket_desc_web_client < 0)
                {
                    CXX_LOG_ERROR(
                        "sf::OAuthTokenListenerWebServer::startAccept::Failed to receive token. Could not accept a request. error: %s",
                        strerror(errno));
                    throw AuthException("SFOAuthError: " + std::string(strerror(errno)));
                }
            }
            else if (retVal == 0)
            {
                CXX_LOG_ERROR("sf::OAuthTokenListenerWebServer::startAccept::Auth browser timed out. ");
                throw AuthException("SFOAuthError: Auth browser timed out.");
            }
            else
            {
                CXX_LOG_ERROR(
                    "sf::OAuthTokenListenerWebServer::startAccept::Failed to determine status of auth web server. err: %s",
                    strerror(errno));
                throw AuthException("SFOAuthError: " + std::string(strerror(errno)));
            }
        }

        bool OAuthTokenListenerWebServer::receive()
        {
            CXX_LOG_TRACE("sf::OAuthTokenListenerWebServer::receive:::start receive")

                char mesg[20000];
            char* rest_mesg;
            char* method;
            int recvlen;
            memset((void*)mesg, (int)'\0', sizeof(mesg));
            recvlen = (int)recv(m_socket_desc_web_client, mesg, sizeof(mesg) - 1, 0);
            if (recvlen < 0)
            {
                CXX_LOG_ERROR("sf::OAuthTokenListenerWebServer::receive:::Could not receive a request.err: %s", strerror(errno));
                throw AuthException("SFOAuthError: " + std::string(strerror(errno)));
            }

            CXX_LOG_TRACE("sf::OAuthTokenListenerWebServer::receive::---------")
                CXX_LOG_TRACE("sf::OAuthTokenListenerWebServer::receive::%s", maskOAuthSecret(mesg).c_str())
                CXX_LOG_TRACE("sf::OAuthTokenListenerWebServer::receive::---------")
                method = sf_strtok(mesg, " \t\n", &rest_mesg);
            if (strncmp(method, "GET\0", 4) == 0)
            {
                parseAndRespondGetRequest(method, &rest_mesg);
                return true;
            }
            else
            {
                std::string errorDesc = "Unexpected method while waiting for a request: " + std::string(method);
                CXX_LOG_ERROR("sf::OAuthTokenListenerWebServer::receive::%s", errorDesc.c_str());
                throw AuthException("SFOAuthError: " + std::string(strerror(errno)));
            }
            return false;
        }

        void OAuthTokenListenerWebServer::parseAndRespondGetRequest(char* method, char** rest_mesg)
        {
            SF_UNUSED(method);
            char** position = rest_mesg;
            char* fullPath = sf_strtok(NULL, " \t\n", position);
            char* protocol = sf_strtok(NULL, " \t\n", position);
            sf_strtok(NULL, " :", position); // skip host key

            m_token = "";

            // validate protocol
            if (strncmp(protocol, "HTTP/1.0", 8) != 0 &&
                strncmp(protocol, "HTTP/1.1", 8) != 0)
            {
                fail("400 Bad Request", "HTTP Request has unexpected protocol");
            }

            // validate request path
            if (strncmp(m_path.c_str(), fullPath, m_path.length()) != 0)
            {
                fail("400 Bad Request", "HTTP Request is missing URL");
            }

            // validate request parameters exist
            std::string expectedPath = m_path.length() > 0 ? m_path : "/";
            expectedPath = expectedPath.append("?");
            if (strncmp(fullPath, expectedPath.c_str(), expectedPath.length()) != 0)
            {
                fail("400 Bad Request", "HTTP Request has invalid endpoint path");
            }

            // extract request parameters
            char* paramsPtr;
            char* path = sf_strtok(fullPath, "?", &paramsPtr);
            if (path == nullptr)
            {
                fail("400 Bad Request", "HTTP Request is missing parameters");
            }

            std::vector<std::string> params = splitString(paramsPtr, '&');
            std::string receivedCode;
            std::string receivedState;
            std::string error;
            std::string errorDescription;
            for (auto it = params.begin(); it != params.end(); ++it) {
                std::string item = it->c_str();
                std::vector<std::string> param = splitString(item, '=');
                if (param.at(0) == "code")
                    receivedCode = param.at(1);
                if (param.at(0) == "state")
                    receivedState = param.at(1);
                if (param.at(0) == "error")
                    error = param.at(1);
                if (param.at(0) == "error_description")
                    errorDescription = std::regex_replace(param.at(1), std::regex("\\+"), " ");
            }

            if (!error.empty()) {
                std::string fullError = "Identity Provider responded with error: " + error + ": " + errorDescription;
                fail("400 Bad Request", fullError);
            }

            // validate state and non-empty authorization code
            if (receivedState != m_state || receivedState.empty())
            {
                fail("400 Bad Request", "Identity Provider did not provide expected state parameter!It might indicate an XSS attack.");
            }

            m_token = receivedCode;
            CXX_LOG_TRACE("sf::OAuthTokenListenerWebServer::parseAndRespondGetRequest",
                "Successfully received authorization code from Identity Provider");
            respond("200 OK", successMessage);
        }

        bool OAuthTokenListenerWebServer::isConsentCacheIdToken()
        {
            return true;
        }

        std::string OAuthTokenListenerWebServer::getToken()
        {
            return m_token;
        }

        void OAuthTokenListenerWebServer::respond(std::string)
        {
            respond("200 OK", successMessage);
        }

        void OAuthTokenListenerWebServer::respond(std::string errorCode, std::string message)
        {
            std::stringstream buf;
            buf << "HTTP/1.0 " << errorCode << "\r\n"
                << "Content-Type: text/html" << "\r\n"
                << "Content-Length: " << message.length() << "\r\n\r\n"
                << message;
            send(m_socket_desc_web_client, buf.str().c_str(), (int)buf.str().length(), 0);
            buf.clear();
        }

        void OAuthTokenListenerWebServer::fail(std::string httpError, std::string message)
        {
            CXX_LOG_ERROR("sf::OAuthTokenListenerWebServer::fail", message.c_str());
            respond(httpError, failureMessage); // unless error message is html-escaped it shouldn't be part of response visible in the browser
            throw AuthException(message);
        }
    };

    std::string UrlEncode(std::string url)
    {
        return curl_easy_escape(nullptr, url.c_str(), url.length());
    }

    std::string maskOAuthSecret(const std::string& secret)
    {
#if defined _DEBUG_OAUTH
        return secret;
#else
        SF_UNUSED(secret);
        return "****";
#endif
    }

    std::string maskOAuthSecret(SFURL& secret) {
#if defined _DEBUG_OAUTH
        return secret.toString();
#else
        SF_UNUSED(secret);
        return "****";
#endif
    }
}