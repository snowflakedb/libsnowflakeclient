/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_SNOWFLAKECONNECTION_HPP
#define SNOWFLAKECLIENT_SNOWFLAKECONNECTION_HPP

#include "SnowflakeInclude.hpp"

namespace Snowflake {
    namespace Client {
        class SnowflakeConnection {
        public:

            /* Construct a blank Snowflake Connection */
            SnowflakeConnection(void);

            /*
             * Construct with a connection pointer. Copies connection info from
             * passed in connection
             */
            SnowflakeConnection(Snowflake::CAPI::SF_CONNECT &connection_);

            ~SnowflakeConnection(void);

            Snowflake::CAPI::SF_STATUS connect();

            Snowflake::CAPI::SF_STATUS setAttribute(Snowflake::CAPI::SF_ATTRIBUTE type_,
                                                    const void *value_);

            Snowflake::CAPI::SF_STATUS getAttribute(Snowflake::CAPI::SF_ATTRIBUTE type_,
                                                    void **value_);

            Snowflake::CAPI::SF_STATUS beginTransaction();

            Snowflake::CAPI::SF_STATUS commitTransaction();

            Snowflake::CAPI::SF_STATUS rollbackTransaction();

            //TODO Instead of returning error struct, translate error codes into exceptions



        private:
            Snowflake::CAPI::SF_CONNECT *m_connection;
            bool m_connection_created = 0;
        };
    }
}

#endif //SNOWFLAKECLIENT_SNOWFLAKECONNECTION_HPP
