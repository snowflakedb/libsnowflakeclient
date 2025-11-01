#pragma once
#ifndef ODBC_AUTHENTICATIONCHALLENGETESTPROVIDER_HPP
#define ODBC_AUTHENTICATIONCHALLENGETESTPROVIDER_HPP

#include "../cpp/lib/AuthenticationChallengeProvider.hpp"

namespace Snowflake {
    namespace Client {

        class AuthenticationChallengeTestProvider : public AuthenticationChallengeBaseProvider {
        public:
            std::string generateState() const override { return "abc123"; }
            std::string generateCodeVerifier() const override { return "code-verifier"; }
        };
    } // namespace Client
} // namespace Snowflake

#endif //AUTHENTICATIONCHALLENGETESTPROVIDER_HPP
