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
#include <stdlib.h>
#include <snowflake/basic_types.h>
#include <snowflake/client.h>
#include <snowflake/logger.h>

#include "connection.h"
#include "mock_http_perform.h"
#include "error.h"
#include "memory.h"
#include "constants.h"
#include "client_int.h"
#include "snowflake_util.h"

static void
dump(const char *text, FILE *stream, unsigned char *ptr, size_t size,
     char nohex);

static int my_trace(CURL *handle, curl_infotype type, char *data, size_t size,
                    void *userp);

/* Enable curl verbose logging when the environment variable SF_CURL_VERBOSE is set.
 * This allows turning on libcurl debug without recompiling with DEBUG. */
static int is_curl_verbose_enabled(void)
{
    const char *env = getenv("SF_CURL_VERBOSE");
    if (!env) return 0;
    return (env[0] == '1' || env[0] == 't' || env[0] == 'T' || env[0] == 'y' || env[0] == 'Y');
}

/* Test-only hook to validate verbose toggle behavior */
int sf_is_curl_verbose_enabled_for_test(void)
{
    return is_curl_verbose_enabled();
}

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

    sf_fprintf(stream, "%s, %10.10ld bytes (0x%8.8lx)\n",
            text, (long) size, (long) size);

    for (i = 0; i < size; i += width) {

        sf_fprintf(stream, "%4.4lx: ", (long) i);

        if (!nohex) {
            /* hex not disabled, show it */
            for (c = 0; c < width; c++)
                if (i + c < size)
                    sf_fprintf(stream, "%02x ", ptr[i + c]);
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
            sf_fprintf(stream, "%c",
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
            /* Route curl info to Snowflake logger so it appears in driver logs */
            log_info("CURLDBG: %s", data);
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
    /* Summarize payload type and size to avoid massive hexdumps in logs */
    log_info("CURLDBG: %s (%zu bytes)", text, size);
    return 0;
}

sf_bool STDCALL http_perform(CURL *curl,
                             SF_REQUEST_TYPE request_type,
                             char *url,
                             SF_HEADER *header,
                             char *body,
                             PUT_PAYLOAD* put_payload,
                             cJSON **json,
                             NON_JSON_RESP *non_json_resp,
                             char** resp_headers,
                             int64 network_timeout,
                             sf_bool chunk_downloader,
                             SF_ERROR_STRUCT *error,
                             sf_bool insecure_mode,
                             sf_bool fail_open,
                             int8 retry_on_curle_couldnt_connect_count,
                             int64 renew_timeout,
                             int8 retry_max_count,
                             int64 *elapsed_time,
                             int8 *retried_count,
                             sf_bool *is_renew,
                             sf_bool renew_injection,
                             const char *proxy,
                             const char *no_proxy,
                             sf_bool include_retry_reason,
                             sf_bool is_new_strategy_request) {
    CURLcode res;
    sf_bool ret = SF_BOOLEAN_FALSE;
    sf_bool retry = SF_BOOLEAN_FALSE;
    long int http_code = 0;
    DECORRELATE_JITTER_BACKOFF djb = {
      SF_BACKOFF_BASE,      //base
      SF_BACKOFF_CAP      //cap
    };
    if (SF_BOOLEAN_TRUE == is_new_strategy_request)
    {
      djb.cap = SF_NEW_STRATEGY_BACKOFF_CAP;
    }

    network_timeout = (network_timeout > 0) ? network_timeout : SF_RETRY_TIMEOUT;
    if (elapsed_time) {
        network_timeout -= *elapsed_time;
        if (network_timeout <= 0) {
            network_timeout = 1;
        }
    }
    RETRY_CONTEXT curl_retry_ctx = {
            retried_count ? *retried_count : 0,      //retry_count
            0,      // retry reason
            network_timeout,
            djb.base,      // time to sleep
            &djb,    // Decorrelate jitter
            sf_get_current_time_millis() // start time
    };
    time_t elapsedRetryTime = time(NULL);
    RAW_CHAR_BUFFER buffer = {NULL, 0};
    RAW_CHAR_BUFFER headerBuffer = { NULL, 0 };
    struct data config;
    config.trace_ascii = 1;

    if (curl == NULL) {
        return SF_BOOLEAN_FALSE;
    }

    // Set libcurl error buffer for detailed diagnostics
    char curl_error_buffer[CURL_ERROR_SIZE];
    curl_error_buffer[0] = '\0';

    do {
        // Reset buffer since this may not be our first rodeo
        SF_FREE(buffer.buffer);
        buffer.size = 0;
        SF_FREE(headerBuffer.buffer);
        headerBuffer.size = 0;

        // Generate new request guid, if request guid exists in url
        if (SF_BOOLEAN_TRUE != retry_ctx_update_url(&curl_retry_ctx, url, include_retry_reason)) {
            log_error("Failed to update request url");
            break;
        }

        // Set curl timeout to ensure the request won't exceed network timeout when
        // there is delay due to network issue
        long curl_timeout = network_timeout;
        if (renew_timeout > 0) {
            curl_timeout = renew_timeout > network_timeout ?
              network_timeout : renew_timeout;
        }
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, curl_timeout);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, curl_timeout);

        // Set parameters
        curl_error_buffer[0] = '\0';
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error_buffer);
        res = curl_easy_setopt(curl, CURLOPT_URL, url);
        if (res != CURLE_OK) {
            log_error("Failed to set URL [%s]", curl_easy_strerror(res));
            break;
        }

        if (DEBUG || is_curl_verbose_enabled()) {
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

        // Set proxy
        res = set_curl_proxy(curl, proxy, no_proxy);
        if (res != CURLE_OK) {
          log_error("Failed to set proxy [%s]", curl_easy_strerror(res));
          break;
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
        else if (request_type == HEAD_REQUEST_TYPE)
        {
            /** we want response header only */
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        }
        else if (request_type == PUT_REQUEST_TYPE)
        {
            // we need to upload the data
            curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

            if (!put_payload)
            {
                log_error("Invalid payload for put request");
                break;
            }
            /** set read callback function */
            res = curl_easy_setopt(curl, CURLOPT_READFUNCTION, put_payload->read_callback);
            if (res != CURLE_OK) {
                log_error("Failed to set read function [%s]", curl_easy_strerror(res));
                break;
            }

            /** set data object to pass to callback function */
            res = curl_easy_setopt(curl, CURLOPT_READDATA, put_payload->buffer);
            if (res != CURLE_OK) {
                log_error("Failed to set read data [%s]", curl_easy_strerror(res));
                break;
            }

            /** set size of put */
            if (put_payload->length <= SF_INT32_MAX)
            {
                res = curl_easy_setopt(curl, CURLOPT_INFILESIZE, (long)put_payload->length);
            }
            else
            {
                res = curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)put_payload->length);
            }
        }


        if (!json && non_json_resp)
        {
            res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, non_json_resp->write_callback);
        }
        else
        {
          res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (void*)&char_resp_cb);
        }
        if (res != CURLE_OK) {
            log_error("Failed to set writer [%s]", curl_easy_strerror(res));
            break;
        }

        if (!json && non_json_resp)
        {
            res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, non_json_resp->buffer);
        }
        else
        {
            res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&buffer);
        }
        if (res != CURLE_OK) {
            log_error("Failed to set write data [%s]", curl_easy_strerror(res));
            break;
        }

        res = curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void*)&headerBuffer);
        if (res != CURLE_OK) {
            log_error("Failed to set header data [%s]", curl_easy_strerror(res));
            break;
        }

        res = curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, (void*)&char_resp_cb);
        if (res != CURLE_OK) {
            log_error("Failed to set header function [%s]", curl_easy_strerror(res));
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

        // If insecure mode is set to true, skip OCSP check not matter the value of SF_OCSP_CHECK (global OCSP variable)
        sf_bool ocsp_check;
        if (insecure_mode) {
            ocsp_check = SF_BOOLEAN_FALSE;
        } else {
            ocsp_check = SF_OCSP_CHECK;
        }
        res = curl_easy_setopt(curl, CURLOPT_SSL_SF_OCSP_CHECK, ocsp_check);
        if (res != CURLE_OK) {
            log_error("Unable to set OCSP check enable/disable [%s]",
                      curl_easy_strerror(res));
            break;
        }
        res = curl_easy_setopt(curl, CURLOPT_SSL_SF_OCSP_FAIL_OPEN, fail_open);
        if (res != CURLE_OK) {
            log_error("Unable to set OCSP FAIL_OPEN [%s]",
                      curl_easy_strerror(res));
            break;
        }

        // Set chunk downloader specific stuff here
        if (chunk_downloader) {
            res = curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
            if (res != CURLE_OK) {
                log_error("Unable to set accepted content encoding",
                          curl_easy_strerror(res));
                break;
            }

            if (json)
            {
                // Set the first character in the buffer as a bracket
                buffer.buffer = (char *)SF_CALLOC(1,
                  2); // Don't forget null terminator
                buffer.size = 1;
                sf_strncpy(buffer.buffer, 2, "[", 2);
            }
        }

        // Be optimistic
        retry = SF_BOOLEAN_FALSE;

        log_trace("Running curl call");
        res = curl_easy_perform(curl);

        // forcely trigger renew timeout on the first attempt
        if ((renew_injection) && (renew_timeout > 0) &&
            elapsed_time && (*elapsed_time <= 0))
        {
            sf_sleep_ms(renew_timeout * 1000);
            res = CURLE_OPERATION_TIMEDOUT;
        }

        /* Check for errors */
        if (res != CURLE_OK) {
          if (curl_error_buffer[0] != '\0') {
            log_error("curl error buffer: %s", curl_error_buffer);
          }
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
              sf_sleep_ms(next_sleep_in_secs*1000);
            } else if ((res == CURLE_OPERATION_TIMEDOUT) && (renew_timeout > 0)) {
               retry = SF_BOOLEAN_TRUE;
            } else {
              char msg[1024];
              if (res == CURLE_SSL_CACERT_BADFILE) {
                sf_sprintf(msg, sizeof(msg), "curl_easy_perform() failed. err: %s, CA Cert file: %s",
                        curl_easy_strerror(res), CA_BUNDLE_FILE ? CA_BUNDLE_FILE : "Not Specified");
                }
                else {
                sf_sprintf(msg, sizeof(msg), "curl_easy_perform() failed: %s", curl_easy_strerror(res));
                }
                msg[sizeof(msg)-1] = (char)0;
                log_error(msg);
                if (res == CURLE_SSL_INVALIDCERTSTATUS) {
                  log_error("Detected CURLE_SSL_INVALIDCERTSTATUS (91) - likely OCSP/CRL validation failure.");
                }
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
                sf_sprintf(msg, sizeof(msg), "Received unretryable http code: [%d]",
                           http_code);
                SET_SNOWFLAKE_ERROR(error,
                                    SF_STATUS_ERROR_RETRY,
                                    msg,
                                    SF_SQLSTATE_UNABLE_TO_CONNECT);
              } else {
                curl_retry_ctx.retry_reason = (uint32)http_code;
              }
              if (retry &&
                  ((time(NULL) - elapsedRetryTime) < curl_retry_ctx.retry_timeout) &&
                  ((retry_max_count <= 0) || (curl_retry_ctx.retry_count < retry_max_count)))
              {
                uint32 next_sleep_in_secs = retry_ctx_next_sleep(&curl_retry_ctx);
                log_debug(
                    "curl_easy_perform() Got retryable error http code %d, retry count  %d "
                    "will retry after %d seconds", http_code,
                    curl_retry_ctx.retry_count,
                    next_sleep_in_secs);
                sf_sleep_ms(next_sleep_in_secs * 1000);
              }
              else {
                char msg[1024];
                sf_sprintf(msg, sizeof(msg),
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

        // When renew timeout is reached, stop retry and return to the caller
        // to renew request
        sf_bool renew_timeout_reached = retry && (renew_timeout > 0) && ((time(NULL) - elapsedRetryTime) >= renew_timeout);
        sf_bool renew_timeout_disabled = retry && renew_timeout < 0;
        if (renew_timeout_reached || renew_timeout_disabled) {
            retry  = SF_BOOLEAN_FALSE;
            if (elapsed_time) {
                *elapsed_time += (time(NULL) - elapsedRetryTime);
            }
            if (retried_count) {
                *retried_count = curl_retry_ctx.retry_count;
            }
            if (is_renew) {
                *is_renew = SF_BOOLEAN_TRUE;
            }
        }
    }
    while (retry);

    if (ret && json) {
      // We were successful so parse JSON from text
      if (chunk_downloader) {
            buffer.buffer = (char *) SF_REALLOC(buffer.buffer, buffer.size +
                                                               2); // 1 byte for closing bracket, 1 for null terminator
            sf_memcpy(&buffer.buffer[buffer.size], 1, "]", 1);
            buffer.size += 1;
            // Set null terminator
            buffer.buffer[buffer.size] = '\0';
        }
        snowflake_cJSON_Delete(*json);
        *json = NULL;
        *json = snowflake_cJSON_Parse(buffer.buffer);
        if (*json) {
            ret = SF_BOOLEAN_TRUE;
            if (is_one_time_token_request(*json)) {
                snowflake_cJSON_AddNullToObject(*json, "code");
            }
        } else {
            SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_JSON,
                                "Unable to parse JSON text response.",
                                SF_SQLSTATE_UNABLE_TO_CONNECT);
            ret = SF_BOOLEAN_FALSE;
        }
    }

    SF_FREE(buffer.buffer);

    if (resp_headers)
    {
        *resp_headers = headerBuffer.buffer;
    }
    else
    {
        SF_FREE(headerBuffer.buffer);
    }

    return ret;
}

#ifdef MOCK_ENABLED

sf_bool STDCALL __wrap_http_perform(CURL *curl,
                                    SF_REQUEST_TYPE request_type,
                                    char *url,
                                    SF_HEADER *header,
                                    char *body,
                                    PUT_PAYLOAD* put_payload,
                                    cJSON **json,
                                    NON_JSON_RESP *non_json_resp,
                                    char** resp_headers,
                                    int64 network_timeout,
                                    sf_bool chunk_downloader,
                                    SF_ERROR_STRUCT *error,
                                    sf_bool insecure_mode,
                                    sf_bool fail_open) {
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
