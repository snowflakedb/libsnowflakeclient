#pragma once
#ifndef AUTHENTICATIONWEBBROWSERTESTRUNNER_HPP
#define AUTHENTICATIONWEBBROWSERTESTRUNNER_HPP

#include "../cpp/lib/AuthenticationWebBrowserRunner.hpp"
#include "snowflake/SFURL.hpp"
#include "../cpp/logger/SFLogger.hpp"


namespace Snowflake {
    namespace Client {

        class AuthenticationWebBrowserTestRunner : public IAuthenticationWebBrowserRunner {
        public:
            void startWebBrowser(const std::string& url) override
            {
                CXX_LOG_TRACE("sf::AuthenticationWebBrowserTestRunner::running curl to open a browser::%s", url.c_str())
                ::Snowflake::Client::SFURL sfUrl = ::Snowflake::Client::SFURL::parse(url);
                std::vector<std::string> httpExtraHeaders;
                try {
                    //RestRequest::getInstance()->getInternal(sfUrl,
                    //    &httpExtraHeaders,
                    //    resp,
                    //    false,
                    //    "");
                }
                catch (std::exception& e)
                {
                    CXX_LOG_ERROR("sf", "AuthenticationWebBrowserTestRunner", "running curl to open a browser failed with exception", "%s", e.what())
                        // intentionally not rethrown to mimic external browser behaviour
                }
            }
        };
    } // namespace Client
} // namespace Snowflake

#endif //AUTHENTICATIONWEBBROWSERTESTRUNNER_HPP
