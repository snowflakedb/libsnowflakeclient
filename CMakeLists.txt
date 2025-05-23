# CMakeList for Snowflake Client
#
cmake_minimum_required(VERSION 3.17)
project(snowflakeclient)
include(cmake/platform.cmake)
include(cmake/flags.cmake)

# Enabling tests by Ctest. Don't use INCLUDE(Ctest) as
# we don't need Dart and other tools.
enable_testing()

add_compile_definitions(LOG_USE_COLOR)

option(BUILD_TESTS "True if build tests" on)
option(MOCK "True if mock should be used" off)
set(OPENSSL_VERSION_NUMBER  0x11100000L)

# Developers can uncomment this to enable mock builds on their local VMs
#set(MOCK TRUE)

# Generates compile_commands.json file for clangd to parse.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(SOURCE_FILES
        include/snowflake/basic_types.h
        include/snowflake/client.h
        include/snowflake/logger.h
        include/snowflake/version.h
        include/snowflake/platform.h
        include/snowflake/secure_storage.h
        include/snowflake/client_config_parser.h
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
        lib/snowflake_cpp_util.h
        lib/snowflake_util.c
        lib/snowflake_util.h
        lib/client_int.h
        lib/chunk_downloader.h
        lib/chunk_downloader.c
        lib/mock_http_perform.h
        lib/http_perform.c
)

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
        cpp/SnowflakeAzureClient.hpp
        cpp/SnowflakeAzureClient.cpp
        cpp/SnowflakeGCSClient.hpp
        cpp/SnowflakeGCSClient.cpp
        cpp/SnowflakeTransferException.cpp
        cpp/StatementPutGet.hpp
        cpp/StatementPutGet.cpp
        cpp/StorageClientFactory.hpp
        cpp/StorageClientFactory.cpp
        cpp/RemoteStorageRequestOutcome.hpp
        cpp/util/Base64.hpp
        cpp/util/Base64.cpp
        cpp/util/ByteArrayStreamBuf.cpp
        cpp/util/ByteArrayStreamBuf.hpp
        cpp/util/CompressionUtil.cpp
        cpp/util/CompressionUtil.hpp
        cpp/util/Proxy.cpp
        cpp/util/ThreadPool.hpp
        cpp/util/SnowflakeCommon.hpp
        cpp/util/entities.cpp
        cpp/crypto/CryptoTypes.hpp
        cpp/crypto/Cryptor.hpp
        cpp/crypto/CipherContext.hpp
        cpp/crypto/CipherContext.cpp
        cpp/crypto/Cryptor.cpp
        cpp/crypto/CipherStreamBuf.cpp
        cpp/crypto/CipherStreamBuf.hpp
        cpp/crypto/HashContext.cpp
        cpp/crypto/HashContext.hpp
        cpp/jwt/Jwt.cpp
        cpp/jwt/Jwt.hpp
        cpp/jwt/Signer.cpp
        cpp/jwt/Signer.hpp
        cpp/jwt/Util.cpp
        cpp/jwt/Util.hpp
        cpp/jwt/ClaimSet.hpp
        cpp/jwt/ClaimSet.cpp
        cpp/jwt/Header.cpp
        cpp/jwt/Header.hpp
        cpp/logger/ISFLogger.cpp
        cpp/logger/SFAwsLogger.cpp
        cpp/logger/SFAwsLogger.hpp
        cpp/logger/SFLogger.cpp
        cpp/logger/SFLogger.hpp
        cpp/logger/SecretDetector.cpp
        cpp/logger/SecretDetector.hpp
        cpp/lib/ResultSetPutGet.cpp
        cpp/lib/ResultSetPutGet.hpp
        include/snowflake/IFileTransferAgent.hpp
        include/snowflake/ISFLogger.hpp
        include/snowflake/IStatementPutGet.hpp
        include/snowflake/ITransferResult.hpp
        include/snowflake/PutGetParseResponse.hpp
        include/snowflake/SnowflakeTransferException.hpp
        include/snowflake/IBase64.hpp
        include/snowflake/Proxy.hpp
        include/snowflake/entities.hpp
)

