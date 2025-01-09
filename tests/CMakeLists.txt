# Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.

# include public headers and test utils
include_directories(
        "../include"
        "utils"
        "../cpp"
)

SET(TESTS_C
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
        test_native_timestamp
        test_stmt_functions
        test_ocsp_fail_open
        test_multiple_statements
# FEATURE_INCREASED_MAX_LOB_SIZE_IN_MEMORY is internal switch
# will enable lob test when the change on server side will be published
#        test_lob
#        test_stats
# This test needs the Oauth token, so it should be run manually
       test_manual_connect
        )

SET (TESTS_C_UNIT
        test_unit_connect_parameters
        test_unit_logger
        test_unit_retry_context
        test_get_query_result_response
        test_get_describe_only_query_result
        test_unit_oauth
        test_unit_mfa_auth)

SET(TESTS_CXX
        test_unit_jwt
        test_unit_cipher_stream_buf
        test_unit_cred_renew
        test_unit_encryption
        test_unit_file_metadata_init
        test_unit_file_type_detect
        test_unit_stream_splitter
        test_unit_put_retry
        test_unit_put_fast_fail
        test_unit_put_get_fips
        test_unit_put_get_gcs
        test_unit_thread_pool
        test_unit_base64
        test_jwt
        #test_cpp_select1
        test_unit_proxy
        test_unit_oob
        test_unit_set_get_attributes
        test_unit_snowflake_types_to_string
        test_unit_azure_client
        test_unit_query_context_cache
        test_unit_sfurl)

SET(TESTS_PUTGET
        test_include_aws
        test_parallel_upload_download
        test_simple_put)

SET(TESTS_PERF
        test_perf_string_reads_and_writes
        test_perf_column_evaluation
        test_perf_type_conversion)

SET(TESTS_MOCK
        test_mock_service_name
        test_mock_session_gone)

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

SET(TEST_DATA
        TestOrcFile.test1.orc
        brotli_sample.txt.br
        decode
        decode2
        encode
        encode2
        large_file.csv.gz
        nation.impala.parquet
        one_byte.csv
        public_domain_books.csv
        small_file.csv
        small_file.csv.bz2
        small_file.csv.gz
        small_file.csv.zst
        zero_byte.csv
)

FOREACH (TD ${TEST_DATA})
    configure_file("data/${TD}" "data/${TD}" COPYONLY)
ENDFOREACH ()

message("CLOUD_PROVIDER is set to " $ENV{CLOUD_PROVIDER})
#Due to docker issue the tests are failing on linux.
if (APPLE OR WIN32 OR CLIENT_CODE_COVERAGE)
    set(TESTS_C ${TESTS_C})
    set(TESTS_CXX ${TESTS_CXX} ${TESTS_PUTGET})
elseif(DEFINED ENV{GITHUB_ACTIONS})
    if ("$ENV{BUILD_TYPE}" STREQUAL "Debug")
        message("Skip CXX test on github Linux Debug for disk space issue")
        set (TESTS_CXX "")
    endif ()
endif ()

if (UNIX)
    find_library(CMOCKA_LIB libcmocka.a PATHS ../deps-build/${PLATFORM}/cmocka/lib/ REQUIRED NO_DEFAULT_PATH)
    find_library(AZURE_STORAGE_LITE_LIB libazure-storage-lite.a PATHS ../deps-build/${PLATFORM}/azure/lib/ REQUIRED NO_DEFAULT_PATH)
    find_file(VALGRIND_SUPPRESSION valgrind_suppressions.txt utils)
endif ()

if (WIN32)
    find_library(CMOCKA_LIB cmocka_a.lib PATHS ../deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/cmocka/lib/ REQUIRED NO_DEFAULT_PATH)
    if ($ENV{ARROW_FROM_SOURCE})
        set(BOOST_ALL_LIBS ${BOOST_FILESYSTEM_LIB} ${BOOST_SYSTEM_LIB})
    else ()
        set(BOOST_ALL_LIBS ${BOOST_FILESYSTEM_LIB} ${BOOST_SYSTEM_LIB} ${BOOST_REGEX_LIB})
    endif ()
endif ()

