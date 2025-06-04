#ifndef SNOWFLAKE_CLIENT_INT_H
#define SNOWFLAKE_CLIENT_INT_H

#include "cJSON.h"
#include "paramstore.h"
#include "snowflake/platform.h"
#include "snowflake/client.h"

#define HEADER_SNOWFLAKE_TOKEN_FORMAT "Authorization: Snowflake Token=\"%s\""
#define HEADER_CONTENT_TYPE_APPLICATION_JSON "Content-Type: application/json"
#define HEADER_ACCEPT_TYPE_APPLICATION_SNOWFLAKE "accept: application/snowflake"
#define HEADER_ACCEPT_TYPE_APPLICATION_JSON "accept: application/json"
#define HEADER_C_API_USER_AGENT_FORMAT "User-Agent: %s/%s (%s_%s) %s/%lu"
#define HEADER_C_API_USER_AGENT_MAX_LEN 256
#define HEADER_DIRECT_QUERY_TOKEN_FORMAT "Authorization: %s"
#define HEADER_SERVICE_NAME_FORMAT "X-Snowflake-Service: %s"
#define HEADER_CLIENT_APP_ID_FORMAT "CLIENT_APP_ID: %s"
#define HEADER_CLIENT_APP_VERSION_FORMAT "CLIENT_APP_VERSION: %s"

#define DEFAULT_SNOWFLAKE_REQUEST_TIMEOUT 60

#define SF_DEFAULT_MAX_OBJECT_SIZE 16777216

#define SF_DEFAULT_STAGE_BINDING_THRESHOLD 65280
#define SF_DEFAULT_STAGE_BINDING_MAX_FILESIZE 100 * 1024 * 1024

// defaults for put get configurations
#define SF_DEFAULT_PUT_COMPRESS_LEVEL (-1)
#define SF_MAX_PUT_COMPRESS_LEVEL 9
#define SF_DEFAULT_PUT_MAX_RETRIES 5
#define SF_MAX_PUT_MAX_RETRIES 100
#define SF_DEFAULT_GET_MAX_RETRIES 5
#define SF_MAX_GET_MAX_RETRIES 100
#define SF_DEFAULT_GET_THRESHOLD 5

#define SESSION_URL "/session/v1/login-request"
#define QUERY_URL "/queries/v1/query-request"
#define RENEW_SESSION_URL "/session/token-request"
#define DELETE_SESSION_URL "/session"
#define QUERY_RESULT_URL_FORMAT "/queries/%s/result"
#define QUERY_MONITOR_URL "/monitoring/queries/%s"
// not used for now but add for URL checking on connection requests
#define AUTHENTICATOR_URL "/session/authenticator-request"
#define EXTERNALBROWSER_CONSOLE_URL "/console/login"
#define ABORT_REQUEST_URL "/queries/v1/abort-request"

#define URL_PARAM_REQEST_GUID "request_guid="
#define URL_PARAM_RETRY_COUNT "retryCount="
#define URL_PARAM_RETRY_REASON "retryReason="
#define URL_PARAM_CLIENT_START_TIME "clientStartTime="
#define URL_PARAM_REQUEST_ID "requestId="

#define CLIENT_APP_ID_KEY "CLIENT_APP_ID"
#define CLIENT_APP_VERSION_KEY "CLIENT_APP_VERSION"

// having extra size in url buffer for retry context or something else could
// be added in the future.
#define URL_EXTRA_SIZE 256

#define URL_QUERY_DELIMITER "?"
#define URL_PARAM_DELIM "&"

#define INCORRECT_USERNAME_PASSWORD "390100"
#define SESSION_TOKEN_INVALID_CODE "390104"
#define GONE_SESSION_CODE "390111"
#define SESSION_TOKEN_EXPIRED_CODE "390112"
#define MASTER_TOKEN_EXPIRED_CODE "390114"

#define QUERY_IN_PROGRESS_CODE "333333"
#define QUERY_IN_PROGRESS_ASYNC_CODE "333334"

#define REQUEST_TYPE_RENEW "RENEW"
#define REQUEST_TYPE_CLONE "CLONE"
#define REQUEST_TYPE_ISSUE "ISSUE"

#define DATE_STRING_MAX_SIZE 12
#define SECONDS_IN_AN_HOUR 86400L

/**
 * Maximum one-directional range of offset-based timezones (24 hours)
 */
#define TIMEZONE_OFFSET_RANGE  (int64)(24 * 60);

int uuid4_generate_non_terminated(char *dst);
int uuid4_generate(char *dst);

/**
 * Encryption material
 */
typedef struct SF_ENC_MAT {
  char *query_stage_master_key;
  char query_id[SF_UUID4_LEN];
  int64 smk_id;
} SF_ENC_MAT;

typedef struct SF_STAGE_CRED {
  char *aws_key_id;
  char *aws_secret_key;
  char *aws_token;
  char *azure_sas_token;
  char* gcs_access_token;
} SF_STAGE_CRED;

typedef struct SF_STAGE_INFO {
  char *location_type;
  char *location;
  char *path;
  char *region;
  // An endpoint (Azure, AWS FIPS and GCS custom endpoint override)
  char *endPoint; //For FIPS and Azure support
  // whether to use s3 regional URL (AWS Only)
  // TODO SNOW-1818804: this field will be deprecated when the server returns {@link
  // #useRegionalUrl}
  sf_bool useS3RegionalUrl;
  // whether to use regional URL (AWS and GCS only)
  sf_bool useRegionalUrl;
  sf_bool useVirtualUrl;
  char* storageAccount; // For Azure only
  SF_STAGE_CRED * stage_cred;
} SF_STAGE_INFO;

