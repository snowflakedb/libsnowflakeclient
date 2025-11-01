#pragma once
#ifndef AUTHENTICATIONCHALLENGEPROVIDER_HPP
#define AUTHENTICATIONCHALLENGEPROVIDER_HPP
#include <string>
#include <memory>

namespace Snowflake
{
    namespace Client
    {
        class AuthenticationChallengeBaseProvider {
        public:
            virtual ~AuthenticationChallengeBaseProvider() = default;
            virtual std::string generateState() const = 0;
            virtual std::string generateCodeVerifier() const = 0;
            std::string generateCodeChallenge(const std::string& codeVerifier) const;
            std::string codeChallengeMethod() const;

            static AuthenticationChallengeBaseProvider* getInstance();
            static void setInstance(std::unique_ptr<AuthenticationChallengeBaseProvider> testInstance);
        private:
            static std::unique_ptr<AuthenticationChallengeBaseProvider> instance;
        };

        class AuthenticationChallengeProvider : public AuthenticationChallengeBaseProvider {
        public:
            std::string generateState() const override;
            std::string generateCodeVerifier() const override;
        };//    class AuthenticationChallenge
    } // namespace Client
} // namespace Snowflake

#endif //AUTHENTICATIONCHALLENGEPROVIDER_HPP
