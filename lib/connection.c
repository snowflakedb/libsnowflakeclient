/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <string.h>
#include "connection.h"
#include <snowflake/logger.h>
#include "snowflake/platform.h"
#include "memory.h"
#include "client_int.h"
#include "constants.h"
#include "error.h"

#define curl_easier_escape(curl, string) curl_easy_escape(curl, string, 0)
#define QUERYCODE_LEN 7
#define REQUEST_GUID_KEY_SIZE 13

/*
 * Debug functions from curl example. Should update at somepoint, and possibly remove from header since these are private functions
 */
static void
dump(const char *text, FILE *stream, unsigned char *ptr, size_t size,
     char nohex);

static int my_trace(CURL *handle, curl_infotype type, char *data, size_t size,
                    void *userp);

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

static uint32 uimin(uint32 a, uint32 b) {
    return (a < b) ? a : b;
}

static uint32 uimax(uint32 a, uint32 b) {
    return (a > b) ? a : b;
}


cJSON *STDCALL create_auth_json_body(SF_CONNECT *sf,
                                     const char *application,
                                     const char *int_app_name,
                                     const char *int_app_version,
                                     const char *timezone,
                                     sf_bool autocommit) {
    cJSON *body;
    cJSON *data;
    cJSON *client_env;
    cJSON *session_parameters;
    char os_version[128];

    //Create Client Environment JSON blob
    client_env = snowflake_cJSON_CreateObject();
    snowflake_cJSON_AddStringToObject(client_env, "APPLICATION", application);
    snowflake_cJSON_AddStringToObject(client_env, "OS", sf_os_name());
#ifdef MOCK_ENABLED
    os_version[0] = '0';
    os_version[1] = '\0';
#else
    sf_os_version(os_version, sizeof(os_version));
#endif
    snowflake_cJSON_AddStringToObject(client_env, "OS_VERSION", os_version);

    session_parameters = snowflake_cJSON_CreateObject();
    snowflake_cJSON_AddStringToObject(
        session_parameters,
        "AUTOCOMMIT",
        autocommit == SF_BOOLEAN_TRUE ? SF_BOOLEAN_INTERNAL_TRUE_STR
                                      : SF_BOOLEAN_INTERNAL_FALSE_STR);

    snowflake_cJSON_AddStringToObject(session_parameters, "TIMEZONE", timezone);

    //Create Request Data JSON blob
    data = snowflake_cJSON_CreateObject();
    snowflake_cJSON_AddStringToObject(data, "CLIENT_APP_ID", int_app_name);
#ifdef MOCK_ENABLED
    snowflake_cJSON_AddStringToObject(data, "CLIENT_APP_VERSION", "0.0.0");
#else
    snowflake_cJSON_AddStringToObject(data, "CLIENT_APP_VERSION", int_app_version);
#endif
    snowflake_cJSON_AddStringToObject(data, "ACCOUNT_NAME", sf->account);
    snowflake_cJSON_AddStringToObject(data, "LOGIN_NAME", sf->user);
    // Add password if one exists
    if (sf->password && *(sf->password)) {
        snowflake_cJSON_AddStringToObject(data, "PASSWORD", sf->password);
    }
    snowflake_cJSON_AddItemToObject(data, "CLIENT_ENVIRONMENT", client_env);
    snowflake_cJSON_AddItemToObject(data, "SESSION_PARAMETERS",
                                  session_parameters);

    //Create body
    body = snowflake_cJSON_CreateObject();
    snowflake_cJSON_AddItemToObject(body, "data", data);


    return body;
}

cJSON *STDCALL create_query_json_body(const char *sql_text, int64 sequence_id, const char *request_id) {
    cJSON *body;
    double submission_time;
    // Create body
#ifdef MOCK_ENABLED
    submission_time = 0;
#else
    submission_time = (double) time(NULL) * 1000;
#endif
    body = snowflake_cJSON_CreateObject();
    snowflake_cJSON_AddStringToObject(body, "sqlText", sql_text);
    snowflake_cJSON_AddBoolToObject(body, "asyncExec", SF_BOOLEAN_FALSE);
    snowflake_cJSON_AddNumberToObject(body, "sequenceId", (double) sequence_id);
    snowflake_cJSON_AddNumberToObject(body, "querySubmissionTime", submission_time);
    if (request_id)
    {
        snowflake_cJSON_AddStringToObject(body, "requestId", request_id);
    }
    return body;
}

cJSON *STDCALL create_renew_session_json_body(const char *old_token) {
    cJSON *body;
    // Create body
    body = snowflake_cJSON_CreateObject();
    snowflake_cJSON_AddStringToObject(body, "oldSessionToken", old_token);
    snowflake_cJSON_AddStringToObject(body, "requestType", REQUEST_TYPE_RENEW);

    return body;
}

