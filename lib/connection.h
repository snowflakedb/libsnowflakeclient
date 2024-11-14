/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKE_CONNECTION_H
#define SNOWFLAKE_CONNECTION_H

/*
  Apache arrow is not implemented for WIN32
  The symbol _WIN32 is defined by the compiler to indicate that this is a (32bit) Windows compilation. 
  Unfortunately, for historical reasons, it is also defined for 64-bit compilation.
  The symbol _WIN64 is defined by the compiler to indicate that this is a 64-bit Windows compilation.
  Thus:
  To identify unambiguously whether the compilation is 32-bit Windows, one tests both _WIN32 and _WIN64 as in:
*/
#if !defined(SF_WIN64) && !defined(SF_WIN32) 
#if defined(_WIN64)
#define SF_WIN64
#elif defined(_WIN32)
#define SF_WIN32
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#pragma comment(lib, "wldap32.lib" )
#pragma comment(lib, "crypt32.lib" )
#pragma comment(lib, "Ws2_32.lib")

#define CURL_STATICLIB 
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <snowflake/client.h>
#include "snowflake/platform.h"
#include "cJSON.h"
#include "arraylist.h"

/**
 * Request type
 */
typedef enum SF_REQUEST_TYPE {
            /** not handling any request right now */
            NONE_REQUEST_TYPE,

            /** we are doing a http get */
            GET_REQUEST_TYPE,

            /** we are doing a http put */
            PUT_REQUEST_TYPE,

            /** we are doing a http post */
            POST_REQUEST_TYPE,

            /** we are doing a http delete */
            DELETE_REQUEST_TYPE,
} SF_REQUEST_TYPE;

/**
 * Error types returned by cJSON convenience functions.
 */
typedef enum SF_JSON_ERROR {
    /** No error */
    SF_JSON_ERROR_NONE,

    /** JSON item is not found in provieded blob. */
    SF_JSON_ERROR_ITEM_MISSING,

    /** JSON item is the wrong type. Try a different convenience function. */
    SF_JSON_ERROR_ITEM_WRONG_TYPE,

    /** JSON item value is null */
    SF_JSON_ERROR_ITEM_NULL,

    /** Out of Memory */
    SF_JSON_ERROR_OOM
} SF_JSON_ERROR;

/**
 * Dynamically growing char buffer to hold retrieved in cURL call.
 */
typedef struct RAW_JSON_BUFFER {
    // Char buffer
    char *buffer;
    // Number of characters in char buffer
    size_t size;
} RAW_JSON_BUFFER;

/**
 * URL Parameter struct used to construct an encoded URL.
 */
typedef struct URL_KEY_VALUE {
    // URL param key
    const char *key;
    // URL param value
    char *value;
    // URL param formatted key
    const char *formatted_key;
    // URL param formatted value
    char *formatted_value;
    // Formatted key size
    size_t key_size;
    // Formatted value size
    size_t value_size;
} URL_KEY_VALUE;

// internal definition for backoff time
#define SF_BACKOFF_BASE 1
#define SF_BACKOFF_CAP 16
// max uint32 value to mark no backoff cap for new retry strategy
#define SF_NEW_STRATEGY_BACKOFF_CAP 0xFFFFFFFF
/**
 * Used to keep track of min and max backoff time for a connection retry
 */
typedef struct DECORRELATE_JITTER_BACKOFF {
    // Minimum backoff time
    uint32 base;
    // Maximum backoff time
    uint32 cap;
} DECORRELATE_JITTER_BACKOFF;

/**
 * Connection retry struct to keep track of retry status
 */
typedef struct RETRY_CONTEXT {
    // Number of retries
    uint64 retry_count;
    // http error code as retry reason, 0 for the cases that http error code
    // not available such as network error
    uint32 retry_reason;
    // Retry timeout in number of seconds.
    uint64 retry_timeout;
    // Time to sleep in seconds
    uint32 sleep_time;
    // Decorrelate Jitter is used to determine sleep time
    DECORRELATE_JITTER_BACKOFF *djb;
    // start time to track on retry timeout
    uint64 start_time;
} RETRY_CONTEXT;

