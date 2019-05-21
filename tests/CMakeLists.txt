# Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.

# include public headers and test utils
include_directories(
        "../include"
        "utils"
        "../cpp"
)

SET(TESTS_C
        test_unit_connect_parameters
        test_unit_logger
        test_connect
        test_connect_negative
        test_bind_params
        test_bind_named_params
        test_change_current
        test_select1
        test_check_ctypes
        test_crud
        test_error_handlings
        test_large_result_set
        test_transaction
        test_ping_pong
        test_stmt_with_bad_connect
        test_binary
        test_bool
        test_null
        test_number
        test_variant
        test_time
        test_timestamp_ntz
        test_timestamp_ltz
        test_timestamp_tz
        test_timezone
        test_adjust_fetch_data
        test_issue_76
        test_column_fetch
        test_native_timestamp)

SET(TESTS_CXX
        test_unit_jwt
        test_unit_cipher_stream_buf
        test_unit_cred_renew
        test_unit_file_metadata_init
        test_unit_file_type_detect
        test_unit_stream_splitter
        test_unit_thread_pool
        test_unit_base64
        #test_cpp_select1
        test_unit_proxy)

SET(TESTS_AWS
        test_include_aws
        test_parallel_upload_download
        test_simple_put)

SET(TESTS_AZURE
        test_simple_put_azure)

SET(TESTS_PERF
        test_perf_string_reads_and_writes
        test_perf_column_evaluation
        test_perf_type_conversion)

SET(TESTS_MOCK
        test_mock_service_name)

set(SOURCE_UTILS
        utils/test_setup.c
        utils/test_setup.h
        utils/mock_endpoints.h
        utils/mock_setup.h
        utils/mock_setup.c)

set(SOURCE_UTILS_CXX
        utils/TestSetup.cpp
        utils/TestSetup.hpp
        utils/mock_endpoints.h
        utils/mock_setup.h
        utils/mock_setup.c)

# Only run PUT/GET tests on AWS deployments
if ("$ENV{SNOWFLAKE_CLOUD_ENV}" STREQUAL "AWS")
    message("SNOWFLAKE_CLOUD_ENV is set to " $ENV{SNOWFLAKE_CLOUD_ENV})
    set(TESTS_CXX ${TESTS_CXX} ${TESTS_AWS})
endif()


if (LINUX)
# Only run PUT/GET tests on Azure deployments
    if ("$ENV{SNOWFLAKE_CLOUD_ENV}" STREQUAL "AZURE")
        message("SNOWFLAKE_CLOUD_ENV is set to " $ENV{SNOWFLAKE_CLOUD_ENV})
        message("libazure-storage-lite.a is located at " ${AZURE_STORAGE_LITE_LIB})
        set(TESTS_CXX ${TESTS_CXX} ${TESTS_AZURE})
    endif()
endif()

if (UNIX)
    find_library(CMOCKA_LIB libcmocka.a PATHS ../deps-build/${PLATFORM}/cmocka/lib/ REQUIRED)
    find_library(AZURE_STORAGE_LITE_LIB libazure-storage-lite.a PATHS ../deps-build/${PLATFORM}/azure/lib/ REQUIRED)
    find_file(VALGRIND_SUPPRESSION valgrind_suppressions.txt utils)
endif ()

if (WIN32)
    find_library(CMOCKA_LIB cmocka_a.lib PATHS ../deps-build/${PLATFORM}/${VSDIR}/cmocka/lib/ REQUIRED)
endif ()

message("libcmocka is located at " ${CMOCKA_LIB})
message("libuuid.a is located at " ${UUID_LIB})
message("valgrind suppression file is located at " ${VALGRIND_SUPPRESSION})

if (LINUX)
    set(TESTLIB_OPTS_C snowflakeclient ${CMOCKA_LIB} ${AZURE_STORAGE_LITE_LIB} ${UUID_LIB} -Wl,--whole-archive curl ssl crypto
            -Wl,--no-whole-archive pthread)
    set(TESTLIB_OPTS_CXX snowflakeclient ${CMOCKA_LIB} ${AZURE_STORAGE_LITE_LIB} ${UUID_LIB} -Wl,--whole-archive curl
            ssl crypto pthread aws-cpp-sdk-core aws-cpp-sdk-s3 -Wl,--no-whole-archive)
