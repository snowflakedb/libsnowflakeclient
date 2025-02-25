/*
 * Copyright (c) 2024 Snowflake Computing, Inc. All rights reserved.
 */

#include <string>
#include <iostream>
#include <thread>
#include <cstring>
#include "snowflake/SFURL.hpp"
#include "../lib/connection.h"
#include "../lib/authenticator.h"
#include "../cpp/lib/Authenticator.hpp"
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"
#include "../cpp/logger/SFLogger.hpp"
#ifdef _WIN32
#include <WS2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#define MOCK_GET_RESPONSE "GET /?token=Snowflake-token-12345 HTTP/1.1"

#define MOCK_POST_RESPONSE "POST token=Snowflake-token-12345"
#define MOCK_OPTIONS_RESPONSE "OPTIONS /api/data HTTP/1.1 \n \
Host: api.example.com \n \
Origin : https://client.example.com \n\
Access-Control-Request-Method : POST \n \
Access-Control-Request-Headers : Content - Type, Authorization \n" 

#define REF_PORT 12345
#define REF_SSO_URL "https://sso.com/"
#define REF_PROOF_KEY "MOCK_PROOF_KEY"
#define REF_SAML_TOKEN "MOCK_SAML_TOKEN"
#define CACHE_SERVER_URL_ENV "SF_OCSP_RESPONSE_CACHE_SERVER_URL"
#define RESPONSE "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!"

using namespace Snowflake::Client;

static bool endsWith(const std::string& a, const std::string& b) 
{
    if (b.size() > a.size()) return false;
    return std::equal(a.begin() + a.size() - b.size(), a.end(), b.begin());
}

class MockAuthWebServer : public AuthWebServer
{
public:
    MockAuthWebServer()
    {}

    virtual ~MockAuthWebServer()
    {}

    inline void start()
    {}

    inline void stop()
    {}

    inline int getPort()
    {
        return REF_PORT;
    }

    inline void startAccept()
    {}

    inline bool receive()
    {
        return false;
    }

    inline std::string getSAMLToken()
    {
        return std::string(REF_SAML_TOKEN);
    }

    inline bool isConsentCacheIdToken()
    {
        return true;
    }

    inline int getTimeout()
    {
        return m_timeout;
    }
};

class MockIDP : public IDPAuthenticator
{
public:
    MockIDP(SF_CONNECT* connection) : m_connection(connection)
    {
        m_account = m_connection->account;
        m_authenticator = m_connection->authenticator;
        m_user = m_connection->user;
        m_port = m_connection->port;
        m_host = m_connection->host;
        m_protocol = m_connection->protocol;
        m_retriedCount = 0;
        m_retryTimeout = get_retry_timeout(m_connection);
    }

    ~MockIDP()
    {

    }

    inline bool curlPostCall(SFURL& url, const jsonObject_t& obj, jsonObject_t& resp)
    {
        SF_UNUSED(url);
        SF_UNUSED(obj);

        bool ret = true;
        jsonObject_t data;
        data["tokenUrl"] = jsonValue_t("https://fake.okta.com/tokenurl");
        data["ssoUrl"] = jsonValue_t("https://fake.okta.com/ssourl");
        data["proofKey"] = jsonValue_t("MOCK_PROOF_KEY");
        resp["data"] = jsonValue_t(data);
        resp["sessionToken"] = jsonValue_t("onetimetoken");

        //The curlPostCall is called twice in authenticator 1. getIDPInfo 2. get onetime token
        //This code is to test the get onetime token failure

        return ret;
    }

    inline bool curlGetCall(SFURL& url, jsonObject_t& resp, bool parseJSON, std::string& rawData, bool& isRetry)
    {
        SF_UNUSED(url);
        SF_UNUSED(resp);
        SF_UNUSED(parseJSON);
        SF_UNUSED(rawData);
        SF_UNUSED(isRetry);
        return false;
    };

    std::string getTokenURL();
    std::string getSSOURL();

private:
    SF_CONNECT* m_connection;
};

