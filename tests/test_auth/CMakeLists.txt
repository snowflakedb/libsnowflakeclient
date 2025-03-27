project(test_auth)

add_executable(
        test_auth
        test_oauth.c
        test_okta.c
        test_external_browser.c
        test_runner.c
        auth_utils.c
        ../utils/test_setup.c
)

target_link_libraries(test_auth ${TESTLIB_OPTS_C})

add_test(test_auth test_auth)

set_tests_properties(test_auth PROPERTIES TIMEOUT 120)
