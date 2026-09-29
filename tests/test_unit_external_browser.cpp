
#include <string>
#include <iostream>
#include <thread>
#include <cstring>
#include <atomic>
#include <memory>
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
#define MOCK_POST_RESPONSE "POST / HTTP/1.1\r\nOrigin: https://wronghost.com\r\nContent-Type: application/json\r\n\r\n{\"token\":\"Snowflake-token-12345\",\"consent\":true}"

#define REF_PORT 12345
#define REF_SSO_URL "https://sso.com/"
#define REF_PROOF_KEY "MOCK_PROOF_KEY"
#define REF_SAML_TOKEN "MOCK_SAML_TOKEN"
#define CACHE_SERVER_URL_ENV "SF_OCSP_RESPONSE_CACHE_SERVER_URL"

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

    inline std::string getToken()
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

class OriginTestAuthWebServer : public AuthWebServer
{
public:
    bool allows(const std::string& method, const std::string& request)
    {
        return requestOriginAllowed(method, request);
    }

    bool matches(const std::string& origin) const
    {
        return originMatchesExpected(origin);
    }

    static bool header(
        const std::string& request,
        const std::string& name,
        std::string& value)
    {
        return extractHeader(request, name, value);
    }

    static bool isPreflight(const std::string& request)
    {
        return isCorsPostPreflight(request);
    }

    const std::string& currentOrigin() const
    {
        return m_origin;
    }
};

class MockIDP : public IDPAuthenticator
{
public:
    MockIDP(SF_CONNECT* connection) : m_connection(connection)
    {
        m_account = m_connection->account;
        m_authenticator = m_connection->authenticator;
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
    snowflake_set_attribute(sf, SF_CON_HOST, "wronghost.com");
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_EXTERNAL_BROWSER);
    sf_bool disable_console_login = SF_BOOLEAN_TRUE;
    snowflake_set_attribute(sf, SF_CON_DISABLE_CONSOLE_LOGIN, &disable_console_login);
   
    _snowflake_check_connection_parameters(sf);
    auth_initialize(sf);
    IAuthenticator* auth = static_cast<IAuthenticator*>(sf->auth_object);
    std::string type = typeid(*auth).name();
    assert_true(type.find("AuthenticatorExternalBrowser") != std::string::npos);

    //If disable_console_login is false, the username is required.
    disable_console_login = SF_BOOLEAN_FALSE;
    snowflake_set_attribute(sf, SF_CON_DISABLE_CONSOLE_LOGIN, &disable_console_login);

    _snowflake_check_connection_parameters(sf);
    assert_int_equal(sf->error.error_code, SF_STATUS_ERROR_BAD_CONNECTION_PARAMS);
    snowflake_term(sf);
}