typedef struct SF_HEADER {
    struct curl_slist *header;
    char *header_direct_query_token;
    char *header_service_name;
    char *header_token;
    char *header_app_id;
    char *header_app_version;

    sf_bool use_application_json_accept_type;
    sf_bool renew_session;
} SF_HEADER;

/**
 * Debug struct from curl example. Need to update at somepoint.
 */
struct data {
    char trace_ascii; /* 1 or 0 */
};

/**
* curl response struct to retrieve non-json response
*/
typedef struct non_json_response {
    size_t (*write_callback)(char *ptr, size_t size, size_t nmemb, void *userdata);
    void * buffer;
} NON_JSON_RESP;

/**
 * Macro to get a custom error message to pass to the Snowflake Error object.
 */
#define JSON_ERROR_MSG(e, em, t) \
switch(e) \
{ \
    case SF_JSON_ERROR_ITEM_MISSING: (em) = #t " missing from JSON response"; break; \
    case SF_JSON_ERROR_ITEM_WRONG_TYPE: (em) = #t " is wrong type (expected a string)"; break; \
    case SF_JSON_ERROR_ITEM_NULL: (em) = #t " is null"; break; \
    case SF_JSON_ERROR_OOM: (em) = #t " caused an out of memory error"; break; \
    default: (em) = "Received unknown JSON error code trying to find " #t ; break; \
}

/**
 * Creates connection authorization body as a cJSON blob. cJSON blob must be freed by the caller using cJSON_Delete.
 *
 * @param sf Snowflake Connection object. Uses account, user, and password from Connection struct.
 * @param application Application type.
 * @param int_app_name Client ID.
 * @param int_app_version Client App Version. Used to ensure we reject unsupported clients.
 * @param timezone Timezone
 * @param autocommit Wheter autocommit is enabled.
 * @return Authorization cJSON Body.
 */
cJSON *STDCALL create_auth_json_body(SF_CONNECT *sf, const char *application, const char *int_app_name,
                                     const char *int_app_version, const char* timezone, sf_bool autocommit);

/**
 * Creates a cJSON blob used to execute queries. cJSON blob must be freed by the caller using cJSON_Delete.
 *
 * @param sql_text The sql query to send to Snowflake
 * @param sequence_id Sequence ID from the Snowflake Connection object.
 * @param request_id  requestId to be passed as a part of body instead of header.
 * @param is_describe_only is the query describe only.
 * @return Query cJSON Body.
 */
cJSON *STDCALL create_query_json_body(const char *sql_text, int64 sequence_id, const char *request_id, sf_bool is_describe_only);

/**
 * Creates a cJSON blob that is used to renew a session with Snowflake. cJSON blob must be freed by the caller using
 * cJSON_Delete.
 *
 * @param old_token Expired session token from Snowflake Connection object.
 * @return Renew Session cJSON Body.
 */
cJSON *STDCALL create_renew_session_json_body(const char *old_token);

sf_bool STDCALL create_header(SF_CONNECT *sf, SF_HEADER *header, SF_ERROR_STRUCT *error);

/**
 * Used to issue a cURL POST call to Snowflake. Includes support for ping-pong and renew session. If the request was
 * successful, we return 1, otherwise 0
 *
 * @param sf Snowflake Connection object. Used to get network timeout and pass to renew session
 * @param curl cURL instance that is currently in use for the request
 * @param url URL to send the request to
 * @param header Header passed to cURL for use in the request
 * @param body Body passed to cURL for use in the request
 * @param json Reference to a cJSON pointer that is used to store the JSON response upon a successful request
 * @param error Reference to the Snowflake Error object to set an error if one occurs
 * @param renew_timeout   For key pair authentication. Credentials could expire
 *                        during the connection retry. Set renew timeout in such
 *                        case so http_perform will return when renew_timeout is
 *                        reached and the caller can renew the credentials and
 *                        then go back to the retry by calling curl_post_call() again.
 *                        0 means no renew timeout needed.
 *                        For Okta Authentication, whenever the authentication failed, the connector
 *                        should update the onetime token. In this case, the renew timeout < 0, which means
 *                        the request should be renewed for each request.
 * @param retry_max_count The max number of retry attempts. 0 means no limit.
 * @param retry_timeout   The timeout for retry. Will stop retry when it's exceeded. 0 means no limit.
 * @param elapsed_time    The in/out paramter to record the elapsed time before
 *                        curl_post_call() returned due to renew timeout last time
 * @param retried_count   The in/out paramter to record the number of retry attempts
 *                        has been done before http_perform() returned due to renew
 *                        timeout last time.
 * @param is_renew        The output paramter to indecate whether curl_post_call()
 *                        returns due to renew timeout.
 * @param renew_injection For test purpose, forcely trigger renew timeout.
 * @return Success/failure status of post call. 1 = Success; 0 = Failure
 */
