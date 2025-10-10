#ifndef AUTHENTICATIONWEBBROWSERRUNNER_HPP
#define AUTHENTICATIONWEBBROWSERRUNNER_HPP
#include <string>
#include <memory>

namespace Snowflake {
    namespace Client {

        class IAuthenticationWebBrowserRunner {
        public:
            virtual ~IAuthenticationWebBrowserRunner() = default;
            virtual void startWebBrowser(const std::string& url) = 0;

            static IAuthenticationWebBrowserRunner* getInstance();
            static void setInstance(std::unique_ptr<IAuthenticationWebBrowserRunner> testInstance);
        private:
            static std::unique_ptr<IAuthenticationWebBrowserRunner> instance;
        };

        class AuthenticationWebBrowserRunner : public IAuthenticationWebBrowserRunner {
        public:
            void startWebBrowser(const std::string&) override;
        };

    } // namespace Client
} // namespace Snowflake
#endif //AUTHENTICATIONWEBBROWSERRUNNER_HPP
