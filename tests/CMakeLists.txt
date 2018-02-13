# Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.

# include public headers and test utils
include_directories(
        "../include"
        "utils"
)

SET(TESTS
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
        )

set(SOURCE_UTILS
        utils/test_setup.c
        utils/test_setup.h)

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
    set(TESTLIB_OPTS snowflakeclient ${CMOCKA_LIB} -Wl,--whole-archive curl ssl crypto -Wl,--no-whole-archive)
endif ()
if (APPLE)
    set(TESTLIB_OPTS snowflakeclient ${CMOCKA_LIB} z m "-framework CoreFoundation" -all_load curl ssl crypto)
endif()
if (WIN32)
    set(TESTLIB_OPTS snowflakeclient ${CMOCKA_LIB} libcurl_a libssl_a libcrypto_a zlib_a)
endif()

FOREACH (T ${TESTS})
    add_executable(${T} ${SOURCE_UTILS} ${T}.c)
    target_include_directories(
            ${T} PUBLIC
            ../deps-build/${PLATFORM}/cmocka/include
    )
    target_link_libraries(${T} ${TESTLIB_OPTS})
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