sf_bool STDCALL curl_post_call(SF_CONNECT *sf, CURL *curl, char *url, SF_HEADER *header, char *body,
                               cJSON **json, SF_ERROR_STRUCT *error,
                               int64 renew_timeout, int8 retry_max_count, int64 retry_timeout,
                               int64 *elapsed_time, int8 *retried_count,
                               sf_bool *is_renew, sf_bool renew_injection);

/**
 * Used to issue a cURL GET call to Snowflake. Includes support for renew session. If the request was successful,
 * we return 1, otherwise 0
 *
 * @param sf Snowflake Connection object. Used to get network timeout and pass to renew session
 * @param curl cURL instance that is currently in use for the request
 * @param url URL to send the request to
 * @param header Header passed to cURL for use in the request
 * @param json Reference to a cJSON pointer that is used to store the JSON response upon a successful request
 * @param error Reference to the Snowflake Error object to set an error if one occurs
 * @param renew_timeout   For key pair authentication. Credentials could expire
 *                        during the connection retry. Set renew timeout in such
 *                        case so http_perform will return when renew_timeout is
 *                        reached and the caller can renew the credentials and
 *                        then go back to the retry by calling curl_post_call() again.
 *                        0 means no renew timeout needed.
 *                        For Okta Authentication, whenever the authentication failed, the connector
 *                        should update the onetime token. In this case, the renew timeout < 0, which means
 *                        the request should be renewed for each request.
 * @param retry_max_count The max number of retry attempts. 0 means no limit.
 * @param retry_timeout   The timeout for retry. Will stop retry when it's exceeded. 0 means no limit.
 * @param elapsed_time    The in/out paramter to record the elapsed time before
 *                        curl_post_call() returned due to renew timeout last time
 * @param retried_count   The in/out paramter to record the number of retry attempts
 *                        has been done before http_perform() returned due to renew
 *                        timeout last time.
 * @return Success/failure status of get call. 1 = Success; 0 = Failure
 */
sf_bool STDCALL curl_get_call(SF_CONNECT *sf, CURL *curl, char *url, SF_HEADER *header, cJSON **json,
                              SF_ERROR_STRUCT *error, int64 renew_timeout, int8 retry_max_count, int64 retry_timeout, int64* elapsed_time, int8* retried_count);

/**
 * Used to determine the sleep time during the next backoff caused by request failure.
 *
 * @param djb Decorrelate Jitter Backoff object used to determine min and max backoff time.
 * @param sleep Duration of last sleep in seconds.
 * @param the current number of retry.
 * @return Number of seconds to sleep.
 */
uint32 get_next_sleep_with_jitter(DECORRELATE_JITTER_BACKOFF *djb, uint32 sleep, uint64 retry_count);

/**
 * Creates a URL that is safe to use with cURL. Caller must free the memory associated with the encoded URL.
 *
 * @param curl cURL object for the request.
 * @param protocol Protocol to use in the request. Either HTTP or HTTPS.
 * @param host Host to connect to. Must be set using SF_CONNECT.host which has been set already before connection.
 * @param port Port to connect to. Used when connecting to a non-conventional port.
 * @param url URL path to use.
 * @param vars URL parameters to add to the encoded URL.
 * @param num_args Number of URL parameters.
 * @param error Reference to the Snowflake Error object to set an error if one occurs.
 * @param extraUrlParams an extra set of url parameters (used in case of direct connection
 * to XP resources)
 * @return Returns a pointer to a string which is the the encoded URL.
 */
