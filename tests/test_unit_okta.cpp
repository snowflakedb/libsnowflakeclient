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

using namespace Snowflake::Client;

class MockOkta : public AuthenticatorOKTA {

public:
    MockOkta(SF_CONNECT* connection) : AuthenticatorOKTA(connection), m_connection(connection)
    {};

    ~MockOkta() {};
    bool curlPostCall(SFURL& url, const jsonObject_t& obj, jsonObject_t& resp);
    bool curlGetCall(SFURL& url, jsonObject_t& resp, bool parseJSON, std::string& rawData, bool& isRetry);
    std::string getTokenURL();
    std::string getSSOURL();
    std::string MockOkta::getErrorMessage();

    SF_CONNECT* m_connection;
    bool isCurrentCallFailed = false;
    bool isPostCallFailed = false;
    bool isCurlGetRequestFailed = false;
};

bool MockOkta::curlGetCall(SFURL& url, jsonObject_t& resp, bool parseJSON, std::string& rawData, bool& isRetry) 
{
    rawData = "<form action=\"https&#x3a;&#x2f;&#x2f;host.com&#x2f;fed&#x2f;login/";
    if (isCurlGetRequestFailed) {
        m_errMsg = "SFConnectionFailed:curlGetCall";
    }
    return !isCurlGetRequestFailed;
}

bool MockOkta::curlPostCall(SFURL& url, const jsonObject_t& obj, jsonObject_t& resp)
{
    jsonObject_t data;
    data["tokenUrl"] = picojson::value("https://fake.okta.com/tokenurl");
    data["ssoUrl"] = picojson::value("https://fake.okta.com/ssourl");
    resp["data"] = picojson::value(data);
    resp["sessionToken"] = picojson::value("onetimetoken");
    if (isPostCallFailed) {
        m_errMsg = "SFConnectionFailed:curlPostCall";
    }

    //The curlPostCall is called twice in authenticator 1. getIDPInfo 2. get onetime token
    //This code is to test the get onetime token failure
    if (isCurrentCallFailed) {
        return false;
    }
    else if (isPostCallFailed) {
        isCurrentCallFailed = true;
        return true;
    }
}

std::string MockOkta::getTokenURL() {
    return tokenURLStr;
}

std::string MockOkta::getSSOURL() {
    return ssoURLStr;
}

std::string MockOkta::getErrorMessage() {
    return m_errMsg;
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

    MockOkta okta = MockOkta(sf);
    okta.getIDPInfo();
    assert_string_equal(okta.getTokenURL().c_str(), "https://fake.okta.com/tokenurl");
    assert_string_equal(okta.getSSOURL().c_str(), "https://fake.okta.com/ssourl");

    okta.isCurrentCallFailed = true;
    okta.getIDPInfo();
    assert_string_equal(okta.getErrorMessage().c_str(), "Fail to get authenticator info");

    snowflake_term(sf);
}

void test_okta_getAuthetnicate(void**)
{
    assert_int_equal(getAuthenticatorType("hello"), AUTH_OKTA);
    assert_int_equal(getAuthenticatorType("www.okta.com"), AUTH_OKTA);
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
    assert_true(okta.getErrorMessage() == "");

    jsonObject_t data;
    okta.updateDataMap(data);
    assert_string_equal(data["RAW_SAML_RESPONSE"].get<std::string>().c_str(), "<form action=\"https&#x3a;&#x2f;&#x2f;host.com&#x2f;fed&#x2f;login/");

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

    MockOkta okta = MockOkta(sf);
    okta.authenticate();
    assert_string_equal(okta.getErrorMessage().c_str(), "SFSamlResponseVerificationFailed");

    okta.isCurlGetRequestFailed = true;
    okta.authenticate();
    assert_string_equal(okta.getErrorMessage().c_str(), "SFConnectionFailed:curlGetCall");

    okta.isCurlGetRequestFailed = false;
    okta.isCurrentCallFailed = false;
    okta.isPostCallFailed = true;

    okta.authenticate();
    assert_string_equal(okta.getErrorMessage().c_str(), "SFConnectionFailed:curlPostCall");

    snowflake_set_attribute(sf, SF_CON_AUTHENTICATOR, "https://wrong.okta.com");
    okta = MockOkta(sf);
    okta.authenticate();
    assert_string_equal(okta.getErrorMessage().c_str(), "SFAuthenticatorVerificationFailed: ssoUrl or tokenUrl does not contains same prefix with the authenticator");

    snowflake_term(sf);
}

int main(void) {
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_idp_authenticator),
    cmocka_unit_test(test_okta_getAuthetnicate),
    cmocka_unit_test(test_okta_authenticator_succeed),
    cmocka_unit_test(test_okta_authenticator_fail),
  };
  int ret = cmocka_run_group_tests(tests, NULL, NULL);
  return ret;
}
