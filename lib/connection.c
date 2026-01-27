#include <string.h>
#include "connection.h"
#include <snowflake/logger.h>
#include "snowflake/platform.h"
#include "memory.h"
#include "client_int.h"
#include "constants.h"
#include "error.h"
#include "curl_desc_pool.h"

#define curl_easier_escape(curl, string) curl_easy_escape(curl, string, 0)
#define QUERYCODE_LEN 7
#define REQUEST_GUID_KEY_SIZE 13

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
    char app_path[MAX_PATH];

    //Create Client Environment JSON blob
    client_env = snowflake_cJSON_CreateObject();
    snowflake_cJSON_AddStringToObject(client_env, "APPLICATION", application);
    snowflake_cJSON_AddStringToObject(client_env, "OS", sf_os_name());
#ifdef MOCK_ENABLED
    os_version[0] = '0';
    os_version[1] = '\0';
    strcpy(app_path, "/app/path");
#else
    sf_os_version(os_version, sizeof(os_version));
    sf_get_callers_executable_path(app_path, sizeof(app_path));
#endif
    snowflake_cJSON_AddStringToObject(client_env, "OS_VERSION", os_version);
    snowflake_cJSON_AddStringToObject(client_env, "APPLICATION_PATH", app_path);

    session_parameters = snowflake_cJSON_CreateObject();
    snowflake_cJSON_AddStringToObject(
        session_parameters,
        "AUTOCOMMIT",
        autocommit == SF_BOOLEAN_TRUE ? SF_BOOLEAN_INTERNAL_TRUE_STR
                                      : SF_BOOLEAN_INTERNAL_FALSE_STR);


    snowflake_cJSON_AddStringToObject(session_parameters, "TIMEZONE", timezone);

    //Create Request Data JSON blob
    data = snowflake_cJSON_CreateObject();
    snowflake_cJSON_AddStringToObject(data, CLIENT_APP_ID_KEY, int_app_name);
#ifdef MOCK_ENABLED
    snowflake_cJSON_AddStringToObject(data, CLIENT_APP_VERSION_KEY, "0.0.0");
#else
    snowflake_cJSON_AddStringToObject(data, CLIENT_APP_VERSION_KEY, int_app_version);
#endif
    snowflake_cJSON_AddStringToObject(data, "ACCOUNT_NAME", sf->account);
    if (sf->user && *(sf->user)) {
        snowflake_cJSON_AddStringToObject(data, "LOGIN_NAME", sf->user);
    }
    // Add password if one exists
    if (sf->password && *(sf->password)) {
        snowflake_cJSON_AddStringToObject(data, "PASSWORD", sf->password);

        if (sf->passcode_in_password) {
            snowflake_cJSON_AddStringToObject(data, "EXT_AUTHN_DUO_METHOD", "passcode");
            snowflake_cJSON_AddBoolToObject(data, "passcodeInPassword", SF_BOOLEAN_TRUE);
        }
        else if (sf->passcode && *(sf->passcode))
        {
            snowflake_cJSON_AddStringToObject(data, "EXT_AUTHN_DUO_METHOD", "passcode");
            snowflake_cJSON_AddStringToObject(data, "PASSCODE", sf->passcode);
        }
        else
        {
            snowflake_cJSON_AddStringToObject(data, "EXT_AUTHN_DUO_METHOD", "push");
        }

        if (sf->client_request_mfa_token) {
            snowflake_cJSON_AddBoolToObject(
                session_parameters,
                "CLIENT_REQUEST_MFA_TOKEN",
                1
            );
        }
    }

    if (sf->client_store_temporary_credential && getAuthenticatorType(sf->authenticator) == AUTH_EXTERNALBROWSER)
    {
        snowflake_cJSON_AddBoolToObject(session_parameters, "CLIENT_STORE_TEMPORARY_CREDENTIAL", sf->client_store_temporary_credential);
    }

    snowflake_cJSON_AddItemToObject(data, "CLIENT_ENVIRONMENT", client_env);
    snowflake_cJSON_AddItemToObject(data, "SESSION_PARAMETERS", session_parameters);

    //Create body
    body = snowflake_cJSON_CreateObject();
    snowflake_cJSON_AddItemToObject(body, "data", data);

    // update authentication information to body
    auth_update_json_body(sf, body);

    return body;
}