char * STDCALL encode_url(CURL *curl, const char *protocol, const char *host, const char *port,
                          const char *url, URL_KEY_VALUE* vars, int num_args, SF_ERROR_STRUCT *error, char *extraUrlParams);

/**
 * Determines if a string is empty by checking if the passed in string is NULL or contains a null terminator as its
 * first character
 *
 * @param str String to check
 * @return True (1) if empty, False (0) if not empty
 */
sf_bool is_string_empty(const char * str);

/**
 * A convenience function that copies an item value, specified by the item field, to the dest field if the item
 * exists, isn't null and is the right type.
 *
 * @param dest A reference to the location of a sf_bool variable where the JSON item value should be copied to.
 * @param data A reference to the cJSON object to find the value specified by item.
 * @param item The name (key) of the object item that you are looking for.
 * @return Returns an SF_JSON_ERROR return code; can be processed or ignored by the caller.
 */
SF_JSON_ERROR STDCALL json_copy_bool(sf_bool *dest, cJSON *data, const char *item);

/**
 * A convenience function that copies an item value, specified by the item field, to the dest field if the item
 * exists, isn't null and is the right type.
 *
 * @param dest A reference to the location of a 64-bit int where the JSON item value should be copied to.
 * @param data A reference to the cJSON object to find the value specified by item.
 * @param item The name (key) of the object item that you are looking for.
 * @return Returns an SF_JSON_ERROR return code; can be processed or ignored by the caller.
 */
SF_JSON_ERROR STDCALL json_copy_int(int64 *dest, cJSON *data, const char *item);

/**
 * A convenience function that copies an item value, specified by the item field, to the dest field if the item
 * exists, isn't null and is the right type. The caller is responsible for freeing the memory referenced by dest.
 *
 * @param dest A reference to a char pointer where the JSON string pointer should be copied to. The pointer referenced
 *             by dest needs to be freed by the caller.
 * @param data A reference to the cJSON object to find the value specified by item.
 * @param item The name (key) of the object item that you are looking for.
 * @return Returns an SF_JSON_ERROR return code; can be processed or ignored by the caller.
 */
SF_JSON_ERROR STDCALL json_copy_string(char **dest, cJSON *data, const char *item);

/**
 * A convenience function that copies an item value, specified by the item field, to the string buffer specified by
 * dest field if the item exists, isn't null and is the right type.
 *
 * @param dest A reference to the string buffer where the JSON item value should be copied to.
 * @param data A reference to the cJSON object to find the value specified by item.
 * @param item The name (key) of the object item that you are looking for.
 * @param dest_size The size of the string buffer
 * @return Returns an SF_JSON_ERROR return code; can be processed or ignored by the caller.
 */
SF_JSON_ERROR STDCALL json_copy_string_no_alloc(char *dest, cJSON *data, const char *item, size_t dest_size);

/**
 * A convenience function that detaches a cJSON array from a cJSON object blob and sets the reference to the array in
 * dest. Checks to make sure that the item exists, isn't null and is the right type.
 *
 * @param dest A reference to the cJSON pointer where the cJSON array pointer should be copied to.
 * @param data A reference to the cJSON object to find the array specified by item.
 * @param item The name (key) of the object item that you are looking for.
 * @return Returns an SF_JSON_ERROR return code; can be processed or ignored by the caller.
 */
SF_JSON_ERROR STDCALL json_detach_array_from_object(cJSON **dest, cJSON *data, const char *item);

/**
 * A convenience function that detaches a cJSON array from a cJSON array and sets the reference to the array in
 * dest. Checks to make sure that the item exists, isn't null and is the right type.
 *
 * @param dest A reference to the cJSON pointer where the cJSON array pointer should be copied to.
 * @param data A reference to the cJSON object to find the array specified by item.
 * @param item The name (key) of the object item that you are looking for.
 * @return Returns an SF_JSON_ERROR return code; can be processed or ignored by the caller.
 */
