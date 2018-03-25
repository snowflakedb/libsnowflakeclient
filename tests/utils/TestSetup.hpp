/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_TESTSETUP_HPP
#define SNOWFLAKECLIENT_TESTSETUP_HPP

#include <snowflakecpp/Include.hpp>
#include <snowflakecpp/Connection.hpp>
#include <snowflakecpp/Statement.hpp>

class TestSetup {
public:
    static Snowflake::Client::Connection *connectionFactory();

    static Snowflake::Client::Connection *connectionWithAutocommitFactory(const std::string timezone,
                                                                   bool autocommit);

private:
    TestSetup() {};
};


#endif //SNOWFLAKECLIENT_TESTSETUP_HPP