set(SOURCE_FILES_CPP_WRAPPER
        include/snowflake/Connection.hpp
        include/snowflake/Statement.hpp
        include/snowflake/Column.hpp
        include/snowflake/Param.hpp
        include/snowflake/Exceptions.hpp
        include/snowflake/QueryContextCache.hpp
        include/snowflake/BaseClasses.hpp
        include/snowflake/SFURL.hpp
        include/snowflake/CurlDesc.hpp
        include/snowflake/CurlDescPool.hpp
        include/snowflake/IJwt.hpp
        include/snowflake/IAuth.hpp
        include/snowflake/BindUploader.hpp
        include/snowflake/SecureStorage.hpp
        include/snowflake/TomlConfigParser.hpp
        cpp/lib/Exceptions.cpp
        cpp/lib/Connection.cpp
        cpp/lib/Statement.cpp
        cpp/lib/Column.cpp
        cpp/lib/ArrowChunkIterator.cpp
        cpp/lib/ArrowChunkIterator.hpp
        cpp/lib/DataConversion.cpp
        cpp/lib/DataConversion.hpp
        cpp/lib/QueryContextCache.cpp
        cpp/lib/ClientQueryContextCache.cpp
        cpp/lib/ClientQueryContextCache.hpp
        cpp/lib/result_set.cpp
        cpp/lib/ResultSet.cpp
        cpp/lib/ResultSet.hpp
        cpp/lib/ResultSetArrow.cpp
        cpp/lib/ResultSetArrow.hpp
        cpp/lib/ResultSetJson.cpp
        cpp/lib/ResultSetJson.hpp
        cpp/lib/Authenticator.cpp
        cpp/lib/IAuth.cpp
        cpp/lib/Authenticator.hpp
        cpp/lib/BindUploader.cpp
        cpp/lib/ClientBindUploader.hpp
        cpp/lib/ClientBindUploader.cpp
        cpp/lib/client_config_parser.cpp
        cpp/lib/TomlConfigParser.cpp
        cpp/platform/secure_storage.cpp
        cpp/platform/SecureStorage.cpp
        cpp/platform/SecureStorageApple.cpp
        cpp/platform/SecureStorageLinux.cpp
        cpp/platform/SecureStorageWin.cpp
        cpp/platform/FileLock.cpp
        cpp/platform/FileLock.hpp
        cpp/util/SnowflakeCommon.cpp
        cpp/util/SFURL.cpp
        cpp/util/CurlDesc.cpp
        cpp/util/CurlDescPool.cpp
        cpp/util/Sha256.cpp
        cpp/util/Sha256.hpp
        cpp/util/SnowflakeCppUtil.cpp
        cpp/platform/SecureStorage.cpp
        lib/result_set.h
        lib/query_context_cache.h
        lib/curl_desc_pool.h
        lib/authenticator.h
        include/snowflake/WifAttestation.hpp
        cpp/WifAttestation.cpp
        cpp/AwsAttestation.cpp
        cpp/http/HttpClient.cpp
        include/snowflake/HttpClient.hpp
        cpp/GcpAttestation.hpp
        cpp/GcpAttestation.cpp
        cpp/AzureAttestation.hpp
        cpp/AwsAttestation.hpp
        cpp/AzureAttestation.cpp
        cpp/AWSUtils.cpp
        include/snowflake/AWSUtils.hpp
        cpp/OIDCAttestation.cpp
        cpp/OIDCAttestation.hpp
)

if (LINUX)
    set(SOURCE_FILES_CPP_WRAPPER
        ${SOURCE_FILES_CPP_WRAPPER}
            cpp/linux/CacheFile.cpp
            cpp/linux/CacheFile.hpp
    )
endif()

