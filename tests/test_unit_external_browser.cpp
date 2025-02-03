/*
 * Copyright (c) 2024 Snowflake Computing, Inc. All rights reserved.
 */

#include <string>
#include "snowflake/SFURL.hpp"
#include "../lib/connection.h"
#include "../lib/authenticator.h"
#include "../cpp/lib/Authenticator.hpp"
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"
#include "../cpp/logger/SFLogger.hpp"

#define REF_PORT 12345
#define REF_SSO_URL "https://sso.com/"
#define REF_PROOF_KEY "MOCK_PROOF_KEY"
#define REF_SAML_TOKEN "MOCK_SAML_TOKEN"

using namespace Snowflake::Client;

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

class MockAuthWebServer : public IAuthWebServer
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

    inline void setTimeout(int timeout)
    {
        SF_UNUSED(timeout);
    }

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

void test_external_browser(void**)
{
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "test_account");
    snowflake_set_attribute(sf, SF_CON_USER, "test_user");
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "test_password");
    snowflake_set_attribute(sf, SF_CON_HOST, "wronghost.com");
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "externalbrowser");

    IAuthWebServer* authWebServer = new MockAuthWebServer();
    MockExternalBrowser* authenticatorInstance = new MockExternalBrowser(
        sf, authWebServer);
    authenticatorInstance->authenticate();

    jsonObject_t dataMap;
    authenticatorInstance->updateDataMap(dataMap);

    assert_string_equal(dataMap["PROOF_KEY"].get<std::string>().c_str(), "MOCK_PROOF_KEY");
    assert_string_equal(dataMap["TOKEN"].get<std::string>().c_str(), "MOCK_SAML_TOKEN");
    assert_string_equal(dataMap["AUTHENTICATOR"].get<std::string>().c_str(), SF_AUTHENTICATOR_EXTERNAL_BROWSER);
    delete authenticatorInstance;

    snowflake_term(sf);
}

void test_external_browser_error(void**)
{
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "test_account");
    snowflake_set_attribute(sf, SF_CON_USER, "test_user");
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "test_password");
    snowflake_set_attribute(sf, SF_CON_HOST, "wronghost.com");
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "externalbrowser");

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

int main(void) {
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_external_browser),
    cmocka_unit_test(test_external_browser_error),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  return ret;
}
