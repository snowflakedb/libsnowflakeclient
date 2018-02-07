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
	message("Platform: Linux")
endif()
if(APPLE)
    set(PLATFORM darwin)
	message("Platform: Apple OSX")
endif()
if ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    set(PLATFORM win64)
	message("Platform: Windows 64bit")
endif()
if ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
    set(PLATFORM win32)
	message("Platform: Windows 32bit")
endif()

set(CMAKE_VERBOSE_MAKEFILE ON)
if(UNIX)
	# Linux and OSX
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -fPIC -Werror")
endif()
if(LINUX)
	# Linux
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
endif()

if (NOT "$ENV{BUILD_WITH_PROFILE_OPTION}" STREQUAL "")
	# Profiler option
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
endif()

if(UNIX)
	# Linux and OSX
	if (NOT "$ENV{BUILD_WITH_GCOV_OPTION}" STREQUAL "")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -gp -fprofile-arcs -ftest-coverage")
	endif()
endif()

set(SOURCE_FILES
        include/snowflake/basic_types.h
        include/snowflake/client.h
        include/snowflake/logger.h
        include/snowflake/version.h
        lib/client.c
        lib/constants.h
        lib/cJSON.h
        lib/cJSON.c
        lib/logger.c
        lib/arraylist.h
        lib/arraylist.c
        lib/memory.h
        lib/memory.c
        lib/connection.h
        lib/connection.c
        lib/constants.h
        lib/results.h
        lib/results.c
        include/snowflake/platform.h
        lib/platform.c
        lib/uuid4.c
        lib/basic_types.c
        lib/error.h
        lib/error.c
        lib/client_int.h
        lib/chunk_downloader.h
        lib/chunk_downloader.c
		)

if (UNIX)
	# Linux and OSX
    find_library(CURL_LIB libcurl.a PATHS deps-build/${PLATFORM}/curl/lib/ REQUIRED)
    find_library(SSL_LIB libssl.a PATHS deps-build/${PLATFORM}/openssl/lib/ REQUIRED)
    find_library(CRYPTO_LIB libcrypto.a PATHS deps-build/${PLATFORM}/openssl/lib/ REQUIRED)
endif()

if(WIN32)
	add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
    find_library(CURL_LIB libcurl_a.lib PATHS deps-build/${PLATFORM}/curl/lib/ REQUIRED)
    find_library(SSL_LIB libssl.lib PATHS deps-build/${PLATFORM}/openssl/lib/ REQUIRED)
    find_library(CRYPTO_LIB libcrypto.lib PATHS deps-build/${PLATFORM}/openssl/lib/ REQUIRED)
    find_library(ZLIB_LIB zlib_a.lib PATHS deps-build/${PLATFORM}/zlib/lib/ REQUIRED)
endif()

include_directories(
        deps-build/${PLATFORM}/curl/include
        deps-build/${PLATFORM}/openssl/include
        deps-build/${PLATFORM}/zlib/include
        include)

message("libcurl is located at " ${CURL_LIB})
message("libssl is located at " ${SSL_LIB})
message("libcrypto is located at " ${CRYPTO_LIB})

add_library(snowflakeclient STATIC ${SOURCE_FILES})

set_property(TARGET snowflakeclient PROPERTY C_STANDARD 99)

link_directories(
        "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/curl/lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/openssl/lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/zlib/lib"
)

if(LINUX)
    target_link_libraries(snowflakeclient rt dl z)
endif()
if(APPLE)
    target_link_libraries(snowflakeclient dl z)
endif()
if(WIN32)
    target_link_libraries(snowflakeclient zlib)
endif()

add_subdirectory(examples)