void test_external_browser_initialize(void**)
{
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "test_account");
    snowflake_set_attribute(sf, SF_CON_USER, "test_user");
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "test_password");
    snowflake_set_attribute(sf, SF_CON_HOST, "wronghost.com");
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "externalbrowser");
    sf_bool disable_console_login = SF_BOOLEAN_TRUE;
    snowflake_set_attribute(sf, SF_CON_DISABLE_CONSOLE_LOGIN, &disable_console_login);
   
    _snowflake_check_connection_parameters(sf);
    auth_initialize(sf);
    IAuthenticator* auth = static_cast<IAuthenticator*>(sf->auth_object);
    std::string type = typeid(*auth).name();
    assert_true(type.find("AuthenticatorExternalBrowser") != std::string::npos);

    snowflake_term(sf);
}

void test_external_browser_authenticate(void**)
{
    class MockExternalBrowser : public AuthenticatorExternalBrowser
    {
    public:
        MockExternalBrowser(
            SF_CONNECT* connection, IAuthWebServer* authWebServer)
            : AuthenticatorExternalBrowser(connection, authWebServer)
        {
            m_idp = new MockIDP(connection);
        }

        ~MockExternalBrowser()
        {}

        inline void startWebBrowser(std::string ssoUrl)
        {
            SF_UNUSED(ssoUrl);
        }

        inline void cleanError()
        {
            m_errMsg = "";
        }

        bool m_errortesting = false;
    };

    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "test_account");
    snowflake_set_attribute(sf, SF_CON_USER, "test_user");
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "test_password");
    snowflake_set_attribute(sf, SF_CON_HOST, "wronghost.com");
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "externalbrowser");
    int64 timeout = 50;
    snowflake_set_attribute(sf, SF_CON_BROWSER_RESPONSE_TIMEOUT, &timeout);
    sf_bool disable_console_login = SF_BOOLEAN_TRUE;
    snowflake_set_attribute(sf, SF_CON_DISABLE_CONSOLE_LOGIN, &disable_console_login);

    IAuthWebServer* authWebServer = new MockAuthWebServer();
    MockExternalBrowser* authenticatorInstance = new MockExternalBrowser(sf, authWebServer);
    authenticatorInstance->authenticate();

    assert_true(authenticatorInstance->getPort() != 0);
    jsonObject_t dataMap;
    authenticatorInstance->updateDataMap(dataMap);

    assert_string_equal(dataMap["PROOF_KEY"].get<std::string>().c_str(), "MOCK_PROOF_KEY");
    assert_string_equal(dataMap["TOKEN"].get<std::string>().c_str(), "MOCK_SAML_TOKEN");
    assert_string_equal(dataMap["AUTHENTICATOR"].get<std::string>().c_str(), SF_AUTHENTICATOR_EXTERNAL_BROWSER);
    assert_int_equal(static_cast<MockAuthWebServer*>(authWebServer)->getTimeout(),50);

    delete authenticatorInstance;
        
    disable_console_login = SF_BOOLEAN_FALSE;
    snowflake_set_attribute(sf, SF_CON_DISABLE_CONSOLE_LOGIN, &disable_console_login);

    authWebServer = new MockAuthWebServer();
    authenticatorInstance = new MockExternalBrowser(sf, authWebServer);
    authenticatorInstance->authenticate();

    dataMap.clear();
    authenticatorInstance->updateDataMap(dataMap);

    assert_true(!dataMap["PROOF_KEY"].get<std::string>().empty());
    assert_string_equal(dataMap["TOKEN"].get<std::string>().c_str(), "MOCK_SAML_TOKEN");
    assert_string_equal(dataMap["AUTHENTICATOR"].get<std::string>().c_str(), SF_AUTHENTICATOR_EXTERNAL_BROWSER);

    delete authenticatorInstance;

    snowflake_term(sf);
}

void createMockClient(int port, const char* response) {
#ifdef _WIN32
    Sleep(2000);
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(std::chrono::milliseconds(2000)));
#endif

    int mock_client = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(mock_client, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Mock server connection failed\n";
        return;
    }

    send(mock_client, response, strlen(response), 0);
#ifndef _WIN32
     close(mock_client);
#else
     closesocket(mock_client);
#endif
}