void test_external_browser_authenticate(void**)
{
    class MockExternalBrowser : public AuthenticatorExternalBrowser
    {
    public:
        MockExternalBrowser(
            SF_CONNECT* connection, IAuthWebServer* authWebServer)
            : AuthenticatorExternalBrowser(connection, authWebServer, new MockIDP(connection))
        {}

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
    snowflake_set_attribute(sf, SF_CON_HOST, "wronghost.com");
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_EXTERNAL_BROWSER);
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
    snowflake_set_attribute(sf, SF_CON_USER, "test_user");
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

void createMockClients(
    int port,
    std::vector<std::string> responses,
    int initialDelayMs,
    int requestDelayMs,
    std::shared_ptr<std::atomic<bool>> preflightAnswered)
{
#ifdef _WIN32
    Sleep(initialDelayMs);
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(initialDelayMs));
#endif
    for (const std::string& response : responses)
    {
        int mockClient = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in serverAddress = {};
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);
        if (connect(mockClient, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
        {
#ifndef _WIN32
            close(mockClient);
#else
            closesocket(mockClient);
#endif
            return;
        }
        send(mockClient, response.c_str(), response.size(), 0);
        if (preflightAnswered && response.find("OPTIONS ") == 0)
        {
            char preflightResponse[2048] = {};
            int received = (int)recv(
                mockClient,
                preflightResponse,
                sizeof(preflightResponse),
                0);
            if (received > 0 &&
                std::string(preflightResponse, (unsigned long)received).find(
                    "Access-Control-Allow-Origin: https://wronghost.com") !=
                    std::string::npos)
            {
                preflightAnswered->store(true);
            }
        }
#ifndef _WIN32
        close(mockClient);
#else
        closesocket(mockClient);
#endif
        std::this_thread::sleep_for(std::chrono::milliseconds(requestDelayMs));
    }
}

class MockExternalBrowser : public AuthenticatorExternalBrowser
{
public:
    MockExternalBrowser(
        SF_CONNECT* connection, IAuthWebServer* authWebServer)
        : AuthenticatorExternalBrowser(connection, authWebServer, new MockIDP(connection))
    {}

    ~MockExternalBrowser()
    {}


    inline void startWebBrowser(std::string ssoUrl)
    {
        SF_UNUSED(ssoUrl);
        if (m_responses.empty())
        {
            std::thread(createMockClient, getPort(), m_response.c_str()).detach();
        }
        else
        {
            std::thread(
                createMockClients,
                getPort(),
                m_responses,
                m_initial_delay_ms,
                m_request_delay_ms,
                m_preflight_answered).detach();
        }
    }

    std::string m_response;
    std::vector<std::string> m_responses;
    int m_initial_delay_ms = 2000;
    int m_request_delay_ms = 100;
    std::shared_ptr<std::atomic<bool>> m_preflight_answered;
};

SF_CONNECT* createExternalBrowserConnection()
{
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "test_account");
    snowflake_set_attribute(sf, SF_CON_HOST, "wronghost.com");
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_EXTERNAL_BROWSER);
    return sf;
}

void test_auth_web_server_success(void**) 
{
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "test_account");
    snowflake_set_attribute(sf, SF_CON_HOST, "wronghost.com");
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_EXTERNAL_BROWSER);

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

    auth->m_response = "GET /?token=null-origin-token HTTP/1.1\r\nOrigin: null\r\n\r\n";
    auth->authenticate();
    dataMap.clear();
    auth->updateDataMap(dataMap);
    assert_string_equal(dataMap["TOKEN"].get<std::string>().c_str(), "null-origin-token");
    
    delete auth;
    snowflake_term(sf);
}

void test_auth_web_server_fail(void**)
{
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "test_account");
    snowflake_set_attribute(sf, SF_CON_HOST, "wronghost.com");
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_EXTERNAL_BROWSER);

    MockExternalBrowser* auth = new MockExternalBrowser(sf, nullptr);
    auth->m_response = "GET /?token=Snowflake-token-12345 HTTP/1.3";
    auth->authenticate();
    assert_string_equal(sf->error.msg, "sf::AuthWebServer::parseAndRespondGetRequest::Not HTTP request");
    delete auth;

    snowflake_term(sf);
}

void test_auth_web_server_origin_matching(void**)
{
    OriginTestAuthWebServer server;
    server.setExpectedOrigin(SFURL::parse("https://Account.snowflakecomputing.com:443"));

    assert_true(server.matches("https://account.snowflakecomputing.com"));
    assert_true(server.matches("HTTPS://ACCOUNT.SNOWFLAKECOMPUTING.COM:443"));
    assert_false(server.matches("http://account.snowflakecomputing.com"));
    assert_false(server.matches("https://other.snowflakecomputing.com"));
    assert_false(server.matches("https://account.snowflakecomputing.com:8443"));
    assert_false(server.matches("https://account.snowflakecomputing.com.example.com"));

    server.setExpectedOrigin(SFURL::parse("http://account.snowflakecomputing.com:80"));
    assert_true(server.matches("http://account.snowflakecomputing.com"));
}

