# Copyright (c) 2017 Snowflake Computing, Inc. All rights reserved.
#
# CMakeList for Snowflake Client++
#

set(SOURCE_FILES_CXX
        include/snowflake++/SnowflakeInclude.hpp
        include/snowflake++/SnowflakeConnection.hpp
        include/snowflake++/SnowflakeStatement.hpp
        lib/SnowflakeConnection.cpp
        lib/SnowflakeStatement.cpp)

set(TESTLIB_OPTS_CXX snowflakeclient -Wl,--whole-archive curl ssl crypto pthread aws-cpp-sdk-core aws-cpp-sdk-s3 -Wl,--no-whole-archive)

add_library(snowflakeclient++ STATIC ${SOURCE_FILES_CXX})

set_target_properties(snowflakeclient++ PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(snowflakeclient++ ${TESTLIB_OPTS_CXX})