if (UNIX)
    if (LINUX)
        set(AWSLIBBIT "lib64")
    elseif(APPLE)
        set(AWSLIBBIT "lib")
    endif()
    # Common to both Linux and Apple
    find_library(OOB_LIB libtelemetry.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/oob/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(CURL_LIB libcurl.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/curl/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(SSL_LIB libssl.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/openssl/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(CRYPTO_LIB libcrypto.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/openssl/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_CORE_LIB libaws-cpp-sdk-core.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_S3_LIB libaws-cpp-sdk-s3.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_STS_LIB libaws-cpp-sdk-sts.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_C_AUTH_LIB libaws-c-auth.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_C_CAL_LIB libaws-c-cal.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_C_COMMON_LIB libaws-c-common.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_C_COMPRESSION_LIB libaws-c-compression.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_C_EVENT_STREAM_LIB libaws-c-event-stream.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_C_CHECKSUMS_LIB libaws-checksums.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_C_HTTP_LIB libaws-c-http.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_C_IO_LIB libaws-c-io.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_C_MQTT_LIB libaws-c-mqtt.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_CRT_CPP_LIB libaws-crt-cpp.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_C_S3_LIB libaws-c-s3.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_C_SDKUTILS_LIB libaws-c-sdkutils.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/aws/${AWSLIBBIT}/ REQUIRED NO_DEFAULT_PATH)
    find_library(AZURE_STORAGE_LITE_LIB libazure-storage-lite.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/azure/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(BOOST_FILESYSTEM_LIB libboost_filesystem.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/boost/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(BOOST_URL_LIB libboost_url.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/boost/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(BOOST_REGEX_LIB libboost_regex.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/boost/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(BOOST_SYSTEM_LIB libboost_system.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/boost/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(ARROW_JEMALLOCPIC_LIB libjemalloc_pic.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
    if ($ENV{ARROW_FROM_SOURCE})
    else()
        find_library(ARROW_FLATBUFFERS_LIB libflatbuffers.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
        find_library(ARROW_GFLAGS_LIB libgflags.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
        find_library(ARROW_GLOG_LIB libglog.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
        find_library(ARROW_LZ4_LIB liblz4.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
        find_library(ARROW_LZO2_LIB liblzo2.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
        find_library(ARROW_SNAPPY_LIB libsnappy.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
        find_library(ARROW_ZSTD_LIB libzstd.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
        find_library(ARROW_BROTLIDEC_LIB libbrotlidec-static.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
        find_library(ARROW_BROTLIENC_LIB libbrotlienc-static.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
        find_library(ARROW_BROTLICOMMON_LIB libbrotlicommon-static.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
    endif ()
    #if(MOCK)
        find_library(CMOCKA_LIB libcmocka.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/cmocka/lib/ REQUIRED NO_DEFAULT_PATH)
    #endif()
endif()

if (LINUX)
    # Linux
    find_library(ARROW_ARROW_LIB libarrow.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/arrow/lib64/ REQUIRED NO_DEFAULT_PATH)
    find_library(UUID_LIB libuuid.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/uuid/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_S2N_LIB libs2n.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/aws/lib64/ REQUIRED NO_DEFAULT_PATH)
    #if(MOCK)
        find_library(CMOCKA_LIB libcmocka.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/cmocka/lib/ REQUIRED NO_DEFAULT_PATH)
    #endif()
endif ()

if (APPLE)
    # macOS
    if(ENV{ARCH} STREQUAL "x64")
        find_library(ARROW_ARROW_LIB libarrow.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/arrow/lib64/ REQUIRED NO_DEFAULT_PATH)
    else()
        find_library(ARROW_ARROW_LIB libarrow.a PATHS deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/arrow/lib/ REQUIRED NO_DEFAULT_PATH)
    endif()
endif ()

if (WIN32)
    # Windows
    option(DYNAMIC_RUNTIME "Dynamic runtime" OFF)
    set(VSDIR "vs15" CACHE STRING "Used to specify visual studio version of libsnowflakeclient dependecies")
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
    find_library(OOB_LIB libtelemetry_a.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/oob/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(CURL_LIB libcurl_a.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/curl/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(SSL_LIB libssl_a.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/openssl/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(CRYPTO_LIB libcrypto_a.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/openssl/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(ZLIB_LIB zlib_a.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/zlib/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_CORE_LIB aws-cpp-sdk-core.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/aws/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_S3_LIB aws-cpp-sdk-s3.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/aws/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_STS_LIB aws-cpp-sdk-sts.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/aws/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_C_AUTH_LIB aws-c-auth.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/aws/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_C_CAL_LIB aws-c-cal.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/aws/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_C_COMMON_LIB aws-c-common.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/aws/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_C_COMPRESSION_LIB aws-c-compression.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/aws/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_C_EVENT_STREAM_LIB aws-c-event-stream.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/aws/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_C_CHECKSUMS_LIB aws-checksums.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/aws/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_C_HTTP_LIB aws-c-http.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/aws/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_C_IO_LIB aws-c-io.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/aws/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_C_MQTT_LIB aws-c-mqtt.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/aws/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_CRT_CPP_LIB aws-crt-cpp.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/aws/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_C_S3_LIB aws-c-s3.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/aws/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(AWS_C_SDKUTILS_LIB aws-c-sdkutils.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/aws/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(AZURE_STORAGE_LITE_LIB azure-storage-lite.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/azure/lib/ REQUIRED NO_DEFAULT_PATH)
	if ($ENV{ARROW_FROM_SOURCE})
        find_library(BOOST_FILESYSTEM_LIB libboost_filesystem.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/boost/lib/ REQUIRED NO_DEFAULT_PATH)
        find_library(BOOST_REGEX_LIB libboost_regex.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/boost/lib/ REQUIRED NO_DEFAULT_PATH)
        find_library(BOOST_SYSTEM_LIB libboost_system.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/boost/lib/ REQUIRED NO_DEFAULT_PATH)
        find_library(BOOST_URL_LIB libboost_url.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/boost/lib/ REQUIRED NO_DEFAULT_PATH)
    else()
        if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
            find_library(BOOST_FILESYSTEM_LIB boost_filesystem-vc140-mt-gd.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/boost/lib/ REQUIRED NO_DEFAULT_PATH)
            find_library(BOOST_REGEX_LIB boost_regex-vc140-mt-gd.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/boost/lib/ REQUIRED NO_DEFAULT_PATH)
            find_library(BOOST_SYSTEM_LIB boost_system-vc140-mt-gd.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/boost/lib/ REQUIRED NO_DEFAULT_PATH)
        else()
            message(deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/boost/lib/)
            find_library(BOOST_FILESYSTEM_LIB boost_filesystem-vc140-mt.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/boost/lib/ REQUIRED NO_DEFAULT_PATH)
            find_library(BOOST_REGEX_LIB boost_regex-vc140-mt.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/boost/lib/ REQUIRED NO_DEFAULT_PATH)
            find_library(BOOST_SYSTEM_LIB boost_system-vc140-mt.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/boost/lib/ REQUIRED NO_DEFAULT_PATH)
        endif()
    endif()
    # No arrow on Win-32bit
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        if ($ENV{ARROW_FROM_SOURCE})
            find_library(ARROW_ARROW_LIB arrow_static.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow/lib/ REQUIRED NO_DEFAULT_PATH)
        else()
            find_library(ARROW_ARROW_LIB arrow.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow/lib/ REQUIRED NO_DEFAULT_PATH)
        # The following file names differ based on build type
            if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
                find_library(ARROW_BZ2_LIB bz2d.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
                find_library(ARROW_GFLAGS_LIB gflags_static_debug.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
                find_library(ARROW_LZ4_LIB lz4d.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
                find_library(ARROW_SNAPPY_LIB snappyd.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
                find_library(ARROW_ZLIB_LIB zlibd.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
                find_library(ARROW_ZSTD_LIB zstdd.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
            else()
                find_library(ARROW_BZ2_LIB bz2.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
                find_library(ARROW_GFLAGS_LIB gflags_static.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
                find_library(ARROW_LZ4_LIB lz4.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
                find_library(ARROW_SNAPPY_LIB snappy.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
                find_library(ARROW_ZLIB_LIB zlib.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
                find_library(ARROW_ZSTD_LIB zstd.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
            endif()
            find_library(ARROW_BROTLICOMMON_LIB brotlicommon-static.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
            find_library(ARROW_BROTLIDEC_LIB brotlidec-static.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
            find_library(ARROW_BROTLIENC_LIB brotlienc-static.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
            find_library(ARROW_DOUBLECONVERSION_LIB double-conversion.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
            find_library(ARROW_EVENT_LIB event.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
            find_library(ARROW_EVENTCORE_LIB event_core.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
            find_library(ARROW_EVENTEXTRA_LIB event_extra.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
            find_library(ARROW_FLATBUFFERS_LIB flatbuffers.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
            find_library(ARROW_GLOG_LIB glog.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow_deps/lib/ REQUIRED NO_DEFAULT_PATH)
        endif()
    endif()
    if(MOCK)
        find_library(CMOCKA_LIB cmocka_a.lib PATHS deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/cmocka/lib/ REQUIRED NO_DEFAULT_PATH)
    endif()
endif ()

if (LINUX)
    include_directories(
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/oob/include
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/curl/include
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/openssl/include
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/zlib/include
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/boost/include
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/arrow/include
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/aws/include
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/azure/include
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/cmocka/include
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/uuid/include
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/picojson/include
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/tomlplusplus/include
            include
            lib)
endif()

if (APPLE)
    include_directories(
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/oob/include
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/curl/include
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/openssl/include
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/zlib/include
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/boost/include
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/arrow/include
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/aws/include
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/azure/include
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/cmocka/include
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/picojson/include
            deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/tomlplusplus/include
            include
            lib)
endif()

if (WIN32)
    include_directories(
            deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/oob/include
            deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/curl/include
            deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/openssl/include
            deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/zlib/include
            deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/boost/include
            deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/aws/include
            deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/azure/include
            deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/cmocka/include
            deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/picojson/include
            deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/tomlplusplus/include
            include
            lib)
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        include_directories(deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow/include)
    endif ()
endif()

message("libtelemetry is located at " ${OOB_LIB})
message("libcurl is located at " ${CURL_LIB})
message("libssl is located at " ${SSL_LIB})
message("libcrypto is located at " ${CRYPTO_LIB})
message("libboost_regex is located at " ${BOOST_REGEX_LIB})
message("libarrow is located at " ${ARROW_ARROW_LIB})
message("libaws-cpp-sdk-core is located at " ${AWS_CORE_LIB})
message("libaws-cpp-sdk-s3 is located at " ${AWS_S3_LIB})
message("libazure-storage-lite.a is located at " ${AZURE_STORAGE_LITE_LIB})
if (LINUX)
    message("libuuid is located at " ${UUID_LIB})
endif ()
if (MOCK)
    message("libcmocka is located at " ${CMOCKA_LIB})
endif ()

add_library(snowflakeclient STATIC ${SOURCE_FILES} ${SOURCE_FILES_PUT_GET} ${SOURCE_FILES_CPP_WRAPPER})
target_compile_features(snowflakeclient PUBLIC cxx_std_17)
target_compile_features(snowflakeclient PUBLIC c_std_99)
if (UNIX)
    target_compile_definitions(snowflakeclient PUBLIC _LARGEFILE64_SOURCE)
endif ()

if (LINUX)
    target_compile_options(snowflakeclient PUBLIC -pthread)
    target_link_options(snowflakeclient PUBLIC -pthread)
endif ()
#set (CMAKE_CXX_STANDARD 11)

if(LINUX)
    link_directories(
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/oob/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/curl/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/openssl/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/zlib/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/arrow/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/arrow_deps/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/aws/lib64"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/azure/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/boost/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/uuid/lib"
    )
    if(MOCK)
        link_directories("${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/cmocka/lib")
    endif()
elseif(WIN32)
    link_directories(
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/oob/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/curl/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/openssl/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/aws/lib"
    )
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        link_directories(
                "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow/lib"
                "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/arrow_deps/lib"
                "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/boost/lib"
        )
    endif()
    if(MOCK)
        link_directories("${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/cmocka/lib")
    endif()
elseif()
    link_directories(
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/oob/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/curl/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/openssl/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/arrow/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/arrow_deps/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/azure/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/boost/lib"
    )
    if(MOCK)
        link_directories("${CMAKE_CURRENT_SOURCE_DIR}/deps-build/${PLATFORM}/${CMAKE_BUILD_TYPE}/cmocka/lib")
    endif()
endif()

if (LINUX)
    # Linux
    target_link_libraries(snowflakeclient rt dl z)
endif ()
if (APPLE)
    # OSX. no librt is required.
    target_link_libraries(snowflakeclient dl z)
endif ()

add_subdirectory(examples)

if (BUILD_TESTS)
    add_subdirectory(tests)
endif()
