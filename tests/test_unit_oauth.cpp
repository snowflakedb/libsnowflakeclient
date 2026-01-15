#include <string>
#include "snowflake/SFURL.hpp"
#include "../lib/connection.h"
#include "../lib/authenticator.h"
#include "../cpp/lib/AuthenticatorOAuth.hpp"
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"
#include "memory.h"

using namespace Snowflake::Client;


/*
 * Test auth connection when the token was not provided.
 */
void test_oauth_with_no_token(void** unused)
{
    SF_UNUSED(unused);
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "test_account");
    snowflake_set_attribute(sf, SF_CON_USER, "test_user");
    snowflake_set_attribute(sf, SF_CON_HOST, "host");
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "oauth");

    SF_STATUS status = snowflake_connect(sf);
    assert_int_not_equal(status, SF_STATUS_SUCCESS);
    SF_ERROR_STRUCT* error = snowflake_error(sf);
    assert_int_equal(error->error_code, SF_STATUS_ERROR_BAD_CONNECTION_PARAMS);
    snowflake_term(sf);
}

/*
 * Test the request body with the oauth connection.
 */
void test_json_data_in_oauth(void** unused)
{
    SF_UNUSED(unused);
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "test_account");
    snowflake_set_attribute(sf, SF_CON_USER, "test_user");
    snowflake_set_attribute(sf, SF_CON_HOST, "host");
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_OAUTH);
    snowflake_set_attribute(sf, SF_CON_OAUTH_TOKEN, "mock_token");

    cJSON* body = create_auth_json_body(
        sf,
        sf->application,
        sf->application_name,
        sf->application_version,
        sf->timezone,
        sf->autocommit);
    cJSON* data = snowflake_cJSON_GetObjectItem(body, "data");

    assert_string_equal(snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(data, "authenticator")), "oauth");
    assert_string_equal(snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(data, "token")), "mock_token");

    body = snowflake_cJSON_CreateObject();
    auth_update_json_body(sf, body);
    data = snowflake_cJSON_GetObjectItem(body, "data");
    assert_string_equal(snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(data, "authenticator")), "oauth");
    assert_string_equal(snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(data, "token")), "mock_token");
    snowflake_term(sf);
}

class OAuthTokenListenerWebServerMock : public OAuthTokenListenerWebServer {
public:
    OAuthTokenListenerWebServerMock() : OAuthTokenListenerWebServer() {}
    std::string getToken() override { return std::string("authorisationCode123"); }
    void startAccept(std::string state) override {
        SF_UNUSED(state);
    }
    bool receive() override { return true; }
};

class WebBrowserRunnerMock : public IAuthenticationWebBrowserRunner {
public:
    void startWebBrowser(const std::string& url) override {
        m_url = url;
    }
    std::string getUrl() { return m_url; }
private:
    std::string m_url = std::string("");
};


std::string getValueFromUrl(std::string encodedUrl, std::string prefix) {
    auto searchStartPos = encodedUrl.find(prefix);
    searchStartPos += std::string(prefix).length();
    auto searchFinishPos = encodedUrl.find('&', searchStartPos);
    if (searchFinishPos == std::string::npos)
        searchFinishPos = encodedUrl.length();
    std::string searchValue = encodedUrl.substr(searchStartPos, searchFinishPos - searchStartPos);
    int searchValueUnencodedLength;
    std::string searchValueUnencoded = curl_easy_unescape(nullptr, searchValue.data(), searchValue.length(), &searchValueUnencodedLength);
    return searchValueUnencoded;
}


class MockOAuth : public AuthenticatorOAuth
{
public:
    MockOAuth(SF_CONNECT* connection,
        IAuthWebServer* authWebServer = nullptr,
        IAuthenticationWebBrowserRunner* webBrowserRunner = nullptr)
        : AuthenticatorOAuth(connection, authWebServer, webBrowserRunner)
    {
    }
    bool executeRestRequest(SFURL& url, const std::string& body, jsonObject_t& resp) override
    {
        SF_UNUSED(url);
        SF_UNUSED(body);
        SF_UNUSED(resp);
        resp["token_type"] = picojson::value("Bearer");
        resp["expires_in"] = picojson::value("1");
        resp["access_token"] = picojson::value("at");
        resp["refresh_token"] = picojson::value("refresh");
        resp["scope"] = picojson::value("session:role:ANALYST");
        return true;
    }
};

SF_CONNECT* get_test_connection()
{
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "test_account");
    snowflake_set_attribute(sf, SF_CON_USER, "test_user");
    snowflake_set_attribute(sf, SF_CON_HOST, "host");
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_ROLE, "ANALYSIST");
    snowflake_set_attribute(sf, SF_CON_APPLICATION_NAME, SF_API_NAME);
    snowflake_set_attribute(sf, SF_CON_APPLICATION_VERSION, SF_API_VERSION);
    return sf;
}