sf_bool STDCALL create_header(SF_CONNECT *sf, SF_HEADER *header, SF_ERROR_STRUCT *error) {
    sf_bool ret = SF_BOOLEAN_FALSE;
    size_t header_token_size;
    size_t header_direct_query_token_size;
    size_t header_service_name_size;
    const char *token = header->renew_session ? sf->master_token : sf->token;

    // Generate header tokens
    if (token) {
        header_token_size = strlen(HEADER_SNOWFLAKE_TOKEN_FORMAT) - 2 +
                            strlen(token) + 1;
        header->header_token = (char *) SF_CALLOC(1, header_token_size);
        if (!header->header_token) {
            SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_OUT_OF_MEMORY,
                                "Ran out of memory trying to create header token",
                                SF_SQLSTATE_UNABLE_TO_CONNECT);
            goto error;
        }
        sb_sprintf(header->header_token, header_token_size,
                 HEADER_SNOWFLAKE_TOKEN_FORMAT, token);
    } else if (sf->direct_query_token) {
        header_direct_query_token_size = strlen(HEADER_DIRECT_QUERY_TOKEN_FORMAT) - 2 +
                                         strlen(sf->direct_query_token) + 1;
        header->header_direct_query_token = (char *) SF_CALLOC(1, header_direct_query_token_size);
        if (!header->header_direct_query_token) {
            SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_OUT_OF_MEMORY,
                                "Ran out of memory trying to create header direct query token",
                                SF_SQLSTATE_UNABLE_TO_CONNECT);
            goto error;
        }
        sb_sprintf(header->header_direct_query_token, header_direct_query_token_size,
                 HEADER_DIRECT_QUERY_TOKEN_FORMAT, sf->direct_query_token);
    }

    // Generate service name if it exists
    if (sf->service_name) {
        header_service_name_size = strlen(HEADER_SERVICE_NAME_FORMAT) - 2 +
                                         strlen(sf->service_name) + 1;
        header->header_service_name = (char *) SF_CALLOC(1, header_service_name_size);
        if (!header->header_service_name) {
            SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_OUT_OF_MEMORY,
                                "Ran out of memory trying to create header service name",
                                SF_SQLSTATE_UNABLE_TO_CONNECT);
            goto error;
        }
        sb_sprintf(header->header_service_name, header_service_name_size,
                 HEADER_SERVICE_NAME_FORMAT, sf->service_name);
    }

    if (header->header_token) {
        header->header = curl_slist_append(header->header, header->header_token);
    }
    if (header->header_direct_query_token) {
        header->header = curl_slist_append(header->header, header->header_direct_query_token);
    }
    if (header->header_service_name) {
        header->header = curl_slist_append(header->header, header->header_service_name);
    }
    header->header = curl_slist_append(header->header, HEADER_CONTENT_TYPE_APPLICATION_JSON);
    header->header = curl_slist_append(header->header,
                               header->use_application_json_accept_type ?
                               HEADER_ACCEPT_TYPE_APPLICATION_JSON :
                               HEADER_ACCEPT_TYPE_APPLICATION_SNOWFLAKE);

    if (SF_HEADER_USER_AGENT != NULL){
      header->header = curl_slist_append(header->header, SF_HEADER_USER_AGENT);
    }
    else
    {
      log_trace("SF_HEADER_USER_AGENT is null");
    }

    log_trace("Created header");

    // All good :dancingpenguin:
    ret = SF_BOOLEAN_TRUE;

error:
    return ret;
}

