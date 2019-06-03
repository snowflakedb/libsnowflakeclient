/*
 * Copyright (c) 2017-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_MOCK_SETUP_H
#define SNOWFLAKECLIENT_MOCK_SETUP_H

#ifdef __cplusplus
extern "C" {
#endif

// cmocka start
#include <stddef.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
// cmocka end

// Setup mock data for service name login
void setup_mock_login_service_name();

// Setup mock data for running a service name query
void setup_mock_query_service_name();

// Setup a generic delete connection request
void setup_mock_delete_connection_service_name();

void setup_mock_login_standard();

void setup_mock_query_session_gone();

void setup_mock_query_standard();

void setup_mock_delete_connection_session_gone();

void setup_mock_delete_connection_standard();

#ifdef __cplusplus
}
#endif

#endif //SNOWFLAKECLIENT_MOCK_SETUP_H