void test_oauth_invalid_parameters(void** unused)
{
    SF_UNUSED(unused);
    // missing both the client id and the client secret
    SF_CONNECT* test1 = get_test_connection();
    // missing clinet secret
    SF_CONNECT* test2 = get_test_connection();
    snowflake_set_attribute(test2, SF_CON_OAUTH_CLIENT_ID, "clientid123");
    // missing both role and auth_role
    SF_CONNECT* test3 = get_test_connection();
    SF_FREE(test3->role);
    snowflake_set_attribute(test3, SF_CON_OAUTH_CLIENT_ID, "clientid123");
    snowflake_set_attribute(test3, SF_CON_OAUTH_CLIENT_SECRET, "clientid123Password");


    SF_CONNECT* sf[3] = { test1, test2, test3 };

    for (int8 i = 0; i < 3; i++)
    {
        snowflake_set_attribute(sf[i], SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_OAUTH_AUTHORIZATION_CODE);
        SF_STATUS status = snowflake_connect(sf[i]);
        assert_int_not_equal(status, SF_STATUS_SUCCESS);
        SF_ERROR_STRUCT* error = snowflake_error(sf[i]);
        assert_int_equal(error->error_code, SF_STATUS_ERROR_BAD_CONNECTION_PARAMS);

        snowflake_set_attribute(sf[i], SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_OAUTH_AUTHORIZATION_CODE);
        status = snowflake_connect(sf[i]);
        assert_int_not_equal(status, SF_STATUS_SUCCESS);
        error = snowflake_error(sf[i]);
        assert_int_equal(error->error_code, SF_STATUS_ERROR_BAD_CONNECTION_PARAMS);
        snowflake_term(sf[i]);
    }
}

void test_oauth_start_browser_url(void** unused) {
    SF_UNUSED(unused);
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "test_account");
    snowflake_set_attribute(sf, SF_CON_HOST, "testaccount.snowflakecomputing.com");
    snowflake_set_attribute(sf, SF_CON_USER, "test_user");
    snowflake_set_attribute(sf, SF_CON_HOST, "host");
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_ROLE, "ANALYST");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_OAUTH_AUTHORIZATION_CODE);
    snowflake_set_attribute(sf, SF_CON_OAUTH_CLIENT_ID, "client123");
    snowflake_set_attribute(sf, SF_CON_OAUTH_CLIENT_SECRET, "client123Password");
    snowflake_set_attribute(sf, SF_CON_OAUTH_REDIRECT_URI, "http://127.0.0.1:8001/oauth2/v1/redirect_uri");

    auto* webServer = new OAuthTokenListenerWebServerMock();
    auto* webBrowserRunner = new WebBrowserRunnerMock();

    MockOAuth* auth = new MockOAuth(sf, webServer, webBrowserRunner);
    auth->authenticate();
    std::string url = webBrowserRunner->getUrl();
    std::string redirectUri = getValueFromUrl(url, "redirect_uri=");
    std::string clientId = getValueFromUrl(url, "client_id=");
    std::string scope = getValueFromUrl(url, "scope=");
    std::string codeChallengeMethod = getValueFromUrl(url, "code_challenge_method=");
    assert_string_equal(redirectUri.c_str(), "http://127.0.0.1:8001/oauth2/v1/redirect_uri");
    assert_string_equal(clientId.c_str(), "client123");
    assert_string_equal(scope.c_str(), "session:role:ANALYST");
    assert_string_equal(codeChallengeMethod.c_str(), "S256");

    delete auth;
    snowflake_term(sf);
}

void test_oauth_start_browser_url_no_redirect_url(void** unused) {
    SF_UNUSED(unused);
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "test_account");
    snowflake_set_attribute(sf, SF_CON_HOST, "testaccount.snowflakecomputing.com");
    snowflake_set_attribute(sf, SF_CON_USER, "test_user");
    snowflake_set_attribute(sf, SF_CON_HOST, "host");
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_ROLE, "ANALYST");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_OAUTH_AUTHORIZATION_CODE);
    snowflake_set_attribute(sf, SF_CON_OAUTH_CLIENT_ID, "client123");
    snowflake_set_attribute(sf, SF_CON_OAUTH_CLIENT_SECRET, "client123Password");

    auto* webServer = new OAuthTokenListenerWebServerMock();
    auto* webBrowserRunner = new WebBrowserRunnerMock();

    AuthenticatorOAuth* auth = new MockOAuth(sf, webServer, webBrowserRunner);
    auth->authenticate();

    int port = webServer->getPort();
    assert_true(port > 0);

    std::string url = webBrowserRunner->getUrl();
    std::string redirectUri = getValueFromUrl(url, "redirect_uri=");
    std::string clientId = getValueFromUrl(url, "client_id=");
    std::string scope = getValueFromUrl(url, "scope=");
    std::string codeChallengeMethod = getValueFromUrl(url, "code_challenge_method=");
    std::string expectedRedirectUrl = "http://127.0.0.1:" + std::to_string(port);

    assert_string_equal(redirectUri.c_str(), expectedRedirectUrl.c_str());
    assert_string_equal(clientId.c_str(), "client123");
    assert_string_equal(scope.c_str(), "session:role:ANALYST");
    assert_string_equal(codeChallengeMethod.c_str(), "S256");

    delete auth;
    snowflake_term(sf);
}

void test_mask_oauth_secret_string(void** unused)
{
    SF_UNUSED(unused);
    auto masked = Snowflake::Client::maskOAuthSecret("this should be masked as it contains token: not-for-the-logs-value");
    assert_string_equal(masked.c_str(), "****");
}

void test_mask_oauth_secret_sf_url(void** unused)
{
    SF_UNUSED(unused);
    Snowflake::Client::SFURL sfUrl = Snowflake::Client::SFURL::parse("https://some.host.com/with/secrets?token=abc&user=jdoe");
    auto masked = Snowflake::Client::maskOAuthSecret(sfUrl);
    assert_string_equal(masked.c_str(), "****");
}

int main(void)
{
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_oauth_with_no_token),
      cmocka_unit_test(test_json_data_in_oauth),
      cmocka_unit_test(test_oauth_invalid_parameters),
      cmocka_unit_test(test_oauth_start_browser_url),
      cmocka_unit_test(test_oauth_start_browser_url_no_redirect_url),
      cmocka_unit_test(test_mask_oauth_secret_string),
      cmocka_unit_test(test_mask_oauth_secret_sf_url),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    snowflake_global_term();
    return ret;
}
