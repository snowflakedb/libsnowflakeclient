project(test_oauth_wiremock)

add_subdirectory(wiremock)

set(OAUTH_COMMON_SOURCES
    UnitOAuthBase.cpp
    UnitOAuthBase.hpp
    AuthenticationChallengeTestProvider.hpp
    AuthenticationWebBrowserTestRunner.hpp
    wiremock.cpp
    wiremock.hpp
    ../utils/test_setup.h
    ../utils/TestSetup.hpp
    ../utils/test_setup.c
    ../utils/TestSetup.cpp
)

add_executable(test_oauth_authorization_flow
    ${OAUTH_COMMON_SOURCES}
    UnitOauthAuthorizationFlowTest.cpp
)

add_executable(test_oauth_client_credentials
    ${OAUTH_COMMON_SOURCES}
    UnitOAuthClientCredentialsTest.cpp
)

add_compile_options(-Wall -Wno-error=ignored-attributes)

target_link_libraries(test_oauth_authorization_flow ${TESTLIB_OPTS_C})
target_link_libraries(test_oauth_client_credentials ${TESTLIB_OPTS_C})

add_test(test_oauth_authorization_flow test_oauth_authorization_flow)
add_test(test_oauth_authorization_flow test_oauth_authorization_flow)