sf_bool STDCALL curl_post_call(SF_CONNECT *sf,
                               CURL *curl,
                               char *url,
                               SF_HEADER *header,
                               char *body,
                               cJSON **json,
                               SF_ERROR_STRUCT *error) {
    const char *error_msg;
    SF_JSON_ERROR json_error;
    char query_code[QUERYCODE_LEN];
    char *result_url = NULL;
    cJSON *data = NULL;
    SF_HEADER *new_header = NULL;
    sf_bool ret = SF_BOOLEAN_FALSE;
    sf_bool stop = SF_BOOLEAN_FALSE;

    // Set to 0
    memset(query_code, 0, QUERYCODE_LEN);

    do {
        if (!http_perform(curl, POST_REQUEST_TYPE, url, header, body, json,
                          sf->network_timeout, SF_BOOLEAN_FALSE, error, sf->insecure_mode) ||
            !*json) {
            // Error is set in the perform function
            break;
        }
        if ((json_error = json_copy_string_no_alloc(query_code, *json, "code",
                                                    QUERYCODE_LEN)) !=
            SF_JSON_ERROR_NONE &&
            json_error != SF_JSON_ERROR_ITEM_NULL) {
            JSON_ERROR_MSG(json_error, error_msg, "Query code");
            SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_JSON, error_msg,
                                SF_SQLSTATE_UNABLE_TO_CONNECT);
            break;
        }

        // No query code means things went well, just break and return
        if (query_code[0] == '\0') {
            ret = SF_BOOLEAN_TRUE;
            break;
        }

        if (strcmp(query_code, SESSION_TOKEN_EXPIRED_CODE) == 0) {
            if (!renew_session(curl, sf, error)) {
                // Error is set in renew session function
                break;
            } else {
                // Create new header since we have a new token
                new_header = sf_header_create();
                new_header->use_application_json_accept_type = SF_BOOLEAN_FALSE;
                new_header->renew_session = SF_BOOLEAN_FALSE;
                if (!create_header(sf, new_header, error)) {
                    break;
                }
                if (!curl_post_call(sf, curl, url, new_header, body, json,
                                    error)) {
                    // Error is set in curl call
                    break;
                }
            }
        }
        else if (strcmp(query_code, SESSION_TOKEN_INVALID_CODE) == 0) {
            SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_CONNECTION_NOT_EXIST,
                                ERR_MSG_SESSION_TOKEN_INVALID, SF_SQLSTATE_CONNECTION_NOT_EXIST);
            break;
        }
        else if (strcmp(query_code, GONE_SESSION_CODE) == 0) {
            SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_CONNECTION_NOT_EXIST,
                                ERR_MSG_GONE_SESSION, SF_SQLSTATE_CONNECTION_NOT_EXIST);
            break;
        }

        while (strcmp(query_code, QUERY_IN_PROGRESS_CODE) == 0 ||
               strcmp(query_code, QUERY_IN_PROGRESS_ASYNC_CODE) == 0) {
            // Remove old result URL and query code if this isn't our first rodeo
            SF_FREE(result_url);
            memset(query_code, 0, QUERYCODE_LEN);
            data = snowflake_cJSON_GetObjectItem(*json, "data");
            if (json_copy_string(&result_url, data, "getResultUrl") !=
                SF_JSON_ERROR_NONE) {
                stop = SF_BOOLEAN_TRUE;
                JSON_ERROR_MSG(json_error, error_msg, "Result URL");
                SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_JSON, error_msg,
                                    SF_SQLSTATE_UNABLE_TO_CONNECT);
                break;
            }

            log_trace("ping pong starting...");
            if (!request(sf, json, result_url, NULL, 0, NULL, header,
                         GET_REQUEST_TYPE, error, SF_BOOLEAN_FALSE)) {
                // Error came from request up, just break
                stop = SF_BOOLEAN_TRUE;
                break;
            }

            if (
              (json_error = json_copy_string_no_alloc(query_code, *json, "code",
                                                      QUERYCODE_LEN)) !=
              SF_JSON_ERROR_NONE &&
              json_error != SF_JSON_ERROR_ITEM_NULL) {
                stop = SF_BOOLEAN_TRUE;
                JSON_ERROR_MSG(json_error, error_msg, "Query code");
                SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_JSON, error_msg,
                                    SF_SQLSTATE_UNABLE_TO_CONNECT);
                break;
            }
        }

        if (stop) {
            break;
        }

        ret = SF_BOOLEAN_TRUE;
    }
    while (0); // Dummy loop to break out of

    SF_FREE(result_url);
    sf_header_destroy(new_header);

    return ret;
}

sf_bool STDCALL curl_get_call(SF_CONNECT *sf,
                              CURL *curl,
                              char *url,
                              SF_HEADER *header,
                              cJSON **json,
                              SF_ERROR_STRUCT *error) {
    SF_JSON_ERROR json_error;
    const char *error_msg;
    char query_code[QUERYCODE_LEN];
    char *result_url = NULL;
    SF_HEADER *new_header = NULL;
    sf_bool ret = SF_BOOLEAN_FALSE;

    // Set to 0
    memset(query_code, 0, QUERYCODE_LEN);

    do {
        if (!http_perform(curl, GET_REQUEST_TYPE, url, header, NULL, json,
                          sf->network_timeout, SF_BOOLEAN_FALSE, error, sf->insecure_mode) ||
            !*json) {
            // Error is set in the perform function
            break;
        }
        if ((json_error = json_copy_string_no_alloc(query_code, *json, "code",
                                                    QUERYCODE_LEN)) !=
            SF_JSON_ERROR_NONE &&
            json_error != SF_JSON_ERROR_ITEM_NULL) {
            JSON_ERROR_MSG(json_error, error_msg, "Query code");
            SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_JSON, error_msg,
                                SF_SQLSTATE_UNABLE_TO_CONNECT);
            break;
        }

        // No query code means things went well, just break and return
        if (query_code[0] == '\0') {
            ret = SF_BOOLEAN_TRUE;
            break;
        }

        if (strcmp(query_code, SESSION_TOKEN_EXPIRED_CODE) == 0) {
            if (!renew_session(curl, sf, error)) {
                // Error is set in renew session function
                break;
            } else {
                // Create new header since we have a new token
                new_header = sf_header_create();
                if (!create_header(sf, new_header, error)) {
                    break;
                }
                if (!curl_get_call(sf, curl, url, new_header, json, error)) {
                    // Error is set in curl call
                    break;
                }
            }
        }
        else if (strcmp(query_code, SESSION_TOKEN_INVALID_CODE) == 0) {
            SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_CONNECTION_NOT_EXIST,
                                ERR_MSG_SESSION_TOKEN_INVALID, SF_SQLSTATE_CONNECTION_NOT_EXIST);
            break;
        }
        else if (strcmp(query_code, GONE_SESSION_CODE) == 0) {
            SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_CONNECTION_NOT_EXIST,
                                ERR_MSG_GONE_SESSION, SF_SQLSTATE_CONNECTION_NOT_EXIST);
            break;
        }

        ret = SF_BOOLEAN_TRUE;
    }
    while (0); // Dummy loop to break out of

    SF_FREE(result_url);
    sf_header_destroy(new_header);

    return ret;
}

