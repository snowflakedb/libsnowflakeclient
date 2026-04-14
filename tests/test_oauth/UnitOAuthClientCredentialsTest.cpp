#include "UnitOAuthBase.hpp"
#include "../wiremock/wiremock.hpp"
#include "../cpp/logger/SFLogger.hpp"
#include "test_setup.h"
#include "TestSetup.hpp"

using namespace std;
using namespace Snowflake::Client;
using UnitOAuthBase::initAuthChallengeTestProvider;

SF_CONNECT* createConnection() {
    return UnitOAuthBase::createConnection(AUTH_OAUTH_CLIENT_CREDENTIALS);
}

WiremockRunner* wiremock;

int setup(void **) {
  sf_bool disable_verify_peer = SF_BOOLEAN_TRUE;
  snowflake_global_set_attribute(SF_GLOBAL_DISABLE_VERIFY_PEER, &disable_verify_peer);

  initAuthChallengeTestProvider();
  wiremock = new WiremockRunner();

  return 0;
}

int teardown(void **) {
  if (wiremock) 
  {
    delete wiremock;
    wiremock = nullptr;
  }
  return 0;
}

void test_successful_oauth_client_credentials_flow(void** unused)
{
    SF_UNUSED(unused);
    wiremock->resetMapping();
    wiremock->initMappingFromMultiFile("./wiremock/idp_responses/idp_client_successful.json",
        {
          "./wiremock/snowflake_responses/snowflake_oauth_login_successful.json",
          "./wiremock/snowflake_responses/snowflake_disconnect_successful.json"
        });
    SF_CONNECT* sf = createConnection();
    CXX_LOG_INFO("sf::UnitOAuthClientCredentialsTest::Connect::Connecting to Snowflake");
    assert_true(snowflake_connect(sf) == SF_STATUS_SUCCESS);
    CXX_LOG_INFO("sf::UnitOAuthClientCredentialsTest::Connect::Connected to Snowflake successfully");
    assert_false(std::string(sf->token).empty());
    assert_string_equal(sf->token, "token-t1");

    assert_true(snowflake_term(sf) == SF_STATUS_SUCCESS);
}

void test_failure_oauth_client_credentials_flow(void** unused)
{
    SF_UNUSED(unused);
    wiremock->resetMapping();
    wiremock->initMappingFromFile("./wiremock/idp_responses/idp_client_token_request_error.json");
    SF_CONNECT* sf = createConnection();

    CXX_LOG_INFO("sf::UnitOAuthClientCredentialsTest::Connect::Connecting to Snowflake");
    SF_STATUS status = snowflake_connect(sf);
    assert_false(status == SF_STATUS_SUCCESS);
}

void test_re_authentication_and_refresh_token_is_valid(void** unused)
{
    SF_UNUSED(unused);
    wiremock->resetMapping();
    wiremock->initMappingFromMultiFile("./wiremock/snowflake_responses/snowflake_login_failed.json", {
      "./wiremock/idp_responses/idp_refresh_successful.json",
      "./wiremock/snowflake_responses/snowflake_oauth_login_successful.json",
      "./wiremock/snowflake_responses/snowflake_disconnect_successful.json",
        });
    CXX_LOG_INFO("sf::UnitOAuthTest::RefreshTokenAndConnect::Prepare wiremock mapping");

    SF_CONNECT* sf = createConnection();
    snowflake_set_attribute(sf, SF_CON_OAUTH_TOKEN, "expired-access-token-123");
    snowflake_set_attribute(sf, SF_CON_OAUTH_REFRESH_TOKEN, "refresh-token-123");

    CXX_LOG_INFO("sf::UnitOAuthTest::RefreshTokenAndConnect::Connecting to Snowflake using cached access token");
    assert_true(snowflake_connect(sf) == SF_STATUS_SUCCESS);
    assert_string_equal(sf->token, "token-t1");
    assert_string_equal(sf->oauth_token, "access-token-123");
    assert_true(std::string(sf->oauth_refresh_token).empty());

    assert_true(snowflake_term(sf) == SF_STATUS_SUCCESS);
}

void test_re_authentication_and_refresh_token_is_expired(void** unused)
{
    SF_UNUSED(unused);
    wiremock->resetMapping();
    wiremock->initMappingFromMultiFile("./wiremock/snowflake_responses/snowflake_login_failed.json", {
       "./wiremock/idp_responses/idp_refresh_failed.json",
       "./wiremock/idp_responses/idp_client_successful_after_refresh.json",
       "./wiremock/snowflake_responses/snowflake_oauth_login_successful.json",
       "./wiremock/snowflake_responses/snowflake_disconnect_successful.json",
        });

    SF_CONNECT* sf = createConnection();
    snowflake_set_attribute(sf, SF_CON_OAUTH_TOKEN, "expired-access-token-123");
    snowflake_set_attribute(sf, SF_CON_OAUTH_REFRESH_TOKEN, "expired-refresh-token-123");
    CXX_LOG_INFO("sf::UnitOAuthTest::ExpiredRefreshToken::Connecting to Snowflake using cached access token");
    assert_true(snowflake_connect(sf) == SF_STATUS_SUCCESS);

    assert_string_equal(sf->token, "token-t1");
    assert_string_equal(sf->oauth_token, "access-token-123");
    assert_string_equal(sf->oauth_refresh_token, "refresh-token-123");
    assert_true(snowflake_term(sf) == SF_STATUS_SUCCESS);
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_successful_oauth_client_credentials_flow),
      cmocka_unit_test(test_failure_oauth_client_credentials_flow),
      cmocka_unit_test(test_re_authentication_and_refresh_token_is_valid),
      cmocka_unit_test(test_re_authentication_and_refresh_token_is_expired),
    };
    int ret = cmocka_run_group_tests(tests, setup, teardown);
    return ret;
}
