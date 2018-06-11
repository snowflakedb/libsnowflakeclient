# Copyright (c) 2017 Snowflake Computing, Inc. All rights reserved.
#
# CMakeList for Snowflake Client
#
cmake_minimum_required(VERSION 2.8)
project(snowflakeclient)

# Enabling tests by Ctest. Don't use INCLUDE(Ctest) as
# we don't need Dart and other tools.
enable_testing()

add_definitions(-DLOG_USE_COLOR)

if (UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif ()

if (LINUX)
    set(PLATFORM linux)
    message("Platform: Linux")
endif ()
if (APPLE)
    set(PLATFORM darwin)
    message("Platform: Apple OSX")
endif ()
if ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    set(PLATFORM win64)
    message("Platform: Windows 64bit")
endif ()
if ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
    set(PLATFORM win32)
    message("Platform: Windows 32bit")
endif ()

set(CMAKE_VERBOSE_MAKEFILE ON)
if (UNIX)
    # Linux and OSX
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -fPIC -Werror")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -std=gnu++11 -fPIC")
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
        lib/arraylist.h
        lib/arraylist.c
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
        lib/chunk_downloader.c)

set (SOURCE_FILES_PUT_GET
        cpp/EncryptionProvider.cpp
        cpp/FileCompressionType.cpp
        cpp/FileCompressionType.hpp
        cpp/FileMetadata.hpp
        cpp/FileMetadataInitializer.cpp
        cpp/FileMetadataInitializer.hpp
        cpp/FileTransferAgent.cpp
        cpp/FileTransferAgent.hpp
        cpp/FileTransferExecutionResult.cpp
        cpp/FileTransferExecutionResult.hpp
        cpp/IFileTransferAgent.cpp
        cpp/IStorageClient.hpp
        cpp/SnowflakeS3Client.hpp
        cpp/SnowflakeS3Client.cpp
        cpp/StatementPutGet.hpp
        cpp/StatementPutGet.cpp
        cpp/StorageClientFactory.hpp
        cpp/StorageClientFactory.cpp
        cpp/RemoteStorageRequestOutcome.hpp
        cpp/crypto/CryptoTypes.hpp
        cpp/util/Base64.hpp
        cpp/util/Base64.cpp
        cpp/util/ByteArrayStreamBuf.cpp
        cpp/util/ByteArrayStreamBuf.hpp
        cpp/util/CompressionUtil.cpp
        cpp/util/CompressionUtil.hpp
        cpp/util/ThreadPool.hpp
        cpp/crypto/Cryptor.hpp
        cpp/crypto/CipherContext.hpp
        cpp/crypto/CipherContext.cpp
        cpp/crypto/Cryptor.cpp
        cpp/util/SnowflakeCommon.hpp
        cpp/crypto/CipherStreamBuf.cpp
        cpp/crypto/CipherStreamBuf.hpp
        cpp/crypto/HashContext.cpp
        cpp/crypto/HashContext.hpp
        cpp/logger/ISFLogger.cpp
        cpp/logger/SFAwsLogger.cpp
        cpp/logger/SFAwsLogger.hpp
        cpp/logger/SFLogger.cpp
        cpp/logger/SFLogger.hpp
        include/snowflake/IFileTransferAgent.hpp
        include/snowflake/ISFLogger.hpp
        include/snowflake/IStatementPutGet.hpp
        include/snowflake/ITransferResult.hpp
        include/snowflake/PutGetParseResponse.hpp
        )

set(SOURCE_FILES_CPP_WRAPPER
        include/snowflake/Connection.hpp
        include/snowflake/Statement.hpp
        include/snowflake/Column.hpp
        include/snowflake/Param.hpp
        include/snowflake/Exceptions.hpp
        cpp/lib/Exceptions.cpp
        cpp/lib/Connection.cpp
        cpp/lib/Statement.cpp
        cpp/lib/Column.cpp)

