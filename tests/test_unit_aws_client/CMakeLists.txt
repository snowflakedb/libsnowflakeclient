project(test_usr_pwd_mfa)

configure_file(fail_first_upload_attempt.json . COPYONLY)

add_executable(test_unit_aws_client
    ../wiremock/wiremock.cpp
    ../wiremock/wiremock.hpp
    ../utils/test_setup.h
    ../utils/TestSetup.hpp
    ../utils/test_setup.c
    ../utils/TestSetup.cpp
    test_unit_aws_client.cpp
)

add_compile_options(-Wall -Wno-error=ignored-attributes)

target_link_libraries(test_unit_aws_client ${TESTLIB_OPTS_C})

add_test(test_unit_aws_client test_unit_aws_client)