class MockExternalBrowser : public AuthenticatorExternalBrowser
{
public:
    MockExternalBrowser(
        SF_CONNECT* connection, IAuthWebServer* authWebServer)
        : AuthenticatorExternalBrowser(connection, authWebServer)
    {
        m_idp = new MockIDP(connection);
    }

    ~MockExternalBrowser()
    {}


    inline void startWebBrowser(std::string ssoUrl)
    {
        SF_UNUSED(ssoUrl);
        std::thread mockServerThread(createMockClient, getPort(), m_response.c_str());
        mockServerThread.detach();
    }

    std::string m_response;
};

void test_auth_web_server_success(void**) 
{
    log_set_quiet(0);
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "test_account");
    snowflake_set_attribute(sf, SF_CON_USER, "test_user");
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "test_password");
    snowflake_set_attribute(sf, SF_CON_HOST, "wronghost.com");
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "externalbrowser");

    MockExternalBrowser* auth = new MockExternalBrowser(sf, nullptr);
    auth->m_response = MOCK_GET_RESPONSE;
    auth->authenticate();
    assert_int_equal(sf->error.error_code, SF_STATUS_SUCCESS);

    jsonObject_t dataMap;
    auth->updateDataMap(dataMap);

    assert_string_equal(dataMap["TOKEN"].get<std::string>().c_str(), "Snowflake-token-12345");
    assert_string_equal(dataMap["PROOF_KEY"].get<std::string>().c_str(), "MOCK_PROOF_KEY");
    assert_string_equal(dataMap["AUTHENTICATOR"].get<std::string>().c_str(), SF_AUTHENTICATOR_EXTERNAL_BROWSER);

    auth->m_response = MOCK_POST_RESPONSE;
    auth->authenticate();
    assert_int_equal(sf->error.error_code, SF_STATUS_SUCCESS);

    dataMap.clear();
    auth->updateDataMap(dataMap);
    assert_string_equal(dataMap["TOKEN"].get<std::string>().c_str(), "Snowflake-token-12345");
    assert_string_equal(dataMap["PROOF_KEY"].get<std::string>().c_str(), "MOCK_PROOF_KEY");
    assert_string_equal(dataMap["AUTHENTICATOR"].get<std::string>().c_str(), SF_AUTHENTICATOR_EXTERNAL_BROWSER);

    //This case only test when the request is OPTIONS. The mock client only works one time, so it should be failed.
    //auth->m_response = MOCK_OPTIONS_RESPONSE;
    //auth->authenticate();
    //assert_string_equal(sf->error.msg, "SFAuthWebBrowserFailed: Failed to receive SAML token. Could not receive a request.");
    delete auth;

    snowflake_term(sf);
    log_set_quiet(1);

}

void test_auth_web_server_fail(void**)
{
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "test_account");
    snowflake_set_attribute(sf, SF_CON_USER, "test_user");
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "test_password");
    snowflake_set_attribute(sf, SF_CON_HOST, "wronghost.com");
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "externalbrowser");

    MockExternalBrowser* auth = new MockExternalBrowser(sf, nullptr);
    auth->m_response = "wrong request";
    auth->authenticate();
    assert_string_equal(sf->error.msg, "SFAuthWebBrowserFailed: Not HTTP request.");
    delete auth;

    auth = new MockExternalBrowser(sf, nullptr);
    auth->m_response = "GET /?token=Snowflake-token-12345 HTTP/1.3";
    auth->authenticate();
    assert_string_equal(sf->error.msg, "AuthWebServer:parseAndRespondGetRequest:Not HTTP request.");
    delete auth;

    auth = new MockExternalBrowser(sf, nullptr);
    auth->m_response = "GET /!token=Snowflake-token-12345 HTTP/1.1";
    auth->authenticate();
    assert_string_equal(sf->error.msg, "AuthWebServer:parseAndRespondGetRequest:No token parameter is found.");
    delete auth;

    auth = new MockExternalBrowser(sf, nullptr);
    auth->m_response = "OPTIONS ";
    auth->authenticate();
    assert_string_equal(sf->error.msg, "AuthWebServer:parseAndRespondOptionsRequest:no Access-Control-Request-Headers or Origin header.");
    delete auth;

    auth = new MockExternalBrowser(sf, nullptr);
    auth->m_response = "OPTIONS / api / data HTTP / 1.1 \n Host: api.example.com \n Origin : https ://client.example.com \nAccess-Control-Request-Method : GET";
    auth->authenticate();
    assert_string_equal(sf->error.msg, "AuthWebServer:parseAndRespondOptionsRequest:POST method is not requested");
    delete auth;

    snowflake_term(sf);
}