SF_JSON_ERROR STDCALL json_detach_array_from_array(cJSON **dest, cJSON *data, int index);

/**
 * A convenience function that detaches a cJSON object from a cJSON array and sets the reference to the array in
 * dest. Checks to make sure that the item exists, isn't null and is the right type.
 *
 * @param dest A reference to the cJSON pointer where the cJSON object pointer should be copied to.
 * @param data A reference to the cJSON object to find the array specified by item.
 * @param item The name (key) of the object item that you are looking for.
 * @return Returns an SF_JSON_ERROR return code; can be processed or ignored by the caller.
 */
SF_JSON_ERROR STDCALL json_detach_object_from_array(cJSON **dest, cJSON *data, int index);

/**
 * Returns all the keys in a JSON object. To save memory, the keys in the array list are references
 * to the keys in the cJSON structs. DO NOT free these keys in the arraylist. Once you delete the cJSON
 * object, you must also destroy the arraylist containing the object keys.
 *
 * @param item A cJSON object that will not be altered in the function.
 * @return An arraylist containing all the keys in the object. This arraylist must be freed by the caller at some point.
 */
ARRAY_LIST *json_get_object_keys(const cJSON *item);

/**
 * A write callback function to use to write the response text received from the cURL response. The raw JSON buffer
 * will grow in size until
 *
 * @param data The data to copy in the buffer.
 * @param size The size (in bytes) of each data member.
 * @param nmemb The number of data members.
 * @param raw_json The Raw JSON Buffer object that grows in size to copy multiple writes for a single cURL call.
 * @return The number of bytes copied into the buffer.
 */
size_t json_resp_cb(char *data, size_t size, size_t nmemb, RAW_JSON_BUFFER *raw_json);

/**
 * Performs an HTTP request with retry.
 *
 * @param curl The cURL object to use in the request.
 * @param request_type The type of HTTP request.
 * @param url The fully qualified URL to use for the HTTP request.
 * @param header The header to use for the HTTP request.
 * @param body The body to send over the HTTP request. If running GET request, set this to NULL.
 * @param json A reference to a cJSON pointer where we should store a successful request.
 * @param non_json_resp A reference to a non-json response to retrieve response in non-json format.
 *                      Used only when json is set to NULL.
 * @param network_timeout The network request timeout to use for each request try.
 * @param chunk_downloader A boolean value determining whether or not we are running this request from the chunk
 *                         downloader. Each chunk that we download from AWS is invalid JSON so we need to add an
 *                         opening square bracket at the beginning of the text buffer and a closing square bracket
 *                         at the end of the text buffer.
 * @param error Reference to the Snowflake Error object to set an error if one occurs.
 * @param insecure_mode Insecure mode disable OCSP check when set to true
 * @param fail_open OCSP FAIL_OPEN mode when set to true
 * @param retry_on_curle_couldnt_connect_count number of times retrying server connection on CURLE_COULDNT_CONNECT error
 * @param renew_timeout   For key pair authentication. Credentials could expire
 *                        during the connection retry. Set renew timeout in such
 *                        case so http_perform will return when renew_timeout is
 *                        reached and the caller can renew the credentials and
 *                        then go back to the retry by calling http_perform() again.
 *                        0 means no renew timeout needed.
 * @param retry_max_count The max number of retry attempts. 0 means no limit.
 * @param elapsed_time    The in/out paramter to record the elapsed time before
 *                        http_perform() returned due to renew timeout last time
 * @param retried_count   The in/out paramter to record the number of retry attempts
 *                        has been done before http_perform() returned due to renew
 *                        timeout last time.
 * @param is_renew        The output paramter to indecate whether http_perform()
 *                        returns due to renew timeout.
 * @param renew_injection For test purpose, forcely trigger the autentication renew.
 * @param proxy           proxy setting
 * @param no_proxy        exclusion of proxy
 *
 * @return Success/failure status of http request call. 1 = Success; 0 = Failure/renew timeout
 */
