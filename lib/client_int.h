/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

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

#define DEFAULT_SNOWFLAKE_BASE_URL "snowflakecomputing.com"
#define DEFAULT_SNOWFLAKE_REQUEST_TIMEOUT 60

#define SESSION_URL "/session/v1/login-request"
#define QUERY_URL "/queries/v1/query-request"
#define RENEW_SESSION_URL "/session/token-request"
#define DELETE_SESSION_URL "/session"

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
} SF_STAGE_CRED;

typedef struct SF_STAGE_INFO {
  char *location_type;
  char *location;
  char *path;
  char *region;
  char *storageAccount; // For Azure only
  char *endPoint; //For Azure only.
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
 * @param sf_use_application_json_accept type true if this is a put/get command
 *
 * @return 0 if success, otherwise an errno is returned.
 */
SF_STATUS STDCALL _snowflake_execute_ex(SF_STMT *sfstmt,
                                        sf_bool use_application_json_accept_type);

/**
 * @return true if this is a put/get command, otherwise false
 */
sf_bool STDCALL _is_put_get_command(char* sql_text);

/**
 * @return POSITIONAL or NAMED based on the type of params
 */
PARAM_TYPE STDCALL _snowflake_get_param_style(const SF_BIND_INPUT *input);

#endif //SNOWFLAKE_CLIENT_INT_H
