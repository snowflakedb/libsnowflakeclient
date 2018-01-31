# Copyright (c) 2017 Snowflake Computing, Inc. All rights reserved.
#
# CMakeList for Snowflake Client
#
cmake_minimum_required(VERSION 2.8)
project(snowflakeclient)

add_definitions(-DLOG_USE_COLOR)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

if(LINUX)
    set(PLATFORM linux)
endif()
if(APPLE)
    set(PLATFORM darwin)
endif()

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -fPIC -Werror")
if(LINUX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
endif()

if (NOT "$ENV{BUILD_WITH_PROFILE_OPTION}" STREQUAL "")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
endif()

if (NOT "$ENV{BUILD_WITH_GCOV_OPTION}" STREQUAL "")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -gp -fprofile-arcs -ftest-coverage")
endif()

set(SOURCE_FILES
        include
        lib/client.c
        lib/constants.h
        lib/cJSON.c
        lib/logger.c
        lib/arraylist.c
        lib/memory.c
        lib/connection.c
        lib/results.c
        lib/uuid4.c
        lib/basic_types.c
        lib/error.c
        lib/client_int.h
        lib/chunk_downloader.c)

find_library(CURL_LIB libcurl.a PATHS deps-build/${PLATFORM}/curl/lib/ REQUIRED)
find_library(SSL_LIB libssl.a PATHS deps-build/${PLATFORM}/openssl/lib/ REQUIRED)
find_library(CRYPTO_LIB libcrypto.a PATHS deps-build/${PLATFORM}/openssl/lib/ REQUIRED)

include_directories(
        deps-build/${PLATFORM}/curl/include
        deps-build/${PLATFORM}/openssl/include
        include)

message("libcurl is located at " ${CURL_LIB})
message("libssl is located at " ${SSL_LIB})
message("libcrypto is located at " ${CRYPTO_LIB})

add_library(snowflakeclient STATIC ${SOURCE_FILES})

set_property(TARGET snowflakeclient PROPERTY C_STANDARD 99)

link_directories(
        "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/curl/lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/openssl/lib"
)

if(LINUX)
    target_link_libraries(snowflakeclient rt dl z)
endif()
if(APPLE)
    target_link_libraries(snowflakeclient dl z)
endif()

add_subdirectory(examples)
