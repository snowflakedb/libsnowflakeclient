/*
 * Copyright (c) 2017-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifdef MOCK_ENABLED

#include <stddef.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>

#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <string.h>
#include <snowflake/basic_types.h>
#include <snowflake/client.h>
#include <snowflake/logger.h>

#include "connection.h"
#include "mock_http_perform.h"
#include "error.h"
#include "memory.h"
#include "constants.h"
#include "client_int.h"

#define REQUEST_GUID_KEY_SIZE 13

static void
dump(const char *text, FILE *stream, unsigned char *ptr, size_t size,
     char nohex);

static int my_trace(CURL *handle, curl_infotype type, char *data, size_t size,
                    void *userp);

static void my_sleep_ms(uint32 sleepMs);

static
void dump(const char *text,
          FILE *stream, unsigned char *ptr, size_t size,
          char nohex) {
    size_t i;
    size_t c;

    unsigned int width = 0x10;

    if (nohex)
        /* without the hex output, we can fit more on screen */
        width = 0x40;

    fprintf(stream, "%s, %10.10ld bytes (0x%8.8lx)\n",
            text, (long) size, (long) size);

    for (i = 0; i < size; i += width) {

        fprintf(stream, "%4.4lx: ", (long) i);

        if (!nohex) {
            /* hex not disabled, show it */
            for (c = 0; c < width; c++)
                if (i + c < size)
                    fprintf(stream, "%02x ", ptr[i + c]);
                else
                    fputs("   ", stream);
        }

        for (c = 0; (c < width) && (i + c < size); c++) {
            /* check for 0D0A; if found, skip past and start a new line of output */
            if (nohex && (i + c + 1 < size) && ptr[i + c] == 0x0D &&
                ptr[i + c + 1] == 0x0A) {
                i += (c + 2 - width);
                break;
            }
            fprintf(stream, "%c",
                    (ptr[i + c] >= 0x20) && (ptr[i + c] < 0x80) ? ptr[i + c]
                                                                : '.');
            /* check again for 0D0A, to avoid an extra \n if it's at width */
            if (nohex && (i + c + 2 < size) && ptr[i + c + 1] == 0x0D &&
                ptr[i + c + 2] == 0x0A) {
                i += (c + 3 - width);
                break;
            }
        }
        fputc('\n', stream); /* newline */
    }
    fflush(stream);
}

static
int my_trace(CURL *handle, curl_infotype type,
             char *data, size_t size,
             void *userp) {
    struct data *config = (struct data *) userp;
    const char *text;
    (void) handle; /* prevent compiler warning */

    switch (type) {
        case CURLINFO_TEXT:
            fprintf(stderr, "== Info: %s", data);
            /* FALLTHROUGH */
        default: /* in case a new one is introduced to shock us */
            return 0;

        case CURLINFO_HEADER_OUT:
            text = "=> Send header";
            break;
        case CURLINFO_DATA_OUT:
            text = "=> Send data";
            break;
        case CURLINFO_SSL_DATA_OUT:
            text = "=> Send SSL data";
            break;
        case CURLINFO_HEADER_IN:
            text = "<= Recv header";
            break;
        case CURLINFO_DATA_IN:
            text = "<= Recv data";
            break;
        case CURLINFO_SSL_DATA_IN:
            text = "<= Recv SSL data";
            break;
    }

    dump(text, stderr, (unsigned char *) data, size, config->trace_ascii);
    return 0;
}

static
void my_sleep_ms(uint32 sleepMs)
{
#ifdef _WIN32
  Sleep(sleepMs);
#else
  usleep(sleepMs * 1000); // usleep takes sleep time in us (1 millionth of a second)
#endif
}

