project(test_usr_pwd_mfa)

configure_file(login_response.json . COPYONLY)

add_executable(test_usr_pwd_mfa
    ../wiremock/wiremock.cpp
    ../wiremock/wiremock.hpp
    ../utils/test_setup.h
    ../utils/TestSetup.hpp
    ../utils/test_setup.c
    ../utils/TestSetup.cpp
    test_usr_pwd_mfa.cpp
)

add_compile_options(-Wall -Wno-error=ignored-attributes)

target_link_libraries(test_usr_pwd_mfa ${TESTLIB_OPTS_C})

add_test(test_usr_pwd_mfa test_usr_pwd_mfa)