void STDCALL decorrelate_jitter_free(DECORRELATE_JITTER_BACKOFF *djb) {
    SF_FREE(djb);
}

DECORRELATE_JITTER_BACKOFF *
STDCALL decorrelate_jitter_init(uint32 base, uint32 cap) {
    DECORRELATE_JITTER_BACKOFF *djb = (DECORRELATE_JITTER_BACKOFF *) SF_CALLOC(
      1, sizeof(DECORRELATE_JITTER_BACKOFF));
    djb->base = base;
    djb->cap = cap;
    return djb;
}

uint32
decorrelate_jitter_next_sleep(DECORRELATE_JITTER_BACKOFF *djb, uint32 sleep) {
    return uimin(djb->cap, uimax(djb->base, (uint32) (rand() % (sleep * 3))));
}

char * STDCALL encode_url(CURL *curl,
                 const char *protocol,
                 const char *account,
                 const char *host,
                 const char *port,
                 const char *url,
                 URL_KEY_VALUE *vars,
                 int num_args,
                 SF_ERROR_STRUCT *error,
                 char *extraUrlParams) {
    int i;
    sf_bool host_empty = is_string_empty(host);
    sf_bool port_empty = is_string_empty(port);
    const char *format;
    char *encoded_url = NULL;
    // Size used for the url format
    size_t base_url_size = 1; //Null terminator
    // Size used to determine buffer size
    size_t encoded_url_size;
    // Initialize reqeust_guid with a blank uuid that will be replaced for each request
    URL_KEY_VALUE request_guid = {
      "request_guid=",
      "00000000-0000-0000-0000-000000000000",
      NULL,
      NULL,
      0,
      0
    };
    const char *amp = "&";
    size_t amp_size = strlen(amp);

    // Set proper format based on variables passed into encode URL.
    // The format includes format specifiers that will be consumed by empty fields
    // (i.e if port is empty, add an extra specifier so that we have 1 call to snprintf, vs. 4 different calls)
    // Format specifier order is protocol, then account, then host, then port, then url.
    // Base size increases reflect the number of static characters in the format string (i.e. ':', '/', '.')
    if (!port_empty && !host_empty) {
        format = "%s://%s%s:%s%s";
        base_url_size += 4;
        // Set account to an empty string since host overwrites account
        account = "";
    } else if (port_empty && !host_empty) {
        format = "%s://%s%s%s%s";
        base_url_size += 3;
        port = "";
        // Set account to an empty string since host overwrites account
        account = "";
    } else if (!port_empty && host_empty) {
        format = "%s://%s.%s:%s%s";
        base_url_size += 5;
        host = DEFAULT_SNOWFLAKE_BASE_URL;
    } else {
        format = "%s://%s.%s%s%s";
        base_url_size += 4;
        host = DEFAULT_SNOWFLAKE_BASE_URL;
        port = "";
    }
    base_url_size +=
      strlen(protocol) + strlen(account) + strlen(host) + strlen(port) +
      strlen(url) + strlen(URL_QUERY_DELIMITER);

    encoded_url_size = base_url_size;
    // Encode URL parameters and set size info
    for (i = 0; i < num_args; i++) {
        if (vars[i].value && *vars[i].value) {
            vars[i].formatted_key = vars[i].key;
            vars[i].formatted_value = curl_easier_escape(curl, vars[i].value);
        } else {
            vars[i].formatted_key = "";
            vars[i].formatted_value = curl_easier_escape(curl, "");
        }
        vars[i].key_size = strlen(vars[i].formatted_key);
        vars[i].value_size = strlen(vars[i].formatted_value);
        // Add an ampersand for each URL parameter since we are going to add request_guid to the end
        encoded_url_size += vars[i].key_size + vars[i].value_size + amp_size;
    }

    // Encode request_guid and set size info
    request_guid.formatted_key = request_guid.key;
    request_guid.formatted_value = curl_easier_escape(curl, request_guid.value);
    request_guid.key_size = strlen(request_guid.formatted_key);
    request_guid.value_size = strlen(request_guid.formatted_value);
    encoded_url_size += request_guid.key_size + request_guid.value_size;


    encoded_url_size += extraUrlParams ?
                        strlen(extraUrlParams) + strlen(URL_PARAM_DELIM) : 0;

    encoded_url = (char *) SF_CALLOC(1, encoded_url_size);
    if (!encoded_url) {
        SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_OUT_OF_MEMORY,
                            "Ran out of memory trying to create encoded url",
                            SF_SQLSTATE_UNABLE_TO_CONNECT);
        goto cleanup;
    }
    sb_sprintf(encoded_url, base_url_size, format, protocol, account, host, port,
             url);

    // Initially add the query delimiter "?"
    sb_strncat(encoded_url, encoded_url_size, URL_QUERY_DELIMITER, strlen(URL_QUERY_DELIMITER));

    // Add encoded URL parameters to encoded_url buffer
    for (i = 0; i < num_args; i++) {
        sb_strncat(encoded_url, encoded_url_size, vars[i].formatted_key, vars[i].key_size);
        sb_strncat(encoded_url, encoded_url_size, vars[i].formatted_value, vars[i].value_size);
        sb_strncat(encoded_url, encoded_url_size, amp, amp_size);
    }

    // Add encoded request_guid to encoded_url buffer
    sb_strncat(encoded_url, encoded_url_size, request_guid.formatted_key, request_guid.key_size);
    sb_strncat(encoded_url, encoded_url_size, request_guid.formatted_value, request_guid.value_size);

    // Adding the extra url param (setter of extraUrlParams is responsible to make
    // sure extraUrlParams is correct)
    if (extraUrlParams && !is_string_empty(extraUrlParams))
    {
      strncat(encoded_url, URL_PARAM_DELIM, 1);
      strncat(encoded_url, extraUrlParams, strlen(extraUrlParams));
    }

    log_debug("URL: %s", encoded_url);