sf_bool STDCALL http_perform(CURL *curl,
                             SF_REQUEST_TYPE request_type,
                             char *url,
                             SF_HEADER *header,
                             char *body,
                             cJSON **json,
                             int64 network_timeout,
                             sf_bool chunk_downloader,
                             SF_ERROR_STRUCT *error,
                             sf_bool insecure_mode,
                             int8 retry_on_curle_couldnt_connect_count) {
    CURLcode res;
    sf_bool ret = SF_BOOLEAN_FALSE;
    sf_bool retry = SF_BOOLEAN_FALSE;
    long int http_code = 0;
    DECORRELATE_JITTER_BACKOFF djb = {
      1,      //base
      16      //cap
    };
    network_timeout = (network_timeout > 0) ? network_timeout : SF_LOGIN_TIMEOUT;
    RETRY_CONTEXT curl_retry_ctx = {
            0,      //retry_count
            network_timeout,
            1,      // time to sleep
            &djb    // Decorrelate jitter
    };
    time_t elapsedRetryTime = time(NULL);
    RAW_JSON_BUFFER buffer = {NULL, 0};
    struct data config;
    config.trace_ascii = 1;

    if (curl == NULL) {
        return SF_BOOLEAN_FALSE;
    }

    //TODO set error buffer

    // Find request GUID in the supplied URL
    char *request_guid_ptr = strstr(url, "request_guid=");
    // Set pointer to the beginning of the UUID string if request GUID exists
    if (request_guid_ptr) {
        request_guid_ptr = request_guid_ptr + REQUEST_GUID_KEY_SIZE;
    }

    do {
        // Reset buffer since this may not be our first rodeo
        SF_FREE(buffer.buffer);
        buffer.size = 0;

        // Generate new request guid, if request guid exists in url
        if (request_guid_ptr && uuid4_generate_non_terminated(request_guid_ptr)) {
            log_error("Failed to generate new request GUID");
            break;
        }

        // Set parameters
        res = curl_easy_setopt(curl, CURLOPT_URL, url);
        if (res != CURLE_OK) {
            log_error("Failed to set URL [%s]", curl_easy_strerror(res));
            break;
        }

        if (DEBUG) {
            curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, my_trace);
            curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &config);

            /* the DEBUGFUNCTION has no effect until we enable VERBOSE */
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
        }

        if (header) {
            res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header->header);
            if (res != CURLE_OK) {
                log_error("Failed to set header [%s]", curl_easy_strerror(res));
                break;
            }
        }

        // Post type stuffs
        if (request_type == POST_REQUEST_TYPE) {
            res = curl_easy_setopt(curl, CURLOPT_POST, 1);
            if (res != CURLE_OK) {
                log_error("Failed to set post [%s]", curl_easy_strerror(res));
                break;
            }

            if (body) {
                res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
            } else {
                res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
            }
            if (res != CURLE_OK) {
                log_error("Failed to set body [%s]", curl_easy_strerror(res));
                break;
            }
        }

        res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (void*)&json_resp_cb);
        if (res != CURLE_OK) {
            log_error("Failed to set writer [%s]", curl_easy_strerror(res));
            break;
        }

        res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &buffer);
        if (res != CURLE_OK) {
            log_error("Failed to set write data [%s]", curl_easy_strerror(res));
            break;
        }

        if (DISABLE_VERIFY_PEER) {
            res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            if (res != CURLE_OK) {
                log_error("Failed to disable peer verification [%s]",
                          curl_easy_strerror(res));
                break;
            }
        }

        if (CA_BUNDLE_FILE) {
            res = curl_easy_setopt(curl, CURLOPT_CAINFO, CA_BUNDLE_FILE);
            if (res != CURLE_OK) {
                log_error("Unable to set certificate file [%s]",
                          curl_easy_strerror(res));
                break;
            }
        }

        res = curl_easy_setopt(curl, CURLOPT_SSLVERSION, SSL_VERSION);
        if (res != CURLE_OK) {
            log_error("Unable to set SSL Version [%s]",
                      curl_easy_strerror(res));
            break;
        }

