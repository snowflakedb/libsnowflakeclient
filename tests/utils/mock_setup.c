/*
 * Copyright (c) 2017-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include "mock_setup.h"
#include "mock_endpoints.h"

void setup_mock_login_service_name() {
    expect_string(__wrap_http_perform, url, MOCK_URL_SERVICE_NAME_LOGIN);
    expect_string(__wrap_http_perform, body, MOCK_BODY_SERVICE_NAME_LOGIN);
    expect_string(__wrap_http_perform, request_type_str, MOCK_REQUEST_TYPE_POST);
    expect_value(__wrap_http_perform, header->header_service_name, NULL);
    expect_value(__wrap_http_perform, header->header_token, NULL);
    will_return(__wrap_http_perform, cast_ptr_to_largest_integral_type(MOCK_RESPONSE_SERVICE_NAME_LOGIN));
}

void setup_mock_login_standard() {
    expect_string(__wrap_http_perform, url, MOCK_URL_STANDARD_LOGIN);
    expect_string(__wrap_http_perform, body, MOCK_BODY_STANDARD_LOGIN);
    expect_string(__wrap_http_perform, request_type_str, MOCK_REQUEST_TYPE_POST);
    expect_value(__wrap_http_perform, header->header_service_name, NULL);
    expect_value(__wrap_http_perform, header->header_token, NULL);
    will_return(__wrap_http_perform, cast_ptr_to_largest_integral_type(MOCK_RESPONSE_STANDARD_LOGIN));
}

void setup_mock_query_service_name() {
    expect_string(__wrap_http_perform, url, MOCK_URL_SERVICE_NAME_QUERY);
    expect_string(__wrap_http_perform, body, MOCK_BODY_STANDARD_QUERY);
    expect_string(__wrap_http_perform, request_type_str, MOCK_REQUEST_TYPE_POST);
    expect_string(__wrap_http_perform, header->header_service_name, MOCK_HEADER_SERVICE_NAME);
    expect_string(__wrap_http_perform, header->header_token, MOCK_HEADER_AUTH_TOKEN);
    will_return(__wrap_http_perform, cast_ptr_to_largest_integral_type(MOCK_RESPONSE_SERVICE_NAME_QUERY));
}

void setup_mock_query_session_gone() {
    expect_string(__wrap_http_perform, url, MOCK_URL_STANDARD_QUERY);
    expect_string(__wrap_http_perform, body, MOCK_BODY_STANDARD_QUERY);
    expect_string(__wrap_http_perform, request_type_str, MOCK_REQUEST_TYPE_POST);
    expect_value(__wrap_http_perform, header->header_service_name, NULL);
    expect_string(__wrap_http_perform, header->header_token, MOCK_HEADER_AUTH_TOKEN);
    will_return(__wrap_http_perform, cast_ptr_to_largest_integral_type(MOCK_RESPONSE_SESSION_GONE));
}

void setup_mock_query_standard() {
    expect_string(__wrap_http_perform, url, MOCK_URL_STANDARD_QUERY);
    expect_string(__wrap_http_perform, body, MOCK_BODY_STANDARD_QUERY);
    expect_string(__wrap_http_perform, request_type_str, MOCK_REQUEST_TYPE_POST);
    expect_value(__wrap_http_perform, header->header_service_name, NULL);
    expect_string(__wrap_http_perform, header->header_token, MOCK_HEADER_AUTH_TOKEN);
    will_return(__wrap_http_perform, cast_ptr_to_largest_integral_type(MOCK_RESPONSE_STANDARD_QUERY));
}

void setup_mock_delete_connection_service_name() {
    expect_string(__wrap_http_perform, url, MOCK_URL_DELETE_CONNECTION_SERVICE_NAME);
    expect_value(__wrap_http_perform, body, NULL);
    expect_string(__wrap_http_perform, request_type_str, MOCK_REQUEST_TYPE_POST);
    expect_string(__wrap_http_perform, header->header_service_name, MOCK_HEADER_SERVICE_NAME);
    expect_string(__wrap_http_perform, header->header_token, MOCK_HEADER_AUTH_TOKEN);
    will_return(__wrap_http_perform, cast_ptr_to_largest_integral_type(MOCK_RESPONSE_DELETE_CONNECTION));
}

void setup_mock_delete_connection_session_gone() {
    expect_string(__wrap_http_perform, url, MOCK_URL_DELETE_CONNECTION_STANDARD);
    expect_value(__wrap_http_perform, body, NULL);
    expect_string(__wrap_http_perform, request_type_str, MOCK_REQUEST_TYPE_POST);
    expect_value(__wrap_http_perform, header->header_service_name, NULL);
    expect_string(__wrap_http_perform, header->header_token, MOCK_HEADER_AUTH_TOKEN);
    will_return(__wrap_http_perform, cast_ptr_to_largest_integral_type(MOCK_RESPONSE_SESSION_GONE));
}

void setup_mock_delete_connection_standard() {
    expect_string(__wrap_http_perform, url, MOCK_URL_DELETE_CONNECTION_STANDARD);
    expect_value(__wrap_http_perform, body, NULL);
    expect_string(__wrap_http_perform, request_type_str, MOCK_REQUEST_TYPE_POST);
    expect_value(__wrap_http_perform, header->header_service_name, NULL);
    expect_string(__wrap_http_perform, header->header_token, MOCK_HEADER_AUTH_TOKEN);
    will_return(__wrap_http_perform, cast_ptr_to_largest_integral_type(MOCK_RESPONSE_DELETE_CONNECTION));
}