cleanup:
    // Free created memory
    for (i = 0; i < num_args; i++) {
        curl_free(vars[i].formatted_value);
    }
    curl_free(request_guid.formatted_value);

    return encoded_url;
}

sf_bool is_string_empty(const char *str) {
    return (str && strcmp(str, "") != 0) ? SF_BOOLEAN_FALSE : SF_BOOLEAN_TRUE;
}

SF_JSON_ERROR STDCALL
json_copy_string(char **dest, cJSON *data, const char *item) {
    size_t blob_size;
    cJSON *blob = snowflake_cJSON_GetObjectItem(data, item);
    if (!blob) {
        return SF_JSON_ERROR_ITEM_MISSING;
    } else if (snowflake_cJSON_IsNull(blob)) {
        return SF_JSON_ERROR_ITEM_NULL;
    } else if (!snowflake_cJSON_IsString(blob)) {
        return SF_JSON_ERROR_ITEM_WRONG_TYPE;
    } else {
        blob_size = strlen(blob->valuestring) + 1;
        SF_FREE(*dest);
        *dest = (char *) SF_CALLOC(1, blob_size);
        if (!*dest) {
            return SF_JSON_ERROR_OOM;
        }
        sb_strncpy(*dest, blob_size, blob->valuestring, blob_size);

        if (strcmp(item, "token") == 0 || strcmp(item, "masterToken") == 0) {
            log_debug("Item and Value; %s: ******", item);
        } else {
            log_debug("Item and Value; %s: %s", item, *dest);
        }
    }

    return SF_JSON_ERROR_NONE;
}

SF_JSON_ERROR STDCALL
json_copy_string_no_alloc(char *dest, cJSON *data, const char *item,
                          size_t dest_size) {
    cJSON *blob = snowflake_cJSON_GetObjectItem(data, item);
    if (!blob) {
        return SF_JSON_ERROR_ITEM_MISSING;
    } else if (snowflake_cJSON_IsNull(blob)) {
        return SF_JSON_ERROR_ITEM_NULL;
    } else if (!snowflake_cJSON_IsString(blob)) {
        return SF_JSON_ERROR_ITEM_WRONG_TYPE;
    } else {
        sb_strncpy(dest, dest_size, blob->valuestring, dest_size);
        // If string is not null terminated, then add the terminator yourself
        if (dest[dest_size - 1] != '\0') {
            dest[dest_size - 1] = '\0';
        }
        log_debug("Item and Value; %s: %s", item, dest);
    }

    return SF_JSON_ERROR_NONE;
}

