/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <snowflake/Connection.hpp>

Snowflake::Client::Connection::Connection() {
    this->m_connection = snowflake_init();
}

Snowflake::Client::Connection::~Connection() {
    snowflake_term(this->m_connection);
}

void Snowflake::Client::Connection::connect() {
    SF_STATUS status = snowflake_connect(this->m_connection);

    switch ((int)status) {
        // TODO implement exception throwing based on return status
        case 0:
            break;
        default:
            break;
    }
}

void Snowflake::Client::Connection::setAttribute(SF_ATTRIBUTE type_, const void *value_) {
    // TODO implement this
}