endif ()
if (APPLE)
    set(TESTLIB_OPTS_C snowflakeclient ${CMOCKA_LIB} ${AZURE_STORAGE_LITE_LIB} ${UUID_LIB} z m "-framework CoreFoundation" -all_load
            ${CURL_LIB} ${SSL_LIB} ${CRYPTO_LIB} ${AWS_CORE_LIB} ${AWS_S3_LIB})
    set(TESTLIB_OPTS_CXX snowflakeclient ${CMOCKA_LIB} z m "-framework CoreFoundation" -all_load ${CURL_LIB}
            ${SSL_LIB} ${CRYPTO_LIB} pthread ${AWS_CORE_LIB} ${AWS_S3_LIB} ${AZURE_STORAGE_LITE_LIB} ${UUID_LIB})
endif()
if (WIN32)
    set(TESTLIB_OPTS_C snowflakeclient ${CMOCKA_LIB} ${CURL_LIB} ${SSL_LIB} ${CRYPTO_LIB} ${ZLIB_LIB})
	set(TESTLIB_OPTS_CXX snowflakeclient ${CMOCKA_LIB} ${CURL_LIB} ${SSL_LIB} ${CRYPTO_LIB} ${AWS_S3_LIB} ${AWS_CORE_LIB} ${AZURE_STORAGE_LITE_LIB}
			Version.lib Userenv.lib Bcrypt.lib)
endif()

if (MOCK)
    FOREACH (T ${TESTS_MOCK})
        add_executable(${T} ${SOURCE_UTILS} mock/${T}.c)
        if (WIN32)
            target_include_directories(
                    ${T} PUBLIC
                    ../deps-build/${PLATFORM}/${VSDIR}/cmocka/include
            )
        else()
            target_include_directories(
                    ${T} PUBLIC
                    ../deps-build/${PLATFORM}/cmocka/include
            )
        endif()
        target_link_libraries(${T} ${TESTLIB_OPTS_C})
        add_test(${T} ${T})
        # Removed to suppress false alarm: "--run-libc-freeres=no"
        add_test(valgrind_${T}
                valgrind
                --tool=memcheck
                --leak-check=full
                --error-exitcode=1
                --suppressions=${VALGRIND_SUPPRESSION}
                ./${T})
    ENDFOREACH ()
else()
    FOREACH (T ${TESTS_C})
        add_executable(${T} ${SOURCE_UTILS} ${T}.c)
        if (WIN32)
            target_include_directories(
                    ${T} PUBLIC
                    ../deps-build/${PLATFORM}/${VSDIR}/cmocka/include
            )
        else()
            target_include_directories(
                    ${T} PUBLIC
                    ../deps-build/${PLATFORM}/cmocka/include
            )
        endif()
        target_link_libraries(${T} ${TESTLIB_OPTS_C})
        add_test(${T} ${T})
        # Removed to suppress false alarm: "--run-libc-freeres=no"
        add_test(valgrind_${T}
                valgrind
                --tool=memcheck
                --leak-check=full
                --error-exitcode=1
                --suppressions=${VALGRIND_SUPPRESSION}
                ./${T})
    ENDFOREACH ()


    FOREACH(T ${TESTS_CXX})
        add_executable(${T} ${SOURCE_UTILS} ${SOURCE_UTILS_CXX} ${T}.cpp)
        if (WIN32)
            target_include_directories(
                    ${T} PUBLIC
                    ../deps-build/${PLATFORM}/${VSDIR}/cmocka/include
            )
        else()
            target_include_directories(
                    ${T} PUBLIC
                    ../deps-build/${PLATFORM}/cmocka/include
            )
        endif()
        target_link_libraries(${T} ${TESTLIB_OPTS_CXX})
        add_test(${T} ${T})
        # Removed to suppress false alarm: "--run-libc-freeres=no"
        add_test(valgrind_${T}
                 valgrind
                 --tool=memcheck
                 --leak-check=full
                 --error-exitcode=1
                 --suppressions=${VALGRIND_SUPPRESSION}
                 ./${T})
    ENDFOREACH()

    # Tests only work on Unix at the moment
    if (UNIX)
        FOREACH (T ${TESTS_PERF})
            add_executable(${T} ${SOURCE_UTILS} ${T}.c)
            target_include_directories(
                    ${T} PUBLIC
                    ../deps-build/${PLATFORM}/cmocka/include
            )
            target_link_libraries(${T} ${TESTLIB_OPTS_C})
            add_test(${T} ${T})
            # Removed to suppress false alarm: "--run-libc-freeres=no"
            add_test(valgrind_${T}
                    valgrind
                    --tool=memcheck
                    --leak-check=full
                    --error-exitcode=1
                    --suppressions=${VALGRIND_SUPPRESSION}
                    ./${T})
        ENDFOREACH ()
    endif ()


endif()

# Uncomment when you want to run the OCSP unit tests. Do not run as part of normal Travis testing
# add_subdirectory(unit_test_ocsp)
add_subdirectory(test_bind_datastructures)
