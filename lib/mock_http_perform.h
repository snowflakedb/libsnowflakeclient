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
                                    char *body, PUT_PAYLOAD* put_payload, cJSON **json, NON_JSON_RESP *non_json_resp, char** resp_headers, int64 network_timeout, sf_bool chunk_downloader,
                                    SF_ERROR_STRUCT *error, sf_bool insecure_mode, sf_bool fail_open);

#endif

#ifdef __cplusplus
}
#endif

#endif //SNOWFLAKECLIENT_CONNECTION_MOCK_H
