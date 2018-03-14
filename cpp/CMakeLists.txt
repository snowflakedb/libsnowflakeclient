# Copyright (c) 2017 Snowflake Computing, Inc. All rights reserved.
#
# CMakeList for Snowflake Client++
#

set(SOURCE_FILES_CXX
        include/snowflakecpp/Include.hpp
        include/snowflakecpp/Connection.hpp
        include/snowflakecpp/Statement.hpp
        lib/Connection.cpp
        lib/Statement.cpp
        include/snowflakecpp/Exceptions.hpp
        lib/Exceptions.cpp
        include/snowflakecpp/Column.hpp
        include/snowflakecpp/Param.hpp
        lib/Column.cpp)

set(TESTLIB_OPTS_CXX snowflakeclient -Wl,--whole-archive curl ssl crypto pthread aws-cpp-sdk-core aws-cpp-sdk-s3 -Wl,--no-whole-archive)

add_library(snowflakeclientcpp STATIC ${SOURCE_FILES_CXX})

set_target_properties(snowflakeclientcpp PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(snowflakeclientcpp ${TESTLIB_OPTS_CXX})
