#include "../cpp/lib/AuthenticatorOAuth.hpp"
#include "UnitOAuthBase.hpp"
#include "wiremock.hpp"
#include "../cpp/logger/SFLogger.hpp"
#include "test_setup.h"
#include "TestSetup.hpp"

namespace {
    using namespace Snowflake::Client;

    void configureRunners() {
        UnitOAuthBase::initAuthChallengeTestProvider();
        UnitOAuthBase::initAuthWebBrowserTestRunner();
    }

    SF_CONNECT* createConnection(int port,
        int browserResponseTimeout = SF_BROWSER_RESPONSE_TIMEOUT,
        bool oauthSingleUseRefreshTokens = false)
    {
        return UnitOAuthBase::createConnection(AUTH_OAUTH_AUTHORIZATION_CODE,
            browserResponseTimeout,
            "",
            "",
            "http://localhost:" + std::to_string(port) + "/snowflake/oauth-redirect",
            oauthSingleUseRefreshTokens);
    }

    SF_CONNECT* createConnection(int browserResponseTimeout,
        std::string customAuthEndpoint,
        std::string customTokenEndpoint,
        std::string redirectUri) {
        return UnitOAuthBase::createConnection(AUTH_OAUTH_AUTHORIZATION_CODE,
            browserResponseTimeout,
            std::move(customAuthEndpoint),
            std::move(customTokenEndpoint),
            std::move(redirectUri));
    }

    int randomPort = 65000;
}

Snowflake::Client::WiremockRunner* wiremock;

void test_successful_oauth_authorization_flow(void** unused) {
    SF_UNUSED(unused);
    configureRunners();

    wiremock = new WiremockRunner("./tests/test_oauth/wiremock/idp_responses/idp_auth_successful.json",
        {
            "./tests/test_oauth/wiremock/snowflake_responses/snowflake_login_successful.json",
            "./tests/test_oauth/wiremock/snowflake_responses/snowflake_disconnect_successful.json"
        },
        randomPort
    );
    SF_CONNECT* sf = createConnection(randomPort++);

    CXX_LOG_INFO("sf::UnitOAuthAuthorizationFlow::Successful::Connecting to Snowflake");
    assert_true(snowflake_connect(sf) == SF_STATUS_SUCCESS);
    CXX_LOG_INFO("sf::UnitOAuthAuthorizationFlow::Successful::Connected to Snowflake successfully");
    assert_false(std::string(sf->token).empty());
    assert_string_equal(sf->token, "token-t1");
    assert_true(snowflake_term(sf) == SF_STATUS_SUCCESS);
}

void test_successful_oauth_authorization_flow_with_single_use_refresh_token(void** unused) {
    SF_UNUSED(unused);
    delete wiremock;
    configureRunners();

    wiremock = new WiremockRunner("./tests/test_oauth/wiremock/idp_responses/idp_auth_successful_with_single_use_refresh_token.json",
        {
            "./tests/test_oauth/wiremock/snowflake_responses/snowflake_login_successful.json",
            "./tests/test_oauth/wiremock/snowflake_responses/snowflake_disconnect_successful.json"
        },
        randomPort
    );
    SF_CONNECT* sf = createConnection(randomPort++, 120, true);

    CXX_LOG_INFO("sf::UnitOAuthAuthorizationFlow::Successful::Connecting to Snowflake");
    SF_STATUS status = snowflake_connect(sf);
    assert_true(status == SF_STATUS_SUCCESS);

    CXX_LOG_INFO("sf", "UnitOAuthAuthorizationFlow", "Successful", "Connected to Snowflake successfully");

    assert_false(std::string(sf->token).empty());
    assert_string_equal(sf->token, "token-t1");
    assert_true(snowflake_term(sf) == SF_STATUS_SUCCESS);
}