void test_external_browser_error(void**)
{
    class MockExternalBrowser : public AuthenticatorExternalBrowser
    {
    public:
        MockExternalBrowser(
            SF_CONNECT* connection, IAuthWebServer* authWebServer)
            : AuthenticatorExternalBrowser(connection, authWebServer)
        {}

        ~MockExternalBrowser()
        {}

        inline void startWebBrowser(std::string ssoUrl)
        {
            SF_UNUSED(ssoUrl);
        }

        inline void getLoginUrl(std::map<std::string, std::string>& out, int port)
        {
            SF_UNUSED(port);

            if (m_errortesting)
            {
                m_errMsg = "getLoginUrl Error";
                return;
            }
            out[std::string("LOGIN_URL")] = std::string(REF_SSO_URL);
            out[std::string("PROOF_KEY")] = std::string(REF_PROOF_KEY);
        }

        inline void cleanError()
        {
            m_errMsg = "";
        }

        bool m_errortesting = false;
    };

    class FailMockAuthWebServer : public IAuthWebServer
    {
    public:
        FailMockAuthWebServer()
        {}

        virtual ~FailMockAuthWebServer()
        {}

        inline void start()
        {
            if (m_startError)
            {
                m_errMsg = "Start Error";
            }
        }

        inline void stop()
        {
            if (m_stopError)
            {
                m_errMsg = "Stop Error";
            }
        }

        inline int getPort()
        {
            return REF_PORT;
        }

        inline void startAccept()
        {
            if (m_startAcceptError)
            {
                m_errMsg = "StartAccept Error";
            }
        }

        inline bool receive()
        {
            return false;
        }

        inline std::string getSAMLToken()
        {
            return std::string(REF_SAML_TOKEN);
        }

        inline bool isConsentCacheIdToken()
        {
            return true;
        }

        inline void setTimeout(int timeout)
        {
            SF_UNUSED(timeout);
        }

        inline void cleanError()
        {
            m_errMsg = "";
        }

        bool m_startError = false;
        bool m_stopError = false;
        bool m_startAcceptError = false;
    };

    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "test_account");
    snowflake_set_attribute(sf, SF_CON_USER, "test_user");
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "test_password");
    snowflake_set_attribute(sf, SF_CON_HOST, "wronghost.com");
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "externalbrowser");
    SF_STATUS status = snowflake_connect(sf);
    assert_int_not_equal(status, SF_STATUS_SUCCESS);

    FailMockAuthWebServer* authWebServer = new FailMockAuthWebServer();
    MockExternalBrowser* authenticatorInstance = new MockExternalBrowser(
        sf, authWebServer);
    authWebServer->m_stopError = true;
    authenticatorInstance->authenticate();
    assert_string_equal(authWebServer->getErrorMessage(), "Stop Error");
    authWebServer->cleanError();
    
    authWebServer->m_startAcceptError = true;
    authenticatorInstance->authenticate();
    assert_string_equal(authWebServer->getErrorMessage(), "StartAccept Error");
    authWebServer->cleanError();

    authenticatorInstance->m_errortesting = true;
    authenticatorInstance->authenticate();
    assert_string_equal(authenticatorInstance->getErrorMessage(), "getLoginUrl Error");
    authenticatorInstance->cleanError();

    authWebServer->m_startError = true;
    authenticatorInstance->authenticate();
    assert_string_equal(authWebServer->getErrorMessage(), "Start Error");
    authWebServer->cleanError();

    delete authenticatorInstance;

    snowflake_term(sf);
}