SF_JSON_ERROR STDCALL
json_copy_bool(sf_bool *dest, cJSON *data, const char *item) {
    cJSON *blob = snowflake_cJSON_GetObjectItem(data, item);
    if (!blob) {
        return SF_JSON_ERROR_ITEM_MISSING;
    } else if (snowflake_cJSON_IsNull(blob)) {
        return SF_JSON_ERROR_ITEM_NULL;
    } else if (!snowflake_cJSON_IsBool(blob)) {
        return SF_JSON_ERROR_ITEM_WRONG_TYPE;
    } else {
        *dest = snowflake_cJSON_IsTrue(blob) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
        log_debug("Item and Value; %s: %i", item, *dest);
    }

    return SF_JSON_ERROR_NONE;
}

SF_JSON_ERROR STDCALL
json_copy_int(int64 *dest, cJSON *data, const char *item) {
    cJSON *blob = snowflake_cJSON_GetObjectItem(data, item);
    if (!blob) {
        return SF_JSON_ERROR_ITEM_MISSING;
    } else if (snowflake_cJSON_IsNull(blob)) {
        return SF_JSON_ERROR_ITEM_NULL;
    } else if (!snowflake_cJSON_IsNumber(blob)) {
        return SF_JSON_ERROR_ITEM_WRONG_TYPE;
    } else {
        *dest = (int64) blob->valuedouble;
        log_debug("Item and Value; %s: %i", item, *dest);
    }

    return SF_JSON_ERROR_NONE;
}

SF_JSON_ERROR STDCALL
json_detach_array_from_object(cJSON **dest, cJSON *data, const char *item) {
    cJSON *blob = snowflake_cJSON_DetachItemFromObject(data, item);
    if (!blob) {
        return SF_JSON_ERROR_ITEM_MISSING;
    } else if (snowflake_cJSON_IsNull(blob)) {
        return SF_JSON_ERROR_ITEM_NULL;
    } else if (!snowflake_cJSON_IsArray(blob)) {
        return SF_JSON_ERROR_ITEM_WRONG_TYPE;
    } else {
        if (*dest) {
            snowflake_cJSON_Delete(*dest);
        }
        *dest = blob;
        log_debug("Array: %s", item);
    }

    return SF_JSON_ERROR_NONE;
}

SF_JSON_ERROR STDCALL
json_detach_array_from_array(cJSON **dest, cJSON *data, int index) {
    cJSON *blob = snowflake_cJSON_DetachItemFromArray(data, index);
    if (!blob) {
        return SF_JSON_ERROR_ITEM_MISSING;
    } else if (snowflake_cJSON_IsNull(blob)) {
        return SF_JSON_ERROR_ITEM_NULL;
    } else if (!snowflake_cJSON_IsArray(blob)) {
        return SF_JSON_ERROR_ITEM_WRONG_TYPE;
    } else {
        if (*dest) {
            snowflake_cJSON_Delete(*dest);
        }
        *dest = blob;
        log_debug("Array at Index: %s", index);
    }

    return SF_JSON_ERROR_NONE;
}

SF_JSON_ERROR STDCALL
json_detach_object_from_array(cJSON **dest, cJSON *data, int index) {
    cJSON *blob = snowflake_cJSON_DetachItemFromArray(data, index);
    if (!blob) {
        return SF_JSON_ERROR_ITEM_MISSING;
    } else if (snowflake_cJSON_IsNull(blob)) {
        return SF_JSON_ERROR_ITEM_NULL;
    } else if (!snowflake_cJSON_IsObject(blob)) {
        return SF_JSON_ERROR_ITEM_WRONG_TYPE;
    } else {
        if (*dest) {
            snowflake_cJSON_Delete(*dest);
        }
        *dest = blob;
        log_debug("Object at index: %d", index);
    }

    return SF_JSON_ERROR_NONE;
}

ARRAY_LIST *json_get_object_keys(const cJSON *item) {
    if (!item || !snowflake_cJSON_IsObject(item)) {
        return NULL;
    }
    // Get the first key-value pair in the object
    const cJSON *next = item->child;
    ARRAY_LIST *al = sf_array_list_init();
    size_t counter = 0;
    char *key = NULL;

    while (next) {
        // Get key and add the arraylist
        key = next->string;
        sf_array_list_set(al, key, counter);
        // Increment counter and get the next element
        counter++;
        next = next->next;
    }

    return al;
}

