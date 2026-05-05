#include <string>
#include "snowflake/SFURL.hpp"
#include "../lib/connection.h"
#include "../lib/authenticator.h"
#include "../cpp/lib/Authenticator.hpp"
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"

using namespace Snowflake::Client;

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
    bool curlPostCall(SFURL& url, const jsonObject_t& obj, jsonObject_t& resp);
    bool curlGetCall(SFURL& url, jsonObject_t& resp, bool parseJSON, std::string& rawData, bool& isRetry);

    std::string getTokenURL();
    std::string getSSOURL();

    bool isCurrentCallFailed = false;
    bool isPostCallFailed = false;
    bool isCurlGetRequestFailed = false;

private:
    SF_CONNECT* m_connection;
};

bool MockIDP::curlGetCall(SFURL& url, jsonObject_t& resp, bool parseJSON, std::string& rawData, bool& isRetry)
{
    SF_UNUSED(url);
    SF_UNUSED(parseJSON);
    SF_UNUSED(isRetry);
    SF_UNUSED(resp);

    rawData = "<form action=\"https&#x3a;&#x2f;&#x2f;host.com&#x2f;fed&#x2f;login/";
    if (isCurlGetRequestFailed) {
        m_errMsg = "SFConnectionFailed:curlGetCall.";
    }
    return !isCurlGetRequestFailed;
}

bool MockIDP::curlPostCall(SFURL& url, const jsonObject_t& obj, jsonObject_t& resp)
{
    SF_UNUSED(url);
    SF_UNUSED(obj);

    bool ret = true;
    jsonObject_t data;
    data["tokenUrl"] = jsonValue_t("https://fake.okta.com/tokenurl");
    data["ssoUrl"] = jsonValue_t("https://fake.okta.com/ssourl");
    data["proofKey"] = jsonValue_t("proofKey");
    resp["data"] = jsonValue_t(data);
    resp["sessionToken"] = jsonValue_t("onetimetoken");
    if (isPostCallFailed) {
        m_errMsg = "SFConnectionFailed:curlPostCall.";
    }

    //The curlPostCall is called twice in authenticator 1. getIDPInfo 2. get onetime token
    //This code is to test the get onetime token failure
    if (isCurrentCallFailed) {
        ret = false;
    }
    else if (isPostCallFailed) {
        isCurrentCallFailed = true;
        ret = true;
    }
    return ret;
}

std::string MockIDP::getTokenURL() {
    return tokenURLStr;
}

std::string MockIDP::getSSOURL() {
    return ssoURLStr;
}

class MockOkta : public IAuthenticatorOKTA {

public:
    MockOkta(SF_CONNECT* connection) : IAuthenticatorOKTA(new MockIDP(connection)), m_connection(connection)
    {
        m_disableSamlUrlCheck = m_connection->disable_saml_url_check;
    };
    
    bool getCurrentCallFailed();
    bool getPostCallFailed();
    bool getCurlGetRequestFailed();

    void setCurrentCallFailed(bool value);
    void setPostCallFailed(bool value);
    void setCurlGetRequestFailed(bool value);

private:
    SF_CONNECT* m_connection;

};

class MockOkta2 : public AuthenticatorOKTA {

public:
    MockOkta2(SF_CONNECT* connection) : AuthenticatorOKTA(connection)
    {
        m_samlResponse = "MOCK SAML_RESPONSE";
    };

    ~MockOkta2()
    {
    };
};

bool MockOkta::getCurrentCallFailed() 
{
    if (auto idp = dynamic_cast<MockIDP*>(m_idp.get())) {
        return idp->isCurrentCallFailed;
    }
    return false;
}

bool MockOkta::getPostCallFailed()
{
    if (auto idp = dynamic_cast<MockIDP*>(m_idp.get())) {
        return idp->isPostCallFailed;
    }
    return false;
}

bool MockOkta::getCurlGetRequestFailed()
{
    if (auto idp = dynamic_cast<MockIDP*>(m_idp.get())) {
        return idp->isCurlGetRequestFailed;
    }
    return false;
}

void MockOkta::setCurrentCallFailed(bool value)
{
    if (auto idp = dynamic_cast<MockIDP*>(m_idp.get())) {
        idp->isCurrentCallFailed = value;
    }
}

void MockOkta::setPostCallFailed(bool value)
{
    if (auto idp = dynamic_cast<MockIDP*>(m_idp.get())) {
        idp->isPostCallFailed = value;
    }
}

void MockOkta::setCurlGetRequestFailed(bool value)
{
    if (auto idp = dynamic_cast<MockIDP*>(m_idp.get())) {
        idp->isCurlGetRequestFailed = value;
    }
}


