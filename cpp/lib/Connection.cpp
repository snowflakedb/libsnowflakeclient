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