//#ifndef _WIN32
//        // If insecure mode is set to true, skip OCSP check not matter the value of SF_OCSP_CHECK (global OCSP variable)
//        sf_bool ocsp_check;
//        if (insecure_mode) {
//            ocsp_check = SF_BOOLEAN_FALSE;
//        } else {
//            ocsp_check = SF_OCSP_CHECK;
//        }
//        res = curl_easy_setopt(curl, CURLOPT_SSL_SF_OCSP_CHECK, ocsp_check);
//        if (res != CURLE_OK) {
//            log_error("Unable to set OCSP check enable/disable [%s]",
//                      curl_easy_strerror(res));
//            break;
//        }
//#endif

        // Set chunk downloader specific stuff here
        if (chunk_downloader) {
            res = curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
            if (res != CURLE_OK) {
                log_error("Unable to set accepted content encoding",
                          curl_easy_strerror(res));
                break;
            }

            // Set the first character in the buffer as a bracket
            buffer.buffer = (char *) SF_CALLOC(1,
                                               2); // Don't forget null terminator
            buffer.size = 1;
            sb_strncpy(buffer.buffer, 2, "[", 2);
        }

        // Be optimistic
        retry = SF_BOOLEAN_FALSE;

        log_trace("Running curl call");
        res = curl_easy_perform(curl);
        /* Check for errors */
        if (res != CURLE_OK) {
          if (res == CURLE_COULDNT_CONNECT && curl_retry_ctx.retry_count <
                                              retry_on_curle_couldnt_connect_count)
            {
              retry = SF_BOOLEAN_TRUE;
              uint32 next_sleep_in_secs = retry_ctx_next_sleep(&curl_retry_ctx);
              log_error(
                      "curl_easy_perform() failed connecting to server on attempt %d, "
                      "will retry after %d second",
                      curl_retry_ctx.retry_count,
                      next_sleep_in_secs);
              my_sleep_ms(next_sleep_in_secs*1000);
            } else {
              char msg[1024];
              if (res == CURLE_SSL_CACERT_BADFILE) {
                sb_sprintf(msg, sizeof(msg), "curl_easy_perform() failed. err: %s, CA Cert file: %s",
                        curl_easy_strerror(res), CA_BUNDLE_FILE ? CA_BUNDLE_FILE : "Not Specified");
                }
                else {
                sb_sprintf(msg, sizeof(msg), "curl_easy_perform() failed: %s", curl_easy_strerror(res));
                }
                msg[sizeof(msg)-1] = (char)0;
                log_error(msg);
                SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_CURL,
                                    msg,
                                    SF_SQLSTATE_UNABLE_TO_CONNECT);
            }
        } else {
            if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code) !=
                CURLE_OK) {
                log_error("Unable to get http response code [%s]",
                          curl_easy_strerror(res));
                SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_CURL,
                                    "Unable to get http response code",
                                    SF_SQLSTATE_UNABLE_TO_CONNECT);
            } else if (http_code != 200) {
              retry = is_retryable_http_code(http_code);
              if (!retry) {
                char msg[1024];
                sb_sprintf(msg, sizeof(msg), "Received unretryable http code: [%d]",
                           http_code);
                SET_SNOWFLAKE_ERROR(error,
                                    SF_STATUS_ERROR_RETRY,
                                    msg,
                                    SF_SQLSTATE_UNABLE_TO_CONNECT);
              }
              if (retry &&
                  ((time(NULL) - elapsedRetryTime) < curl_retry_ctx.retry_timeout)
              )
              {
                uint32 next_sleep_in_secs = retry_ctx_next_sleep(&curl_retry_ctx);
                log_debug(
                    "curl_easy_perform() Got retryable error http code %d, retry count  %d "
                    "will retry after %d seconds", http_code,
                    curl_retry_ctx.retry_count,
                    next_sleep_in_secs);
                my_sleep_ms(next_sleep_in_secs * 1000);
              }
              else {
                char msg[1024];
                sb_sprintf(msg, sizeof(msg),
                           "Exceeded the retry_timeout , http code: [%d]",
                           http_code);
                SET_SNOWFLAKE_ERROR(error,
                                    SF_STATUS_ERROR_RETRY,
                                    msg,
                                    SF_SQLSTATE_UNABLE_TO_CONNECT);
                retry = SF_BOOLEAN_FALSE;
              }
            } else {
                ret = SF_BOOLEAN_TRUE;
            }
        }

        // Reset everything
        reset_curl(curl);
        http_code = 0;
    }
    while (retry);

    // We were successful so parse JSON from text
    if (ret) {
        if (chunk_downloader) {
            buffer.buffer = (char *) SF_REALLOC(buffer.buffer, buffer.size +
                                                               2); // 1 byte for closing bracket, 1 for null terminator
            sb_memcpy(&buffer.buffer[buffer.size], 1, "]", 1);
            buffer.size += 1;
            // Set null terminator
            buffer.buffer[buffer.size] = '\0';
        }
        snowflake_cJSON_Delete(*json);
        *json = NULL;
        *json = snowflake_cJSON_Parse(buffer.buffer);
        if (*json) {
            ret = SF_BOOLEAN_TRUE;
        } else {
            SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_JSON,
                                "Unable to parse JSON text response.",
                                SF_SQLSTATE_UNABLE_TO_CONNECT);
            ret = SF_BOOLEAN_FALSE;
        }
    }

    SF_FREE(buffer.buffer);

    return ret;
}

#ifdef MOCK_ENABLED

sf_bool STDCALL __wrap_http_perform(CURL *curl,
                                    SF_REQUEST_TYPE request_type,
                                    char *url,
                                    SF_HEADER *header,
                                    char *body,
                                    cJSON **json,
                                    int64 network_timeout,
                                    sf_bool chunk_downloader,
                                    SF_ERROR_STRUCT *error,
                                    sf_bool insecure_mode) {
    char *resp;
    const char *request_type_str = request_type == POST_REQUEST_TYPE ? "POST" : "GET";

    // Remove request ID from URL (since it isn't deterministic
    char *found = strchr(url, '?');
    found[0] = '\0';
    // Check that the generated URL is what we expected
    check_expected_ptr(url);
    // Check that body is what we expected
    check_expected(body);
    // Check request type
    check_expected_ptr(request_type_str);
    // Check service name header
    check_expected(header->header_service_name);
    // Check auth token header
    check_expected(header->header_token);

    // Get back mock response (assuming all inputs checked out) and parse as JSON
    resp = mock_ptr_type(char *);
    *json = snowflake_cJSON_Parse(resp);

    return SF_BOOLEAN_TRUE;
}

#endif
