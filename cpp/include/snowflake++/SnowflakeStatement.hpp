/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_SNOWFLAKESTATEMENT_HPP
#define SNOWFLAKECLIENT_SNOWFLAKESTATEMENT_HPP

#include <SnowflakeInclude.hpp>
#include <string>
#include "SnowflakeConnection.hpp"

namespace Snowflake {
    namespace Client {
        class SnowflakeStatement {
            friend class SnowflakeConnection;
        public:

            SnowflakeStatement(SnowflakeConnection &connection_);

            SnowflakeStatement(Snowflake::CAPI::SF_STMT &sf_stmt_);

            ~SnowflakeStatement(void);

            //TODO error structs or exceptions?

            Snowflake::CAPI::SF_STATUS query(const std::string &command_);

            Snowflake::CAPI::int64 affectedRows();

            Snowflake::CAPI::uint64 numRows();

            Snowflake::CAPI::uint64 numFields();

            const char *sqlState();

            Snowflake::CAPI::SF_COLUMN_DESC *desc();

            Snowflake::CAPI::SF_STATUS prepare(const std::string &command_);

            Snowflake::CAPI::SF_STATUS setAttribute(Snowflake::CAPI::SF_STMT_ATTRIBUTE type_,
                                                    const void *value);

            Snowflake::CAPI::SF_STATUS getAttribute(Snowflake::CAPI::SF_STMT_ATTRIBUTE type_,
                                                    void **value);

            Snowflake::CAPI::SF_STATUS execute();

            Snowflake::CAPI::SF_STATUS fetch();

            Snowflake::CAPI::uint64 numParams();

            Snowflake::CAPI::SF_STATUS bindParam(Snowflake::CAPI::SF_BIND_INPUT &sfbind_);

            Snowflake::CAPI::SF_STATUS bindParamArray(Snowflake::CAPI::SF_BIND_INPUT &sfbind_array_,
                                                      size_t size_);

            Snowflake::CAPI::SF_STATUS bindResult(Snowflake::CAPI::SF_BIND_OUTPUT &sfbind_);

            Snowflake::CAPI::SF_STATUS bindResultArray(Snowflake::CAPI::SF_BIND_OUTPUT &sfbind_array_,
                                                       size_t size_);

            const char *sfqid();

        private:
            // C API struct to operate on
            Snowflake::CAPI::SF_STMT m_stmt;
            // Pointer to the connection object that the statement struct will to
            // connect to Snowflake.
            SnowflakeConnection *m_connection;
        };
    }
}


#endif //SNOWFLAKECLIENT_SNOWFLAKESTATEMENT_HPP
