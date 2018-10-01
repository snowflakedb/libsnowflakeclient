project(test_ocsp)

set(CMAKE_C_FLAGS "-std=c++14 -pthread -DSIMBA -D_REENTRANT -DCLUNIX -Wall")

add_executable(
        test_ocsp
        test_ocsp.c)

if (LINUX)
    target_link_libraries(test_ocsp
            rt dl z m
            curl ssl crypto
            )
endif ()