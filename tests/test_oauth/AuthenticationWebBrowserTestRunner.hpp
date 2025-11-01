#pragma once
#ifndef AUTHENTICATIONWEBBROWSERTESTRUNNER_HPP
#define AUTHENTICATIONWEBBROWSERTESTRUNNER_HPP

#include "../cpp/lib/AuthenticationWebBrowserRunner.hpp"
#include "snowflake/SFURL.hpp"
#include "../cpp/logger/SFLogger.hpp"
#include "../../lib/connection.h"
#include "curl_desc_pool.h"

namespace Snowflake {
    namespace Client {

        class AuthenticationWebBrowserTestRunner : public IAuthenticationWebBrowserRunner {
        public:
            void startWebBrowser(const std::string& url) override
            {

                CXX_LOG_TRACE("sf::AuthenticationWebBrowserTestRunner::running curl to open a browser::%s", url.c_str());
                int64 elapsedTime = 0;
                cJSON* resp_data = NULL;
                void* curl_desc;
                CURL* curl;
                curl_desc = get_curl_desc_from_pool(url.c_str(), NULL, NULL);
                curl = get_curl_from_desc(curl_desc);
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);

                http_perform(curl, GET_REQUEST_TYPE, (char*)url.c_str(), NULL, NULL, NULL, &resp_data,
                    NULL, NULL, 120, SF_BOOLEAN_FALSE, NULL, SF_BOOLEAN_TRUE, SF_BOOLEAN_FALSE,
                    SF_BOOLEAN_FALSE, SF_BOOLEAN_TRUE, SF_BOOLEAN_TRUE, SF_BOOLEAN_TRUE, SF_BOOLEAN_TRUE,
                    SF_CRL_DOWNLOAD_TIMEOUT, 0,
                    0, 7, NULL, NULL, NULL, SF_BOOLEAN_FALSE,
                    NULL, NULL, SF_BOOLEAN_FALSE, SF_BOOLEAN_FALSE);
            }
        };
    } // namespace Client
} // namespace Snowflake

#endif //AUTHENTICATIONWEBBROWSERTESTRUNNER_HPP
