#pragma once
#ifndef UNITOAUTHTEST_UNITOAUTHBASE_HPP
#define UNITOAUTHTEST_UNITOAUTHBASE_HPP

#include "../include/snowflake/client.h"
#include "authenticator.h"
#include <string>
#include "../cpp/lib/AuthenticatorOAuth.hpp"

namespace Snowflake::Client::UnitOAuthBase {

    SF_CONNECT* createConnection(
        AuthenticatorType flow,
        int browserResponseTimeout = SF_BROWSER_RESPONSE_TIMEOUT,
        std::string customAuthEndpoint = "",
        std::string customTokenEndpoint = "",
        std::string redirectUri = "",
        bool oauthSingleUseRefreshTokens = false);

    void initAuthChallengeTestProvider();

    void initAuthWebBrowserTestRunner();
}

#endif