void test_custom_urls_oauth_authorization_flow(void** unused) {
    delete wiremock;
    SF_UNUSED(unused);
    configureRunners();

    wiremock = new WiremockRunner("./tests/test_oauth/wiremock/idp_responses/idp_auth_custom_urls.json",
        {
            "./tests/test_oauth/wiremock/snowflake_responses/snowflake_login_successful.json",
            "./tests/test_oauth/wiremock/snowflake_responses/snowflake_disconnect_successful.json"
        },
        randomPort
    );
    SF_CONNECT* sf = createConnection(
        SF_BROWSER_RESPONSE_TIMEOUT,
        std::string("https://") + wiremockHost + ":" + wiremockPort + "/authorization",
        std::string("https://") + wiremockHost + ":" + wiremockPort + "/tokenrequest",
        "http://localhost:" + std::to_string(randomPort++) + "/snowflake/oauth-redirect"
    );

    CXX_LOG_INFO("sf", "UnitOAuthAuthorizationFlow", "CustomURLs", "Connecting to Snowflake");
    SF_STATUS status = snowflake_connect(sf);
    assert_true(status == SF_STATUS_SUCCESS);
    CXX_LOG_INFO("sf", "UnitOAuthAuthorizationFlow", "CustomURLs", "Connected to Snowflake successfully");

    assert_false(std::string(sf->token).empty());
    assert_string_equal(sf->token, "token-t1");
    assert_true(snowflake_term(sf) == SF_STATUS_SUCCESS);
}

void test_invalid_scope_oauth_authorization_flow_in_redirect_url(void** unused)
{
    delete wiremock;
    SF_UNUSED(unused);
    wiremock = new WiremockRunner("./tests/test_oauth/wiremock/idp_responses/idp_auth_invalid_scope.json", {}, randomPort);
    configureRunners();
    SF_CONNECT* sf = createConnection(randomPort++);

    CXX_LOG_INFO("sf", "UnitOAuthAuthorizationFlow", "Invalid scope", "Connecting to Snowflake");
    SF_STATUS status = snowflake_connect(sf);
    assert_false(status == SF_STATUS_SUCCESS);

    CXX_LOG_ERROR("sf::UnitOAuthAuthorizationFlow::Invalid scope::Exception: %s", sf->error.msg);
    assert_true(std::string(sf->error.msg).find("invalid_scope: One or more scopes are not configured for the authorization server resource.") != string::npos);
}

void test_token_request_error_oauth_authorization_flow(void** unused) {
    SF_UNUSED(unused);

    delete wiremock;
    wiremock = new WiremockRunner("./tests/test_oauth/wiremock/idp_responses/idp_auth_token_request_error.json", {}, randomPort);
    configureRunners();
    SF_CONNECT* sf = createConnection(randomPort++);

    CXX_LOG_INFO("sf::UnitOAuthAuthorizationFlow::TokenRequestError::Connecting to Snowflake");
    SF_STATUS status = snowflake_connect(sf);

    assert_false(status == SF_STATUS_SUCCESS);
    CXX_LOG_ERROR("sf::UnitOAuthAuthorizationFlow::TokenRequestError::Exception: %s", sf->error.msg);
    //
    assert_true(std::string(sf->error.msg).find("Received unretryable http code: [400]") != string::npos);
}

void test_browser_timeout_oauth_authorization_flow(void** unused) {
    SF_UNUSED(unused);

    delete wiremock;
    wiremock = new WiremockRunner("./tests/test_oauth/wiremock/idp_responses/idp_auth_browser_timeout.json", {}, randomPort);
    configureRunners();
    SF_CONNECT* sf = createConnection(randomPort++, 1);

    CXX_LOG_INFO("sf::UnitOAuthAuthorizationFlow::BrowserTimeout::Connecting to Snowflake");
    SF_STATUS status = snowflake_connect(sf);

    assert_false(status == SF_STATUS_SUCCESS);
    CXX_LOG_ERROR("sf::UnitOAuthAuthorizationFlow::BrowserTimeout::Exception: %s", sf->error.msg);
    assert_true(std::string(sf->error.msg).find("Auth browser timed out") != string::npos);
}

void test_invalid_access_token_in_oauth_authorization_flow(void** unused)
{
    SF_UNUSED(unused);

    delete wiremock;
    wiremock = new WiremockRunner("./tests/test_oauth/wiremock/idp_responses/idp_auth_invalid_access_token.json", {}, randomPort);
    configureRunners();
    SF_CONNECT* sf = createConnection(randomPort++);

    CXX_LOG_INFO("sf::UnitOAuthAuthorizationFlow::BrowserTimeout::Connecting to Snowflake");
    SF_STATUS status = snowflake_connect(sf);

    assert_false(status == SF_STATUS_SUCCESS);
    assert_true(std::string(sf->error.msg).find("Invalid Identity Provider response: access token not provided") != string::npos);
}

