#define CATCH_CONFIG_MAIN

#include "UnitOAuthBase.hpp"

#include <string>

#include "AuthenticationChallengeTestProvider.hpp"
#include "AuthenticationWebBrowserTestRunner.hpp"
#include "wiremock.hpp"

namespace Snowflake {
    namespace Client
    {
        struct OAuthFlowTestConnectionParams {
            std::string authenticator, authEndpoint, tokenEndpoint, redirectURI;
            bool oauthSingleUseRefreshTokens;
        };

        const std::string& strOr(const std::string& first, const std::string& second) {
            return first.empty() ? second : first;
        }

        const std::string wireMockUri = std::string("https://") + wiremockHost + ":" + wiremockPort;

        OAuthFlowTestConnectionParams createAuthorizationCodeParams(AuthenticatorType flow,
            const std::string& customAuthEndpoint,
            const std::string& customTokenEndpoint,
            const std::string& redirectUri,
            const bool& oauthSingleUseRefreshTokens) {

            switch (flow) {
            case AUTH_OAUTH_AUTHORIZATION_CODE:
                return {
                  SF_AUTHENTICATOR_OAUTH_AUTHORIZATION_CODE,
                  strOr(customAuthEndpoint, wireMockUri + "/auth/authorize"),
                  strOr(customTokenEndpoint, wireMockUri + "/auth/token"),
                  redirectUri,
                  oauthSingleUseRefreshTokens
                };
            case AUTH_OAUTH_CLIENT_CREDENTIALS:
                return {
                  SF_AUTHENTICATOR_OAUTH_CLIENT_CREDENTIALS,
                  "",
                  strOr(customTokenEndpoint, wireMockUri + "/oauth/token-request"),
                  "",
                  false
                };
            default:
                return {};
            }
        }

        SF_CONNECT* UnitOAuthBase::createConnection(
            AuthenticatorType flow,
            int browserResponseTimeout,
            std::string customAuthEndpoint,
            std::string customTokenEndpoint,
            std::string redirectUri,
            bool oauthSingleUseRefreshTokens)
        {
            OAuthFlowTestConnectionParams params = createAuthorizationCodeParams(flow,
                customAuthEndpoint,
                customTokenEndpoint,
                redirectUri,
                oauthSingleUseRefreshTokens);
            SF_CONNECT* sf = snowflake_init();
            snowflake_set_attribute(sf, SF_CON_HOST, wiremockHost);
            snowflake_set_attribute(sf, SF_CON_PORT, wiremockPort);
            snowflake_set_attribute(sf, SF_CON_ACCOUNT, "snowdriverswarsaw.qa6.us-west-2.aws");
            snowflake_set_attribute(sf, SF_CON_USER, "qa@snowflakecomputing.com");
            snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, (params.authenticator).c_str());
            snowflake_set_attribute(sf, SF_CON_DATABASE, "TEST_DB");
            snowflake_set_attribute(sf, SF_CON_SCHEMA, "TEST_SCHEMA");
            snowflake_set_attribute(sf, SF_CON_WAREHOUSE, "TEST_WAREHOUSE");
            snowflake_set_attribute(sf, SF_CON_ROLE, "ANALYST");
            snowflake_set_attribute(sf, SF_CON_OAUTH_AUTHORIZATION_ENDPOINT, (params.authEndpoint).c_str());
            snowflake_set_attribute(sf, SF_CON_OAUTH_TOKEN_ENDPOINT, (params.tokenEndpoint).c_str());
            snowflake_set_attribute(sf, SF_CON_OAUTH_REDIRECT_URI, (params.redirectURI).c_str());
            snowflake_set_attribute(sf, SF_CON_OAUTH_CLIENT_ID, "123");
            snowflake_set_attribute(sf, SF_CON_OAUTH_CLIENT_SECRET, "client-secret-value");
            snowflake_set_attribute(sf, SF_CON_OAUTH_SCOPE, "session:role:ANALYST");
            sf_bool insecureMode = SF_BOOLEAN_TRUE;
            snowflake_set_attribute(sf, SF_CON_INSECURE_MODE, &insecureMode);
            sf->single_use_refresh_token = params.oauthSingleUseRefreshTokens;

            return sf;
        }

        void UnitOAuthBase::initAuthChallengeTestProvider() {
            AuthenticationChallengeBaseProvider::setInstance(std::make_unique<AuthenticationChallengeTestProvider>());
        }

        void UnitOAuthBase::initAuthWebBrowserTestRunner() {
            IAuthenticationWebBrowserRunner::setInstance(std::make_unique<AuthenticationWebBrowserTestRunner>());
        }
    }
}
    