/**
 * Put/Get command parse response
 */
struct SF_PUT_GET_RESPONSE {
  void *src_list;
  int64 parallel;
  // put threshold only
  int64 threshold;
  sf_bool auto_compress;
  sf_bool overwrite;
  char source_compression[SF_SOURCE_COMPRESSION_TYPE_LEN];
  sf_bool client_show_encryption_param;
  char command[SF_COMMAND_LEN];
  // for put command, encMat is a single object while for get, it is a list of
  // enc_mat
  SF_ENC_MAT *enc_mat_put;
  void * enc_mat_get;
  SF_STAGE_INFO *stage_info;
  char *localLocation;
};

typedef struct NAMED_PARAMS
{
    void ** name_list;
    unsigned int used;
    unsigned int allocd;
}NamedParams;

/**
 * Query metadata
 */
typedef struct SF_QUERY_METADATA {
  SF_QUERY_STATUS status;
  char *qid;
} SF_QUERY_METADATA;

/**
 * Allocate memory for put get response struct
 */
void STDCALL sf_put_get_response_deallocate(SF_PUT_GET_RESPONSE *put_get_response);


/**
 * Deallocate memory for put get response struct
 */
SF_PUT_GET_RESPONSE *STDCALL sf_put_get_response_allocate();

/**
 * Executes a statement.
 * @param sfstmt SNOWFLAKE_STMT context.
 * @param is_put_get_command type true if this is a put/get command
 * @param raw_response_buffer optional pointer to an SF_QUERY_RESULT_CAPTURE,
 * if the query response is to be captured.
 * @param is_describe_only should the statement be executed in describe only mode
 * @param is_async_exec should it execute asynchronously
 *
 * @return 0 if success, otherwise an errno is returned.
 */
SF_STATUS STDCALL _snowflake_execute_ex(SF_STMT *sfstmt,
                                        sf_bool is_put_get_command,
                                        sf_bool is_native_put_get,
                                        struct SF_QUERY_RESULT_CAPTURE* result_capture,
                                        sf_bool is_describe_only,
                                        sf_bool is_async_exec);

/**
 * @return true if this is a put/get command, otherwise false
 */
sf_bool STDCALL _is_put_get_command(char* sql_text);

/**
 * @return POSITIONAL or NAMED based on the type of params
 */
PARAM_TYPE STDCALL _snowflake_get_param_style(const SF_BIND_INPUT *input);

/**
 * @param sfstmt SNOWFLAKE_STMT context.
 * @param index The parameter set index (for batch execution), -1 to return all
 *        parameter sets (non-batch execution)
 * @return parameter bindings in cJSON.
 */
cJSON* STDCALL _snowflake_get_binding_json(SF_STMT *sfstmt, int64 index);

#ifdef __cplusplus
extern "C" {
#endif
/**
 * Legacy approach of Executing a query, not to execute put/get natively.
 * Should only be called internally for put/get queries.
 *
 * @param sf SNOWFLAKE_STMT context.
 * @param command a query or command that returns results.
 * @return 0 if success, otherwise an errno is returned.
 */
SF_STATUS STDCALL
_snowflake_query_put_get_legacy(SF_STMT* sfstmt, const char* command, size_t command_size);

/**
 * Executes put get command natively.
 * @param sfstmt SNOWFLAKE_STMT context.
 * @param upload_stream Internal support for bind uploading, pointer to std::basic_iostream<char>.
 * @param stream_size The data size of upload_stream.
 * @param stream_upload_max_retries The max number of retries for stream uploading.
 *                                  Internal support for bind uploading so we can disable retry
 *                                  and fallback to regular binding directly.
 * @param raw_response_buffer optional pointer to an SF_QUERY_RESULT_CAPTURE,
 *
 * @return 0 if success, otherwise an errno is returned.
 */
SF_STATUS STDCALL _snowflake_execute_put_get_native(
                      SF_STMT *sfstmt,
                      void* upload_stream,
                      size_t stream_size,
                      int stream_upload_max_retries,
                      struct SF_QUERY_RESULT_CAPTURE* result_capture);

/*
 * @return size of single binding value per data type.
 */
size_t STDCALL _snowflake_get_binding_value_size(SF_BIND_INPUT* bind);

#define SF_PARAM_NAME_BUF_LEN 20
/**
 * Get parameter binding by index for both POSITIONAL and NAMED cases.
 * @param sfstmt SNOWFLAKE_STMT context.
 * @param index The 0 based index of parameter binding to get.
 * @param name Output the name of binding.
 * @param name_buf The buffer to store name.
                   Used for POSITIONAL and name will point to this buffer in such case.
 * @param name_buf_len The size of name_buf.
 * @return parameter binding with specified index.
 */
SF_BIND_INPUT* STDCALL _snowflake_get_binding_by_index(SF_STMT* sfstmt,
  size_t index,
  char** name,
  char* name_buf,
  size_t name_buf_len);

sf_bool STDCALL _snowflake_needs_stage_binding(SF_STMT* sfstmt);

/**
 * Upload parameter bindings through internal stage.
 * @param sfstmt SNOWFLAKE_STMT context.
 *
 * @return Stage path for uploaded bindings if success,
 *         otherwise NULL is returned and error is put in sfstmt->error.
 */
char* STDCALL
_snowflake_stage_bind_upload(SF_STMT* sfstmt);

#ifdef __cplusplus
} // extern "C"
#endif

#endif //SNOWFLAKE_CLIENT_INT_H