if (LINUX)
    # Linux
    find_library(CURL_LIB libcurl.a PATHS deps-build/${PLATFORM}/curl/lib/ REQUIRED)
    find_library(SSL_LIB libssl.a PATHS deps-build/${PLATFORM}/openssl/lib/ REQUIRED)
    find_library(CRYPTO_LIB libcrypto.a PATHS deps-build/${PLATFORM}/openssl/lib/ REQUIRED)
    find_library(AWS_CORE_LIB libaws-cpp-sdk-core.a PATHS deps-build/${PLATFORM}/aws/lib64/ REQUIRED)
    find_library(AWS_S3_LIB libaws-cpp-sdk-s3.a PATHS deps-build/${PLATFORM}/aws/lib64/ REQUIRED)
endif ()

if (APPLE)
    find_library(CURL_LIB libcurl.a PATHS deps-build/${PLATFORM}/curl/lib/ REQUIRED)
    find_library(SSL_LIB libssl.a PATHS deps-build/${PLATFORM}/openssl/lib/ REQUIRED)
    find_library(CRYPTO_LIB libcrypto.a PATHS deps-build/${PLATFORM}/openssl/lib/ REQUIRED)
    find_library(AWS_CORE_LIB libaws-cpp-sdk-core.a PATHS deps-build/${PLATFORM}/aws/lib/ REQUIRED)
    find_library(AWS_S3_LIB libaws-cpp-sdk-s3.a PATHS deps-build/${PLATFORM}/aws/lib/ REQUIRED)
endif()

if (WIN32)
    # Windows
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
    find_library(CURL_LIB libcurl_a.lib PATHS deps-build/${PLATFORM}/curl/lib/ REQUIRED)
    find_library(SSL_LIB libssl_a.lib PATHS deps-build/${PLATFORM}/openssl/lib/ REQUIRED)
    find_library(CRYPTO_LIB libcrypto_a.lib PATHS deps-build/${PLATFORM}/openssl/lib/ REQUIRED)
    find_library(ZLIB_LIB zlib_a.lib PATHS deps-build/${PLATFORM}/zlib/lib/ REQUIRED)
	find_library(AWS_CORE_LIB aws-cpp-sdk-core.lib PATHS deps-build/${PLATFORM}/aws/lib/ REQUIRED)
	find_library(AWS_S3_LIB aws-cpp-sdk-s3.lib PATHS deps-build/${PLATFORM}/aws/lib/ REQUIRED)
endif ()

include_directories(
        deps-build/${PLATFORM}/curl/include
        deps-build/${PLATFORM}/openssl/include
        deps-build/${PLATFORM}/zlib/include
        deps-build/${PLATFORM}/aws/include
        include
        lib)

message("libcurl is located at " ${CURL_LIB})
message("libssl is located at " ${SSL_LIB})
message("libcrypto is located at " ${CRYPTO_LIB})
message("libaws-cpp-sdk-core is located at " ${AWS_CORE_LIB})
message("libaws-cpp-sdk-s3 is located at " ${AWS_S3_LIB})


add_library(snowflakeclient STATIC ${SOURCE_FILES} ${SOURCE_FILES_PUT_GET} ${SOURCE_FILES_CPP_WRAPPER})

set_target_properties(snowflakeclient PROPERTIES LINKER_LANGUAGE CXX)
set_property(TARGET snowflakeclient PROPERTY C_STANDARD 99)
#set (CMAKE_CXX_STANDARD 11)

if(LINUX)
    link_directories(
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/curl/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/openssl/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/zlib/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/aws/lib64"
    )
elseif()
	link_directories(
        "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/curl/lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/openssl/lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/aws/lib"
    )
endif()

if (LINUX)
    # Linux
    target_link_libraries(snowflakeclient rt dl z)
endif ()
if (APPLE)
    # OSX. no librt is required.
    target_link_libraries(snowflakeclient dl z)
endif ()
if (WIN32)
    # Windows
    target_link_libraries(snowflakeclient)
endif ()

add_subdirectory(examples)
add_subdirectory(tests)