void test_auth_web_server_origin_serialization(void**)
{
    OriginTestAuthWebServer server;
    server.setExpectedOrigin(SFURL::parse("https://account.snowflakecomputing.com:443"));

    // Origin serialization carries the scheme, host and port only.
    assert_true(server.matches("https://account.snowflakecomputing.com/"));
    assert_false(server.matches("https://user@account.snowflakecomputing.com"));
    assert_false(server.matches("https://account.snowflakecomputing.com@other.example.com"));
    assert_false(server.matches("https://account.snowflakecomputing.com/callback"));
    assert_false(server.matches("https://account.snowflakecomputing.com?token=value"));
    assert_false(server.matches("https://account.snowflakecomputing.com/?token=value"));
    assert_false(server.matches("https://account.snowflakecomputing.com#fragment"));
    assert_false(server.matches("https://account.snowflakecomputing.com:443/x?y#z"));

    assert_false(server.matches("ftp://account.snowflakecomputing.com"));
    assert_false(server.matches("file://account.snowflakecomputing.com"));
    assert_false(server.matches("account.snowflakecomputing.com"));
    assert_false(server.matches(""));
    assert_false(server.matches("https://"));
    assert_false(server.matches("https://account.snowflakecomputing.com:"));
    assert_false(server.matches("https://account.snowflakecomputing.com:44a"));
}

void test_auth_web_server_origin_headers(void**)
{
    OriginTestAuthWebServer server;
    server.setExpectedOrigin(SFURL::parse("https://account.snowflakecomputing.com"));

    std::string origin;
    std::string crlfRequest =
        "POST / HTTP/1.1\r\nContent-Type: text/plain\r\n\r\n"
        "token=value\r\nOrigin: https://account.snowflakecomputing.com";
    assert_false(OriginTestAuthWebServer::header(crlfRequest, "Origin", origin));
    assert_false(server.allows("POST", crlfRequest));

    std::string lfRequest =
        "POST / HTTP/1.1\nContent-Type: text/plain\n\n"
        "token=value\nOrigin: https://account.snowflakecomputing.com";
    assert_false(OriginTestAuthWebServer::header(lfRequest, "Origin", origin));
    assert_false(server.allows("POST", lfRequest));

    assert_true(server.allows("GET", "GET /?token=value HTTP/1.1\r\n\r\n"));
    assert_true(server.allows(
        "GET",
        "GET /?token=value HTTP/1.1\r\nOrigin: null\r\n\r\n"));
    assert_false(server.allows(
        "GET",
        "GET /?token=value HTTP/1.1\r\nOrigin: https://other.example.com\r\n\r\n"));
    assert_false(server.allows("HEAD", "HEAD / HTTP/1.1\r\n\r\n"));
    assert_false(server.allows(
        "PATCH",
        "PATCH / HTTP/1.1\r\nOrigin: https://account.snowflakecomputing.com\r\n\r\n"));
}

void test_auth_web_server_preflight(void**)
{
    OriginTestAuthWebServer server;
    server.setExpectedOrigin(SFURL::parse("https://account.snowflakecomputing.com"));
    std::string request =
        "OPTIONS / HTTP/1.1\r\n"
        "Origin: https://account.snowflakecomputing.com\r\n"
        "Access-Control-Request-Method: post\r\n"
        "Access-Control-Request-Headers: content-type\r\n\r\n";
    assert_true(server.allows("OPTIONS", request));
    assert_true(OriginTestAuthWebServer::isPreflight(request));

    std::string foreignOrigin = request;
    size_t origin = foreignOrigin.find("account.snowflakecomputing.com");
    foreignOrigin.replace(origin, strlen("account.snowflakecomputing.com"), "other.example.com");
    assert_false(server.allows("OPTIONS", foreignOrigin));

    std::string getMethod = request;
    size_t method = getMethod.find("post");
    getMethod.replace(method, strlen("post"), "GET");
    assert_false(OriginTestAuthWebServer::isPreflight(getMethod));

    std::string extraHeader = request;
    size_t header = extraHeader.find("content-type");
    extraHeader.replace(header, strlen("content-type"), "content-type, x-custom");
    assert_false(OriginTestAuthWebServer::isPreflight(extraHeader));

    std::string emptyTokens = request;
    header = emptyTokens.find("content-type");
    emptyTokens.replace(header, strlen("content-type"), ", content-type, ,");
    assert_true(OriginTestAuthWebServer::isPreflight(emptyTokens));

    std::string omittedHeaders =
        "OPTIONS / HTTP/1.1\r\n"
        "Origin: https://account.snowflakecomputing.com\r\n"
        "Access-Control-Request-Method: POST\r\n\r\n";
    assert_true(OriginTestAuthWebServer::isPreflight(omittedHeaders));
}

