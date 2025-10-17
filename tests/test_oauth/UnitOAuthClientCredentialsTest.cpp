#include "../cpp/lib/AuthenticatorOAuth.hpp"
#include "UnitOAuthBase.hpp"
#include "wiremock.hpp"
#include "../cpp/logger/SFLogger.hpp"
#include "test_setup.h"
#include "TestSetup.hpp"

using namespace std;

namespace {
    using namespace Snowflake::Client;
using UnitOAuthBase::initAuthChallengeTestProvider;

SF_CONNECT* createConnection() {
  return UnitOAuthBase::createConnection(AUTH_OAUTH_CLIENT_CREDENTIALS);
}
}

void test_successful_oauth_client_credentials_flow(void** unused)
{
    initAuthChallengeTestProvider();
    auto wiremock = WiremockRunner("wiremock/idp_responses/idp_client_successful.json",
        {
          "wiremock/snowflake_responses/snowflake_login_successful.json",
          "wiremock/snowflake_responses/snowflake_disconnect_successful.json"
        });
    SF_CONNECT* sf = createConnection();


    CXX_LOG_INFO("sf", "UnitOAuthClientCredentialsTest", "Connect", "Connecting to Snowflake");
    SF_STATUS status = snowflake_connect(sf);
    if (status != SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    };
    CXX_LOG_INFO("sf", "UnitOAuthClientCredentialsTest", "Connect", "Connected to Snowflake successfully");
    assert_false(std::string(sf->token).empty());
    assert_string_equal(sf->token, "token-t1");
}

void test_failure_oauth_client_credentials_flow(void** unused)
{
    initAuthChallengeTestProvider();
    auto wiremock = WiremockRunner("wiremock/idp_responses/idp_client_token_request_error.json", {});
    SF_CONNECT* sf = createConnection();

    CXX_LOG_INFO("sf", "UnitOAuthClientCredentialsTest", "Connect", "Connecting to Snowflake");
        SF_STATUS status = snowflake_connect(sf);
    if (status == SF_STATUS_SUCCESS) {
        dump_error(&(sf->error));
    };
}

void test_re_authentication_and_refresh_token_is_valid(void** unused)
{
    initAuthChallengeTestProvider();
    auto wiremock = WiremockRunner("wiremock/snowflake_responses/snowflake_login_failed.json", {
      "wiremock/idp_responses/idp_refresh_successful.json",
      "wiremock/snowflake_responses/snowflake_login_successful.json",
      "wiremock/snowflake_responses/snowflake_disconnect_successful.json",
        });
    CXX_LOG_INFO("sf", "UnitOAuthTest", "RefreshTokenAndConnect", "Prepare wiremock mapping");

    SF_CONNECT* sf = createConnection();
    snowflake_set_attribute(sf, SF_CON_OAUTH_TOKEN, "expired-access-token-123");
    sf->oauth_refresh_token = "refresh - token - 123";

    //  try {
    //    CXX_LOG_INFO("sf::UnitOAuthTest::RefreshTokenAndConnect::Connecting to Snowflake using cached access token")
    //    SF_STATUS status = snowflake_connect(sf);

    //  } catch (const ReauthenticationRequest& ex) {
    //    CXX_LOG_INFO("sf::UnitOAuthTest::RefreshTokenAndConnect::Connecting to Snowflake using cached access token")
    //    REQUIRE(ex.getCode() == Connection::S_OAUTH_ACCESS_TOKEN_EXPIRED_GS_CODE);
    //  }

    //  CXX_LOG_INFO("sf", "UnitOAuthTest", "RefreshTokenAndConnect", "Refresh token and connect to Snowflake")
    //  std::string sessionToken = connection.connect();
    //  CXX_LOG_INFO("sf", "UnitOAuthTest", "RefreshTokenAndConnect", "Connected to Snowflake successfully")
    //  REQUIRE(sessionToken == "token-t1");
    //  REQUIRE(connection.getOAuthAccessToken() == "access-token-123");
    //  REQUIRE(connection.getOAuthRefreshToken().empty());

    //  connection.disconnect();
    //} catch (const std::exception& e) {
    //  CXX_LOG_ERROR("sf", "UnitOAuthTest", "RefreshTokenAndConnect", "Exception: %s", e.what())
    //  REQUIRE(false);
    //}
}

void test_re_authentication_and_refresh_token_is_expired(void** unused)
{
    /* try {
       initAuthChallengeTestProvider();
       CXX_LOG_INFO("sf", "UnitOAuthTest", "ExpiredRefreshToken", "Prepare wiremock mapping");
       auto wiremock = WiremockRunner("wiremock/snowflake_responses/snowflake_login_failed.json",{
         "wiremock/idp_responses/idp_refresh_failed.json",
         "wiremock/idp_responses/idp_client_successful_after_refresh.json",
         "wiremock/snowflake_responses/snowflake_login_successful.json",
         "wiremock/snowflake_responses/snowflake_disconnect_successful.json",
       });
       Connection connection = createConnection();
       connection.resetOAuthAccessToken("expired-access-token-123");
       connection.resetOAuthRefreshToken("expired-refresh-token-123");
       CXX_LOG_INFO("sf", "UnitOAuthTest", "ExpiredRefreshToken", "Connecting to Snowflake using cached access token");
       try {
         connection.connect();
         REQUIRE(false);
       } catch (const ReauthenticationRequest& ex) {
         CXX_LOG_INFO("sf", "UnitOAuthTest", "ExpiredRefreshToken", "Failed to connect to Snowflake using cached access token")
         REQUIRE(ex.getCode() == Connection::S_OAUTH_ACCESS_TOKEN_EXPIRED_GS_CODE);
       }
       REQUIRE(connection.getOAuthAccessToken().empty());
       CXX_LOG_INFO("sf", "UnitOAuthTest", "ExpiredRefreshToken", "Trying to refresh, failing and obtaining new tokens using Client Credentials");
       std::string sessionToken = connection.connect();

       CXX_LOG_INFO("sf", "UnitOAuthTest", "ExpiredRefreshToken", "Connected to Snowflake successfully")
       REQUIRE(sessionToken == "token-t1");
       REQUIRE(connection.getOAuthAccessToken() == "access-token-123");
       REQUIRE(connection.getOAuthRefreshToken() == "refresh-token-123");

       connection.disconnect();
     } catch (const std::exception& e) {
       CXX_LOG_ERROR("sf", "UnitOAuthTest", "RefreshTokenAndConnect", "Exception: %s", e.what())
       REQUIRE(false);
     }*/
}

int main(void) {
    initialize_test(SF_BOOLEAN_FALSE);
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_successful_oauth_client_credentials_flow),
      cmocka_unit_test(test_failure_oauth_client_credentials_flow),
      cmocka_unit_test(test_re_authentication_and_refresh_token_is_valid),
      cmocka_unit_test(test_re_authentication_and_refresh_token_is_expired),
    };
    int ret = cmocka_run_group_tests(tests, NULL, NULL);
    return ret;
}

