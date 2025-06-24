#ifndef SNOWFLAKE_HEADER_CUSTOMIZER_H
#define SNOWFLAKE_HEADER_CUSTOMIZER_H

#include "snowflake/basic_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * HTTP request methods
 */
typedef enum {
    SF_HTTPREQ_GET,
    SF_HTTPREQ_POST,
    SF_HTTPREQ_PUT,
    SF_HTTPREQ_HEAD
} SF_HTTPREQ_METHOD;

/* This is the SF_CON_HEADER_CUSTOMIZER callback prototype.
 * Is called on sending each http request and allows changing request headers
 * for proxy authentication purpose.
 *
 * @param http_method The http request method.
 * @param target_uri A string of the target URI of the http request
 * @param existing_headers A string combined all existing headers in the format
 *                         of <header>:<value>, separated by CRLF.
 * @param is_retry True if the request is a retry attempt, otherwise it's an initial attempt.
 * @param invoke_once Output argument.
 *                    True: header customizer needs to be called only once for the initial
 *                          attempt of a given HTTP request.
 *                    False: header customizer needs to be called for the initial attempt and
 *                           before every subsequent retry attempt of that same HTTP request
 *                           (essential for time-sensitive/dynamic headers).
 * @param new_headers Output argument.A string combined all new headers need to be added/replaced
 *                    into request headers, in the format of <header>:<value>, separated by CRLF.
 * @param new_headers_buflen The length of the buffer of new_headers.
 * @param new_headers_len Output argument. The actual length of the combined string of new headers.
 *                        If the buffer size of new_headers is not enough to store the entire string,
 *                        header customizer will be called again with increased buffer size of new_headers.
 */
typedef sf_bool (*HEADER_CUSTOMIZER)(SF_HTTPREQ_METHOD http_method,
                                     const char* target_uri,
                                     const char* existing_headers,
                                     sf_bool is_retry,
                                     sf_bool* invoke_once,
                                     char* new_headers,
                                     size_t new_headers_buflen,
                                     size_t* new_headers_len);

#ifdef __cplusplus
}
#endif

#endif //SNOWFLAKE_HEADER_CUSTOMIZER_H
