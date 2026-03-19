project(test_default_auth)

configure_file(login_response.json . COPYONLY)

add_executable(test_default_auth
    ../wiremock/wiremock.cpp
    ../wiremock/wiremock.hpp
    ../utils/test_setup.h
    ../utils/TestSetup.hpp
    ../utils/test_setup.c
    ../utils/TestSetup.cpp
    test_default_auth.cpp
)

add_compile_options(-Wall -Wno-error=ignored-attributes)

target_link_libraries(test_default_auth ${TESTLIB_OPTS_C})

add_test(test_default_auth test_default_auth)