void test_auth_web_server_waits_for_valid_callback(void**)
{
    SF_CONNECT* sf = createExternalBrowserConnection();

    MockExternalBrowser* auth = new MockExternalBrowser(sf, nullptr);
    auth->m_responses = {
        "OPTIONS / HTTP/1.1\r\n"
        "Origin: https://wronghost.com\r\n"
        "Access-Control-Request-Method: post\r\n"
        "Access-Control-Request-Headers: content-type\r\n\r\n",
        "POST / HTTP/1.1\r\nContent-Type: text/plain\r\n\r\n"
        "token=body-origin-token\r\nOrigin: https://wronghost.com",
        "GET /?token=foreign-origin-token HTTP/1.1\r\n"
        "Origin: https://other.example.com\r\n\r\n",
        "GET /?token=valid-token HTTP/1.1\r\n\r\n"
    };
    auth->authenticate();

    jsonObject_t dataMap;
    auth->updateDataMap(dataMap);
    assert_string_equal(dataMap["TOKEN"].get<std::string>().c_str(), "valid-token");

    delete auth;
    snowflake_term(sf);
}

void test_auth_web_server_preflight_without_requested_headers(void**)
{
    SF_CONNECT* sf = createExternalBrowserConnection();
    MockExternalBrowser* auth = new MockExternalBrowser(sf, nullptr);
    auth->m_preflight_answered.reset(new std::atomic<bool>(false));
    auth->m_responses = {
        "OPTIONS / HTTP/1.1\r\n"
        "Origin: https://wronghost.com\r\n"
        "Access-Control-Request-Method: POST\r\n\r\n",
        "POST / HTTP/1.1\r\n"
        "Origin: https://wronghost.com\r\n"
        "Content-Type: application/json\r\n\r\n"
        "{\"token\":\"omitted-headers-token\",\"consent\":true}"
    };
    auth->authenticate();

    jsonObject_t dataMap;
    auth->updateDataMap(dataMap);
    assert_string_equal(
        dataMap["TOKEN"].get<std::string>().c_str(),
        "omitted-headers-token");
    assert_true(auth->m_preflight_answered->load());

    delete auth;
    snowflake_term(sf);
}

void test_auth_web_server_foreign_post_then_valid_callback(void**)
{
    SF_CONNECT* sf = createExternalBrowserConnection();
    MockExternalBrowser* auth = new MockExternalBrowser(sf, nullptr);
    auth->m_responses = {
        "POST / HTTP/1.1\r\n"
        "Origin: https://other.example.com\r\n"
        "Content-Type: application/json\r\n\r\n"
        "{\"token\":\"foreign-token\",\"consent\":true}",
        "POST / HTTP/1.1\r\n"
        "Origin: https://wronghost.com\r\n"
        "Content-Type: application/json\r\n\r\n"
        "{\"token\":\"valid-post-token\",\"consent\":true}"
    };
    auth->authenticate();

    jsonObject_t dataMap;
    auth->updateDataMap(dataMap);
    assert_string_equal(
        dataMap["TOKEN"].get<std::string>().c_str(),
        "valid-post-token");

    delete auth;
    snowflake_term(sf);
}

