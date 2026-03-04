project(test_platform_detection)

configure_file(aws_ec2_instance_success.json . COPYONLY)
configure_file(aws_identity_success.json . COPYONLY)
configure_file(azure_managed_identity_success.json . COPYONLY)
configure_file(azure_vm_success.json . COPYONLY)
configure_file(gce_identity_success.json . COPYONLY)
configure_file(gce_vm_success.json . COPYONLY)
configure_file(timeout_response.json . COPYONLY)

add_executable(test_platform_detection
    ../wiremock/wiremock.cpp
    ../wiremock/wiremock.hpp
    ../utils/test_setup.h
    ../utils/TestSetup.hpp
    ../utils/test_setup.c
    ../utils/TestSetup.cpp
    test_platform_detection.cpp
)

add_compile_options(-Wall -Wno-error=ignored-attributes)

target_link_libraries(test_platform_detection ${TESTLIB_OPTS_C})

add_test(test_platform_detection test_platform_detection)