sf_bool STDCALL http_perform(CURL *curl, SF_REQUEST_TYPE request_type, char *url, SF_HEADER *header,
                             char *body, cJSON **json, NON_JSON_RESP* non_json_resp, int64 network_timeout, sf_bool chunk_downloader,
                             SF_ERROR_STRUCT *error, sf_bool insecure_mode, sf_bool fail_open,
                             int8 retry_on_curle_couldnt_connect_count,
                             int64 renew_timeout, int8 retry_max_count,
                             int64 *elapsed_time, int8 *retried_count,
                             sf_bool *is_renew, sf_bool renew_injection,
                             const char *proxy, const char *no_proxy,
                             sf_bool include_retry_reason,
                             sf_bool is_login_request);

sf_bool STDCALL http_perform_internal(CURL* curl, SF_REQUEST_TYPE request_type, char* url, SF_HEADER* header,
    char* body, cJSON** json, NON_JSON_RESP* non_json_resp, int64 network_timeout, sf_bool chunk_downloader,
    SF_ERROR_STRUCT* error, sf_bool insecure_mode, sf_bool fail_open,
    int8 retry_on_curle_couldnt_connect_count,
    int64 renew_timeout, int8 retry_max_count,
    int64* elapsed_time, int8* retried_count,
    sf_bool* is_renew, sf_bool renew_injection,
    const char* proxy, const char* no_proxy,
    sf_bool include_retry_reason,
    sf_bool is_login_request, sf_bool parse_json, char** raw_data);
/**
 * Returns true if HTTP code is retryable, false otherwise.
 *
 * @param code The HTTP code to test.
 * @return Retryable/Non-retryable. 1 = Retryable; 0 = Non-retryable
 */
sf_bool STDCALL is_retryable_http_code(long int code);

/**
 * Renews a session once the session token has expired.
 *
 * @param curl cURL object to use for the renew session request to Snowflake
 * @param sf The Snowflake Connection object to use for connection details.
 * @param error Reference to the Snowflake Error object to set an error if one occurs.
 * @return Success/failure status of session renewal. 1 = Success; 0 = Failure
 */
sf_bool STDCALL renew_session(CURL * curl, SF_CONNECT *sf, SF_ERROR_STRUCT *error);

/**
 * Runs a request to Snowflake. Encodes the URL and creates the cURL object that is used for the request.
 *
 * @param sf The Snowflake Connection object to use for connection details.
 * @param json A reference to a cJSON pointer. Holds the response of the request.
 * @param url The URL path for the request. A full URL will be constructed using this path.
 * @param url_params URL parameters to add to the encoded URL.
 * @param num_url_params Number of URL parameters.
 * @param body JSON body text to send as part of the request. If running a GET request, set to NULL.
 * @param header Header to use in the request.
 * @param request_type Type of request.
 * @param error Reference to the Snowflake Error object to set an error if one occurs.
 * @param use_application_json_accept_type true for put/get command, default is false
 * @param renew_timeout   For key pair authentication. Credentials could expire
 *                        during the connection retry. Set renew timeout in such
 *                        case so request() will return when renew_timeout is
 *                        reached and the caller can renew the credentials and
 *                        then go back to the retry by calling request() again.
 *                        0 means no renew timeout needed.
 * @param retry_max_count The max number of retry attempts. 0 means no limit.
 * @param elapsed_time    The in/out paramter to record the elapsed time before
 *                        request() returned due to renew timeout last time
 * @param retried_count   The in/out paramter to record the number of retry attempts
 *                        has been done before request() returned due to renew
 *                        timeout last time.
 * @param is_renew        The output paramter to indecate whether http_perform()
 *                        returns due to renew timeout.
 * @param renew_injection For test purpose only. Forcely trigger renew timeout.
 * @return Success/failure status of request. 1 = Success; 0 = Failure
 */
sf_bool STDCALL request(SF_CONNECT *sf, cJSON **json, const char *url, URL_KEY_VALUE* url_params, int num_url_params,
                        char *body, SF_HEADER *header, SF_REQUEST_TYPE request_type, SF_ERROR_STRUCT *error,
                        sf_bool use_application_json_accept_type,
                        int64 renew_timeout, int8 retry_max_count, int64 retry_timeout,
                        int64 *elapsed_time, int8 *retried_count,
                        sf_bool *is_renew, sf_bool renew_injection);

