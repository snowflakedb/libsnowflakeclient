/*
 * Copyright (c) 2017-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_CONNECTION_MOCK_H
#define SNOWFLAKECLIENT_CONNECTION_MOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <snowflake/client.h>
#include "connection.h"

#ifdef MOCK_ENABLED

// The parameters for this are identical to http_perform located in connection.h
// This is just the mock interface
sf_bool STDCALL __wrap_http_perform(CURL *curl, SF_REQUEST_TYPE request_type, char *url, SF_HEADER *header,
                                    char *body, cJSON **json, int64 network_timeout, sf_bool chunk_downloader,
                                    SF_ERROR_STRUCT *error, sf_bool insecure_mode);

#endif

#ifdef __cplusplus
}
#endif

#endif //SNOWFLAKECLIENT_CONNECTION_MOCK_H
