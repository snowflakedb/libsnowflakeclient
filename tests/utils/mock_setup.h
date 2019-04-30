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

void setup_mock_login_service_name();

void setup_mock_query_service_name();

void setup_mock_delete_connection();

#ifdef __cplusplus
}
#endif

#endif //SNOWFLAKECLIENT_MOCK_SETUP_H
