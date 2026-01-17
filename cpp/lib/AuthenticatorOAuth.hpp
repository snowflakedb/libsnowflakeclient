#pragma once
#ifndef SNOWFLAKE_ODBC_AUTHENTICATOROAUTH_HPP
#define SNOWFLAKE_ODBC_AUTHENTICATOROAUTH_HPP

#include <future>
#include "snowflake/client.h"
#include "snowflake/SFURL.hpp"
#include "../include/snowflake/IAuth.hpp"

#include "Authenticator.hpp"
#include "AuthenticationChallengeProvider.hpp"
#include "AuthenticationWebBrowserRunner.hpp"
#include "snowflake/IAuth.hpp"

namespace Snowflake {
    namespace Client {

        using namespace Snowflake::Client::IAuth;
        using SFURL = Snowflake::Client::SFURL;



        
    }
}
#endif //SNOWFLAKE_ODBC_AUTHENTICATOROAUTH_HPP
