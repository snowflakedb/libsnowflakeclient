project(test_redirect)

configure_file(http_307_retry.json . COPYONLY)
configure_file(http_308_retry.json . COPYONLY)

add_executable(test_redirect
    ../wiremock/wiremock.cpp
    ../wiremock/wiremock.hpp
    ../utils/test_setup.h
    ../utils/TestSetup.hpp
    ../utils/test_setup.c
    ../utils/TestSetup.cpp
    test_redirect.cpp
)

add_compile_options(-Wall -Wno-error=ignored-attributes)

target_link_libraries(test_redirect ${TESTLIB_OPTS_C})

add_test(test_redirect test_redirect)

