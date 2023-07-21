/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <snowflake/Statement.hpp>

Snowflake::Client::Statement::Statement(Connection &connection_) {
    this->m_connection = &connection_;
}

Snowflake::Client::Statement::~Statement() {
    // TODO implement this
}