/**
 * Resets curl instance.
 *
 * @param curl cURL object.
 */
void STDCALL reset_curl(CURL *curl);

/**
 * Frees up the retry Context object
 *
 * @param retry_ctx Retry Context object.
 */
void STDCALL retry_ctx_free(RETRY_CONTEXT *retry_ctx);

/**
 * Determines next sleep duration for request retry. Sets new sleep duration value in Retry Context.
 *
 * As a side effect will set context's retry_count with the new value and increment retry_count
 * @param retry_ctx Retry Context object. Used to determine next sleep.
 * @return Returns number of seconds to sleep.
 */
uint32 STDCALL retry_ctx_next_sleep(RETRY_CONTEXT *retry_ctx);

/**
* Update request url with retry retryCount, retryReason and renewed request guid.
* Only update urls generated with encode_url(), which have request guid at the end.
* As a side effect will set context's retry_reason to 0 as the initial value for
* the next retry attempt.
*
* @param retry_ctx Retry Context object, where the retry count and retry reason from.
* @param url The request url to be updated.
* @return SF_BOOLEAN_TRUE if succeeded, otherwise SF_BOOLEAN_FALSE.
*/
sf_bool STDCALL retry_ctx_update_url(RETRY_CONTEXT *retry_ctx, char* url,
                                     sf_bool include_retry_reason);

/**
 * Convenience function to set tokens in Snowflake Connect object from cJSON blob. Returns success/failure.
 *
 * @param sf The Snowflake Connection object to update the session/master keys of.
 * @param data cJSON blob containing new keys.
 * @param session_token_str Session token JSON key.
 * @param master_token_str Master token JSON key.
 * @param error Reference to the Snowflake Error object to set an error if one occurs.
 * @return Success/failure of key setting. 1 = Success; 0 = Failure
 */
sf_bool STDCALL set_tokens(SF_CONNECT *sf, cJSON *data, const char *session_token_str, const char *master_token_str,
                           SF_ERROR_STRUCT *error);

SF_HEADER* STDCALL sf_header_create();

void STDCALL sf_header_destroy(SF_HEADER *sf_header);

/**
* Set proxy settings to curl instance.
*
* @param curl The curl instance.
* @param proxy The proxy setting.
* @param no_proxy The no proxy setting.
* @return CURLE_OK if success, curl error code otherwise.
*/
CURLcode set_curl_proxy(CURL *curl, const char* proxy, const char* no_proxy);

/**
* Determines if the url is request against to
* login-request
* authenticator-request
* token-request
*
* @param url Url string to check
* @return True (1) if it's request needs new retry strategy, False (0) if not.
*/
sf_bool is_new_retry_strategy_url(const char * url);

/**
* Utility function to choose a random float number in range
*
* @param min The minimum value of the range
* @param max the maximum value of the range
* @return A random float number between min and max
*/
float choose_random(float min, float max);

/**
* Utility function to caculate jitter from current wait time
*
* @param cur_wait_time the current wait time.
* @return A random float number between +/-50%
*/
float get_jitter(int cur_wait_time);

// add CLIENT_APP_ID/CLIENT_APP_VERSION in header for login rquests
sf_bool add_appinfo_header(SF_CONNECT *sf, SF_HEADER *header, SF_ERROR_STRUCT *error);

/*
* Get login timeout from connection
*/
int64 get_login_timeout(SF_CONNECT *sf);

/*
* Get max retry number for connection
*/
int8 get_login_retry_count(SF_CONNECT *sf);

/*
* Get retry timeout from connection
*/
int64 get_retry_timeout(SF_CONNECT *sf);

/*
* Get current time since epoch in milliseconds
*/
uint64 sf_get_current_time_millis();

/*
* a function to check that this request is whether the one time token request.
*/
sf_bool is_one_time_token_request(cJSON* resp);

/*
* a function to check that this request is whether the response includes the SAML response.
*/
sf_bool is_saml_response(char* response);
#ifdef __cplusplus
}
#endif

#endif //SNOWFLAKE_CONNECTION_H