void test_auth_web_server_preflight_origin_not_reused(void**)
{
    SF_CONNECT* sf = createExternalBrowserConnection();
    OriginTestAuthWebServer* server = new OriginTestAuthWebServer();
    MockExternalBrowser* auth = new MockExternalBrowser(sf, server);
    auth->m_responses = {
        "OPTIONS / HTTP/1.1\r\n"
        "Origin: https://wronghost.com\r\n"
        "Access-Control-Request-Method: POST\r\n"
        "Access-Control-Request-Headers: Content-Type\r\n\r\n",
        "GET /?token=originless-get-token HTTP/1.1\r\n\r\n"
    };
    auth->authenticate();

    jsonObject_t dataMap;
    auth->updateDataMap(dataMap);
    assert_string_equal(
        dataMap["TOKEN"].get<std::string>().c_str(),
        "originless-get-token");
    assert_true(server->currentOrigin().empty());

    delete auth;
    snowflake_term(sf);
}

void test_auth_web_server_keeps_listening_for_non_token_requests(void**)
{
    SF_CONNECT* sf = createExternalBrowserConnection();
    MockExternalBrowser* auth = new MockExternalBrowser(sf, nullptr);
    auth->m_responses = {
        "HEAD / HTTP/1.1\r\n\r\n",
        "PATCH / HTTP/1.1\r\nOrigin: https://wronghost.com\r\n\r\n",
        "DELETE / HTTP/1.1\r\n\r\n",
        "GET /favicon.ico HTTP/1.1\r\n\r\n",
        "GET / HTTP/1.1\r\n\r\n",
        "POST / HTTP/1.1\r\n"
        "Origin: https://wronghost.com\r\n"
        "Content-Type: application/json\r\n\r\n{}",
        "GET /?token=kept-listening-token HTTP/1.1\r\n\r\n"
    };
    auth->authenticate();

    jsonObject_t dataMap;
    auth->updateDataMap(dataMap);
    assert_string_equal(
        dataMap["TOKEN"].get<std::string>().c_str(),
        "kept-listening-token");

    delete auth;
    snowflake_term(sf);
}

void test_auth_web_server_uses_overall_timeout(void**)
{
    SF_CONNECT* sf = createExternalBrowserConnection();
    int64 timeout = 1;
    snowflake_set_attribute(sf, SF_CON_BROWSER_RESPONSE_TIMEOUT, &timeout);

    MockExternalBrowser* auth = new MockExternalBrowser(sf, nullptr);
    auth->m_initial_delay_ms = 100;
    auth->m_request_delay_ms = 600;
    auth->m_responses = {
        "GET /?token=first HTTP/1.1\r\n"
        "Origin: https://other.example.com\r\n\r\n",
        "GET /?token=second HTTP/1.1\r\n"
        "Origin: https://other.example.com\r\n\r\n"
    };

    std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
    auth->authenticate();
    int64 elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();

    assert_true(elapsedMs >= 800);
    assert_true(elapsedMs < 1600);
    assert_non_null(strstr(sf->error.msg, "Auth browser timed out"));

    delete auth;
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
    std::string host = "testaccount.privatelink.snowflakecomputing." + topDomain;
    snowflake_set_attribute(sf, SF_CON_HOST, host.c_str());
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_EXTERNAL_BROWSER);
    sf_bool disable_console_login = SF_BOOLEAN_TRUE;
    snowflake_set_attribute(sf, SF_CON_DISABLE_CONSOLE_LOGIN, &disable_console_login);
    snowflake_global_set_attribute(SF_GLOBAL_OCSP_CHECK, &SF_BOOLEAN_TRUE);
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

