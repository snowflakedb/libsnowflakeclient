project(test_ocsp)

add_executable(
        test_ocsp
        test_ocsp.c)

target_link_libraries(test_ocsp ${TESTLIB_OPTS_C})
target_compile_definitions(test_ocsp PRIVATE SIMBA _REENTRANT CLUNIX)

add_test(test_ocsp test_ocsp)
