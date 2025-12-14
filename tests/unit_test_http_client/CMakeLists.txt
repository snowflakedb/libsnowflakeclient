project(test_unit_http_client)

set(WIREMOCK_CONFIGS
    get_request.json
    post_request.json
    put_request.json
    delete_request.json
    custom_headers.json
    error_responses.json
    large_response.json
)

add_executable(test_unit_http_client
    ../wiremock/wiremock.cpp
    ../wiremock/wiremock.hpp
    ../utils/test_setup.h
    ../utils/TestSetup.hpp
    ../utils/test_setup.c
    ../utils/TestSetup.cpp
    test_unit_http_client.cpp
)

add_compile_options(-Wall -Wno-error=ignored-attributes)

target_link_libraries(test_unit_http_client ${TESTLIB_OPTS_CXX})

add_test(test_unit_http_client test_unit_http_client)

FOREACH (CONFIG ${WIREMOCK_CONFIGS})
    configure_file("${CONFIG}" "${CONFIG}" COPYONLY)
ENDFOREACH ()
