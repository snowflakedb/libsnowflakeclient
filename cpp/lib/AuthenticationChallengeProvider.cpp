extern "C" {
#include "../lib/client_int.h"
}
#include "AuthenticationChallengeProvider.hpp"
#include "Authenticator.hpp"
#include <string>
#include <openssl/rand.h>
#include "snowflake/IBase64.hpp"

namespace Snowflake
{
    namespace Client
    {
        using Base64 = ::Snowflake::Client::Util::IBase64;

        std::unique_ptr<AuthenticationChallengeBaseProvider> AuthenticationChallengeBaseProvider::instance
            = std::make_unique<AuthenticationChallengeProvider>();

        AuthenticationChallengeBaseProvider* AuthenticationChallengeBaseProvider::getInstance()
        {
            return instance.get();
        }

        std::string AuthenticationChallengeBaseProvider::generateCodeChallenge(const std::string& codeVerifier) const
        {
            std::vector<char> sha256 = AuthenticatorJWT::SHA256(std::vector(codeVerifier.begin(), codeVerifier.end()));
            return Base64::encodeURLNoPadding(sha256);
        }

        std::string AuthenticationChallengeBaseProvider::codeChallengeMethod() const
        {
            return "S256";
        }

        void AuthenticationChallengeBaseProvider::setInstance(std::unique_ptr<AuthenticationChallengeBaseProvider> testInstance)
        {
            instance = std::move(testInstance);
        }

        std::string AuthenticationChallengeProvider::generateState() const
        {
            std::vector<char> vec(SF_UUID4_LEN);
            uuid4_generate(vec.data());
            return Base64::encodeURLNoPadding(vec);
        }

        std::string AuthenticationChallengeProvider::generateCodeVerifier() const
        {
            std::vector<char> randomness(32);
            RAND_bytes(reinterpret_cast<unsigned char*>(randomness.data()), randomness.size());
            return Base64::encodeURLNoPadding(randomness);
        }
    } // namespace Client
} // namespace Snowflake
