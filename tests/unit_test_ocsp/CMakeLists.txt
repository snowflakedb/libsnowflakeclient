project(test_ocsp)

add_executable(
        test_ocsp
        test_ocsp.c)

target_link_libraries(test_ocsp ${TESTLIB_OPTS_C})
target_compile_definitions(test_ocsp PRIVATE SIMBA _REENTRANT CLUNIX)

add_test(test_ocsp test_ocsp)

# validate Snowflake curl options exist at runtime and can be set
add_executable(
        test_curlopts_present
        test_curlopts_present.c)
target_link_libraries(test_curlopts_present ${TESTLIB_OPTS_C})
add_test(test_curlopts_present test_curlopts_present)

# extra cases for encodeUrlData coverage
add_executable(
        test_encode_url_extra
        test_encode_url_extra.c)
target_link_libraries(test_encode_url_extra ${TESTLIB_OPTS_C})
add_test(test_encode_url_extra test_encode_url_extra)

# OCSP verify injection/path-building test
add_executable(
        test_ocsp_verify_injection
        test_ocsp_verify_injection.c)
target_link_libraries(test_ocsp_verify_injection ${TESTLIB_OPTS_C})
add_test(test_ocsp_verify_injection test_ocsp_verify_injection)
