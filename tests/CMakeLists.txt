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
        test_bind_results
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
        test_perf_string_reads_and_writes
        test_perf_column_evaluation
        test_perf_type_conversion
        )

SET(TESTS_CXX
        test_include_aws
        test_parallel_upload_download
        test_simple_put
        test_unit_cipher_stream_buf
        test_unit_cred_renew
        test_unit_file_metadata_init
        test_unit_file_type_detect
        test_unit_stream_splitter
        test_unit_thread_pool)
        #test_cpp_select1)

set(SOURCE_UTILS
        utils/test_setup.c
        utils/test_setup.h)

set(SOURCE_UTILS_CXX
        utils/TestSetup.cpp
        utils/TestSetup.hpp)

if (UNIX)
    find_library(CMOCKA_LIB libcmocka.a PATHS ../deps-build/${PLATFORM}/cmocka/lib/ REQUIRED)
    find_file(VALGRIND_SUPPRESSION valgrind_suppressions.txt utils)
endif ()

if (WIN32)
    find_library(CMOCKA_LIB cmocka_a.lib PATHS ../deps-build/${PLATFORM}/cmocka/lib/ REQUIRED)
endif ()

message("libcmocka is located at " ${CMOCKA_LIB})
message("valgrind suppression file is located at " ${VALGRIND_SUPPRESSION})

if (LINUX)
    set(TESTLIB_OPTS_C snowflakeclient ${CMOCKA_LIB} -Wl,--whole-archive curl ssl crypto
            -Wl,--no-whole-archive pthread)
    set(TESTLIB_OPTS_CXX snowflakeclient ${CMOCKA_LIB} -Wl,--whole-archive curl
            ssl crypto pthread aws-cpp-sdk-core aws-cpp-sdk-s3 -Wl,--no-whole-archive)
endif ()
if (APPLE)
    set(TESTLIB_OPTS_C snowflakeclient ${CMOCKA_LIB} z m "-framework CoreFoundation" -all_load
            ${CURL_LIB} ${SSL_LIB} ${CRYPTO_LIB} ${AWS_CORE_LIB} ${AWS_S3_LIB})
    set(TESTLIB_OPTS_CXX snowflakeclient ${CMOCKA_LIB} z m "-framework CoreFoundation" -all_load ${CURL_LIB}
            ${SSL_LIB} ${CRYPTO_LIB} pthread ${AWS_CORE_LIB} ${AWS_S3_LIB})
endif()
if (WIN32)
    set(TESTLIB_OPTS_C snowflakeclient ${CMOCKA_LIB} ${CURL_LIB} ${SSL_LIB} ${CRYPTO_LIB} ${ZLIB_LIB})
	set(TESTLIB_OPTS_CXX snowflakeclient ${CMOCKA_LIB} ${CURL_LIB} ${SSL_LIB} ${CRYPTO_LIB} ${AWS_S3_LIB} ${AWS_CORE_LIB} 
			Version.lib Userenv.lib Bcrypt.lib)
endif()

FOREACH (T ${TESTS_C})
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


FOREACH(T ${TESTS_CXX})
    add_executable(${T} ${SOURCE_UTILS} ${SOURCE_UTILS_CXX} ${T}.cpp)
    target_include_directories(
            ${T} PUBLIC
            ../deps-build/${PLATFORM}/cmocka/include
    )
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