void test_sso_token_cache(void**)
{
    class MockExternalBrowser : public AuthenticatorExternalBrowser
    {
    public:
        MockExternalBrowser(
            SF_CONNECT* connection, IAuthWebServer* authWebServer)
            : AuthenticatorExternalBrowser(connection, authWebServer, new MockIDP(connection))
        {
            m_proofKey = "RENEW_PROOF_KEY";
            m_token = "RENEW_SAML_TOKEN";
        }

        ~MockExternalBrowser()
        {}

        inline void authenticate()
        {
            if (!isRenew) {
                assert_true(SF_BOOLEAN_FALSE);
            }
        }

        bool isRenew = false;
    };

    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "test_account");
    snowflake_set_attribute(sf, SF_CON_USER, "test_user");
    snowflake_set_attribute(sf, SF_CON_HOST, "host");
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "test");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, SF_AUTHENTICATOR_EXTERNAL_BROWSER);
    sf_bool client_store_temporary_credential = SF_BOOLEAN_TRUE;
    snowflake_set_attribute(sf, SF_CON_CLIENT_STORE_TEMPORARY_CREDENTIAL, &client_store_temporary_credential);

    sf->token_cache = secure_storage_init();
    char* original_token = secure_storage_get_credential(sf->token_cache, sf->host, sf->user, ID_TOKEN);
    if (original_token != NULL)
    {
        secure_storage_remove_credential(sf->token_cache, sf->host, sf->user, ID_TOKEN);
    }
    secure_storage_save_credential(sf->token_cache, sf->host, sf->user, ID_TOKEN, "mock_sso_token");
    sf->sso_token = secure_storage_get_credential(sf->token_cache, sf->host, sf->user, ID_TOKEN);

    if (!sf->sso_token) {
        CXX_LOG_DEBUG("Failed to read the sso token from storage. For testing, add sso token directly.")
        sf->sso_token = (char*)malloc(strlen("mock_sso_token") + 1);
        strcpy(sf->sso_token, "mock_sso_token");
    }

    IAuthWebServer* webserver = new MockAuthWebServer();
    MockExternalBrowser* auth = new MockExternalBrowser(sf, webserver);
    sf->auth_object = static_cast<Snowflake::Client::IAuthenticator*>(auth);

    cJSON* body = NULL;
    body = create_auth_json_body(
        sf,
        sf->application,
        sf->application_name,
        sf->application_version,
        sf->timezone,
        sf->autocommit);

    cJSON* data = snowflake_cJSON_GetObjectItem(body, "data");

    assert_string_equal(snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(data, "AUTHENTICATOR")), SF_AUTHENTICATOR_ID_TOKEN);
    assert_string_equal(snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(data, "TOKEN")), "mock_sso_token");

    auth->isRenew = true;
    auth_renew_json_body(sf, body);
    data = snowflake_cJSON_GetObjectItem(body, "data");
    assert_string_equal(snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(data, "AUTHENTICATOR")), SF_AUTHENTICATOR_EXTERNAL_BROWSER);
    assert_string_equal(snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(data, "TOKEN")), "RENEW_SAML_TOKEN");
    assert_string_equal(snowflake_cJSON_GetStringValue(snowflake_cJSON_GetObjectItem(data, "PROOF_KEY")), "RENEW_PROOF_KEY");

    secure_storage_remove_credential(sf->token_cache, sf->host, sf->user, ID_TOKEN);
    if (original_token != NULL) 
    {
        secure_storage_save_credential(sf->token_cache, sf->host, sf->user, ID_TOKEN, original_token);
    }
    
    snowflake_cJSON_Delete(body);
    snowflake_term(sf);
}

int main(void) {
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_external_browser_initialize),
    cmocka_unit_test(test_external_browser_authenticate),
    cmocka_unit_test(test_auth_web_server_success),
    cmocka_unit_test(test_auth_web_server_fail),
    cmocka_unit_test(test_auth_web_server_origin_matching),
    cmocka_unit_test(test_auth_web_server_origin_serialization),
    cmocka_unit_test(test_auth_web_server_origin_headers),
    cmocka_unit_test(test_auth_web_server_preflight),
    cmocka_unit_test(test_auth_web_server_waits_for_valid_callback),
    cmocka_unit_test(test_auth_web_server_preflight_without_requested_headers),
    cmocka_unit_test(test_auth_web_server_foreign_post_then_valid_callback),
    cmocka_unit_test(test_auth_web_server_preflight_origin_not_reused),
    cmocka_unit_test(test_auth_web_server_keeps_listening_for_non_token_requests),
    cmocka_unit_test(test_auth_web_server_uses_overall_timeout),
    cmocka_unit_test(test_unit_authenticator_external_browser_privatelink),
    cmocka_unit_test(test_authenticator_external_browser_privatelink_with_china_domain),
    cmocka_unit_test(test_sso_token_cache),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  return ret;
}