void test_idp_authenticator(void**)
{
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "test_account");
    snowflake_set_attribute(sf, SF_CON_USER, "test_user");
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "test_password");
    snowflake_set_attribute(sf, SF_CON_HOST, "host.com");
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "https://fake.okta.com");

    jsonObject_t dataMap;
    MockIDP idp = MockIDP(sf);
    idp.getIDPInfo(dataMap);
    assert_string_equal(idp.getTokenURL().c_str(), "https://fake.okta.com/tokenurl");
    assert_string_equal(idp.getSSOURL().c_str(), "https://fake.okta.com/ssourl");

    idp.isCurrentCallFailed = true;
    idp.getIDPInfo(dataMap);
    assert_string_equal(idp.getErrorMessage(), "Fail to get authenticator info.");

    snowflake_term(sf);
}

void test_okta_getAuthenticator(void**)
{
    assert_int_equal(getAuthenticatorType("hello"), AUTH_OKTA);
    assert_int_equal(getAuthenticatorType("www.okta.com"), AUTH_OKTA);
}

void test_okta_initializie_and_terminatie(void**)
{
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "test_account");
    snowflake_set_attribute(sf, SF_CON_USER, "test_user");
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "test_password");
    snowflake_set_attribute(sf, SF_CON_HOST, "host.com");
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "https://fake.okta.com");

    auth_initialize(sf);
    IAuthenticator* auth = static_cast<IAuthenticator*>(sf->auth_object);
    std::string type = typeid(*auth).name();
    assert_true(type.find("AuthenticatorOKTA") != std::string::npos);

    auth_terminate(sf);
    assert_true(sf->auth_object == nullptr);
}

void test_okta_authenticator_succeed(void**)
{
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "test_account");
    snowflake_set_attribute(sf, SF_CON_USER, "test_user");
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "test_password");
    snowflake_set_attribute(sf, SF_CON_HOST, "host.com");
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "https://fake.okta.com");

    MockOkta okta = MockOkta(sf);
    okta.authenticate();
    assert_true(!okta.isError());

    jsonObject_t data;
    okta.updateDataMap(data);
    assert_string_equal(data["RAW_SAML_RESPONSE"].get<std::string>().c_str(), "<form action=\"https&#x3a;&#x2f;&#x2f;host.com&#x2f;fed&#x2f;login/");

    data.clear();
    MockOkta2 okta2 = MockOkta2(sf);
    okta2.updateDataMap(data);
    assert_string_equal(data["RAW_SAML_RESPONSE"].get<std::string>().c_str(), "MOCK SAML_RESPONSE");
    snowflake_term(sf);
}

void test_okta_authenticator_fail(void**)
{
    SF_CONNECT* sf = snowflake_init();
    snowflake_set_attribute(sf, SF_CON_ACCOUNT, "test_account");
    snowflake_set_attribute(sf, SF_CON_USER, "test_user");
    snowflake_set_attribute(sf, SF_CON_PASSWORD, "test_password");
    snowflake_set_attribute(sf, SF_CON_HOST, "wronghost.com");
    snowflake_set_attribute(sf, SF_CON_PORT, "443");
    snowflake_set_attribute(sf, SF_CON_PROTOCOL, "https");
    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "https://fake.okta.com");
    sf_bool disable_saml_url_check = SF_BOOLEAN_FALSE;
    snowflake_set_attribute(sf, SF_CON_DISABLE_SAML_URL_CHECK, &disable_saml_url_check);

    auth_initialize(sf);
    auth_authenticate(sf);
    assert_int_not_equal(sf->error.error_code, SF_STATUS_SUCCESS);

    AuthenticatorOKTA* auth = new AuthenticatorOKTA(sf);
    SFURL url;
    jsonObject_t body;
    bool isRetry = false;
    std::string rawData;
    auth->m_idp->curlGetCall(url, body, false, rawData, isRetry);
    assert_int_equal(isRetry, SF_BOOLEAN_TRUE);
    delete auth;

    MockOkta okta = MockOkta(sf);
    okta.authenticate();
    assert_string_equal(okta.getErrorMessage(), "SFSamlResponseVerificationFailed.");

    okta.setCurlGetRequestFailed(true);
    okta.authenticate();
    assert_string_equal(okta.m_idp->getErrorMessage(), "SFConnectionFailed:curlGetCall.");

    okta.setCurlGetRequestFailed(false);
    okta.setCurrentCallFailed(false);
    okta.setPostCallFailed(true);

    okta.authenticate();
    assert_string_equal(okta.m_idp->getErrorMessage(), "SFConnectionFailed:curlPostCall.");

    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "https://wrong.okta.com");
    MockOkta okta2 = MockOkta(sf);
    okta2.authenticate();
    assert_string_equal(okta2.getErrorMessage(), "SFAuthenticatorVerificationFailed: ssoUrl or tokenUrl does not contains same prefix with the authenticator.");

    snowflake_term(sf);
}

int main(void) {
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_idp_authenticator),
    cmocka_unit_test(test_okta_getAuthenticator),
    cmocka_unit_test(test_okta_initializie_and_terminatie),
    cmocka_unit_test(test_okta_authenticator_succeed),
    cmocka_unit_test(test_okta_authenticator_fail),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  return ret;
}
