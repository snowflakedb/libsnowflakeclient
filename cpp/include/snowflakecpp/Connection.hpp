/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_SNOWFLAKECONNECTION_HPP
#define SNOWFLAKECLIENT_SNOWFLAKECONNECTION_HPP

//#include <string>
#include "Include.hpp"

namespace Snowflake {
    namespace Client {
        class Connection {
        public:

            /* Construct a blank Snowflake Connection */
            Connection(void);

            /*
             * Construct with a connection pointer. Copies connection info from
             * passed in connection
             */
            Connection(SF_CONNECT &connection_);

            ~Connection(void);

            void connect();

            void setAttribute(SF_ATTRIBUTE type_,
                                                    const void *value_);

            void getAttribute(SF_ATTRIBUTE type_,
                                                    void **value_);

            void beginTransaction();

            void commitTransaction();

            void rollbackTransaction();

            //TODO Instead of returning error struct, translate error codes into exceptions

            /*
             * Get error message from error struct. Error message is set when there is an exception
             */
            const char * err_msg();

        private:
            SF_CONNECT *m_connection;
            // Whether the class created the connection or it was passed by reference to us
            bool m_connection_created = false;
        };
    }
}

#endif //SNOWFLAKECLIENT_SNOWFLAKECONNECTION_HPP
