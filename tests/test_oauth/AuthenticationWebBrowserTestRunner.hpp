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
                CXX_LOG_TRACE("sf::AuthenticationWebBrowserTestRunner::running curl instead opening a browser::%s", url.c_str());
                cJSON* resp_data = NULL;
                void* curl_desc = get_curl_desc_from_pool(url.c_str(), NULL, NULL);
                CURL* curl = get_curl_from_desc(curl_desc);

                SF_CRL_CONFIG crl_config = {
                    SF_BOOLEAN_FALSE,                // check
                    SF_BOOLEAN_TRUE,                 // advisory
                    SF_BOOLEAN_TRUE,                 // allow_no_crl
                    SF_BOOLEAN_TRUE,                 // disk_caching
                    SF_BOOLEAN_TRUE,                 // memory_caching
                    SF_CRL_DOWNLOAD_TIMEOUT,         // download_timeout
                    SF_CRL_DOWNLOAD_MAX_SIZE_DEFAULT  // download_max_size
                };
                http_perform(curl, GET_REQUEST_TYPE, (char*)url.c_str(), NULL, NULL, NULL, &resp_data,
                    NULL, NULL, 120, 120, SF_BOOLEAN_FALSE, NULL, SF_BOOLEAN_TRUE, SF_BOOLEAN_FALSE,
                    &crl_config,
                    0, 7, NULL, NULL, NULL, SF_BOOLEAN_FALSE,
                    NULL, NULL, SF_BOOLEAN_FALSE, SF_BOOLEAN_FALSE);
            }
        };
    } // namespace Client
} // namespace Snowflake

#endif //AUTHENTICATIONWEBBROWSERTESTRUNNER_HPP
