# Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
#
# CMakeList for Snowflake Client
#
cmake_minimum_required(VERSION 2.8)
project(snowflakeclient)

# Enabling tests by Ctest. Don't use INCLUDE(Ctest) as
# we don't need Dart and other tools.
enable_testing()

add_definitions(-DLOG_USE_COLOR)

option(BUILD_TESTS "True if build tests" on)
option(MOCK "True if mock should be used" off)
set(OPENSSL_VERSION_NUMBER  0x11100000L)
# Developers can uncomment this to enable mock builds on their local VMs
#set(MOCK TRUE)

# Generates compile_commands.json file for clangd to parse.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if ($ENV{XP_BUILD})
    message("-- Building libsnowflakeclient for XP")
    add_definitions(-DLIBSFCLI_FOR_XP)
endif ()

if (MOCK)
    set(MOCK_OBJECT_WRAPPER_FLAGS -Wl,--wrap=http_perform)
    add_definitions(-DMOCK_ENABLED)
else()
    set(MOCK_OBJECT_WRAPPER_FLAGS )
endif ()

if (UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif ()

if (LINUX)
    set(PLATFORM linux)
    message("Platform: Linux")
endif ()


set(CMAKE_VERBOSE_MAKEFILE ON)
if (UNIX)
    # Linux and OSX
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -std=gnu99 -g -fPIC -Werror ${MOCK_OBJECT_WRAPPER_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -std=gnu++11 -fPIC -Werror ${MOCK_OBJECT_WRAPPER_FLAGS}")
endif ()
if (LINUX)
    # Linux. MacOS doesn't require pthread option
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
endif ()

if (LINUX)
    # Profiler for Linux
    if (NOT "$ENV{BUILD_WITH_PROFILE_OPTION}" STREQUAL "")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
    endif ()

    # Code coverage for Linux
    if (NOT "$ENV{BUILD_WITH_GCOV_OPTION}" STREQUAL "")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -gp -fprofile-arcs -ftest-coverage")
    endif ()
endif ()

set(SOURCE_FILES
        include/snowflake/basic_types.h
        include/snowflake/client.h
        include/snowflake/logger.h
        include/snowflake/version.h
        include/snowflake/platform.h
        lib/client.c
        lib/constants.h
        lib/cJSON.h
        lib/cJSON.c
        lib/logger.c
        lib/lib_common.h
        lib/paramstore.c
        lib/paramstore.h
        lib/arraylist.h
        lib/arraylist.c
        lib/treemap.h
        lib/treemap.c
        lib/rbtree.h
        lib/rbtree.c
        lib/memory.h
        lib/memory.c
        lib/connection.h
        lib/connection.c
        lib/constants.h
        lib/results.h
        lib/results.c
        lib/platform.c
        lib/uuid4.c
        lib/basic_types.c
        lib/error.h
        lib/error.c
        lib/client_int.h
        lib/chunk_downloader.h
        lib/chunk_downloader.c
        lib/mock_http_perform.h
        lib/http_perform.c)

set (SOURCE_FILES_PUT_GET
        cpp/logger/ISFLogger.cpp
        cpp/logger/SFLogger.cpp
        cpp/logger/SFLogger.hpp
        include/snowflake/ISFLogger.hpp
        )

set(SOURCE_FILES_CPP_WRAPPER
	cpp/logger/SFLoggerCWrapper.cpp
	cpp/logger/SFLoggerCWrapper.h)

if (LINUX)
    # Linux
    find_library(CURL_LIB libcurl.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/curl/lib/ REQUIRED)
    find_library(SSL_LIB libssl.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/openssl/lib/ REQUIRED)
    find_library(CRYPTO_LIB libcrypto.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/openssl/lib/ REQUIRED)
endif ()

if (LINUX)
    include_directories(
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/curl/include
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/openssl/include
            include
            lib)
endif()


add_library(snowflakeclient STATIC ${SOURCE_FILES} ${SOURCE_FILES_PUT_GET} ${SOURCE_FILES_CPP_WRAPPER})

set_target_properties(snowflakeclient PROPERTIES LINKER_LANGUAGE CXX)
set_property(TARGET snowflakeclient PROPERTY C_STANDARD 99)
#set (CMAKE_CXX_STANDARD 11)

if(LINUX)
    link_directories(
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/curl/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/openssl/lib"
    )
endif()

if (LINUX)
    # Linux
    target_link_libraries(snowflakeclient rt dl z)
endif ()


add_subdirectory(examples)
