project(test_ocsp)

set(CMAKE_C_FLAGS "-pthread -DSIMBA -D_REENTRANT -DCLUNIX -Wall")

add_executable(
        test_ocsp
        test_ocsp.c)

target_link_libraries(test_ocsp ${TESTLIB_OPTS_C})

add_test(test_ocsp test_ocsp)