message("libcmocka is located at " ${CMOCKA_LIB})
if (LINUX)
    message("libuuid.a is located at " ${UUID_LIB})
endif ()
message("valgrind suppression file is located at " ${VALGRIND_SUPPRESSION})

if (LINUX)
    set(TESTLIB_OPTS_C snowflakeclient ${CMOCKA_LIB} ${ARROW_ALL_LIBS} ${AZURE_STORAGE_LITE_LIB} ${AWS_ALL_LIBS} -Wl,--whole-archive telemetry curl ssl crypto uuid
            -Wl,--no-whole-archive pthread -Wl,--as-needed -static-libgcc -static-libstdc++)
    set(TESTLIB_OPTS_CXX snowflakeclient ${CMOCKA_LIB} ${ARROW_ALL_LIBS} ${AZURE_STORAGE_LITE_LIB} ${AWS_ALL_LIBS} -Wl,--whole-archive telemetry curl ssl crypto uuid
            pthread -Wl,--no-whole-archive -Wl,--as-needed -static-libgcc -static-libstdc++)
endif ()
if (APPLE)
    set(TESTLIB_OPTS_C snowflakeclient ${CMOCKA_LIB} ${ARROW_ALL_LIBS} z m "-framework CoreFoundation -framework SystemConfiguration -framework security" -all_load
            ${OOB_LIB} ${CURL_LIB} ${SSL_LIB} ${CRYPTO_LIB} pthread ${AZURE_STORAGE_LITE_LIB} ${AWS_ALL_LIBS} ${UUID_LIB})
    set(TESTLIB_OPTS_CXX snowflakeclient ${CMOCKA_LIB} ${ARROW_ALL_LIBS} z m "-framework CoreFoundation -framework SystemConfiguration -framework security" -all_load ${CURL_LIB}
            ${OOB_LIB} ${SSL_LIB} ${CRYPTO_LIB} pthread ${AZURE_STORAGE_LITE_LIB} ${AWS_ALL_LIBS} ${UUID_LIB})
endif()
if (WIN32)
    if (WIN32_DEBUG)
        if(CMAKE_SIZEOF_VOID_P EQUAL 4)
            set(TESTLIB_OPTS_C snowflakeclient ${CMOCKA_LIB} ${ARROW_ALL_LIBS} ${AWS_ALL_LIBS} ${OOB_LIB} ${CURL_LIB} ${SSL_LIB} ${CRYPTO_LIB} ${ZLIB_LIB} ${AZURE_STORAGE_LITE_LIB}
                    Version.lib Userenv.lib Bcrypt.lib ucrtd.lib Secur32.lib Ncrypt.lib Shlwapi.lib)
            set(TESTLIB_OPTS_CXX snowflakeclient ${CMOCKA_LIB} ${ARROW_ALL_LIBS} ${AWS_ALL_LIBS} ${OOB_LIB} ${CURL_LIB} ${SSL_LIB} ${CRYPTO_LIB} ${ZLIB_LIB} ${AZURE_STORAGE_LITE_LIB}
                    Version.lib Userenv.lib Bcrypt.lib ucrtd.lib Secur32.lib Ncrypt.lib Shlwapi.lib)
        endif()
    else()
        set(TESTLIB_OPTS_C snowflakeclient ${CMOCKA_LIB} ${AWS_ALL_LIBS} ${AZURE_STORAGE_LITE_LIB} ${ARROW_ALL_LIBS} ${OOB_LIB} ${CURL_LIB} ${SSL_LIB} ${CRYPTO_LIB} ${ZLIB_LIB}
                Version.lib Userenv.lib Bcrypt.lib Secur32.lib Ncrypt.lib Shlwapi.lib)
        set(TESTLIB_OPTS_CXX snowflakeclient ${CMOCKA_LIB} ${AWS_ALL_LIBS} ${AZURE_STORAGE_LITE_LIB} ${ARROW_ALL_LIBS} ${CURL_LIB} ${OOB_LIB} ${SSL_LIB} ${CRYPTO_LIB} ${ZLIB_LIB}
                Version.lib Userenv.lib Bcrypt.lib Secur32.lib Ncrypt.lib Shlwapi.lib)
    endif()