size_t
json_resp_cb(char *data, size_t size, size_t nmemb, RAW_JSON_BUFFER *raw_json) {
    size_t data_size = size * nmemb;
    log_debug("Curl response size: %zu", data_size);
    raw_json->buffer = (char *) SF_REALLOC(raw_json->buffer,
                                           raw_json->size + data_size + 1);
    // Start copying where last null terminator existed
    sb_memcpy(&raw_json->buffer[raw_json->size], data_size, data, data_size);
    raw_json->size += data_size;
    // Set null terminator
    raw_json->buffer[raw_json->size] = '\0';
    return data_size;
}

sf_bool STDCALL is_retryable_http_code(long int code) {
    return ((code >= 500 && code < 600) || code == 400 || code == 403 ||
            code == 408) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
}

sf_bool STDCALL request(SF_CONNECT *sf,
                        cJSON **json,
                        const char *url,
                        URL_KEY_VALUE *url_params,
                        int num_url_params,
                        char *body,
                        SF_HEADER *header,
                        SF_REQUEST_TYPE request_type,
                        SF_ERROR_STRUCT *error,
                        sf_bool use_application_json_accept_type) {
    sf_bool ret = SF_BOOLEAN_FALSE;
    CURL *curl = NULL;
    char *encoded_url = NULL;
    SF_HEADER *my_header = NULL;
    curl = curl_easy_init();
    if (curl) {
        // Use passed in header if one exists
        if (header) {
            my_header = header;
        } else {
            // Create header
            my_header = sf_header_create();
            my_header->use_application_json_accept_type = use_application_json_accept_type;
            my_header->renew_session = SF_BOOLEAN_FALSE;
            if (!create_header(sf, my_header, error)) {
                goto cleanup;
            }
        }

        encoded_url = encode_url(curl, sf->protocol, sf->account, sf->host,
                                 sf->port, url, url_params, num_url_params,
                                 error, sf->directURL_param);
        if (encoded_url == NULL) {
            goto cleanup;
        }

        // Execute request and set return value to result
        if (request_type == POST_REQUEST_TYPE) {
            ret = curl_post_call(sf, curl, encoded_url, my_header, body, json,
                                 error);
        } else if (request_type == GET_REQUEST_TYPE) {
            ret = curl_get_call(sf, curl, encoded_url, my_header, json, error);
        } else {
            SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_REQUEST,
                                "An unknown request type was passed to the request function",
                                SF_SQLSTATE_UNABLE_TO_CONNECT);
            goto cleanup;
        }
    }

cleanup:
    // If we created our own header, then delete it
    if (!header) {
        sf_header_destroy(my_header);
    }
    curl_easy_cleanup(curl);
    SF_FREE(encoded_url);

    return ret;
}

sf_bool STDCALL renew_session(CURL *curl, SF_CONNECT *sf, SF_ERROR_STRUCT *error) {
    sf_bool ret = SF_BOOLEAN_FALSE;
    if (!is_string_empty(sf->directURL))
    {
      SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_REQUEST,
                          "Attempt to renew session with XPR direct URL",
                          SF_SQLSTATE_GENERAL_ERROR);
        return ret;
    }
    SF_JSON_ERROR json_error;
    const char *error_msg = NULL;
    char request_id[SF_UUID4_LEN];
    SF_HEADER *header = NULL;
    cJSON *body = NULL;
    cJSON *json = NULL;
    char *s_body = NULL;
    char *encoded_url = NULL;
    sf_bool success = SF_BOOLEAN_FALSE;
    cJSON *data = NULL;
    cJSON_bool has_token = 0;
    URL_KEY_VALUE url_params[] = {
      {.key="request_id=", .value=NULL, .formatted_key=NULL, .formatted_value=NULL, .key_size=0, .value_size=0},
    };
    if (!curl) {
        goto cleanup;
    }
    if (is_string_empty(sf->master_token)) {
        SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_REQUEST,
                            "Missing master token when trying to renew session. "
                              "Are you sure your connection was properly setup?",
                            SF_SQLSTATE_UNABLE_TO_CONNECT);
        goto cleanup;
    }
    log_debug("Updating session. Master token: *****");

    // Create header
    header = sf_header_create();
    header->use_application_json_accept_type = SF_BOOLEAN_FALSE;
    header->renew_session = SF_BOOLEAN_TRUE;
    if (!create_header(sf, header, error)) {
        goto cleanup;
    }

    // Create body and convert to string
    body = create_renew_session_json_body(sf->token);
    s_body = snowflake_cJSON_Print(body);

    // Create request id, set in url parameter and encode url
    uuid4_generate(request_id);
    url_params[0].value = request_id;
    encoded_url = encode_url(curl, sf->protocol, sf->account, sf->host,
                             sf->port, RENEW_SESSION_URL, url_params, 1, error,
                             sf->directURL_param);
    if (!encoded_url) {
        goto cleanup;
    }

    // Successful call, non-null json, successful success code, data object and session token must all be present
    // otherwise set an error
    if (!curl_post_call(sf, curl, encoded_url, header, s_body, &json, error) ||
        !json) {
        // Do nothing, let error propogate up from post call
        log_error("Curl call failed during renew session");
        goto cleanup;
    } else if ((json_error = json_copy_bool(&success, json, "success"))) {
        log_error("Error finding success in JSON response for renew session");
        JSON_ERROR_MSG(json_error, error_msg, "Success");
        SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_JSON, error_msg,
                            SF_SQLSTATE_UNABLE_TO_CONNECT);
        goto cleanup;
    } else if (!success) {
        log_error("Renew session was unsuccessful");
        SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_RESPONSE,
                            "Request returned as being unsuccessful",
                            SF_SQLSTATE_UNABLE_TO_CONNECT);
        goto cleanup;
    } else if (!(data = snowflake_cJSON_GetObjectItem(json, "data"))) {
        log_error("Missing data field in response");
        SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_JSON,
                            "No data object in JSON response",
                            SF_SQLSTATE_UNABLE_TO_CONNECT);
        goto cleanup;
    } else if (!(has_token = snowflake_cJSON_HasObjectItem(data, "sessionToken"))) {
        log_error("No session token in JSON response");
        SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_JSON,
                            "No session token in JSON response",
                            SF_SQLSTATE_UNABLE_TO_CONNECT);
        goto cleanup;
    } else {
        log_debug("Successful renew session");
        if (!set_tokens(sf, data, "sessionToken", "masterToken", error)) {
            goto cleanup;
        }
        log_debug("Finished updating session");
    }

    ret = SF_BOOLEAN_TRUE;

