/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */

#include <snowflakecpp/Connection.hpp>

Snowflake::Client::Connection::Connection() {
    this->m_connection = snowflake_init();
    this->m_connection_created = false;
}

Snowflake::Client::Connection::~Connection() {
    snowflake_term(this->m_connection);
}

void Snowflake::Client::Connection::connect() {
    SF_STATUS status = snowflake_connect(this->m_connection);

    switch (status) {
        // TODO implement exception throwing based on return status
    }
}

void Snowflake::Client::Connection::setAttribute(SF_ATTRIBUTE type_, const void *value_) {
    SF_STATUS status = snowflake_set_attribute(this->m_connection, type_, value_);

    switch (status) {
        // TODO implement exception throwing based on return status
    }
}


