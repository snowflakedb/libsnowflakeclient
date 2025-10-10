#pragma once
#ifndef SNOWFLAKE_ODBC_AUTHENTICATOROAUTH_HPP
#define SNOWFLAKE_ODBC_AUTHENTICATOROAUTH_HPP

#include <future>
#include "snowflake/client.h"



#include "snowflake/SFURL.hpp"
#include "../include/snowflake/IAuth.hpp"

#include "Authenticator.hpp"
#include "AuthenticationChallengeProvider.hpp"
#include "AuthenticationWebBrowserRunner.hpp"



namespace Snowflake {
    namespace Client {
        using namespace Snowflake::Client::IAuth;

        using SFURL = Snowflake::Client::SFURL;

        struct AuthorizationCodeRequest;
        struct AuthorizationCodeResponse;
        struct AccessTokenRequest;
        struct AccessTokenResponse;
        struct RefreshAccessTokenRequest;

        enum class OAuthFlowType
        {
            OAUTH_FLOW_AUTHORIZATION_CODE,
            OAUTH_FLOW_CLIENT_CREDENTIALS,
            OAUTH_UNSUPPORTED,
        };

        /*
         * Supports OAuth 2.1:
         * - authorization code flow with PKCE
         * - client credentials flow
         */
        class AuthenticatorOAuth : public IAuthenticator, public AuthErrorHandler
        {
        public:
            static const std::string S_LOCALHOST;
            static const std::string S_LOCALHOST_URL;
            static const std::string S_OAUTH_DEFAULT_AUTHORIZATION_URL_POSTFIX;
            static const std::string S_OAUTH_DEFAULT_TOKEN_URL_POSTFIX;

            AuthenticatorOAuth(SF_CONNECT* connection,
                IAuthWebServer* authWebServer = nullptr,
                IAuthenticationWebBrowserRunner* webBrowserRunner = nullptr);
            //    IRestRequestProvider* restRequestProvider = nullptr);



            void authenticate() override;
            void updateDataMap(jsonObject_t& dataMap) override;
        private:
            void authorizationCodeFlow();
            void clientCredentialsFlow();
            bool refreshAccessTokenFlow();
            void validateConfiguration();
            void handleInvalidResponse(const bool success, const std::string& errorMessage);
            bool executeRestRequest(SFURL& endPoint, const std::string& body, jsonObject_t& resp);
            AuthorizationCodeResponse executeAuthorizationCodeRequest(AuthorizationCodeRequest& authorizationCodeRequest);
            AccessTokenResponse executeAccessTokenRequest(AccessTokenRequest& request);
            AccessTokenResponse executeRefreshAccessTokenRequest(RefreshAccessTokenRequest& request);
            std::string createAccessTokenRequestBody(AccessTokenRequest& request);
            std::string createRefreshAccessTokenRequestBody(const RefreshAccessTokenRequest& request);
            void startWebBrowser(const std::string& url);
            static std::string oauthWebServerTask(IAuthWebServer* authWebServer, const SFURL& redirectUrl, const std::string& state, const int browserResponseTimeout);
            std::future<std::string> asyncStartOAuthWebserver(const AuthorizationCodeRequest& authorizationCodeRequest) const;
            void refreshDynamicRedirectUri(int portUsed, AuthorizationCodeRequest& authorizationCodeRequest);
            std::string oauthAuthorizationFlow();
            void resetTokens(std::string accessToken, std::string refreshToken);
            static OAuthFlowType authenticatorToFlowType(AuthenticatorType authenticatorType);

            SF_CONNECT* m_connection;
            AuthenticationChallengeBaseProvider* m_challengeProvider;
            IAuthenticationWebBrowserRunner* m_webBrowserRunner;
            //IRestRequestProvider* const m_restRequestProvider;

            std::unique_ptr<IAuthWebServer> m_authWebServer;
            typedef Snowflake::Client::Util::IBase64 Base64;

            OAuthFlowType m_oauthFlow;
            SFURL m_authEndpoint;
            SFURL m_tokenEndpoint;
            std::string m_clientId;
            std::string m_clientSecret;
            std::string m_authScope;
            SFURL m_redirectUri;
            bool m_redirectUriDynamicDefault;
            bool m_singleUseRefreshTokens;

            std::string m_token;
        };

        class AuthException : public std::exception
        {
            std::string problem;
        public:
            explicit AuthException(std::string message) : problem(message) {}

            const std::string& cause() const { return problem; };
        };

        /**
          * Web Server used to receive IdP token from GS or external IdP.
          * TODO: replace with boost::beast
          * https://snowflakecomputing.atlassian.net/browse/SNOW-1865356
          */
        class OAuthTokenListenerWebServer : public IAuthWebServer
        {
        private:
#ifdef _WIN32
            SOCKET m_socket_descriptor; // socket
            SOCKET m_socket_desc_web_client; // socket (client)
#else
            int m_socket_descriptor; // socket
            int m_socket_desc_web_client; // socket (client)
#endif
            int m_port; // port to listen, 0 for random port to be used
            int m_real_port; // actual port used when randomly picked
            std::string m_host;
            std::string m_path;
            std::string m_token;
            std::string m_state;
            std::string m_redirectUri;
            std::string m_origin;
            int m_timeout;

        public:
            OAuthTokenListenerWebServer();
            void start() override;
            int start(std::string host, int port, std::string path) override;
            void stop() override;
            void startAccept() override;
            void startAccept(std::string state) override;
            bool receive() override;
            int getPort() override;
            std::string getToken() override;
            bool isConsentCacheIdToken() override;
            void setTimeout(int timeout) override;
        private:
            void parseAndRespondGetRequest(char* method, char** rest_mesg);
            void respond(std::string);
            void respond(std::string httpError, std::string message);
            void fail(std::string httpError, std::string message);
            constexpr static const char* successMessage = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"/>\n"
                "<title>Authorization Code Granted for Snowflake</title></head>\n"
                "<body><h4>Your identity was confirmed</h4>"
                "Access to Snowflake has been granted to the ODBC driver.\n"
                "You can close this window now and go back where you started from.\n"
                "</body></html>";
            constexpr static const char* failureMessage = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"/>\n"
                "<title>Authorization Code Failed for Snowflake</title></head>\n"
                "<body><h4>Could not validate your identity</h4>"
                "Access to Snowflake could not have been granted to the ODBC driver.\n"
                "You can close this window now and try again.\n"
                "</body></html>";
        };

        std::string UrlEncode(std::string url);
        std::string maskOAuthSecret(const std::string& secret);
        std::string maskOAuthSecret(SFURL& secret);
    }
}
#endif //SNOWFLAKE_ODBC_AUTHENTICATOROAUTH_HPP