cleanup:
    sf_header_destroy(header);
    snowflake_cJSON_Delete(body);
    snowflake_cJSON_Delete(json);
    SF_FREE(s_body);
    SF_FREE(encoded_url);

    return ret;
}

void STDCALL reset_curl(CURL *curl) {
    curl_easy_reset(curl);
}

void STDCALL retry_ctx_free(RETRY_CONTEXT *retry_ctx) {
    SF_FREE(retry_ctx);
}

RETRY_CONTEXT *STDCALL retry_ctx_init(uint64 timeout) {
    RETRY_CONTEXT *retry_ctx = (RETRY_CONTEXT *) SF_CALLOC(1,
                                                           sizeof(RETRY_CONTEXT));
    retry_ctx->retry_timeout = timeout;
    retry_ctx->retry_count = 0;
    retry_ctx->sleep_time = 1;
    retry_ctx->djb = decorrelate_jitter_init(1, 16);
    return retry_ctx;
}

uint32 STDCALL retry_ctx_next_sleep(RETRY_CONTEXT *retry_ctx) {
    retry_ctx->sleep_time = decorrelate_jitter_next_sleep(retry_ctx->djb,
                                                          retry_ctx->sleep_time);
    return retry_ctx->sleep_time;
}

sf_bool STDCALL set_tokens(SF_CONNECT *sf,
                           cJSON *data,
                           const char *session_token_str,
                           const char *master_token_str,
                           SF_ERROR_STRUCT *error) {
    // Get token
    if (json_copy_string(&sf->token, data, session_token_str)) {
        log_error("No valid token found in response");
        SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_JSON,
                            "Cannot find valid session token in response",
                            SF_SQLSTATE_UNABLE_TO_CONNECT);
        return SF_BOOLEAN_FALSE;
    }
    // Get master token
    if (json_copy_string(&sf->master_token, data, master_token_str)) {
        log_error("No valid master token found in response");
        SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_JSON,
                            "Cannot find valid master token in response",
                            SF_SQLSTATE_UNABLE_TO_CONNECT);
        return SF_BOOLEAN_FALSE;
    }

    return SF_BOOLEAN_TRUE;
}

SF_HEADER* STDCALL sf_header_create() {
    SF_HEADER *sf_header = (SF_HEADER *) SF_CALLOC(1, sizeof(SF_HEADER));
    sf_header->header = NULL;
    sf_header->header_direct_query_token = NULL;
    sf_header->header_service_name = NULL;
    sf_header->header_token = NULL;
    sf_header->use_application_json_accept_type = SF_BOOLEAN_FALSE;
    sf_header->renew_session = SF_BOOLEAN_FALSE;
    return sf_header;
}

void STDCALL sf_header_destroy(SF_HEADER *sf_header) {
    if (!sf_header) {
        return;
    }

    SF_FREE(sf_header->header_token);
    SF_FREE(sf_header->header_service_name);
    SF_FREE(sf_header->header_direct_query_token);
    curl_slist_free_all(sf_header->header);
    SF_FREE(sf_header);
}
