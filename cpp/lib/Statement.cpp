/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */

#include <snowflakecpp/Statement.hpp>

Snowflake::Client::Statement::Statement(Connection &connection_) {
    this->m_connection = &connection_;

}

Snowflake::Client::Statement::~Statement() {
    // TODO implement this
}

void Snowflake::Client::Statement::bindResult(SF_BIND_OUTPUT &sfbind_) {
    // TODO implement this
}

void Snowflake::Client::Statement::query(const std::string &command_) {
    // TODO implement this
}

SF_STATUS Snowflake::Client::Statement::fetch() {
    return SF_STATUS_EOF;
}


