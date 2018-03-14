/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_SNOWFLAKESTATEMENT_HPP
#define SNOWFLAKECLIENT_SNOWFLAKESTATEMENT_HPP

#include <snowflakecpp/Include.hpp>
#include <string>
#include "Connection.hpp"

namespace Snowflake {
    namespace Client {
        class Statement {
            friend class Connection;
        public:

            Statement(Connection &connection_);

            Statement(Snowflake::CAPI::SF_STMT &sf_stmt_);

            ~Statement(void);

            //TODO error structs or exceptions?

            void query(const std::string &command_);

            Snowflake::CAPI::int64 affectedRows();

            Snowflake::CAPI::uint64 numRows();

            Snowflake::CAPI::uint64 numFields();

            const char *sqlState();

            Snowflake::CAPI::SF_COLUMN_DESC *desc();

            void prepare(const std::string &command_);

            void setAttribute(Snowflake::CAPI::SF_STMT_ATTRIBUTE type_,
                                                    const void *value);

            void getAttribute(Snowflake::CAPI::SF_STMT_ATTRIBUTE type_,
                                                    void **value);

            void execute();

            Snowflake::CAPI::SF_STATUS fetch();

            Snowflake::CAPI::uint64 numParams();

            void bindParam(Snowflake::CAPI::SF_BIND_INPUT &sfbind_);

            void bindParamArray(Snowflake::CAPI::SF_BIND_INPUT sfbind_array_[],
                                                      size_t size_);

            void bindResult(Snowflake::CAPI::SF_BIND_OUTPUT &sfbind_);

            void bindResultArray(Snowflake::CAPI::SF_BIND_OUTPUT sfbind_array_[],
                                                       size_t size_);

            const char *sfqid();

        private:
            // C API struct to operate on
            Snowflake::CAPI::SF_STMT m_stmt;
            // Pointer to the connection object that the statement struct will to
            // connect to Snowflake.
            Connection *m_connection;
        };
    }
}


#endif //SNOWFLAKECLIENT_SNOWFLAKESTATEMENT_HPP
