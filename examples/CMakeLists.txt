# Copyright (c) 2017 Snowflake Computing, Inc. All rights reserved.

# includes catch.hpp, a test framework
include_directories(
        "../include"
        "utils"
        "../cpp"
)

SET(EXAMPLES
        connect
        connect_negative
        change_current
        select1
        transaction
        ping_pong
        stmt_with_bad_connect
        crud
        error_handling
        large_result_set
        selectvar
        selectnum
        selectbool
        selectbin
        timezone
        selectnull
        selecttsntz
        selecttsltz
        selecttstz
        selecttime
        check_ctypes
        bind_params
        bind_results)

set(EXAMPLES_CPP
        simple_put_get)

set(SOURCE_UTILS
        example_setup.c)

if (LINUX)
    FOREACH(EX ${EXAMPLES})
        add_executable(ex_${EX} ${SOURCE_UTILS} ${EX}.c)
        target_link_libraries(
            ex_${EX}
            snowflakeclient
            -Wl,--whole-archive curl ssl crypto -Wl,--no-whole-archive
        )
    ENDFOREACH()

    FOREACH(EX ${EXAMPLES_CPP})
        add_executable(ex_${EX} ${SOURCE_UTILS} ${EX}.cpp)
        target_link_libraries(
                ex_${EX}
                snowflakeclient
                -Wl,--whole-archive curl ssl crypto -Wl,--no-whole-archive
        )
    ENDFOREACH()
endif()
if (APPLE)
    FOREACH(EX ${EXAMPLES})
        add_executable(ex_${EX} ${SOURCE_UTILS} ${EX}.c)
        target_link_libraries(
            ex_${EX}
            z m
            "-framework CoreFoundation"
            snowflakeclient
            -all_load curl ssl crypto
        )
    ENDFOREACH()
endif()
if (WIN32)
    FOREACH(EX ${EXAMPLES})
        add_executable(ex_${EX} ${SOURCE_UTILS} ${EX}.c)
        target_link_libraries(
            ex_${EX}
            snowflakeclient
            libcurl_a libssl_a libcrypto_a zlib_a
		)
    ENDFOREACH()
endif()