void test_missing_access_token_in_oauth_authorization_flow(void** unused)
{
    SF_UNUSED(unused);

    delete wiremock;
    wiremock = new WiremockRunner("./tests/test_oauth/wiremock/idp_responses/idp_auth_missing_access_token.json", {}, randomPort);
    configureRunners();
    SF_CONNECT* sf = createConnection(randomPort++);

    CXX_LOG_INFO("sf", "UnitOAuthAuthorizationFlow", "BrowserTimeout", "Connecting to Snowflake");
    SF_STATUS status = snowflake_connect(sf);

    assert_false(status == SF_STATUS_SUCCESS);
    assert_true(std::string(sf->error.msg).find("Invalid Identity Provider response: access token not provided") != string::npos);
}

void test_validate_http_oauth_redirect_listener_detects_occupied_port(void** unused)
{
    SF_UNUSED(unused);
    OAuthTokenListenerWebServer listener1;
    OAuthTokenListenerWebServer listener2;
    int portUsed = listener1.start("localhost", randomPort, "myPath");
    assert_int_equal(portUsed, randomPort);
    try {
        listener2.start("localhost", randomPort++, "myPath");
        assert_false(true);
    }
    catch (const AuthException& e)
    {
        std::string error = std::string(e.cause());
        assert_true(error.find("Address already in use") != std::string::npos);
    }
}

void test_successful_oauth_authorization_flow_with_root_path_in_redirect_uri(void** unused)
{
    delete wiremock;
    SF_UNUSED(unused);
    configureRunners();

    wiremock = new WiremockRunner("./tests/test_oauth/wiremock/idp_responses/idp_auth_successful_root_redirect_uri_path.json",
        {
                "./tests/test_oauth/wiremock/snowflake_responses/snowflake_login_successful.json",
                "./tests/test_oauth/wiremock/snowflake_responses/snowflake_disconnect_successful.json"
        },
        randomPort
    );
    SF_CONNECT* sf = createConnection(SF_BROWSER_RESPONSE_TIMEOUT,
        "",
        "",
        "http://127.0.0.1:" + std::to_string(randomPort++)
    );

    CXX_LOG_INFO("sf::UnitOAuthAuthorizationFlow::Successful::Connecting to Snowflake");
    snowflake_connect(sf);
    CXX_LOG_INFO("sf::UnitOAuthAuthorizationFlow::Successful::Connected to Snowflake successfully");

    assert_false(std::string(sf->token).empty());
    assert_string_equal(sf->token, "token-t1");
    assert_true(snowflake_term(sf) == SF_STATUS_SUCCESS);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    sf_bool disable_verify_peer = SF_BOOLEAN_TRUE;
    sf_bool enable_redirect = SF_BOOLEAN_TRUE;
    snowflake_global_set_attribute(SF_GLOBAL_DISABLE_VERIFY_PEER, &disable_verify_peer);
    snowflake_global_set_attribute(SF_GLOBAL_ENALBE_REDIRECT, &enable_redirect);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_successful_oauth_authorization_flow),
        cmocka_unit_test(test_successful_oauth_authorization_flow_with_single_use_refresh_token),
        cmocka_unit_test(test_custom_urls_oauth_authorization_flow),
        cmocka_unit_test(test_invalid_scope_oauth_authorization_flow_in_redirect_url),
        cmocka_unit_test(test_token_request_error_oauth_authorization_flow),
        cmocka_unit_test(test_browser_timeout_oauth_authorization_flow),
        cmocka_unit_test(test_invalid_access_token_in_oauth_authorization_flow),
        cmocka_unit_test(test_missing_access_token_in_oauth_authorization_flow),
        cmocka_unit_test(test_validate_http_oauth_redirect_listener_detects_occupied_port),
        cmocka_unit_test(test_successful_oauth_authorization_flow_with_root_path_in_redirect_uri),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    delete wiremock;
    return ret;
}