cJSON *STDCALL create_query_json_body(const char *sql_text,
                                      int64 sequence_id,
                                      const char *request_id,
                                      sf_bool is_describe_only,
                                      int64 multi_stmt_count)
{
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
    snowflake_cJSON_AddNumberToObject(body, "sequenceId", (double) sequence_id);
    snowflake_cJSON_AddNumberToObject(body, "querySubmissionTime", submission_time);
    snowflake_cJSON_AddBoolToObject(body, "describeOnly", is_describe_only);
    if (request_id)
    {
        snowflake_cJSON_AddStringToObject(body, "requestId", request_id);
    }

    cJSON* parameters = NULL;
    if (multi_stmt_count >= 0)
    {
        parameters = snowflake_cJSON_CreateObject();
        snowflake_cJSON_AddNumberToObject(parameters, "MULTI_STATEMENT_COUNT", (double)multi_stmt_count);
    }

#ifdef SF_WIN32
    if (!parameters)
    {
        parameters = snowflake_cJSON_CreateObject();
    }
    snowflake_cJSON_AddStringToObject(parameters, "C_API_QUERY_RESULT_FORMAT", "JSON");
    // temporary code to fake as ODBC to have multiple statements enabled
    snowflake_cJSON_AddStringToObject(parameters, "ODBC_QUERY_RESULT_FORMAT", "JSON");
#endif
    if (parameters)
    {
        snowflake_cJSON_AddItemToObject(body, "parameters", parameters);
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
        sf_sprintf(header->header_token, header_token_size,
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
        sf_sprintf(header->header_direct_query_token, header_direct_query_token_size,
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
        sf_sprintf(header->header_service_name, header_service_name_size,
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
      log_debug("SF_HEADER_USER_AGENT is null");
    }

    log_debug("Created header");

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
                               SF_ERROR_STRUCT *error,
                               int64 renew_timeout,
                               int8 retry_max_count,
                               int64 retry_timeout,
                               int64 *elapsed_time,
                               int8 *retried_count,
                               sf_bool *is_renew,
                               sf_bool renew_injection) {
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

    sf_bool is_new_strategy_url = is_new_retry_strategy_url(url);
    if (SF_BOOLEAN_TRUE == is_new_strategy_url)
    {
      if (!add_appinfo_header(sf, header, error)) {
        return ret;
      }
    }

    do {
        if (!http_perform(curl, POST_REQUEST_TYPE, url, header, body, NULL, json, NULL, NULL,
                          retry_timeout, sf->network_timeout, SF_BOOLEAN_FALSE, error,
                          sf->insecure_mode, sf->ocsp_fail_open,
                          sf->crl_check, sf->crl_advisory, sf->crl_allow_no_crl,
                          sf->crl_disk_caching, sf->crl_memory_caching,
                          sf->crl_download_timeout,
                          sf->retry_on_curle_couldnt_connect_count, renew_timeout, retry_max_count,
                          elapsed_time, retried_count, is_renew,
                          renew_injection, sf->proxy, sf->no_proxy,
                          sf->include_retry_reason, is_new_strategy_url) ||
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
                                    error, renew_timeout, retry_max_count, retry_timeout,
                                    elapsed_time, retried_count, is_renew, renew_injection)) {
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

        sf_bool isAsyncExec = SF_BOOLEAN_FALSE;
        cJSON *json_body = snowflake_cJSON_Parse(body);
        if (json_body && snowflake_cJSON_IsObject(json_body)) {
          cJSON* async = snowflake_cJSON_GetObjectItem(json_body, "asyncExec");
          if (async && snowflake_cJSON_IsBool(async)) {
            isAsyncExec = snowflake_cJSON_IsTrue(async);
          }
        }

        if (!isAsyncExec) {
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

            log_debug("Ping pong starting...");
            if (!request(sf, json, result_url, NULL, 0, NULL, header,
                         GET_REQUEST_TYPE, error, SF_BOOLEAN_FALSE,
                         0, retry_max_count, retry_timeout, NULL, NULL, NULL, SF_BOOLEAN_FALSE)) {
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
                              SF_ERROR_STRUCT *error,
                              int64 renew_timeout,
                              int8 retry_max_count,
                              int64 retry_timeout,
                              int64* elapsed_time,
                              int8* retried_count) {
    SF_JSON_ERROR json_error;
    const char *error_msg;
    char query_code[QUERYCODE_LEN];
    char *result_url = NULL;
    SF_HEADER *new_header = NULL;
    sf_bool ret = SF_BOOLEAN_FALSE;

    // Set to 0
    memset(query_code, 0, QUERYCODE_LEN);

    do {
        if (!http_perform(curl, GET_REQUEST_TYPE, url, header, NULL, NULL, json, NULL, NULL,
                          get_retry_timeout(sf), sf->network_timeout, SF_BOOLEAN_FALSE, error,
                          sf->insecure_mode, sf->ocsp_fail_open,
                          sf->crl_check, sf->crl_advisory, sf->crl_allow_no_crl,
                          sf->crl_disk_caching, sf->crl_memory_caching,
                          sf->crl_download_timeout,
                          sf->retry_on_curle_couldnt_connect_count, renew_timeout, retry_max_count, elapsed_time, retried_count, NULL,
                          SF_BOOLEAN_FALSE, sf->proxy, sf->no_proxy, SF_BOOLEAN_FALSE, SF_BOOLEAN_FALSE) ||
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
                if (!curl_get_call(sf, curl, url, new_header, json, error, renew_timeout, retry_max_count, retry_timeout,elapsed_time, retried_count)) {
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
get_next_sleep_with_jitter(DECORRELATE_JITTER_BACKOFF *djb, uint32 sleep, uint64 retry_count) {
  float cur_wait_time = sleep;
  cur_wait_time = choose_random(cur_wait_time + get_jitter(sleep),
                                pow(2, retry_count) + get_jitter(sleep));
  // no cap for new retry strategy while keep the existing cap for other requests
  if ((djb->cap != SF_NEW_STRATEGY_BACKOFF_CAP) && (cur_wait_time > djb->cap))
  {
    cur_wait_time = djb->cap;
  }
  // at least wait for 1 seconds
  if (cur_wait_time < 1)
  {
    cur_wait_time = 1;
  }

  return (uint32)cur_wait_time;
}

char * STDCALL encode_url(CURL *curl,
                 const char *protocol,
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
      URL_PARAM_REQEST_GUID,
      "00000000-0000-0000-0000-000000000000",
      NULL,
      NULL,
      0,
      0
    };
    const char *amp = "&";
    size_t amp_size = strlen(amp);

    if (host_empty)
    {
      SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_CONNECTION_PARAMS,
                          "Invalid host trying to create encoded url",
                          SF_SQLSTATE_UNABLE_TO_CONNECT);
      goto cleanup;
    }

    // Set proper format based on variables passed into encode URL.
    // The format includes format specifiers that will be consumed by empty fields
    // (i.e if port is empty, add an extra specifier so that we have 1 call to snprintf, vs. 4 different calls)
    // Format specifier order is protocol, then then host, then port, then url.
    // Base size increases reflect the number of static characters in the format string (i.e. ':', '/', '.')
    if (!port_empty) {
        format = "%s://%s:%s%s";
        base_url_size += 4;
    } else {
        format = "%s://%s%s%s";
        base_url_size += 3;
        port = "";
    }
    base_url_size +=
      strlen(protocol) + strlen(host) + strlen(port) +
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

    encoded_url_size += URL_EXTRA_SIZE;

    encoded_url = (char *) SF_CALLOC(1, encoded_url_size);
    if (!encoded_url) {
        SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_OUT_OF_MEMORY,
                            "Ran out of memory trying to create encoded url",
                            SF_SQLSTATE_UNABLE_TO_CONNECT);
        goto cleanup;
    }
    sf_sprintf(encoded_url, base_url_size, format, protocol, host, port,
             url);

    // Initially add the query delimiter "?"
    sf_strncat(encoded_url, encoded_url_size, URL_QUERY_DELIMITER, strlen(URL_QUERY_DELIMITER));

    // Add encoded URL parameters to encoded_url buffer
    for (i = 0; i < num_args; i++) {
        sf_strncat(encoded_url, encoded_url_size, vars[i].formatted_key, vars[i].key_size);
        sf_strncat(encoded_url, encoded_url_size, vars[i].formatted_value, vars[i].value_size);
        sf_strncat(encoded_url, encoded_url_size, amp, amp_size);
    }

    // Add encoded request_guid to encoded_url buffer
    sf_strncat(encoded_url, encoded_url_size, request_guid.formatted_key, request_guid.key_size);
    sf_strncat(encoded_url, encoded_url_size, request_guid.formatted_value, request_guid.value_size);

    // Adding the extra url param (setter of extraUrlParams is responsible to make
    // sure extraUrlParams is correct)
    if (extraUrlParams && !is_string_empty(extraUrlParams))
    {
      sf_strncat(encoded_url, encoded_url_size, URL_PARAM_DELIM, 1);
      sf_strncat(encoded_url, encoded_url_size, extraUrlParams, encoded_url_size);
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
        // We don't check the return status everywhere
        // make sure that the value is set to NULL to enable
        // NULL checks.
        SF_FREE(*dest);
        return SF_JSON_ERROR_ITEM_MISSING;
    } else if (snowflake_cJSON_IsNull(blob)) {
        SF_FREE(*dest);
        *dest = NULL;
        return SF_JSON_ERROR_ITEM_NULL;
    } else if (!snowflake_cJSON_IsString(blob)) {
        SF_FREE(*dest);
        *dest = NULL;
        return SF_JSON_ERROR_ITEM_WRONG_TYPE;
    } else {
        blob_size = strlen(blob->valuestring) + 1;
        SF_FREE(*dest);
        *dest = (char *) SF_CALLOC(1, blob_size);
        if (!*dest) {
            return SF_JSON_ERROR_OOM;
        }
        sf_strncpy(*dest, blob_size, blob->valuestring, blob_size);

        if (strstr(item, "token") || strstr(item, "Token") || strstr(item, "TOKEN") ||
            strstr(item, "key") || strstr(item, "Key") || strstr(item, "KEY")) {
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
        sf_strncpy(dest, dest_size, blob->valuestring, dest_size);
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
char_resp_cb(char *data, size_t size, size_t nmemb, RAW_CHAR_BUFFER *raw_buf) {
    size_t data_size = size * nmemb;
    log_debug("Curl response size: %zu", data_size);
    raw_buf->buffer = (char *) SF_REALLOC(raw_buf->buffer,
                                          raw_buf->size + data_size + 1);
    // Start copying where last null terminator existed
    sf_memcpy(&raw_buf->buffer[raw_buf->size], data_size, data, data_size);
    raw_buf->size += data_size;
    // Set null raw_buf
    raw_buf->buffer[raw_buf->size] = '\0';
    return data_size;
}

sf_bool STDCALL is_retryable_http_code(long int code) {
    return ((code >= 500 && code < 600) || code == 403 ||
            code == 408 || code == 429) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
}

sf_bool STDCALL request(SF_CONNECT *sf,
                        cJSON **json,
                        const char *url_path,
                        URL_KEY_VALUE *url_params,
                        int num_url_params,
                        char *body,
                        SF_HEADER *header,
                        SF_REQUEST_TYPE request_type,
                        SF_ERROR_STRUCT *error,
                        sf_bool use_application_json_accept_type,
                        int64 renew_timeout,
                        int8 retry_max_count,
                        int64 retry_timeout,
                        int64 *elapsed_time,
                        int8 *retried_count,
                        sf_bool *is_renew,
                        sf_bool renew_injection) {
    sf_bool ret = SF_BOOLEAN_FALSE;
    int url_size = (sf->protocol ? strlen(sf->protocol) : 0) +
      (sf->host ? strlen(sf->host) : 0) + (sf->port ? strlen(sf->port) : 0) + 5;
    char *url = (char *)SF_CALLOC(1, url_size);
    sf_sprintf(url, url_size, "%s://%s:%s", sf->protocol, sf->host, sf->port);
    void* curl_desc = get_curl_desc_from_pool(url, sf->proxy, sf->no_proxy);
    CURL *curl = get_curl_from_desc(curl_desc);
    char *encoded_url = NULL;
    SF_HEADER *my_header = NULL;
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

        encoded_url = encode_url(curl, sf->protocol, sf->host,
                                 sf->port, url_path, url_params, num_url_params,
                                 error, sf->directURL_param);
        if (encoded_url == NULL) {
            goto cleanup;
        }

        // Execute request and set return value to result
        if (request_type == POST_REQUEST_TYPE) {
            ret = curl_post_call(sf, curl, encoded_url, my_header, body, json,
                                 error, renew_timeout, retry_max_count, retry_timeout,
                                 elapsed_time, retried_count, is_renew,
                                 renew_injection);
        } else if (request_type == GET_REQUEST_TYPE) {
            ret = curl_get_call(sf, curl, encoded_url, my_header, json, error,
                renew_timeout, retry_max_count, retry_timeout, elapsed_time, retried_count);
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
    free_curl_desc(curl_desc);
    SF_FREE(encoded_url);
    SF_FREE(url);

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
    encoded_url = encode_url(curl, sf->protocol, sf->host,
                             sf->port, RENEW_SESSION_URL, url_params, 1, error,
                             sf->directURL_param);
    if (!encoded_url) {
        goto cleanup;
    }

    // Successful call, non-null json, successful success code, data object and session token must all be present
    // otherwise set an error
    if (!curl_post_call(sf, curl, encoded_url, header, s_body, &json, error,
                        0, sf->retry_count, get_retry_timeout(sf), NULL, NULL, NULL, SF_BOOLEAN_FALSE) ||
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

uint32 STDCALL retry_ctx_next_sleep(RETRY_CONTEXT *retry_ctx) {
    uint32 cur_wait_time = retry_ctx->sleep_time;
    ++retry_ctx->retry_count;
    // caculate next sleep time
    retry_ctx->sleep_time = get_next_sleep_with_jitter(retry_ctx->djb, retry_ctx->sleep_time, retry_ctx->retry_count);

    // limit the sleep time within retry timeout
    uint32 time_elapsed = time(NULL) - (retry_ctx->start_time / 1000);
    if (time_elapsed >= retry_ctx->retry_timeout)
    {
      // retry timeout is checked before calling retry_ctx_next_sleep
      // so we just get bad timing here, sleep 1 seconds so the timeout
      // can be caught right after
      return 1;
    }
    return uimin(cur_wait_time, (uint32)(retry_ctx->retry_timeout - time_elapsed));
}

sf_bool STDCALL retry_ctx_update_url(RETRY_CONTEXT *retry_ctx,
                                     char* url,
                                     sf_bool include_retry_reason) {
    char *request_guid_ptr = strstr(url, URL_PARAM_REQEST_GUID);
    if (!request_guid_ptr)
    {
      // no update for url doesn't have request guid
      return SF_BOOLEAN_TRUE;
    }

    // extra space is allocated in encode_url()
    size_t buf_size = URL_EXTRA_SIZE;
    int written_bytes = 0;

    // macro to simplify the code
    #define SPRINT_TO_BUFFER(ptr, fmt, param)                       \
    {                                                               \
      if (1 < buf_size) { /* at least 1 char available */           \
        written_bytes = sf_sprintf(ptr, buf_size, fmt, param);      \
      }                                                             \
      if (1 >= buf_size || 0 == written_bytes) {                    \
        log_error("Out of space: retry_ctx_update_url");            \
        return SF_BOOLEAN_FALSE;                                    \
      }                                                             \
      ptr += written_bytes;                                         \
      buf_size -= written_bytes;                                    \
    }

    // for non-query request, renew guid only
    if (!strstr(url, QUERY_URL))
    {
      request_guid_ptr += strlen(URL_PARAM_REQEST_GUID);
      if (uuid4_generate_non_terminated(request_guid_ptr)) {
        log_error("Failed to generate new request GUID");
        return SF_BOOLEAN_FALSE;
      }
      return SF_BOOLEAN_TRUE;
    }

    char * retry_context_ptr = strstr(url, URL_PARAM_RETRY_COUNT);
    if (!retry_context_ptr)
    {
      // original url that doesn't have retry context yet, replace start from guid
      retry_context_ptr = request_guid_ptr;
    }

    // retry count
    SPRINT_TO_BUFFER(retry_context_ptr, "%s", URL_PARAM_RETRY_COUNT);
    SPRINT_TO_BUFFER(retry_context_ptr, "%llu", retry_ctx->retry_count);
    SPRINT_TO_BUFFER(retry_context_ptr, "%s", URL_PARAM_DELIM);

    if (include_retry_reason == SF_BOOLEAN_TRUE)
    {
      // retry reason
      SPRINT_TO_BUFFER(retry_context_ptr, "%s", URL_PARAM_RETRY_REASON);
      SPRINT_TO_BUFFER(retry_context_ptr, "%lu", retry_ctx->retry_reason);
      SPRINT_TO_BUFFER(retry_context_ptr, "%s", URL_PARAM_DELIM);
      // clear retry reason for the next retry attempt
      retry_ctx->retry_reason = 0;
    }

    // add client start time in milliseconds
    SPRINT_TO_BUFFER(retry_context_ptr, "%s", URL_PARAM_CLIENT_START_TIME);
    SPRINT_TO_BUFFER(retry_context_ptr, "%llu", retry_ctx->start_time);
    SPRINT_TO_BUFFER(retry_context_ptr, "%s", URL_PARAM_DELIM);

    // add request guid after retry context
    request_guid_ptr = retry_context_ptr;

    SPRINT_TO_BUFFER(request_guid_ptr, "%s", URL_PARAM_REQEST_GUID);
    if (uuid4_generate(request_guid_ptr)) {
      log_error("Failed to generate new request GUID");
      return SF_BOOLEAN_FALSE;
    }

    return SF_BOOLEAN_TRUE;
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
    sf_header->header_app_id = NULL;
    sf_header->header_app_version = NULL;
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
    SF_FREE(sf_header->header_app_id);
    SF_FREE(sf_header->header_app_version);
    curl_slist_free_all(sf_header->header);
    SF_FREE(sf_header);
}

sf_bool is_new_retry_strategy_url(const char * url)
{
  if (!url)
  {
    return SF_BOOLEAN_FALSE;
  }

  if (strstr(url, SESSION_URL) ||
      strstr(url, RENEW_SESSION_URL) ||
      strstr(url, AUTHENTICATOR_URL))
  {
    return SF_BOOLEAN_TRUE;
  }

  return SF_BOOLEAN_FALSE;
}

sf_bool add_appinfo_header(SF_CONNECT *sf, SF_HEADER *header, SF_ERROR_STRUCT *error) {
  sf_bool ret = SF_BOOLEAN_FALSE;
  size_t header_appid_size;
  size_t header_appver_size;

  // Generate header tokens
  header_appid_size = strlen(HEADER_CLIENT_APP_ID_FORMAT) - 2 +
    strlen(sf->application_name) + 1;
  header_appver_size = strlen(HEADER_CLIENT_APP_VERSION_FORMAT) - 2 +
    strlen(sf->application_version) + 1;

  // check NULL first to ensure the header won't be added twice
  if (!header->header_app_id)
  {
    header->header_app_id = (char *)SF_CALLOC(1, header_appid_size);
    if (!header->header_app_id) {
      SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_OUT_OF_MEMORY,
        "Ran out of memory trying to create header CLIENT_APP_ID",
        SF_SQLSTATE_UNABLE_TO_CONNECT);
      goto error;
    }
    sf_sprintf(header->header_app_id, header_appid_size,
      HEADER_CLIENT_APP_ID_FORMAT, sf->application_name);
    header->header = curl_slist_append(header->header, header->header_app_id);
  }

  if (!header->header_app_version)
  {
    header->header_app_version = (char *)SF_CALLOC(1, header_appver_size);
    if (!header->header_app_version) {
      SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_OUT_OF_MEMORY,
        "Ran out of memory trying to create header CLIENT_APP_VERSION",
        SF_SQLSTATE_UNABLE_TO_CONNECT);
      goto error;
    }
    sf_sprintf(header->header_app_version, header_appver_size,
      HEADER_CLIENT_APP_VERSION_FORMAT, sf->application_version);
    header->header = curl_slist_append(header->header, header->header_app_version);
  }

  log_debug("Added application infor header");

  ret = SF_BOOLEAN_TRUE;

error:
  return ret;
}

float choose_random(float min, float max)
{
  // get a number between 0 ~ 1
  float scale = rand() / (float)RAND_MAX;
  // scale to min ~ max
  return min + scale * (max - min);
}

float get_jitter(int cur_wait_time) {
  float multiplication_factor = choose_random(-1, 1);
  float jitter_amount = 0.5 * cur_wait_time * multiplication_factor;
  return jitter_amount;
}

// utility function to get the less one, take 0 as special value for unlimited
int64 get_less_one(int64 a, int64 b)
{
  if (a == 0)
  {
    return b;
  }
  if (b == 0)
  {
    return a;
  }
  return a < b ? a : b;
}

int64 get_login_timeout(SF_CONNECT *sf)
{
  return get_less_one(sf->login_timeout, sf->retry_timeout);
}

int64 get_retry_timeout(SF_CONNECT *sf)
{
  return sf->retry_timeout;
}

int8 get_login_retry_count(SF_CONNECT *sf)
{
  return (int8)get_less_one(sf->retry_on_connect_count, sf->retry_count);
}

sf_bool is_one_time_token_request(cJSON* resp)
{
  return snowflake_cJSON_HasObjectItem(resp, "cookieToken") || snowflake_cJSON_HasObjectItem(resp, "sessionToken") || snowflake_cJSON_HasObjectItem(resp, "token_type");;
}

size_t non_json_resp_write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
  return char_resp_cb(ptr, size, nmemb, userdata);
}

sf_bool is_password_required(AuthenticatorType auth)
{
    switch (auth)
    {
      case AUTH_JWT:
      case AUTH_OAUTH:
      case AUTH_PAT:
      case AUTH_EXTERNALBROWSER:
      case AUTH_OAUTH_AUTHORIZATION_CODE:
      case AUTH_OAUTH_CLIENT_CREDENTIALS:
      case AUTH_WIF:
        return 0;
      default:
        return 1;
    }
}

sf_bool is_secure_storage_auth(AuthenticatorType auth)
{
    switch (auth)
    {
      case AUTH_USR_PWD_MFA:
      case AUTH_SNOWFLAKE:
      case AUTH_EXTERNALBROWSER:
      case AUTH_OAUTH_AUTHORIZATION_CODE:
        return 1;
      default:
        return 0;
    }
}