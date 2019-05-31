# Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.

# include public headers and test utils
include_directories(
        "../include"
        "utils")

set(SOURCE_UTILS
        utils/example_setup.c
        utils/example_setup.h)

if (LINUX)
    FOREACH(EX ${EXAMPLES})
        add_executable(ex_${EX} ${SOURCE_UTILS} ${EX}.c)
        target_link_libraries(
                ex_${EX}
                snowflakeclient
                -Wl,--whole-archive curl ssl crypto -Wl,--no-whole-archive
        )
    ENDFOREACH ()
endif ()
if (APPLE)
    FOREACH (EX ${EXAMPLES})
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
    FOREACH (EX ${EXAMPLES})
        add_executable(ex_${EX} ${SOURCE_UTILS} ${EX}.c)
        target_link_libraries(
                ex_${EX}
                snowflakeclient
                libcurl_a libssl_a libcrypto_a zlib_a
        )
    ENDFOREACH ()
endif ()
