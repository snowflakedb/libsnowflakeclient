project(test_qcc)

configure_file(login_response.json . COPYONLY)
configure_file(table_creation.json . COPYONLY)
configure_file(first_row_insertion.json . COPYONLY)
configure_file(duplicated_data_insertion.json . COPYONLY)

add_executable(test_qcc
    ../wiremock/wiremock.cpp
    ../wiremock/wiremock.hpp
    ../utils/test_setup.h
    ../utils/TestSetup.hpp
    ../utils/test_setup.c
    ../utils/TestSetup.cpp
    test_qcc.cpp
)

add_compile_options(-Wall -Wno-error=ignored-attributes)

target_link_libraries(test_qcc ${TESTLIB_OPTS_C})

add_test(test_qcc test_qcc)

