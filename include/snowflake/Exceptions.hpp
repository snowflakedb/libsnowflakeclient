/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_EXCEPTIONS_HPP
#define SNOWFLAKECLIENT_EXCEPTIONS_HPP

#include <exception>
#include "client.h"

namespace Snowflake
{
    namespace Client
    {
        namespace Exception
        {
            class SnowflakeException : public std::exception {
            public:
                SnowflakeException(SF_ERROR_STRUCT* error);

                const char* what() const throw();

                SF_STATUS code();

                const char* sqlstate();

                const char* msg();

                const char* sfqid();

                const char* file();

                int line();

            protected:
                SF_ERROR_STRUCT* error;
            };

            class GeneralException : public SnowflakeException {
            public:
                GeneralException(SF_ERROR_STRUCT* error) : SnowflakeException(error) {};
            };

            class RenewTimeoutException : public std::exception
            {
            public:
                RenewTimeoutException(int64 elapsedSeconds,
                    int8 retriedCount,
                    bool isCurlTimeoutNoBackoff) :
                    m_elapsedSeconds(elapsedSeconds),
                    m_retriedCount(retriedCount),
                    m_isCurlTimeoutNoBackoff(isCurlTimeoutNoBackoff)
                {}

                int64 getElapsedSeconds()
                {
                    return m_elapsedSeconds;
                }

                int8 getRetriedCount()
                {
                    return m_retriedCount;
                }

                bool isCurlTimeoutNoBackoff()
                {
                    return m_isCurlTimeoutNoBackoff;
                }

                virtual const char* what() const noexcept
                {
                    return "internal renew timeout exception";
                }

            private:
                int64 m_elapsedSeconds;
                int8 m_retriedCount;
                // The flag indicate if the renew exception is thrown for renew the request
                // within curl timeout and no backoff made
                bool m_isCurlTimeoutNoBackoff;
            };

            struct AuthException : public std::exception
            {
                AuthException(SF_ERROR_STRUCT* error) : message_(error->msg) {}
                AuthException(const std::string& message) : message_(message) {}

                const char* what() const noexcept
                {
                    return message_.c_str();
                }

                std::string message_;
            };
        }
    }
}
#endif //SNOWFLAKECLIENT_EXCEPTIONS_HPP