endif()
set(TESTLIB_OPTS_C_UNIT ${TESTLIB_OPTS_C})
if (BUILD_SHARED_LIBS)
    set(TESTLIB_OPTS_C snowflakeclient_dynamic ${CMOCKA_LIB})
endif()

if (MOCK)
    FOREACH (T ${TESTS_MOCK})
        add_executable(${T} ${SOURCE_UTILS} mock/${T}.c)
        if (WIN32)
            target_include_directories(
                    ${T} PUBLIC
                    ../deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/cmocka/include
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
                    ../deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/cmocka/include
            )
            set_target_properties(
		    ${T} PROPERTIES LINK_FLAGS "/ignore:4099")
        else()
            target_include_directories(
                    ${T} PUBLIC
                    ../deps-build/${PLATFORM}/cmocka/include
            )
        endif()
        target_link_libraries(${T} ${TESTLIB_OPTS_C})
        add_test(${T} ${T})
        # Removed to suppress false alarm: "--run-libc-freeres=no"
        if (LINUX)
          if ("$(CMAKE_OS_ARCH)" STREQUAL "x86_64")
            add_test(valgrind_${T}
                valgrind
                --tool=memcheck
                --leak-check=full
                --error-exitcode=1
                --suppressions=${VALGRIND_SUPPRESSION}
                ./${T})
          endif()
        endif()
    ENDFOREACH ()

    FOREACH (T ${TESTS_C_UNIT})
        add_executable(${T} ${SOURCE_UTILS} ${T}.c)
        if (WIN32)
            target_include_directories(
                    ${T} PUBLIC
                    ../deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/cmocka/include
            )
            set_target_properties(
		    ${T} PROPERTIES LINK_FLAGS "/ignore:4099")
        else()
            target_include_directories(
                    ${T} PUBLIC
                    ../deps-build/${PLATFORM}/cmocka/include
            )
        endif()
        target_link_libraries(${T} ${TESTLIB_OPTS_C_UNIT})
        add_test(${T} ${T})
        # Removed to suppress false alarm: "--run-libc-freeres=no"
        if (LINUX)
          if ("$(CMAKE_OS_ARCH)" STREQUAL "x86_64")
            add_test(valgrind_${T}
                valgrind
                --tool=memcheck
                --leak-check=full
                --error-exitcode=1
                --suppressions=${VALGRIND_SUPPRESSION}
                ./${T})
          endif()
        endif()
    ENDFOREACH ()

    if (NOT WIN32_DEBUG)
    # Skip building CXX tests for Win32 Debug. The build fails
    # with error LNK2001: unresolved external symbol __CrtDbgReportW
    FOREACH(T ${TESTS_CXX})
        add_executable(${T} ${SOURCE_UTILS} ${SOURCE_UTILS_CXX} ${T}.cpp)
        if (WIN32)
            target_include_directories(
                    ${T} PUBLIC
                    ../deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/cmocka/include
                    ../deps-build/${PLATFORM}/${VSDIR}/${CMAKE_BUILD_TYPE}/openssl/include
            )
            set_target_properties(
		    ${T} PROPERTIES LINK_FLAGS "/ignore:4099")
        else()
            target_include_directories(
                    ${T} PUBLIC
                    ../deps-build/${PLATFORM}/cmocka/include
                    ../deps-build/${PLATFORM}/openssl/include
            )
        endif()
        target_link_libraries(${T} ${TESTLIB_OPTS_CXX})
        add_test(${T} ${T})
        # Removed to suppress false alarm: "--run-libc-freeres=no"
        if (LINUX)
          if ("$(CMAKE_OS_ARCH)" STREQUAL "x86_64")
            add_test(valgrind_${T}
                 valgrind
                 --tool=memcheck
                 --leak-check=full
                 --error-exitcode=1
                 --suppressions=${VALGRIND_SUPPRESSION}
                 ./${T})
          endif()
        endif()
    ENDFOREACH()
    ENDIF ()

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
            set_tests_properties(${T} PROPERTIES ENVIRONMENT CABUNDLE_PATH=${SNOWFLAKE_TEST_CA_BUNDLE_FILE})
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

if (UNIX)
    add_subdirectory(unit_test_ocsp)
endif()
add_subdirectory(test_bind_datastructures)
