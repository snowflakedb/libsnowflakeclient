#ifndef SNOWFLAKECLIENT_TESTSETUP_HPP
#define SNOWFLAKECLIENT_TESTSETUP_HPP

#include <snowflake/client.h>
#include <snowflake/Connection.hpp>
#include <snowflake/Statement.hpp>

class TestSetup
{
public:
    // Used by put/get tests to get <workspace>/test/data/ directory
    static std::string getDataDir();

private:
    TestSetup() {};
};

#endif // SNOWFLAKECLIENT_TESTSETUP_HPP