void unit_authenticator_external_browser_privatelink(const std::string& topDomain = "com")
{
    class EnvVar
    {
    public:
        EnvVar() : exist_(false)
        {
            char* cache_server_url_env = getenv(CACHE_SERVER_URL_ENV);
            if (nullptr != cache_server_url_env) {
                original_value_ = std::string(cache_server_url_env);
                exist_ = true;
            }
        }
        ~EnvVar()
        {
            if (exist_) {
                sf_setenv(CACHE_SERVER_URL_ENV, original_value_.c_str());
            }
            else {
                sf_unsetenv(CACHE_SERVER_URL_ENV);
            }
        }

        static std::string get()
        {
            char* cache_server_url_env = getenv(CACHE_SERVER_URL_ENV);
            const std::string cache_server_url(cache_server_url_env ? cache_server_url_env : "");
            return std::string(cache_server_url_env ? cache_server_url_env : "");
        }

        static void validate(const std::string& topDomain = "com")
        {
            const std::string PRIVATELINK_URL_SUFFIX = std::string(".privatelink.snowflakecomputing.") + topDomain + "/ocsp_response_cache.json";
            const std::string cache_server_url = get();
            assert_true(endsWith(cache_server_url, PRIVATELINK_URL_SUFFIX));
        }

    private:
        std::string original_value_;
        bool exist_;

    };

    class MockAuthenticatorExternalBrowser : public AuthenticatorExternalBrowser
    {
    public:
        MockAuthenticatorExternalBrowser(
            SF_CONNECT* connection, IAuthWebServer* authWebServer, const std::string& topDomain = "com")
            : AuthenticatorExternalBrowser(connection, authWebServer), m_topDomain(topDomain)
        {}

        inline void startWebBrowser(std::string ssoUrl)
        {
            assert_string_equal(ssoUrl.c_str(), REF_SSO_URL);
        }

        inline void getLoginUrl(std::map<std::string, std::string>& out, int port)
        {
            assert_int_equal(port, REF_PORT);
            out[std::string("LOGIN_URL")] = std::string(REF_SSO_URL);
            out[std::string("PROOF_KEY")] = std::string(REF_PROOF_KEY);
        }

        inline void authenticate()
        {
            EnvVar::validate(m_topDomain);
            AuthenticatorExternalBrowser::authenticate();
        }

    private:
        std::string m_topDomain;
    };

    // Helper class to show/validate environment variable SF_OCSP_RESPONSE_CACHE_SERVER_URL
    // It also restore the original value after test.
    // note: function is not allowed in a function scope, but a class is.
    

    EnvVar original_env_value; // load now and restore when test ends

    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "test_account");
    snowflake_set_attribute(sf, SF_CON_USER, "test_user");
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "test_password");
    std::string host = "testaccount.privatelink.snowflakecomputing." + topDomain;
    snowflake_set_attribute(sf, SF_CON_HOST, host.c_str());
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "externalbrowser");
    sf_bool disable_console_login = SF_BOOLEAN_TRUE;
    snowflake_set_attribute(sf, SF_CON_DISABLE_CONSOLE_LOGIN, &disable_console_login);
    _snowflake_check_connection_parameters(sf);

    MockAuthWebServer* webSever = new MockAuthWebServer();
    sf->auth_object = static_cast<Snowflake::Client::IAuthenticator*>(
        new MockAuthenticatorExternalBrowser(sf, webSever, topDomain));
    jsonObject_t dataMap;

    static_cast<Snowflake::Client::IAuthenticator*>(sf->auth_object)->authenticate();

    snowflake_term(sf);
}

void test_unit_authenticator_external_browser_privatelink(void**)
{
    unit_authenticator_external_browser_privatelink();

}

void test_authenticator_external_browser_privatelink_with_china_domain(void**)
{
    unit_authenticator_external_browser_privatelink("cn");
}

int main(void) {
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_external_browser_initialize),
    cmocka_unit_test(test_external_browser_authenticate),
    cmocka_unit_test(test_external_browser_error),
    cmocka_unit_test(test_auth_web_server_success),
    cmocka_unit_test(test_auth_web_server_fail),
    cmocka_unit_test(test_unit_authenticator_external_browser_privatelink),
    cmocka_unit_test(test_authenticator_external_browser_privatelink_with_china_domain),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  return ret;
}
