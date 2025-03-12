/*
 * Copyright (c) 2018-2025 Snowflake Computing, Inc. All rights reserved.
 */

#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <openssl/crypto.h>
#include <snowflake/client.h>
#include <snowflake/client_config_parser.h>
#include "constants.h"
#include "client_int.h"
#include "connection.h"
#include "memory.h"
#include "results.h"
#include "result_set.h"
#include "error.h"
#include "chunk_downloader.h"
#include "authenticator.h"
#include "query_context_cache.h"
#include "snowflake_util.h"
#include "heart_beat_background.h"

#ifdef _WIN32
#include <Shellapi.h>
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#else
#include <unistd.h>
#endif

#define curl_easier_escape(curl, string) curl_easy_escape(curl, string, 0)

// Define internal constants
sf_bool DISABLE_VERIFY_PEER;
char *CA_BUNDLE_FILE;
int32 SSL_VERSION;
sf_bool DEBUG;
sf_bool SF_OCSP_CHECK;
char *SF_HEADER_USER_AGENT = NULL;

static char* CLIENT_CONFIG_FILE = NULL;
static char *LOG_PATH = NULL;
static FILE *LOG_FP = NULL;

static SF_MUTEX_HANDLE log_lock;
static SF_MUTEX_HANDLE gmlocaltime_lock;

static SF_STATUS STDCALL
_snowflake_internal_query(SF_CONNECT *sf, const char *sql);

static SF_STATUS STDCALL
_set_parameters_session_info(SF_CONNECT *sf, cJSON *data);

static void STDCALL _set_current_objects(SF_STMT *sfstmt, cJSON *data);

static SF_STATUS STDCALL
_reset_connection_parameters(SF_CONNECT *sf, cJSON *parameters,
                             cJSON *session_info, sf_bool do_validate);

static const char* const query_status_names[] = {
    "ABORTED",
    "ABORTING",
    "BLOCKED",
    "DISCONNECTED",
    "FAILED_WITH_ERROR",
    "FAILED_WITH_INCIDENT",
    "NO_DATA",
    "RUNNING",
    "QUEUED",
    "QUEUED_REPAIRING_WAREHOUSE",
    "RESTARTED",
    "RESUMING_WAREHOUSE",
    "SUCCESS",
    "UNKNOWN"
};

/**
 * Validate partner application name.
 * @param application partner application name
 */
sf_bool validate_application(const char *application);

/**
 * Helper function to get SF_QUERY_STATUS given the string representation
 * @param query_status the string representation of the query status
 */
SF_QUERY_STATUS get_status_from_string(const char *query_status) {
  if (query_status == NULL) {
    return SF_QUERY_STATUS_UNKNOWN;
  }
  int idx = 0, last = 0;
  for (idx = 0, last = (int)SF_QUERY_STATUS_UNKNOWN; idx <= last; ++idx) {
    size_t len = strlen(query_status_names[idx]);
    if (sf_strncasecmp(query_status_names[idx], query_status, len) == 0) {
      return (SF_QUERY_STATUS)idx;
    }
  }
  return SF_QUERY_STATUS_UNKNOWN;
}

/**
 * Get the metadata of the query
 * 
 * @param sfstmt The SF_STMT context.
 * 
 * The query metadata
 */
SF_QUERY_METADATA *get_query_metadata(SF_STMT* sfstmt) {
  cJSON *resp = NULL;
  cJSON *data = NULL;
  cJSON *queries = NULL;
  char *s_resp = NULL;
  SF_QUERY_METADATA *query_metadata = (SF_QUERY_METADATA*)SF_CALLOC(1, sizeof(SF_QUERY_METADATA));
  query_metadata->status = SF_QUERY_STATUS_UNKNOWN;
  size_t url_size = strlen(QUERY_MONITOR_URL) - 2 + strlen(sfstmt->sfqid) + 1;
  char *status_query = (char*)SF_CALLOC(1, url_size);
  sf_sprintf(status_query, url_size, QUERY_MONITOR_URL, sfstmt->sfqid);

  if (request(sfstmt->connection, &resp, status_query, NULL, 0, NULL, NULL,
    GET_REQUEST_TYPE, &sfstmt->error, SF_BOOLEAN_TRUE,
    0, sfstmt->connection->retry_count, get_retry_timeout(sfstmt->connection),
    NULL, NULL, NULL, SF_BOOLEAN_FALSE)) {

    s_resp = snowflake_cJSON_Print(resp);
    log_trace("GET %s returned response:\n%s", status_query, s_resp);

    data = snowflake_cJSON_GetObjectItem(resp, "data");

    queries = snowflake_cJSON_GetObjectItem(data, "queries");
    cJSON* query = snowflake_cJSON_GetArrayItem(queries, 0);

    query_metadata->qid = sfstmt->sfqid;
    cJSON* status = snowflake_cJSON_GetObjectItem(query, "status");
    if (snowflake_cJSON_IsString(status)) {
      query_metadata->status = get_status_from_string(snowflake_cJSON_GetStringValue(status));
    } else {
      log_error(
        "Error parsing query status from query with id: %s", sfstmt->sfqid);
    }

    snowflake_cJSON_Delete(resp);
    SF_FREE(s_resp);
    SF_FREE(status_query);
    return query_metadata;
  }
  SF_FREE(status_query);
  log_error("No query metadata found. Query id: %s", sfstmt->sfqid);
  query_metadata->status = SF_QUERY_STATUS_UNKNOWN;
  return query_metadata;
}


SF_QUERY_STATUS STDCALL snowflake_get_query_status(SF_STMT *sfstmt) {
  SF_QUERY_METADATA *metadata = get_query_metadata(sfstmt);
  SF_QUERY_STATUS ret = metadata->status;
  SF_FREE(metadata);
  return ret;
}

/**
 * Helper function to determine if the query is still running
 * @param query_status the query status
 */
sf_bool is_query_still_running(SF_QUERY_STATUS query_status) {
  return (query_status == SF_QUERY_STATUS_RUNNING) ||
    (query_status == SF_QUERY_STATUS_QUEUED) ||
    (query_status == SF_QUERY_STATUS_RESUMING_WAREHOUSE) ||
    (query_status == SF_QUERY_STATUS_QUEUED_REPAIRING_WAREHOUSE) ||
    (query_status == SF_QUERY_STATUS_NO_DATA);
}

/**
 * Get the results of the async query
 * @param sfstmt The SF_STMT context
 */
sf_bool get_real_results(SF_STMT *sfstmt) {
  //Get status until query is complete or timed out
  SF_QUERY_METADATA* metadata = get_query_metadata(sfstmt);
  SF_QUERY_STATUS query_status = metadata->status;
  int retry = 0;
  int no_data_retry = 0;
  int no_data_max_retries = 30;
  int retry_pattern[] = {1, 1, 2, 3, 4, 8, 10};
  int max_retries = 7;
  while (query_status != SF_QUERY_STATUS_SUCCESS) {
    if (!is_query_still_running(query_status) && query_status != SF_QUERY_STATUS_SUCCESS) {
      log_error("Query status is done running and did not succeed. Status is %s",
        query_status_names[query_status]);
      break;
    }
    if (query_status == SF_QUERY_STATUS_NO_DATA) {
      no_data_retry++;
      if (no_data_retry >= no_data_max_retries) {
        log_error(
          "Cannot retrieve data on the status of this query. No information returned from server for queryID=%s",
          sfstmt->sfqid);
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error,
          SF_STATUS_ERROR_GENERAL,
          "Cannot retrieve data on the status of this query.",
          NULL,
          sfstmt->sfqid);
        break;
      }
    }

    if (retry < max_retries) {
      int sleep_time = retry_pattern[retry] * 500;
      sf_sleep_ms(sleep_time);
      retry++;
    } else {
      log_error(
        "Cannot retrieve data on the status of this query. Max retries hit with queryID=%s", sfstmt->sfqid);
      char msg[1024];
      sf_sprintf(msg, sizeof(msg),
        "Cannot retrieve data on the status of this query. Max retries hit. Query status: %d", query_status);
      SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error,
        SF_STATUS_ERROR_GENERAL,
        msg,
        NULL,
        sfstmt->sfqid);
      return SF_BOOLEAN_FALSE;
    }
    SF_FREE(metadata);
    metadata = get_query_metadata(sfstmt);
    query_status = metadata->status;
  }

  // Get query results
  if (snowflake_next_result(sfstmt) != SF_STATUS_SUCCESS) {
    snowflake_propagate_error(sfstmt->connection, sfstmt);
    return SF_BOOLEAN_FALSE;
  }

  sfstmt->is_async_results_fetched = SF_BOOLEAN_TRUE;
  SF_FREE(metadata);
  return SF_BOOLEAN_TRUE;
}

#define _SF_STMT_TYPE_DML 0x3000
#define _SF_STMT_TYPE_INSERT (_SF_STMT_TYPE_DML + 0x100)
#define _SF_STMT_TYPE_UPDATE (_SF_STMT_TYPE_DML + 0x200)
#define _SF_STMT_TYPE_DELETE (_SF_STMT_TYPE_DML + 0x300)
#define _SF_STMT_TYPE_MERGE (_SF_STMT_TYPE_DML + 0x400)
#define _SF_STMT_TYPE_MULTI_TABLE_INSERT (_SF_STMT_TYPE_DML + 0x500)
#define _SF_STMT_TYPE_MULTI_STMT 0xA000

/**
 * Detects statement type is DML
 * @param stmt_type_id statement type id
 * @return SF_BOOLEAN_TRUE if the statement is DM or SF_BOOLEAN_FALSE
 */
static sf_bool detect_stmt_type(int64 stmt_type_id) {
    sf_bool ret;
    switch (stmt_type_id) {
        case _SF_STMT_TYPE_DML:
        case _SF_STMT_TYPE_INSERT:
        case _SF_STMT_TYPE_UPDATE:
        case _SF_STMT_TYPE_DELETE:
        case _SF_STMT_TYPE_MERGE:
        case _SF_STMT_TYPE_MULTI_TABLE_INSERT:
            ret = SF_BOOLEAN_TRUE;
            break;
        default:
            ret = SF_BOOLEAN_FALSE;
            break;
    }
    return ret;
}

#define _SF_STMT_SQL_BEGIN "begin"
#define _SF_STMT_SQL_COMMIT "commit"
#define _SF_STMT_SQL_ROLLBACK "rollback"

/**
 * precomputed 10^N for int64
 */
static const int64 pow10_int64[] = {
    1LL,
    10LL,
    100LL,
    1000LL,
    10000LL,
    100000LL,
    1000000LL,
    10000000LL,
    100000000LL,
    1000000000LL
};

/**
 * Find string size, create buffer, copy over, and return.
 * @param var output buffer
 * @param str source string
 */
static void alloc_buffer_and_copy(char **var, const char *str) {
    size_t str_size;
    SF_FREE(*var);
    // If passed in string is null, then return since *var is already null from being freed
    if (str) {
        str_size = strlen(str) + 1; // For null terminator
        *var = (char *) SF_CALLOC(1, str_size);
        sf_strncpy(*var, str_size, str, str_size);
    } else {
        *var = NULL;
    }
}

/**
 * Lock/Unlock log file for writing by mutex
 * @param udata user data (not used)
 * @param lock non-zere for lock or zero for unlock
 */
static void log_lock_func(void *udata, int lock) {
    if (lock) {
        _mutex_lock(&log_lock);
    } else {
        _mutex_unlock(&log_lock);
    }
}

/**
 * Reset the connection parameters with the returned parameteres
 * @param sf SF_CONNECT object
 * @param parameters the returned parameters
 */
static SF_STATUS STDCALL _reset_connection_parameters(
    SF_CONNECT *sf, cJSON *parameters, cJSON *session_info,
    sf_bool do_validate) {
    if (parameters != NULL) {
        int i, len;
        for (i = 0, len = snowflake_cJSON_GetArraySize(parameters); i < len; ++i) {
            cJSON *p1 = snowflake_cJSON_GetArrayItem(parameters, i);
            cJSON *name = snowflake_cJSON_GetObjectItem(p1, "name");
            cJSON *value = snowflake_cJSON_GetObjectItem(p1, "value");
            if (strcmp(name->valuestring, "TIMEZONE") == 0) {
                if (sf->timezone == NULL ||
                    strcmp(sf->timezone, value->valuestring) != 0) {
                    alloc_buffer_and_copy(&sf->timezone, value->valuestring);
                }
            } else if (strcmp(name->valuestring, "SERVICE_NAME") == 0) {
                if (sf->service_name == NULL ||
                    strcmp(sf->service_name, value->valuestring) != 0) {
                    alloc_buffer_and_copy(&sf->service_name, value->valuestring);
                }
            } else if (strcmp(name->valuestring, "C_API_QUERY_RESULT_FORMAT") == 0) {
                if (sf->query_result_format == NULL ||
                    strcmp(sf->query_result_format, value->valuestring) != 0) {
                    alloc_buffer_and_copy(&sf->query_result_format, value->valuestring);
                }
            }
            else if (strcmp(name->valuestring, "QUERY_CONTEXT_CACHE_SIZE") == 0) {
                sf->qcc_capacity = snowflake_cJSON_GetUint64Value(value);
                qcc_set_capacity(sf, sf->qcc_capacity);
            }
            else if (strcmp(name->valuestring, "VARCHAR_AND_BINARY_MAX_SIZE_IN_RESULT") == 0) {
                sf->max_varchar_size = snowflake_cJSON_GetUint64Value(value);
                sf->max_binary_size = sf->max_varchar_size / 2;
            }
            else if (strcmp(name->valuestring, "VARIANT_MAX_SIZE_IN_RESULT") == 0) {
                sf->max_variant_size = snowflake_cJSON_GetUint64Value(value);
            }
            else if (strcmp(name->valuestring, "ENABLE_STAGE_S3_PRIVATELINK_FOR_US_EAST_1") == 0) {
                sf->use_s3_regional_url = snowflake_cJSON_IsTrue(value) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
            }
            else if ((strcmp(name->valuestring, "CLIENT_STAGE_ARRAY_BINDING_THRESHOLD") == 0) &&
                     !sf->binding_threshold_overridden) {
                sf->stage_binding_threshold = snowflake_cJSON_GetUint64Value(value);
            }
            else if (strcmp(name->valuestring, "CLIENT_SESSION_KEEP_ALIVE") == 0)
            {
                sf->client_session_keep_alive = snowflake_cJSON_IsTrue(value) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
            }
            else if (strcmp(name->valuestring, "CLIENT_SESSION_KEEP_ALIVE_HEARTBEAT_FREQUENCY") == 0)
            {
                sf->client_session_keep_alive_heartbeat_frequency = snowflake_cJSON_GetUint64Value(value);
            }
        }
    }
    SF_STATUS ret = SF_STATUS_ERROR_GENERAL;
    if (session_info != NULL) {
        char msg[1024];
        // database
        cJSON *db = snowflake_cJSON_GetObjectItem(session_info, "databaseName");
        if (do_validate && sf->database && sf->database[0] != (char) 0 &&
            db->valuestring == NULL) {
            sf_sprintf(msg, sizeof(msg), "Specified database doesn't exists: [%s]",
                    sf->database);
            SET_SNOWFLAKE_ERROR(
                &sf->error,
                SF_STATUS_ERROR_APPLICATION_ERROR,
                msg,
                SF_SQLSTATE_UNABLE_TO_CONNECT
            );
            goto cleanup;
        }
        alloc_buffer_and_copy(&sf->database, db->valuestring);

        // schema
        cJSON *schema = snowflake_cJSON_GetObjectItem(session_info,
                                                      "schemaName");
        if (do_validate && sf->schema && sf->schema[0] != (char) 0 &&
            schema->valuestring == NULL) {
            sf_sprintf(msg, sizeof(msg), "Specified schema doesn't exists: [%s]",
                    sf->schema);
            SET_SNOWFLAKE_ERROR(
                &sf->error,
                SF_STATUS_ERROR_APPLICATION_ERROR,
                msg,
                SF_SQLSTATE_UNABLE_TO_CONNECT
            );
            goto cleanup;

        }
        alloc_buffer_and_copy(&sf->schema, schema->valuestring);

        // warehouse
        cJSON *warehouse = snowflake_cJSON_GetObjectItem(session_info,
                                                         "warehouseName");
        if (do_validate && sf->warehouse && sf->warehouse[0] != (char) 0 &&
            warehouse->valuestring == NULL) {
            sf_sprintf(msg, sizeof(msg), "Specified warehouse doesn't exists: [%s]",
                    sf->warehouse);
            SET_SNOWFLAKE_ERROR(
                &sf->error,
                SF_STATUS_ERROR_APPLICATION_ERROR,
                msg,
                SF_SQLSTATE_UNABLE_TO_CONNECT
            );
            goto cleanup;
        }
        alloc_buffer_and_copy(&sf->warehouse, warehouse->valuestring);

        // role
        cJSON *role = snowflake_cJSON_GetObjectItem(session_info, "roleName");
        // No validation is required as already done by the server
        alloc_buffer_and_copy(&sf->role, role->valuestring);
    }
    ret = SF_STATUS_SUCCESS;
cleanup:
    return ret;
}

void STDCALL _snowflake_memory_hooks_setup(SF_USER_MEM_HOOKS *hooks) {
    if (hooks == NULL)
    {
        /* Reset hooks */
        global_hooks.alloc = malloc;
        global_hooks.dealloc = free;
        global_hooks.realloc = realloc;
        global_hooks.calloc = calloc;
        return;
    }

    global_hooks.alloc = malloc;
    if (hooks->alloc_fn != NULL)
    {
        global_hooks.alloc = hooks->alloc_fn;
    }

    global_hooks.dealloc = free;
    if (hooks->dealloc_fn != NULL)
    {
        global_hooks.dealloc = hooks->dealloc_fn;
    }

    global_hooks.realloc = realloc;
    if (hooks->realloc_fn != NULL)
    {
        global_hooks.realloc = hooks->realloc_fn;
    }

    global_hooks.calloc = calloc;
    if (hooks->calloc_fn != NULL)
    {
        global_hooks.calloc = hooks->calloc_fn;
    }
}

/*
 * Initializes logging file
 */
static sf_bool STDCALL log_init(const char *log_path, SF_LOG_LEVEL log_level) {
    _mutex_init(&log_lock);
    _mutex_init(&gmlocaltime_lock);

    sf_bool ret = SF_BOOLEAN_FALSE;
    time_t current_time;
    struct tm *time_info;
    char time_str[15];
    time(&current_time);
    struct tm tmbuf;
    time_info = sf_localtime(&current_time, &tmbuf);
    strftime(time_str, sizeof(time_str), "%Y%m%d%H%M%S", time_info);
    const char *sf_log_path;
    char log_path_buf[MAX_PATH];
    const char *sf_log_level_str;
    char log_level_buf[64];
    SF_LOG_LEVEL sf_log_level = log_level;
    char strerror_buf[SF_ERROR_BUFSIZE];

    client_config clientConfig = { 0 };
    if (!log_path || (strlen(log_path) == 0) || sf_log_level == SF_LOG_DEFAULT)
    {
      char client_config_file[MAX_PATH] = { 0 };
      snowflake_global_get_attribute(
        SF_GLOBAL_CLIENT_CONFIG_FILE, client_config_file, sizeof(client_config_file));
      load_client_config(client_config_file, &clientConfig);
    }

    size_t log_path_size = 1; //Start with 1 to include null terminator
    log_path_size += strlen(time_str);

    /* The environment variables takes precedence over the specified parameters.
       Specified parameters takes precedence over client config */
    // Check environment variable
    sf_log_path = sf_getenv_s("SNOWFLAKE_LOG_PATH", log_path_buf, sizeof(log_path_buf));
    if (sf_log_path == NULL) {
      // Check specified parameters
      if (log_path && strlen(log_path) != 0) {
        sf_log_path = log_path;
        // Check client config
      } else if (strlen(clientConfig.logPath) != 0) {
        sf_log_path = clientConfig.logPath;
      }
    }

    // Check environment variable
    sf_log_level_str = sf_getenv_s("SNOWFLAKE_LOG_LEVEL", log_level_buf, sizeof(log_level_buf));
    if (sf_log_level_str != NULL) {
      sf_log_level = log_from_str_to_level(sf_log_level_str);
      // Check specified parameters
    } else if (sf_log_level == SF_LOG_DEFAULT) {
      // Check client config
      if (strlen(clientConfig.logLevel) != 0) {
        sf_log_level = log_from_str_to_level(clientConfig.logLevel);
      } else {
        sf_log_level = SF_LOG_FATAL;
      }
    }

    // Set logging level
    if (DEBUG) {
        log_set_quiet(SF_BOOLEAN_FALSE);
    } else {
        log_set_quiet(SF_BOOLEAN_TRUE);
    }
    log_set_level(sf_log_level);
    log_set_lock(&log_lock_func);

    // If log path is specified, use absolute path. Otherwise set logging dir to be relative to current directory
    log_path_size += 30; // Size of static format characters
    if (sf_log_path && (strlen(sf_log_path) != 0)) {
      if (strcasecmp(sf_log_path, "STDOUT") != 0) {
        log_path_size += strlen(sf_log_path);
        LOG_PATH = (char *) SF_CALLOC(1, log_path_size);
        sf_sprintf(LOG_PATH, log_path_size, "%s/snowflake_%s.txt", sf_log_path,
                 (char *) time_str);
      } else { LOG_PATH = ""; }
    } else {
        LOG_PATH = (char *) SF_CALLOC(1, log_path_size);
        sf_sprintf(LOG_PATH, log_path_size, "logs/snowflake_%s.txt",
                 (char *) time_str);
    }
    if (LOG_PATH != NULL) {
      if (strlen(LOG_PATH) != 0) {
        // Set the log path only, the log file will be created when actual log output is needed.
        log_set_path(LOG_PATH);
      } else {
        log_set_quiet(0);
      }
    } else {
        sf_fprintf(stderr,
                "Log path is NULL. Was there an error during path construction?\n");
        return ret;
    }

    snowflake_global_set_attribute(SF_GLOBAL_LOG_LEVEL, log_from_level_to_str(sf_log_level));

    return SF_BOOLEAN_TRUE;
}

/**
 * Cleans up memory allocated for log init and closes log file.
 */
static void STDCALL log_term() {
    log_close();
    SF_FREE(LOG_PATH);
    _mutex_term(&gmlocaltime_lock);
    _mutex_term(&log_lock);
}

/**
 * Process connection parameters
 * @param sf SF_CONNECT
 */
SF_STATUS STDCALL
_snowflake_check_connection_parameters(SF_CONNECT *sf) {
    AuthenticatorType auth_type = getAuthenticatorType(sf->authenticator);
    if (AUTH_UNSUPPORTED == auth_type) {
        // Invalid authenticator
        log_error(ERR_MSG_AUTHENTICATOR_UNSUPPORTED);
        SET_SNOWFLAKE_ERROR(
            &sf->error,
            SF_STATUS_ERROR_BAD_CONNECTION_PARAMS,
            ERR_MSG_AUTHENTICATOR_UNSUPPORTED,
            SF_SQLSTATE_UNABLE_TO_CONNECT);
        return SF_STATUS_ERROR_GENERAL;
    }

    if (is_string_empty(sf->account)) {
        // Invalid account
        log_error(ERR_MSG_ACCOUNT_PARAMETER_IS_MISSING);
        SET_SNOWFLAKE_ERROR(
            &sf->error,
            SF_STATUS_ERROR_BAD_CONNECTION_PARAMS,
            ERR_MSG_ACCOUNT_PARAMETER_IS_MISSING,
            SF_SQLSTATE_UNABLE_TO_CONNECT);
        return SF_STATUS_ERROR_GENERAL;
    }

    if (is_string_empty(sf->user)) {
        // Invalid user name
        log_error(ERR_MSG_USER_PARAMETER_IS_MISSING);
        SET_SNOWFLAKE_ERROR(
            &sf->error,
            SF_STATUS_ERROR_BAD_CONNECTION_PARAMS,
            ERR_MSG_USER_PARAMETER_IS_MISSING,
            SF_SQLSTATE_UNABLE_TO_CONNECT);
        return SF_STATUS_ERROR_GENERAL;
    }

    if ((AUTH_JWT != auth_type) && (AUTH_OAUTH != auth_type) && (AUTH_PAT != auth_type) && (is_string_empty(sf->password))) {
        // Invalid password
        log_error(ERR_MSG_PASSWORD_PARAMETER_IS_MISSING);
        SET_SNOWFLAKE_ERROR(
            &sf->error,
            SF_STATUS_ERROR_BAD_CONNECTION_PARAMS,
            ERR_MSG_PASSWORD_PARAMETER_IS_MISSING,
            SF_SQLSTATE_UNABLE_TO_CONNECT);
        return SF_STATUS_ERROR_GENERAL;
    }

    if ((AUTH_JWT == auth_type) && (is_string_empty(sf->priv_key_file))) {
        // Invalid key file path
        log_error(ERR_MSG_PRIVKEYFILE_PARAMETER_IS_MISSING);
        SET_SNOWFLAKE_ERROR(
            &sf->error,
            SF_STATUS_ERROR_BAD_CONNECTION_PARAMS,
            ERR_MSG_PRIVKEYFILE_PARAMETER_IS_MISSING,
            SF_SQLSTATE_UNABLE_TO_CONNECT);
        return SF_STATUS_ERROR_GENERAL;
    }

    if ((AUTH_OAUTH == auth_type) && (is_string_empty(sf->oauth_token))) {
        // Invalid token
        log_error(ERR_MSG_OAUTH_TOKEN_PARAMETER_IS_MISSING);
        SET_SNOWFLAKE_ERROR(
            &sf->error,
            SF_STATUS_ERROR_BAD_CONNECTION_PARAMS,
            ERR_MSG_OAUTH_TOKEN_PARAMETER_IS_MISSING,
            SF_SQLSTATE_UNABLE_TO_CONNECT);
        return SF_STATUS_ERROR_GENERAL;
    }

    if ((AUTH_PAT == auth_type) && (is_string_empty(sf->programmatic_access_token))) {
        log_error(ERR_MSG_PAT_PARAMETER_IS_MISSING);
        SET_SNOWFLAKE_ERROR(
            &sf->error,
            SF_STATUS_ERROR_BAD_CONNECTION_PARAMS,
            ERR_MSG_PAT_PARAMETER_IS_MISSING,
            SF_SQLSTATE_UNABLE_TO_CONNECT);
        return SF_STATUS_ERROR_GENERAL;
    }

    if (SF_BOOLEAN_FALSE == validate_application(sf->application)) {
        // Invalid parnter application name
        log_error(ERR_MSG_APPLICATION_PARAMETER_INVALID);
        SET_SNOWFLAKE_ERROR(
            &sf->error,
            SF_STATUS_ERROR_BAD_CONNECTION_PARAMS,
            ERR_MSG_APPLICATION_PARAMETER_INVALID,
            SF_SQLSTATE_UNABLE_TO_CONNECT);
        return SF_STATUS_ERROR_GENERAL;
    }

    if (!sf->host) {
        // construct a host parameter if not specified,
        char buf[1024];
        if (sf->region) {
            if (strncasecmp(sf->region, "cn-", 3) == 0)
            {
                //region started with "cn-", use "cn" for top domain
                sf_sprintf(buf, sizeof(buf), "%s.%s.snowflakecomputing.cn",
                         sf->account, sf->region);
            }
            else
            {
                sf_sprintf(buf, sizeof(buf), "%s.%s.snowflakecomputing.com",
                         sf->account, sf->region);
            }
        } else {
            sf_sprintf(buf, sizeof(buf), "%s.snowflakecomputing.com",
                     sf->account);
        }
        alloc_buffer_and_copy(&sf->host, buf);
    }

    char* top_domain = strrchr(sf->host, '.');
    char host_without_top_domain[1024] = { 0 };
    if (top_domain)
    {
        top_domain++;
        size_t length = top_domain - sf->host;
        sf_strncpy(host_without_top_domain, sizeof(host_without_top_domain), sf->host,length);
    }
    else
    {
        // It's basically impossible not finding top domain in host.
        // Log the entire host just in case.
        top_domain = sf->host;
        host_without_top_domain[0] = '\0';
    }

    //check privatelink
    if (ends_with(host_without_top_domain, PRIVATELINK_HOSTNAME_SUFFIX))
    {
        char url_buf[4096];
        sf_snprintf(url_buf, sizeof(url_buf), sizeof(url_buf)-1, "http://ocsp.%s/%s",
           sf->host, "ocsp_response_cache.json");
        if (getenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL")) 
        {
            log_warn(
                "sf", "Connection", "connect",
                "Replace SF_OCSP_RESPONSE_CACHE_SERVER_URL from %s to %s",
                getenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL"),url_buf);
        }
        log_trace(
            "sf", "Connection", "connect",
            "Setting SF_OCSP_RESPONSE_CACHE_SERVER_URL to %s",
            url_buf);
        sf_setenv("SF_OCSP_RESPONSE_CACHE_SERVER_URL", url_buf);
    }

    log_info("Connecting to %s Snowflake domain", (strcasecmp(top_domain, "cn") == 0) ? "CHINA" : "GLOBAL");

    // split account and region if connected by a dot.
    char *dot_ptr = strchr(sf->account, (int) '.');
    if (dot_ptr) {
        char *extracted_account = NULL;
        char *extracted_region = NULL;
        alloc_buffer_and_copy(&extracted_region, dot_ptr + 1);
        *dot_ptr = '\0';
        if (strcmp(extracted_region, "global") == 0) {
            char *dash_ptr = strrchr(sf->account, (int) '-');
            // If there is an external ID then just remove it from account
            if (dash_ptr) {
                *dash_ptr = '\0';
            }
        }
        alloc_buffer_and_copy(&extracted_account, sf->account);
        SF_FREE(sf->account);
        SF_FREE(sf->region);
        sf->account = extracted_account;
        sf->region = extracted_region;
    }
    if (!sf->protocol) {
        alloc_buffer_and_copy(&sf->protocol, "https");
    }
    if (!sf->port) {
        alloc_buffer_and_copy(&sf->port, "443");
    }

    log_debug("application name: %s", sf->application_name);
    log_debug("application version: %s", sf->application_version);
    log_debug("authenticator: %s", sf->authenticator);
    log_debug("user: %s", sf->user);
    log_debug("password: %s", sf->password ? "****" : sf->password);
    if (AUTH_JWT == auth_type) {
        log_debug("priv_key_file: %s", sf->priv_key_file);
        log_debug("jwt_timeout: %d", sf->jwt_timeout);
        log_debug("jwt_cnxn_wait_time: %d", sf->jwt_cnxn_wait_time);
    }
    if (AUTH_OAUTH == auth_type) {
        log_debug("oauth_token: %s", sf->oauth_token ? "provided" : "not provided");
    }
    if (AUTH_PAT == auth_type) {
        log_debug("programmatic_access_token: %s", sf->programmatic_access_token ? "provided" : "not provided");
    }
    log_debug("host: %s", sf->host);
    log_debug("port: %s", sf->port);
    log_debug("account: %s", sf->account);
    log_debug("region: %s", sf->region);
    log_debug("database: %s", sf->database);
    log_debug("schema: %s", sf->schema);
    log_debug("warehouse: %s", sf->warehouse);
    log_debug("role: %s", sf->role);
    log_debug("protocol: %s", sf->protocol);
    log_debug("autocommit: %s", sf->autocommit ? "true": "false");
    log_debug("insecure_mode: %s", sf->insecure_mode ? "true" : "false");
    log_debug("ocsp_fail_open: %s", sf->ocsp_fail_open ? "true" : "false");
    log_debug("timezone: %s", sf->timezone);
    log_debug("login_timeout: %d", sf->login_timeout);
    log_debug("network_timeout: %d", sf->network_timeout);
    log_debug("retry_timeout: %d", sf->retry_timeout);
    log_debug("retry_count: %d", sf->retry_count);
    log_debug("qcc_disable: %s", sf->qcc_disable ? "true" : "false");
    log_debug("include_retry_reason: %s", sf->include_retry_reason ? "true" : "false");
    log_debug("use_s3_regional_url: %s", sf->use_s3_regional_url ? "true" : "false");
    log_debug("put_use_urand_dev: %s", sf->put_use_urand_dev ? "true" : "false");
    log_debug("put_compress_level: %d", sf->put_compress_level);
    log_debug("put_temp_dir: %s", sf->put_temp_dir ? sf->put_temp_dir : "");
    log_debug("put_fastfail: %s", sf->put_fastfail ? "true" : "false");
    log_debug("put_maxretries: %d", sf->put_maxretries);
    log_debug("get_fastfail: %s", sf->get_fastfail ? "true" : "false");
    log_debug("get_maxretries: %d", sf->get_maxretries);
    log_debug("get_threshold: %d", sf->get_threshold);

    return SF_STATUS_SUCCESS;
}


SF_STATUS STDCALL snowflake_global_init(
    const char *log_path, SF_LOG_LEVEL log_level, SF_USER_MEM_HOOKS *hooks) {
    SF_STATUS ret = SF_STATUS_ERROR_GENERAL;

    // Initialize constants
    DISABLE_VERIFY_PEER = SF_BOOLEAN_FALSE;
    CA_BUNDLE_FILE = NULL;
    SF_HEADER_USER_AGENT = NULL;
    SSL_VERSION = CURL_SSLVERSION_TLSv1_2;
    DEBUG = SF_BOOLEAN_FALSE;
    SF_OCSP_CHECK = SF_BOOLEAN_TRUE;

    _snowflake_memory_hooks_setup(hooks);
    sf_memory_init();
    sf_error_init();
    if (!log_init(log_path, log_level)) {
        // no way to log error because log_init failed.
        sf_fprintf(stderr, "Error during log initialization");
        goto cleanup;
    }
    CURLcode curl_ret = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (curl_ret != CURLE_OK) {
        log_fatal("curl_global_init() failed: %s",
                  curl_easy_strerror(curl_ret));
        goto cleanup;
    }

    if (SF_HEADER_USER_AGENT == NULL) {
#ifdef __STDC__
        long c_version = __STDC_VERSION__;
#else
        long c_version = 0L;
#endif
        char platform_version[128];
        sf_os_version(platform_version, sizeof(platform_version));
        const char *platform = sf_os_name();
        SF_HEADER_USER_AGENT = (char *) SF_CALLOC(1, HEADER_C_API_USER_AGENT_MAX_LEN);
        sf_sprintf(SF_HEADER_USER_AGENT, HEADER_C_API_USER_AGENT_MAX_LEN, HEADER_C_API_USER_AGENT_FORMAT, SF_API_NAME, SF_API_VERSION, platform, platform_version, "STDC", c_version);
    }

    ret = SF_STATUS_SUCCESS;

cleanup:
    return ret;
}

SF_STATUS STDCALL snowflake_global_term() {
    curl_global_cleanup();

    // Cleanup Constants
    SF_FREE(CA_BUNDLE_FILE);
    SF_FREE(SF_HEADER_USER_AGENT);

    log_term();
    sf_alloc_map_to_log(SF_BOOLEAN_TRUE);
    sf_error_term();
    sf_memory_term();
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL
snowflake_global_set_attribute(SF_GLOBAL_ATTRIBUTE type, const void *value) {
    switch (type) {
        case SF_GLOBAL_DISABLE_VERIFY_PEER:
            DISABLE_VERIFY_PEER = *(sf_bool *) value;
            break;
        case SF_GLOBAL_CA_BUNDLE_FILE:
            alloc_buffer_and_copy(&CA_BUNDLE_FILE, value);
            break;
        case SF_GLOBAL_SSL_VERSION:
            SSL_VERSION = *(int32 *) value;
            break;
        case SF_GLOBAL_DEBUG:
            DEBUG = *(sf_bool *) value;
            if (DEBUG) {
                log_set_quiet(SF_BOOLEAN_FALSE);
            } else {
                log_set_quiet(SF_BOOLEAN_TRUE);
            }
            break;
        case SF_GLOBAL_OCSP_CHECK:
            SF_OCSP_CHECK = *(sf_bool *) value;
            break;
        case SF_GLOBAL_CLIENT_CONFIG_FILE:
            alloc_buffer_and_copy(&CLIENT_CONFIG_FILE, value);
            break;
        default:
            break;
    }
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL
snowflake_global_get_attribute(SF_GLOBAL_ATTRIBUTE type, void *value, size_t size) {
    switch (type) {
        case SF_GLOBAL_DISABLE_VERIFY_PEER:
            *((sf_bool *) value) = DISABLE_VERIFY_PEER;
            break;
        case SF_GLOBAL_CA_BUNDLE_FILE:
            if (CA_BUNDLE_FILE) {
                if (strlen(CA_BUNDLE_FILE) > size - 1) {
                    return SF_STATUS_ERROR_BUFFER_TOO_SMALL;
                }
                sf_strncpy(value, size, CA_BUNDLE_FILE, size);
            }
            break;
        case SF_GLOBAL_SSL_VERSION:
            *((int32 *) value) = SSL_VERSION;
            break;
        case SF_GLOBAL_DEBUG:
            *((sf_bool *) value) = DEBUG;
            break;
        case SF_GLOBAL_OCSP_CHECK:
            *((sf_bool *) value) = SF_OCSP_CHECK;
            break;
        case SF_GLOBAL_CLIENT_CONFIG_FILE:
            if (CLIENT_CONFIG_FILE) {
              if (strlen(CLIENT_CONFIG_FILE) > size - 1) {
                return SF_STATUS_ERROR_BUFFER_TOO_SMALL;
              }
              sf_strncpy(value, size, CLIENT_CONFIG_FILE, size);
            }
            break;
        case SF_GLOBAL_LOG_LEVEL:
            {
              if (strlen(log_from_level_to_str(log_get_level())) > size - 1) {
                return SF_STATUS_ERROR_BUFFER_TOO_SMALL;
              }
              sf_strncpy(value, size, log_from_level_to_str(log_get_level()), size);
              break;
            }
        case SF_GLOBAL_LOG_PATH:
            if (LOG_PATH) {
              if (strlen(LOG_PATH) > size - 1) {
                return SF_STATUS_ERROR_BUFFER_TOO_SMALL;
              }
              sf_strncpy(value, size, LOG_PATH, size);
            }
            break;
        default:
            break;
    }
    return SF_STATUS_SUCCESS;
}

SF_CONNECT *STDCALL snowflake_init() {
    SF_CONNECT *sf = (SF_CONNECT *) SF_CALLOC(1, sizeof(SF_CONNECT));

    // Make sure memory was actually allocated
    if (sf) {
        // seed the rand
        srand(time(NULL));
        // Initialize object with default values
        sf->host = NULL;
        sf->port = NULL;
        sf->user = NULL;
        sf->password = NULL;
        sf->database = NULL;
        sf->account = NULL;
        sf->region = NULL;
        sf->role = NULL;
        sf->warehouse = NULL;
        sf->schema = NULL;
        alloc_buffer_and_copy(&sf->protocol, "https");
        sf->passcode = NULL;
        sf->passcode_in_password = SF_BOOLEAN_FALSE;
        sf->insecure_mode = SF_BOOLEAN_FALSE;
        sf->ocsp_fail_open = SF_BOOLEAN_TRUE;
        sf->autocommit = SF_BOOLEAN_TRUE;
#if defined(__APPLE__) || defined(_WIN32)
        sf->client_request_mfa_token = SF_BOOLEAN_TRUE;
#else
        sf->client_request_mfa_token = SF_BOOLEAN_FALSE;
#endif
        sf->qcc_disable = SF_BOOLEAN_FALSE;
        sf->include_retry_reason = SF_BOOLEAN_TRUE;
        sf->timezone = NULL;
        sf->service_name = NULL;
        sf->query_result_format = NULL;
        sf->auth_object = NULL;
        alloc_buffer_and_copy(&sf->authenticator, SF_AUTHENTICATOR_DEFAULT);
        alloc_buffer_and_copy(&sf->application_name, SF_API_NAME);
        alloc_buffer_and_copy(&sf->application_version, SF_API_VERSION);
        sf->application = NULL;

        _mutex_init(&sf->mutex_parameters);

        sf->token = NULL;
        sf->master_token = NULL;
        sf->login_timeout = SF_LOGIN_TIMEOUT;
        sf->network_timeout = 0;
        sf->retry_timeout = SF_RETRY_TIMEOUT;
        sf->sequence_counter = 0;
        _mutex_init(&sf->mutex_sequence_counter);
        sf->request_id[0] = '\0';
        clear_snowflake_error(&sf->error);

        sf->priv_key_file = NULL;
        sf->priv_key_file_pwd = NULL;
        sf->jwt_timeout = SF_JWT_TIMEOUT;
        sf->jwt_cnxn_wait_time = SF_JWT_CNXN_WAIT_TIME;

        sf->proxy = NULL;
        sf->no_proxy = NULL;

        sf->directURL_param = NULL;
        sf->directURL = NULL;
        sf->direct_query_token = NULL;
        sf->retry_on_curle_couldnt_connect_count = 0;
        sf->retry_on_connect_count = 0;
        sf->retry_count = SF_MAX_RETRY;

        sf->qcc_capacity = SF_QCC_CAPACITY_DEF;
        sf->qcc_disable = SF_BOOLEAN_FALSE;
        sf->qcc = NULL;

        sf->max_varchar_size = SF_DEFAULT_MAX_OBJECT_SIZE;
        sf->max_binary_size = SF_DEFAULT_MAX_OBJECT_SIZE / 2;
        sf->max_variant_size = SF_DEFAULT_MAX_OBJECT_SIZE;

        sf->oauth_token = NULL;
        sf->programmatic_access_token = NULL;

        sf->use_s3_regional_url = SF_BOOLEAN_FALSE;
        sf->put_use_urand_dev = SF_BOOLEAN_FALSE;
        sf->put_compress_level = SF_DEFAULT_PUT_COMPRESS_LEVEL;
        sf->put_temp_dir = NULL;
        sf->put_fastfail = SF_BOOLEAN_FALSE;
        sf->put_maxretries = SF_DEFAULT_PUT_MAX_RETRIES;
        sf->get_fastfail = SF_BOOLEAN_FALSE;
        sf->get_maxretries = SF_DEFAULT_GET_MAX_RETRIES;
        sf->get_threshold = SF_DEFAULT_GET_THRESHOLD;

        _mutex_init(&sf->mutex_stage_bind);
        sf->binding_stage_created = SF_BOOLEAN_FALSE;
        sf->stage_binding_threshold = SF_DEFAULT_STAGE_BINDING_THRESHOLD;
        sf->client_session_keep_alive = SF_BOOLEAN_TRUE;
        sf->client_session_keep_alive_heartbeat_frequency = SF_DEFAULT_CLIENT_SESSION_ALIVE_HEARTBEAT_FREQUENCY;
        _mutex_init(&sf->mutex_heart_beat);
        sf->is_heart_beat_on = SF_BOOLEAN_FALSE;
        sf->master_token_validation_time = SF_DEFAULT_MASTER_TOKEN_VALIDATION_TIME;
        sf->is_heart_beat_debug_mode = SF_BOOLEAN_FALSE;
    }

    return sf;
}

SF_STATUS STDCALL snowflake_term(SF_CONNECT *sf) {
    // Ensure object is not null
    if (!sf) {
        return SF_STATUS_ERROR_CONNECTION_NOT_EXIST;
    }
    cJSON *resp = NULL;
    char *s_resp = NULL;
    clear_snowflake_error(&sf->error);

    stop_heart_beat_for_this_session(sf);

    if (sf->token && sf->master_token) {
        /* delete the session */
        URL_KEY_VALUE url_params[] = {
            {.key="delete=", .value="true", .formatted_key=NULL, .formatted_value=NULL, .key_size=0, .value_size=0}
        };
        if (request(sf, &resp, DELETE_SESSION_URL, url_params,
                    sizeof(url_params) / sizeof(URL_KEY_VALUE), NULL, NULL,
                    POST_REQUEST_TYPE, &sf->error, SF_BOOLEAN_FALSE,
                    0, sf->retry_count, get_retry_timeout(sf), NULL, NULL, NULL, SF_BOOLEAN_FALSE)) {
            s_resp = snowflake_cJSON_Print(resp);
            log_trace("JSON response:\n%s", s_resp);
            /* Even if the session deletion fails, it will be cleaned after 7 days.
             * Catching error here won't help
             */
        }
        snowflake_cJSON_Delete(resp);
        SF_FREE(s_resp);
    }

    auth_terminate(sf);
    // SNOW-715510: TODO Enable token cache
/*
    cred_cache_term(sf->token_cache);
*/
    qcc_terminate(sf);

    _mutex_term(&sf->mutex_sequence_counter);
    _mutex_term(&sf->mutex_parameters);
    _mutex_term(&sf->mutex_stage_bind);
    _mutex_term(&sf->mutex_heart_beat);

    SF_FREE(sf->host);
    SF_FREE(sf->port);
    SF_FREE(sf->user);
    SF_FREE(sf->password);
    SF_FREE(sf->database);
    SF_FREE(sf->account);
    SF_FREE(sf->region);
    SF_FREE(sf->role);
    SF_FREE(sf->warehouse);
    SF_FREE(sf->schema);
    SF_FREE(sf->protocol);
    SF_FREE(sf->passcode);
    SF_FREE(sf->authenticator);
    SF_FREE(sf->application_name);
    SF_FREE(sf->application_version);
    SF_FREE(sf->application);
    SF_FREE(sf->timezone);
    SF_FREE(sf->service_name);
    SF_FREE(sf->query_result_format);
    SF_FREE(sf->master_token);
    SF_FREE(sf->token);
    SF_FREE(sf->directURL);
    SF_FREE(sf->directURL_param);
    SF_FREE(sf->direct_query_token);
    SF_FREE(sf->priv_key_file);
    SF_FREE(sf->priv_key_file_pwd);
    SF_FREE(sf->proxy);
    SF_FREE(sf->no_proxy);
    SF_FREE(sf->oauth_token);
    SF_FREE(sf->session_id);
    SF_FREE(sf);

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL snowflake_connect(SF_CONNECT *sf) {
    sf_bool success = SF_BOOLEAN_FALSE;
    SF_JSON_ERROR json_error;
    if (!sf) {
        // connection object doesn't exists
        return SF_STATUS_ERROR_CONNECTION_NOT_EXIST;
    }
    if (sf->token || sf->master_token) {
        // safe guard not to override the existing connection
        SET_SNOWFLAKE_ERROR(
            &sf->error,
            SF_STATUS_ERROR_APPLICATION_ERROR,
            ERR_MSG_CONNECTION_ALREADY_EXISTS,
            SF_SQLSTATE_CONNECTION_ALREADY_EXIST);
        return SF_STATUS_ERROR_GENERAL;
    }
    // Reset error context
    clear_snowflake_error(&sf->error);

    char os_version[128];
    sf_os_version(os_version, sizeof(os_version));

    log_info("Snowflake C/C++ API: %s, OS: %s, OS Version: %s",
             SF_API_VERSION,
             sf_os_name(),
             os_version);

    if (!is_string_empty(sf->directURL))
    {
        if (is_string_empty(sf->directURL_param) ||
                is_string_empty(sf->direct_query_token))
        {
            return SF_STATUS_ERROR_BAD_CONNECTION_PARAMS;
        }
        return SF_STATUS_SUCCESS;
    }


    cJSON *body = NULL;
    cJSON *data = NULL;
    cJSON *resp = NULL;
    char *s_body = NULL;
    char *s_resp = NULL;
    // Encoded URL to use with libcurl
    URL_KEY_VALUE url_params[] = {
        {.key = "request_id=", .value=sf->request_id, .formatted_key=NULL, .formatted_value=NULL, .key_size=0, .value_size=0},
        {.key = "databaseName=", .value=sf->database, .formatted_key=NULL, .formatted_value=NULL, .key_size=0, .value_size=0},
        {.key = "schemaName=", .value=sf->schema, .formatted_key=NULL, .formatted_value=NULL, .key_size=0, .value_size=0},
        {.key = "warehouse=", .value=sf->warehouse, .formatted_key=NULL, .formatted_value=NULL, .key_size=0, .value_size=0},
        {.key = "roleName=", .value=sf->role, .formatted_key=NULL, .formatted_value=NULL, .key_size=0, .value_size=0},
    };
    SF_STATUS ret = _snowflake_check_connection_parameters(sf);
    if (ret != SF_STATUS_SUCCESS) {
        goto cleanup;
    }

    ret = auth_initialize(sf);
    if (ret != SF_STATUS_SUCCESS) {
        goto cleanup;
    }

    ret = auth_authenticate(sf);
    if (ret != SF_STATUS_SUCCESS) {
        goto cleanup;
    }

    ret = SF_STATUS_ERROR_GENERAL; // reset to the error

    uuid4_generate(sf->request_id);// request id

    // Create body
    body = create_auth_json_body(
        sf,
        sf->application,
        sf->application_name,
        sf->application_version,
        sf->timezone,
        sf->autocommit);
    log_trace("Created body");
    s_body = snowflake_cJSON_Print(body);
    // TODO delete password before printing
    if (DEBUG) {
        log_debug("body:\n%s", s_body);
    }

    // Send request and get data
    // For authentication(JWT) needs renew credentials during retry
    // make loop to renew credentials as needed and track elapsed_time
    // and retried_count as well
    int64 renew_timeout = auth_get_renew_timeout(sf);
    int64 elapsed_time = 0;
    int8 retried_count = 0;
    sf_bool is_renew = SF_BOOLEAN_FALSE;
    sf_bool renew_injection = SF_BOOLEAN_FALSE;
    if ((AUTH_JWT == getAuthenticatorType(sf->authenticator)) &&
        sf->password && (strcmp(sf->password, "renew injection") == 0)) {
        renew_injection = SF_BOOLEAN_TRUE;
    }
    while (!success) {
        if (request(sf, &resp, SESSION_URL, url_params,
                    sizeof(url_params) / sizeof(URL_KEY_VALUE), s_body, NULL,
                    POST_REQUEST_TYPE, &sf->error, SF_BOOLEAN_FALSE,
                    renew_timeout, get_login_retry_count(sf), get_login_timeout(sf), &elapsed_time,
                    &retried_count, &is_renew, renew_injection)) {
            s_resp = snowflake_cJSON_Print(resp);
            log_trace("Here is JSON response:\n%s", s_resp);
            if ((json_error = json_copy_bool(&success, resp, "success")) !=
                SF_JSON_ERROR_NONE) {
                log_error("JSON error: %d", json_error);
                SET_SNOWFLAKE_ERROR(&sf->error, SF_STATUS_ERROR_BAD_JSON,
                                    "No valid JSON response",
                                    SF_SQLSTATE_UNABLE_TO_CONNECT);
                goto cleanup;
            }
            if (!success) {
                cJSON *messageJson = snowflake_cJSON_GetObjectItem(resp, "message");
                char *message = NULL;
                cJSON *codeJson = NULL;
                int64 code = -1;
                if (messageJson) {
                    message = messageJson->valuestring;
                }
                codeJson = snowflake_cJSON_GetObjectItem(resp, "code");
                if (codeJson) {
                    code = strtol(codeJson->valuestring, NULL, 10);
                } else {
                    log_debug("no code element.");
                }

                SET_SNOWFLAKE_ERROR(&sf->error, (SF_STATUS) code,
                                    message ? message : "Query was not successful",
                                    SF_SQLSTATE_UNABLE_TO_CONNECT);
                goto cleanup;
            }

            data = snowflake_cJSON_GetObjectItem(resp, "data");
            if (!set_tokens(sf, data, "token", "masterToken", &sf->error)) {
                goto cleanup;
            }

            json_copy_int(&sf->master_token_validation_time, data, "masterValidityInSeconds");

            cJSON* sessionIDJson = NULL;
            sessionIDJson = snowflake_cJSON_GetObjectItem(data, "sessionID");
            if (sessionIDJson) 
            {
                alloc_buffer_and_copy(&sf->session_id, snowflake_cJSON_Print(sessionIDJson));
            }

            // SNOW-715510: TODO Enable token cache
/*
            char* mfa_token = NULL;
            if (json_copy_string(&mfa_token, data, "mfaToken") == SF_JSON_ERROR_NONE && sf->token_cache) {
              cred_cache_save_credential(sf->token_cache, sf->host, sf->user, MFA_TOKEN, mfa_token);
            }
*/

            _mutex_lock(&sf->mutex_parameters);
            ret = _set_parameters_session_info(sf, data);
            if (sf->client_session_keep_alive)
            {
                start_heart_beat_for_this_session(sf);
            }
            else
            {
                stop_heart_beat_for_this_session(sf);
            }
            qcc_deserialize(sf, snowflake_cJSON_GetObjectItem(data, SF_QCC_RSP_KEY));
            _mutex_unlock(&sf->mutex_parameters);
            if (ret > 0) {
                goto cleanup;
            }
        } else {
            if (is_renew && (renew_timeout > 0)) {
                // renew authentication information in body
                auth_renew_json_body(sf, body);
                s_body = snowflake_cJSON_Print(body);
                continue;
            }
            log_error("No response");
            if (sf->error.error_code == SF_STATUS_SUCCESS) {
                SET_SNOWFLAKE_ERROR(&sf->error, SF_STATUS_ERROR_BAD_JSON,
                                    "No valid JSON response",
                                    SF_SQLSTATE_UNABLE_TO_CONNECT);
            }
            goto cleanup;
        }
    }
    /* we are done... */
    ret = SF_STATUS_SUCCESS;

cleanup:
    // Delete password and passcode for security's sake
    if (sf->password) {
        // Write over password in memory including null terminator
        memset(sf->password, 0, strlen(sf->password) + 1);
        SF_FREE(sf->password);
    }
    if (sf->passcode) {
        // Write over passcode in memory including null terminator
        memset(sf->passcode, 0, strlen(sf->passcode) + 1);
        SF_FREE(sf->passcode);
    }
    if (sf->priv_key_file_pwd) {
      // Write over priv_key_file_pwd in memory including null terminator
      memset(sf->priv_key_file_pwd, 0, strlen(sf->priv_key_file_pwd) + 1);
      SF_FREE(sf->priv_key_file_pwd);
    }
    snowflake_cJSON_Delete(body);
    snowflake_cJSON_Delete(resp);
    SF_FREE(s_body);
    SF_FREE(s_resp);

    return ret;
}

static SF_STATUS STDCALL _set_parameters_session_info(
    SF_CONNECT *sf, cJSON *data) {
    SF_STATUS ret = _reset_connection_parameters(
        sf,
        snowflake_cJSON_GetObjectItem(data, "parameters"),
        snowflake_cJSON_GetObjectItem(data, "sessionInfo"), SF_BOOLEAN_TRUE);
    return ret;
}

static void STDCALL _set_current_objects(SF_STMT *sfstmt, cJSON *data) {
    if (json_copy_string(&sfstmt->connection->database, data,
                         "finalDatabaseName")) {
        log_warn("No valid database found in response");
    }
    if (json_copy_string(&sfstmt->connection->schema, data,
                         "finalSchemaName")) {
        log_warn("No valid schema found in response");
    }
    if (json_copy_string(&sfstmt->connection->warehouse, data,
                         "finalWarehouseName")) {
        log_warn("No valid warehouse found in response");
    }
    if (json_copy_string(&sfstmt->connection->role, data,
                         "finalRoleName")) {
        log_warn("No valid role found in response");
    }
}

SF_STATUS STDCALL snowflake_set_attribute(
    SF_CONNECT *sf, SF_ATTRIBUTE type, const void *value) {
    if (!sf) {
        return SF_STATUS_ERROR_CONNECTION_NOT_EXIST;
    }
    clear_snowflake_error(&sf->error);
    switch (type) {
        case SF_CON_ACCOUNT:
            alloc_buffer_and_copy(&sf->account, value);
            break;
        case SF_CON_REGION:
            alloc_buffer_and_copy(&sf->region, value);
            break;
        case SF_CON_USER:
            alloc_buffer_and_copy(&sf->user, value);
            break;
        case SF_CON_PASSWORD:
            alloc_buffer_and_copy(&sf->password, value);
            break;
        case SF_CON_DATABASE:
            alloc_buffer_and_copy(&sf->database, value);
            break;
        case SF_CON_SCHEMA:
            alloc_buffer_and_copy(&sf->schema, value);
            break;
        case SF_CON_WAREHOUSE:
            alloc_buffer_and_copy(&sf->warehouse, value);
            break;
        case SF_CON_ROLE:
            alloc_buffer_and_copy(&sf->role, value);
            break;
        case SF_CON_HOST:
            alloc_buffer_and_copy(&sf->host, value);
            break;
        case SF_CON_PORT:
            alloc_buffer_and_copy(&sf->port, value);
            break;
        case SF_CON_PROTOCOL:
            alloc_buffer_and_copy(&sf->protocol, value);
            break;
        case SF_CON_PASSCODE:
            alloc_buffer_and_copy(&sf->passcode, value);
            break;
        case SF_CON_PASSCODE_IN_PASSWORD:
            sf->passcode_in_password = *((sf_bool *) value);
            break;
        case SF_CON_APPLICATION_NAME:
            alloc_buffer_and_copy(&sf->application_name, value);
            break;
        case SF_CON_APPLICATION_VERSION:
            alloc_buffer_and_copy(&sf->application_version, value);
            break;
        case SF_CON_APPLICATION:
          alloc_buffer_and_copy(&sf->application, value);
          break;
        case SF_CON_AUTHENTICATOR:
            alloc_buffer_and_copy(&sf->authenticator, value);
            break;
        case SF_CON_OAUTH_TOKEN:
            alloc_buffer_and_copy(&sf->oauth_token, value);
            break;
        case SF_CON_PAT:
            alloc_buffer_and_copy(&sf->programmatic_access_token, value);
            break;
        case SF_CON_INSECURE_MODE:
            sf->insecure_mode = value ? *((sf_bool *) value) : SF_BOOLEAN_FALSE;
            break;
        case SF_CON_OCSP_FAIL_OPEN:
          sf->ocsp_fail_open = value ? *((sf_bool*)value) : SF_BOOLEAN_TRUE;
          break;
        case SF_CON_LOGIN_TIMEOUT:
            sf->login_timeout = value ? *((int64 *) value) : SF_LOGIN_TIMEOUT;
            break;
        case SF_CON_NETWORK_TIMEOUT:
            sf->network_timeout = value ? *((int64 *) value) : SF_LOGIN_TIMEOUT;
            break;
        case SF_CON_RETRY_TIMEOUT:
          sf->retry_timeout = value ? *((int64 *)value) : SF_RETRY_TIMEOUT;
          if ((sf->retry_timeout < SF_RETRY_TIMEOUT) && (sf->retry_timeout != 0))
          {
            sf->retry_timeout = SF_RETRY_TIMEOUT;
          }
          break;
        case SF_CON_AUTOCOMMIT:
            sf->autocommit = value ? *((sf_bool *) value) : SF_BOOLEAN_TRUE;
            break;
        case SF_CON_TIMEZONE:
            alloc_buffer_and_copy(&sf->timezone, value);
            break;
        case SF_DIR_QUERY_URL:
            alloc_buffer_and_copy(&sf->directURL, value);
            break;
        case SF_DIR_QUERY_URL_PARAM:
            alloc_buffer_and_copy(&sf->directURL_param, value);
            break;
        case SF_DIR_QUERY_TOKEN:
            alloc_buffer_and_copy(&sf->direct_query_token, value);
            break;
        case SF_RETRY_ON_CURLE_COULDNT_CONNECT_COUNT:
            sf->retry_on_curle_couldnt_connect_count = value ? *((int8 *) value) : 0;
            break;
        case SF_CON_PRIV_KEY_FILE:
            alloc_buffer_and_copy(&sf->priv_key_file, value);
            break;
        case SF_CON_PRIV_KEY_FILE_PWD:
            alloc_buffer_and_copy(&sf->priv_key_file_pwd, value);
            break;
        case SF_CON_JWT_TIMEOUT:
            sf->jwt_timeout = value ? *((int64 *)value) : SF_JWT_TIMEOUT;
            break;
        case SF_CON_JWT_CNXN_WAIT_TIME:
            sf->jwt_cnxn_wait_time = value ? *((int64 *)value) : SF_JWT_CNXN_WAIT_TIME;
            break;
        case SF_CON_MAX_CON_RETRY:
            sf->retry_on_connect_count = value ? *((int8 *)value) : SF_MAX_RETRY;
            break;
        case SF_CON_MAX_RETRY:
          sf->retry_count = value ? *((int8 *)value) : SF_MAX_RETRY;
          if ((sf->retry_count < SF_MAX_RETRY) && (sf->retry_count != 0))
          {
            sf->retry_count = SF_MAX_RETRY;
          }
          break;
        case SF_CON_PROXY:
            alloc_buffer_and_copy(&sf->proxy, value);
            break;
        case SF_CON_NO_PROXY:
            alloc_buffer_and_copy(&sf->no_proxy, value);
            break;
        case SF_CON_DISABLE_QUERY_CONTEXT_CACHE:
            sf->qcc_disable = value ? *((sf_bool *)value) : SF_BOOLEAN_FALSE;
            break;
        case SF_CON_INCLUDE_RETRY_REASON:
            sf->include_retry_reason = value ? *((sf_bool *)value) : SF_BOOLEAN_TRUE;
            break;
        case SF_CON_DISABLE_SAML_URL_CHECK:
            sf->disable_saml_url_check = value ? *((sf_bool*)value) : SF_BOOLEAN_FALSE;
            break;
        case SF_CON_PUT_TEMPDIR:
            alloc_buffer_and_copy(&sf->put_temp_dir, value);
            break;
        case SF_CON_PUT_COMPRESSLV:
            sf->put_compress_level = value ? *((int8 *)value) : SF_DEFAULT_PUT_COMPRESS_LEVEL;
            if ((sf->put_compress_level > SF_MAX_PUT_COMPRESS_LEVEL) ||
                (sf->put_compress_level < 0))
            {
                sf->put_compress_level = SF_DEFAULT_PUT_COMPRESS_LEVEL;
            }
            break;
        case SF_CON_PUT_USE_URANDOM_DEV:
            sf->put_use_urand_dev = value ? *((sf_bool *)value) : SF_BOOLEAN_FALSE;
            break;
        case SF_CON_PUT_FASTFAIL:
            sf->put_fastfail = value ? *((sf_bool *)value) : SF_BOOLEAN_FALSE;
            break;
        case SF_CON_PUT_MAXRETRIES:
            sf->put_maxretries = value ? *((int8 *)value) : SF_DEFAULT_PUT_MAX_RETRIES;
            if ((sf->put_maxretries > SF_MAX_PUT_MAX_RETRIES) ||
                (sf->put_maxretries < 0))
            {
                sf->put_maxretries = SF_DEFAULT_PUT_MAX_RETRIES;
            }
            break;
        case SF_CON_GET_FASTFAIL:
            sf->get_fastfail = value ? *((sf_bool *)value) : SF_BOOLEAN_FALSE;
            break;
        case SF_CON_GET_MAXRETRIES:
            sf->get_maxretries = value ? *((int8 *)value) : SF_DEFAULT_GET_MAX_RETRIES;
            if ((sf->get_maxretries > SF_MAX_GET_MAX_RETRIES) ||
                (sf->get_maxretries < 0))
            {
                sf->get_maxretries = SF_DEFAULT_GET_MAX_RETRIES;
            }
            break;
        case SF_CON_GET_THRESHOLD:
            sf->get_threshold = value ? *((int64 *)value) : SF_DEFAULT_GET_THRESHOLD;
            break;
        case SF_CON_CLIENT_REQUEST_MFA_TOKEN:
            sf->client_request_mfa_token = value ? *((sf_bool *) value): SF_BOOLEAN_TRUE;
            break;
        case SF_CON_STAGE_BIND_THRESHOLD:
            if (value)
            {
                sf->stage_binding_threshold = *((int64*)value);
                sf->binding_threshold_overridden = SF_BOOLEAN_TRUE;
            }
            else
            {
                sf->stage_binding_threshold = SF_DEFAULT_STAGE_BINDING_THRESHOLD;
                sf->binding_threshold_overridden = SF_BOOLEAN_FALSE;
            }
            break;
        case SF_CON_DISABLE_STAGE_BIND:
          sf->stage_binding_disabled = value ? *((sf_bool*)value) : SF_BOOLEAN_FALSE;
          break;
        case SF_CON_CLIENT_SESSION_KEEP_ALIVE:
            sf->client_session_keep_alive = value ? *((sf_bool*)value) : SF_BOOLEAN_FALSE;
            break;
        case SF_CON_CLIENT_SESSION_KEEP_ALIVE_HEARTBEAT_FREQUENCY:
            sf->client_session_keep_alive_heartbeat_frequency = value ? validate_client_session_keep_alive_heart_beat_frequency(*((uint64*)value)) : SF_DEFAULT_CLIENT_SESSION_ALIVE_HEARTBEAT_FREQUENCY;
            break;
        default:
            SET_SNOWFLAKE_ERROR(&sf->error, SF_STATUS_ERROR_BAD_ATTRIBUTE_TYPE,
                                "Invalid attribute type",
                                SF_SQLSTATE_UNABLE_TO_CONNECT);
            return SF_STATUS_ERROR_APPLICATION_ERROR;
    }

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL snowflake_get_attribute(
    SF_CONNECT *sf, SF_ATTRIBUTE type, void **value) {
    if (!sf) {
        return SF_STATUS_ERROR_CONNECTION_NOT_EXIST;
    }
    clear_snowflake_error(&sf->error);
    switch (type) {
        case SF_CON_ACCOUNT:
            *value = sf->account;
            break;
        case SF_CON_REGION:
            *value = sf->region;
            break;
        case SF_CON_USER:
            *value = sf->user;
            break;
        case SF_CON_PASSWORD:
            *value = sf->password;
            break;
        case SF_CON_DATABASE:
            *value = sf->database;
            break;
        case SF_CON_SCHEMA:
            *value = sf->schema;
            break;
        case SF_CON_WAREHOUSE:
            *value = sf->warehouse;
            break;
        case SF_CON_ROLE:
            *value = sf->role;
            break;
        case SF_CON_HOST:
            *value = sf->host;
            break;
        case SF_CON_PORT:
            *value = sf->port;
            break;
        case SF_CON_PROTOCOL:
            *value = sf->protocol;
            break;
        case SF_CON_PASSCODE:
            *value = sf->passcode;
            break;
        case SF_CON_PASSCODE_IN_PASSWORD:
            *value = &sf->passcode_in_password;
            break;
        case SF_CON_APPLICATION_NAME:
            *value = sf->application_name;
            break;
        case SF_CON_APPLICATION_VERSION:
            *value = sf->application_version;
            break;
        case SF_CON_APPLICATION:
          *value = sf->application;
          break;
        case SF_CON_AUTHENTICATOR:
            *value = sf->authenticator;
            break;
        case SF_CON_OAUTH_TOKEN:
            *value = sf->oauth_token;
            break;
        case SF_CON_INSECURE_MODE:
            *value = &sf->insecure_mode;
            break;
        case SF_CON_OCSP_FAIL_OPEN:
          *value = &sf->ocsp_fail_open;
          break;
        case SF_CON_LOGIN_TIMEOUT:
            *value = &sf->login_timeout;
            break;
        case SF_CON_NETWORK_TIMEOUT:
            *value = &sf->network_timeout;
            break;
        case SF_CON_RETRY_TIMEOUT:
          *value = &sf->retry_timeout;
          break;
        case SF_CON_AUTOCOMMIT:
            *value = &sf->autocommit;
            break;
        case SF_CON_TIMEZONE:
            *value = sf->timezone;
            break;
        case SF_CON_SERVICE_NAME:
            *value = sf->service_name;
            break;
        case SF_DIR_QUERY_URL:
            *value = sf->directURL;
            break;
        case SF_DIR_QUERY_URL_PARAM:
            *value = sf->directURL_param;
            break;
        case SF_DIR_QUERY_TOKEN:
            *value = sf->direct_query_token;
            break;
        case SF_RETRY_ON_CURLE_COULDNT_CONNECT_COUNT:
            *value = &sf->retry_on_curle_couldnt_connect_count;
            break;
        case SF_QUERY_RESULT_TYPE:
            *value = sf->query_result_format;
            break;
        case SF_CON_PRIV_KEY_FILE:
            *value = sf->priv_key_file;
            break;
        case SF_CON_PRIV_KEY_FILE_PWD:
            *value = sf->priv_key_file_pwd;
            break;
        case SF_CON_JWT_TIMEOUT:
            *value = &sf->jwt_timeout;
            break;
        case SF_CON_JWT_CNXN_WAIT_TIME:
            *value = &sf->jwt_cnxn_wait_time;
            break;
        case SF_CON_MAX_CON_RETRY:
            *value = &sf->retry_on_connect_count;
            break;
        case SF_CON_MAX_RETRY:
          *value = &sf->retry_count;
          break;
        case SF_CON_PROXY:
            *value = sf->proxy;
            break;
        case SF_CON_NO_PROXY:
            *value = sf->no_proxy;
            break;
        case SF_CON_DISABLE_QUERY_CONTEXT_CACHE:
            *value = &sf->qcc_disable;
            break;
        case SF_CON_INCLUDE_RETRY_REASON:
            *value = &sf->include_retry_reason;
            break;
        case SF_CON_MAX_VARCHAR_SIZE:
            *value = &sf->max_varchar_size;
            break;
        case SF_CON_MAX_BINARY_SIZE:
            *value = &sf->max_binary_size;
            break;
        case SF_CON_MAX_VARIANT_SIZE:
            *value = &sf->max_variant_size;
            break;
        case SF_CON_DISABLE_SAML_URL_CHECK:
            *value = &sf->disable_saml_url_check;
            break;
        case SF_CON_PUT_TEMPDIR:
            *value = sf->put_temp_dir;
            break;
        case SF_CON_PUT_COMPRESSLV:
            *value = &sf->put_compress_level;
            break;
        case SF_CON_PUT_USE_URANDOM_DEV:
            *value = &sf->put_use_urand_dev;
            break;
        case SF_CON_PUT_FASTFAIL:
            *value = &sf->put_fastfail;
            break;
        case SF_CON_PUT_MAXRETRIES:
            *value = &sf->put_maxretries;
            break;
        case SF_CON_GET_FASTFAIL:
            *value = &sf->get_fastfail;
            break;
        case SF_CON_GET_MAXRETRIES:
            *value = &sf->get_maxretries;
            break;
        case SF_CON_GET_THRESHOLD:
            *value = &sf->get_threshold;
            break;
        case SF_CON_STAGE_BIND_THRESHOLD:
          *value = &sf->stage_binding_threshold;
          break;
        case SF_CON_DISABLE_STAGE_BIND:
          *value = &sf->stage_binding_disabled;
          break;
        case SF_CON_CLIENT_SESSION_KEEP_ALIVE:
            *value = &sf->client_session_keep_alive;
            break;
        case SF_CON_CLIENT_SESSION_KEEP_ALIVE_HEARTBEAT_FREQUENCY:
            *value = &sf->client_session_keep_alive_heartbeat_frequency;
            break;
        default:
            SET_SNOWFLAKE_ERROR(&sf->error, SF_STATUS_ERROR_BAD_ATTRIBUTE_TYPE,
                                "Invalid attribute type",
                                SF_SQLSTATE_UNABLE_TO_CONNECT);
            return SF_STATUS_ERROR_APPLICATION_ERROR;
    }
    return SF_STATUS_SUCCESS;
}

/**
 * Resets SF_COLUMN_DESC in SF_STMT
 * @param sfstmt
 */
static void STDCALL _snowflake_stmt_desc_reset(SF_STMT *sfstmt) {
    int64 i = 0;
    if (sfstmt->desc) {
        /* column metadata */
        for (i = 0; i < sfstmt->total_fieldcount; i++) {
            SF_FREE(sfstmt->desc[i].name);
        }
        SF_FREE(sfstmt->desc);
    }
    sfstmt->desc = NULL;
}

/**
 * Resets SF_ROW_METADATA in SF_STMT
 * @param sfstmt
 */
static void STDCALL _snowflake_stmt_row_metadata_reset(SF_STMT *sfstmt) {
    if (sfstmt->stats) {
        SF_FREE(sfstmt->stats);
    }
    sfstmt->stats = NULL;
}

/**
 * Setup result set from json response.
 * could be result of a regular query or one of the results
 * in multiple statements
 */
static sf_bool setup_result_with_json_resp(SF_STMT* sfstmt, cJSON* data)
{
    // Set Database info
    _mutex_lock(&sfstmt->connection->mutex_parameters);
    /* Set other parameters. Ignore the status */
    _set_current_objects(sfstmt, data);
    _set_parameters_session_info(sfstmt->connection, data);
    qcc_deserialize(sfstmt->connection, snowflake_cJSON_GetObjectItem(data, SF_QCC_RSP_KEY));
    _mutex_unlock(&sfstmt->connection->mutex_parameters);

    // clean up from preivous result
    sfstmt->chunk_rowcount = -1;
    sfstmt->total_rowcount = -1;
    sfstmt->total_fieldcount = -1;
    sfstmt->total_row_index = -1;

    // Destroy chunk downloader
    chunk_downloader_term(sfstmt->chunk_downloader);
    sfstmt->chunk_downloader = NULL;

    int64 stmt_type_id;
    if (json_copy_int(&stmt_type_id, data, "statementTypeId")) {
        /* failed to get statement type id */
        sfstmt->is_dml = SF_BOOLEAN_FALSE;
    } else {
        sfstmt->is_dml = detect_stmt_type(stmt_type_id);
    }

    json_copy_bool(&sfstmt->array_bind_supported, data, "arrayBindSupported");

    cJSON* rowtype = snowflake_cJSON_GetObjectItem(data, "rowtype");
    if (snowflake_cJSON_IsArray(rowtype)) {
        _snowflake_stmt_desc_reset(sfstmt);
        sfstmt->total_fieldcount = snowflake_cJSON_GetArraySize(
          rowtype);
        sfstmt->desc = set_description(sfstmt, rowtype);
    }
    cJSON* stats = snowflake_cJSON_GetObjectItem(data, "stats");
    if (snowflake_cJSON_IsObject(stats)) {
        _snowflake_stmt_row_metadata_reset(sfstmt);
        sfstmt->stats = set_stats(stats);
    } else {
        sfstmt->stats = NULL;
    }

    // Determine query result format and detach rowset object from data.
    cJSON * qrf = snowflake_cJSON_GetObjectItem(data, "queryResultFormat");
    if (qrf) {
      char* qrf_str = snowflake_cJSON_GetStringValue(qrf);
      cJSON* rowset = NULL;

      if (strcmp(qrf_str, "arrow") == 0 || strcmp(qrf_str, "arrow_force") == 0) {
#ifdef SF_WIN32
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT,
          "Query results were fetched using Arrow, "
          "but the client library does not yet support decoding Arrow results", "",
          sfstmt->sfqid);

        return SF_STATUS_ERROR_UNSUPPORTED_QUERY_RESULT_FORMAT;
#endif
        sfstmt->qrf = SF_ARROW_FORMAT;
        rowset = snowflake_cJSON_DetachItemFromObject(data, "rowsetBase64");
        if (!rowset)
        {
          log_error("No valid rowset found in response");
          SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error,
            SF_STATUS_ERROR_BAD_JSON,
            "Missing rowset from response. No results found.",
            SF_SQLSTATE_APP_REJECT_CONNECTION,
            sfstmt->sfqid);
          return SF_BOOLEAN_FALSE;
        }
      } else if (strcmp(qrf_str, "json") == 0) {
        sfstmt->qrf = SF_JSON_FORMAT;
        if (json_detach_array_from_object((cJSON**)(&rowset), data, "rowset"))
        {
          log_error("No valid rowset found in response");
          SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error,
            SF_STATUS_ERROR_BAD_JSON,
            "Missing rowset from response. No results found.",
            SF_SQLSTATE_APP_REJECT_CONNECTION,
            sfstmt->sfqid);
          return SF_BOOLEAN_FALSE;
        }
      } else {
        log_error("Unsupported query result format: %s", qrf_str);
        return SF_BOOLEAN_FALSE;
      }
      // Index starts at 0 and incremented each fetch
      sfstmt->total_row_index = 0;
      cJSON* chunks = NULL;
      cJSON* chunk_headers = NULL;
      char* qrmk = NULL;
      // When the result set is sufficient large, the server response will contain
      // an empty "rowset" object. Instead, it will have a "chunks" object that contains,
      // among other fields, a URL from which the result set can be downloaded in chunks.
      // In this case, we initialize the chunk downloader, which will download in the
      // background as calls to snowflake_fetch() are made.
      if ((chunks = snowflake_cJSON_GetObjectItem(data, "chunks")) != NULL) {
        // We don't care if there is no qrmk, so ignore return code
        json_copy_string(&qrmk, data, "qrmk");
        chunk_headers = snowflake_cJSON_GetObjectItem(data, "chunkHeaders");
        NON_JSON_RESP* (*callback_create_resp)(void) = NULL;
        if (SF_ARROW_FORMAT == sfstmt->qrf) {
          callback_create_resp = callback_create_arrow_resp;
        }
        sfstmt->chunk_downloader = chunk_downloader_init(
          qrmk,
          chunk_headers,
          chunks,
          2, // thread count
          4, // fetch slot
          &sfstmt->error,
          sfstmt->connection->insecure_mode,
          sfstmt->connection->ocsp_fail_open,
          callback_create_resp,
          sfstmt->connection->proxy,
          sfstmt->connection->no_proxy,
          get_retry_timeout(sfstmt->connection),
          sfstmt->connection->retry_count);
        SF_FREE(qrmk);
        if (!sfstmt->chunk_downloader) {
          // Unable to create chunk downloader.
          // Error is set in chunk_downloader_init function.
          return SF_BOOLEAN_FALSE;
        }
        // Even when the result set is split into chunks, JSON format will still
        // response with the first chunk in "rowset", so be sure to include it.
        sfstmt->result_set = rs_create_with_json_result(
          rowset,
          sfstmt->desc,
          sfstmt->qrf,
          sfstmt->connection->timezone);
        // Update chunk row count. Controls the chunk downloader.
        sfstmt->chunk_rowcount = rs_get_row_count_in_chunk(
          sfstmt->result_set);
        // Update total row count. Used in snowflake_num_rows().
        if (json_copy_int(&sfstmt->total_rowcount, data, "total")) {
          log_warn(
            "No total count found in response. Reverting to using array size of results");
          sfstmt->total_rowcount = sfstmt->chunk_rowcount;
        }
      } else {
        // Create a result set object and update the total rowcount.
        sfstmt->result_set = rs_create_with_json_result(
          rowset,
          sfstmt->desc,
          sfstmt->qrf,
          sfstmt->connection->timezone);
        // Update chunk row count. Controls the chunk downloader.
        sfstmt->chunk_rowcount = rs_get_row_count_in_chunk(
          sfstmt->result_set);
        // Update total row count. Used in snowflake_num_rows().
        if (json_copy_int(&sfstmt->total_rowcount, data, "total")) {
          log_warn(
            "No total count found in response. Reverting to using array size of results");
          sfstmt->total_rowcount = sfstmt->chunk_rowcount;
        }
      }
    }
    return SF_BOOLEAN_TRUE;
}

/**
 * Setup result set from json response.
 * could be result of a regular query or one of the results
 * in multiple statements
 */
static sf_bool setup_multi_stmt_result(SF_STMT* sfstmt, cJSON* data)
{
    // Set Database info
    _mutex_lock(&sfstmt->connection->mutex_parameters);
    /* Set other parameters. Ignore the status */
    _set_current_objects(sfstmt, data);
    _set_parameters_session_info(sfstmt->connection, data);
    qcc_deserialize(sfstmt->connection, snowflake_cJSON_GetObjectItem(data, SF_QCC_RSP_KEY));
    _mutex_unlock(&sfstmt->connection->mutex_parameters);

    if (sfstmt->multi_stmt_result_ids)
    {
        snowflake_cJSON_Delete(sfstmt->multi_stmt_result_ids);
        sfstmt->multi_stmt_result_ids = NULL;
    }
    char* result_ids = NULL;
    if (json_copy_string(&result_ids, data, "resultIds"))
    {
        log_error("No valid resultIds found in response");
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error,
            SF_STATUS_ERROR_BAD_RESPONSE,
            "No valid resultIds found in multiple statements response.",
            SF_SQLSTATE_GENERAL_ERROR,
            sfstmt->sfqid);
        return SF_BOOLEAN_FALSE;
    }

    // split result ids with comma(,)
    cJSON* result_ids_json = snowflake_cJSON_CreateArray();
    char* start = result_ids;
    char* end = NULL;
    size_t len = strlen(result_ids);
    while ((start - result_ids) < len)
    {
        end = strchr(start, ',');
        if (!end)
        {
            // last part, set to end of the entire string
            end = result_ids + len;
        }
        *end = '\0';
        snowflake_cJSON_AddItemToArray(result_ids_json, snowflake_cJSON_CreateString(start));
        start = end + 1;
    }

    sfstmt->multi_stmt_result_ids = result_ids_json;

    return SF_STATUS_SUCCESS == snowflake_next_result(sfstmt);
}

SF_STATUS STDCALL snowflake_next_result(SF_STMT* sfstmt)
{
    char* result_url = NULL;
    cJSON* result_id_json = NULL;
    if (sfstmt && sfstmt->is_multi_stmt && sfstmt->multi_stmt_result_ids &&
        snowflake_cJSON_IsArray(sfstmt->multi_stmt_result_ids) &&
        (result_id_json = snowflake_cJSON_DetachItemFromArray(sfstmt->multi_stmt_result_ids, 0)))
    {
        char* result_id = snowflake_cJSON_GetStringValue(result_id_json);
        if (!result_id || (strlen(result_id) == 0))
        {
            log_error("Empty result id found for multiple statements.");
            SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error,
                SF_STATUS_ERROR_BAD_RESPONSE,
                "Empty result id found in multiple statements response.",
                SF_SQLSTATE_GENERAL_ERROR,
                sfstmt->sfqid);
            snowflake_cJSON_Delete(result_id_json);
            return SF_STATUS_ERROR_BAD_RESPONSE;
        }

        size_t url_size = strlen(QUERY_RESULT_URL_FORMAT) - 2 + strlen(result_id) + 1;
        result_url = (char*)SF_CALLOC(1, url_size);

        if (!result_url)
        {
            SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error,
                SF_STATUS_ERROR_OUT_OF_MEMORY,
                "Run out of memory trying to create result url.",
                SF_SQLSTATE_MEMORY_ALLOCATION_ERROR,
                sfstmt->sfqid);
            snowflake_cJSON_Delete(result_id_json);
            return SF_STATUS_ERROR_OUT_OF_MEMORY;
        }
        sf_sprintf(result_url, url_size, QUERY_RESULT_URL_FORMAT, result_id);
        snowflake_cJSON_Delete(result_id_json);
    } else if (sfstmt->is_async) {
        size_t url_size = strlen(QUERY_RESULT_URL_FORMAT) - 2 + strlen(sfstmt->sfqid) + 1;
        result_url = (char*)SF_CALLOC(1, url_size);
        sf_sprintf(result_url, url_size, QUERY_RESULT_URL_FORMAT, sfstmt->sfqid);
    } else {
        // no more results available.
        return SF_STATUS_EOF;
    }

    cJSON* result = NULL;
    if (!request(sfstmt->connection, &result, result_url, NULL, 0, NULL, NULL,
                 GET_REQUEST_TYPE, &sfstmt->error, SF_BOOLEAN_FALSE,
                 0, sfstmt->connection->retry_count, get_retry_timeout(sfstmt->connection),
                 NULL, NULL, NULL, SF_BOOLEAN_FALSE))
    {
        SF_FREE(result_url);
        return sfstmt->error.error_code;
    }
    SF_FREE(result_url);

    cJSON* data = snowflake_cJSON_GetObjectItem(result, "data");

    sf_bool success = SF_BOOLEAN_FALSE;
    if ((json_copy_bool(&success, result, "success") != SF_JSON_ERROR_NONE) || !success)
    {
        cJSON *messageJson = NULL;
        char *message = NULL;
        cJSON *codeJson = NULL;
        int64 code = -1;
        if (json_copy_string_no_alloc(sfstmt->error.sqlstate, data,
                                      "sqlState", SF_SQLSTATE_LEN)) {
            log_debug("No valid sqlstate found in response");
        }
        messageJson = snowflake_cJSON_GetObjectItem(result, "message");
        if (messageJson) {
            message = messageJson->valuestring;
        }
        codeJson = snowflake_cJSON_GetObjectItem(result, "code");
        if (codeJson) {
            code = (int64) strtol(codeJson->valuestring, NULL, 10);
        } else {
            log_debug("no code element.");
        }
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, code,
                                 message ? message
                                         : "Query was not successful",
                                 NULL, sfstmt->sfqid);

        snowflake_cJSON_Delete(result);
        return sfstmt->error.error_code;
    }

    setup_result_with_json_resp(sfstmt, data);
    snowflake_cJSON_Delete(result);

    return SF_STATUS_SUCCESS;
}

/**
 * Returns what kind of params are being bound - Named / Positional
 * based on the first bind input entry. Should be used only if
 * sfstmt->params is NULL;
 *
 * @param bind input
 * @return param type - NAMED or POSITIONAL
 */
PARAM_TYPE STDCALL _snowflake_get_param_style(const SF_BIND_INPUT *input)
{
    PARAM_TYPE retval = INVALID_PARAM_TYPE;

    if (!input)
    {
        goto done;
    }
    if (input->name && !input->idx)
    {
        retval = NAMED;
    }
    else
    {
        retval = POSITIONAL;
    }

    done:
    return retval;
}

/**
 * Returns the kind of params being bound for a particular prepared statement.
 * This is the API that should be used to find param style once sfstmt->params
 * have been initialized.
 *
 * @param sfstmt
 * @return current param type NAMED/ POSITIONAL
 */
PARAM_TYPE STDCALL _snowflake_get_current_param_style(const SF_STMT *sfstmt)
{
    PARAM_STORE *ps = NULL;
    if (!sfstmt || !sfstmt->params)
    {
        return INVALID_PARAM_TYPE;
    }

    ps = (PARAM_STORE *)sfstmt->params;

    return ps->param_style;
}
/**
 * Helper function to initialize named parameter list
 *
 * @param name_list
 */
static void STDCALL _snowflake_allocate_named_param_list(void ** name_list)
{
    NamedParams *nparams = (NamedParams *)SF_CALLOC(1,sizeof(NamedParams));
    nparams->name_list = SF_CALLOC(8, sizeof(void *));
    nparams->allocd = 8;
    nparams->used = 0;
    *name_list = (void *)nparams;
}

/**
 * Helper function to add names of named params to
 * sfstmt.
 *
 * @params
 * name_list - pointer to struture holding the list of names of the params
 * name - the name to be added to the list
 * cur_size - current size of the name list
 */
static void STDCALL _snowflake_add_to_named_param_list(void *name_list, char * name, unsigned int cur_size)
{
    NamedParams *nparams;
    if (!name_list)
    {
        return;
    }
    nparams = (NamedParams *)name_list;
    if (cur_size == nparams->allocd)
    {
        nparams->name_list = SF_REALLOC(nparams->name_list,(2 * cur_size * sizeof(void *)));
        nparams->allocd = 2 * cur_size;
    }
    nparams->name_list[cur_size] = (void *)name;
}

/**
 * Helper function to reset the name list instance in sfstmt
 *
 * @param name_list
 */
static void STDCALL _snowflake_deallocate_named_param_list(void * name_list)
{
    NamedParams * nparams;

    if (!name_list)
    {
        return;
    }
    nparams = (NamedParams *)name_list;
    SF_FREE(nparams->name_list);
    SF_FREE(name_list);
}

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
                                                       size_t name_buf_len)
{
    SF_BIND_INPUT* input = NULL;
    if (_snowflake_get_current_param_style(sfstmt) == POSITIONAL)
    {
        input = (SF_BIND_INPUT*)sf_param_store_get(sfstmt->params,
                                                   index + 1, NULL);
        sf_sprintf(name_buf, name_buf_len, "%lu", (unsigned long)(index + 1));
        *name = name_buf;
    }
    else if (_snowflake_get_current_param_style(sfstmt) == NAMED)
    {
        *name = (char*)(((NamedParams*)sfstmt->name_list)->name_list[index]);
        input = (SF_BIND_INPUT*)sf_param_store_get(sfstmt->params, 0, *name);
    }

    return input;
}

/*
 * @return size of single binding value per data type.
 */
size_t STDCALL _snowflake_get_binding_value_size(SF_BIND_INPUT* bind)
{
    switch (bind->c_type)
    {
    case SF_C_TYPE_INT8:
        return sizeof (int8);
    case SF_C_TYPE_UINT8:
        return sizeof(uint8);
    case SF_C_TYPE_INT64:
        return sizeof(int64);
    case SF_C_TYPE_UINT64:
        return sizeof(uint64);
    case SF_C_TYPE_FLOAT64:
        return sizeof(float64);
    case SF_C_TYPE_BOOLEAN:
        return sizeof(sf_bool);
    case SF_C_TYPE_BINARY:
    case SF_C_TYPE_STRING:
        return bind->len;
    case SF_C_TYPE_TIMESTAMP:
        // TODO Add timestamp case
    case SF_C_TYPE_NULL:
    default:
        return 0;
    }
}

/**
 * @param index The index for array binding. SF_BIND_ALL(-1) for single binding.
 * @return single parameter binding value in cJSON.
 */
#define SF_BIND_ALL -1
static cJSON* get_single_binding_value_json(SF_BIND_INPUT* input, int64 index)
{
    char* value = NULL;
    cJSON* single_val = NULL;
    int64 val_len = 0;
    size_t step = 0;
    void* val_ptr = NULL;

    if (index < 0)
    {
        value = value_to_string(input->value, input->len, input->c_type);
        single_val = snowflake_cJSON_CreateString(value);
        SF_FREE(value);
    }
    else
    {
        val_len = (int64)input->len;
        step = _snowflake_get_binding_value_size(input);
        val_ptr = (char*)(input->value) + step * index;
        if (input->len_ind)
        {
            val_len = input->len_ind[index];
        }

        if (SF_BIND_LEN_NULL == val_len)
        {
            single_val = snowflake_cJSON_CreateNull();
        }
        else
        {
            if ((SF_C_TYPE_STRING == input->c_type) &&
                (SF_BIND_LEN_NTS == val_len))
            {
                val_len = (int64)strlen((char*)val_ptr);
            }

            value = value_to_string(val_ptr, val_len, input->c_type);
            single_val = snowflake_cJSON_CreateString(value);
            SF_FREE(value);
        }
    }

    return single_val;
}

/**
 * @param sfstmt SNOWFLAKE_STMT context.
 * @param index The parameter set index (for batch execution), -1 to return all
 *        parameter sets (non-batch execution)
 * @return parameter bindings in cJSON.
 */
cJSON* STDCALL _snowflake_get_binding_json(SF_STMT* sfstmt, int64 index)
{
    size_t i;
    SF_BIND_INPUT* input;
    const char* type;
    char name_buf[SF_PARAM_NAME_BUF_LEN];
    char* name = NULL;
    cJSON* bindings = NULL;

    if (_snowflake_get_current_param_style(sfstmt) == INVALID_PARAM_TYPE)
    {
      return NULL;
    }
    bindings = snowflake_cJSON_CreateObject();
    for (i = 0; i < sfstmt->params_len; i++)
    {
        cJSON* binding = NULL;
        cJSON* single_val = NULL;

        input = _snowflake_get_binding_by_index(sfstmt, i, &name,
                                                name_buf, SF_PARAM_NAME_BUF_LEN);
        if (input == NULL)
        {
            log_error("_snowflake_execute_ex: No parameter by this name %s", name);
            continue;
        }
        binding = snowflake_cJSON_CreateObject();
        type = snowflake_type_to_string(
            c_type_to_snowflake(input->c_type, SF_DB_TYPE_TIMESTAMP_NTZ));
        // json array for all parameter sets
        if ((sfstmt->paramset_size > 1) && (index < 0))
        {
            cJSON* val_array = snowflake_cJSON_CreateArray();
            // get all parameter sets, use array
            for (int64 j = 0; j < sfstmt->paramset_size; j++)
            {
                single_val = get_single_binding_value_json(input, j);
                snowflake_cJSON_AddItemToArray(val_array, single_val);
            }
            snowflake_cJSON_AddItemToObject(binding, "value", val_array);
        }
        else // single value binding
        {
            single_val = get_single_binding_value_json(input, index);
            snowflake_cJSON_AddItemToObject(binding, "value", single_val);
        }
        snowflake_cJSON_AddStringToObject(binding, "type", type);
        snowflake_cJSON_AddItemToObject(bindings, name, binding);
    }

    return bindings;
}

sf_bool STDCALL _snowflake_needs_stage_binding(SF_STMT* sfstmt)
{
    if (!sfstmt || !sfstmt->connection ||
        (_snowflake_get_current_param_style(sfstmt) == INVALID_PARAM_TYPE) ||
        sfstmt->connection->stage_binding_disabled ||
        sfstmt->paramset_size <= 1 || !sfstmt->array_bind_supported)
    {
        return SF_BOOLEAN_FALSE;
    }

    if (sfstmt->paramset_size * sfstmt->params_len >= sfstmt->connection->stage_binding_threshold)
    {
        return SF_BOOLEAN_TRUE;
    }
    return SF_BOOLEAN_FALSE;
}
/**
 * Resets SNOWFLAKE_STMT parameters.
 *
 * @param sfstmt
 */
static void STDCALL _snowflake_stmt_reset(SF_STMT *sfstmt) {

    clear_snowflake_error(&sfstmt->error);

    sf_strncpy(sfstmt->sfqid, SF_UUID4_LEN, "", sizeof(""));
    sfstmt->request_id[0] = '\0';

    if (sfstmt->sql_text) {
        SF_FREE(sfstmt->sql_text); /* SQL */
    }
    sfstmt->sql_text = NULL;

    if (sfstmt->result_set) {
        rs_destroy(sfstmt->result_set);
    }
    sfstmt->result_set = NULL;

    sfstmt->qrf = SF_FORMAT_UNKNOWN;
    sfstmt->is_dml = SF_BOOLEAN_FALSE;
    sfstmt->is_multi_stmt = SF_BOOLEAN_FALSE;
    if (sfstmt->multi_stmt_result_ids)
    {
        snowflake_cJSON_Delete(sfstmt->multi_stmt_result_ids);
    }
    sfstmt->multi_stmt_result_ids = NULL;

    if (_snowflake_get_current_param_style(sfstmt) == NAMED)
    {
        _snowflake_deallocate_named_param_list(sfstmt->name_list);
    }

    if (sfstmt->params) {
        sf_param_store_deallocate(sfstmt->params);
    }
    sfstmt->params = NULL;
    sfstmt->params_len = 0;
    sfstmt->name_list = NULL;
    sfstmt->array_bind_supported = SF_BOOLEAN_FALSE;
    sfstmt->affected_rows = -1;

    _snowflake_stmt_desc_reset(sfstmt);

    if (sfstmt->stmt_attrs) {
        sf_array_list_deallocate(sfstmt->stmt_attrs);
    }
    sfstmt->stmt_attrs = NULL;

    /* clear error handle */
    clear_snowflake_error(&sfstmt->error);

    sfstmt->chunk_rowcount = -1;
    sfstmt->total_rowcount = -1;
    sfstmt->total_fieldcount = -1;
    sfstmt->total_row_index = -1;

    // Destroy chunk downloader
    chunk_downloader_term(sfstmt->chunk_downloader);
    sfstmt->chunk_downloader = NULL;

    if (sfstmt->put_get_response) {
        // clean up put get response data
        sf_put_get_response_deallocate(sfstmt->put_get_response);
        sfstmt->put_get_response = NULL;
    }
}

SF_PUT_GET_RESPONSE *STDCALL sf_put_get_response_allocate() {
    SF_PUT_GET_RESPONSE *sf_put_get_response = (SF_PUT_GET_RESPONSE *)
        SF_CALLOC(1, sizeof(SF_PUT_GET_RESPONSE));

    SF_STAGE_CRED *sf_stage_cred = (SF_STAGE_CRED *) SF_CALLOC(1,
                                                               sizeof(SF_STAGE_CRED));

    SF_STAGE_INFO *sf_stage_info = (SF_STAGE_INFO *) SF_CALLOC(1,
                                                               sizeof(SF_STAGE_INFO));

    SF_ENC_MAT *sf_enc_mat = (SF_ENC_MAT *) SF_CALLOC(1,
                                                      sizeof(SF_ENC_MAT));

    sf_stage_info->stage_cred = sf_stage_cred;
    sf_put_get_response->stage_info = sf_stage_info;
    sf_put_get_response->enc_mat_put = sf_enc_mat;

    return sf_put_get_response;
}

void STDCALL
sf_put_get_response_deallocate(SF_PUT_GET_RESPONSE *put_get_response) {
    SF_FREE(put_get_response->stage_info->stage_cred->aws_key_id);
    SF_FREE(put_get_response->stage_info->stage_cred->aws_secret_key);
    SF_FREE(put_get_response->stage_info->stage_cred->aws_token);
    SF_FREE(put_get_response->stage_info->stage_cred);
    SF_FREE(put_get_response->stage_info->location_type);
    SF_FREE(put_get_response->stage_info->location);
    SF_FREE(put_get_response->stage_info->path);
    SF_FREE(put_get_response->stage_info->region);
    SF_FREE(put_get_response->stage_info);
    SF_FREE(put_get_response->enc_mat_put->query_stage_master_key);
    SF_FREE(put_get_response->enc_mat_put);
    SF_FREE(put_get_response->localLocation);

    snowflake_cJSON_Delete((cJSON *) put_get_response->src_list);
    snowflake_cJSON_Delete((cJSON *) put_get_response->enc_mat_get);

    SF_FREE(put_get_response);
}


SF_STMT *STDCALL snowflake_stmt(SF_CONNECT *sf) {
    if (!sf) {
        return NULL;
    }

    SF_STMT *sfstmt = (SF_STMT *) SF_CALLOC(1, sizeof(SF_STMT));
    if (sfstmt) {
        _snowflake_stmt_reset(sfstmt);
        sfstmt->connection = sf;
        sfstmt->multi_stmt_count = SF_MULTI_STMT_COUNT_UNSET;
        // single value binding by default
        sfstmt->paramset_size = 1;
        sfstmt->affected_rows = -1;
    }
    return sfstmt;
}

SF_STMT *STDCALL snowflake_init_async_query_result(SF_CONNECT *sf, const char *query_id) {
  if (!sf) {
    return NULL;
  }

  SF_STMT *sfstmt = (SF_STMT *)SF_CALLOC(1, sizeof(SF_STMT));
  if (sfstmt) {
    _snowflake_stmt_reset(sfstmt);
    sfstmt->connection = sf;
    sf_strcpy(sfstmt->sfqid, SF_UUID4_LEN, query_id);
    sfstmt->is_async = SF_BOOLEAN_TRUE;
    sfstmt->is_async_results_fetched = SF_BOOLEAN_FALSE;
  }

  return sfstmt;
}

/**
 * Initializes an SF_QUERY_RESPONSE_CAPTURE struct.
 * Note that these need to be released by calling snowflake_query_result_capture_term().
 *
 * @param input pointer to an uninitialized SF_QUERY_RESULT_CAPTURE struct pointer.
 */
void STDCALL snowflake_query_result_capture_init(SF_QUERY_RESULT_CAPTURE **init) {

    SF_QUERY_RESULT_CAPTURE *capture = (SF_QUERY_RESULT_CAPTURE *) SF_CALLOC(1, sizeof(SF_QUERY_RESULT_CAPTURE));

    capture->capture_buffer = NULL;
    capture->actual_response_size = 0;

    *init = capture;
}

/**
 * Leave the memory management of the bind input to the user
 * The user will allocate memory for the SF_BIND_INPUT and
 * pass it to the init function for sane initialization
 *
 * @param bind input
 */
void STDCALL snowflake_bind_input_init(SF_BIND_INPUT * input)
{
    if (!input)
    {
        log_error("snowflake_bind_input: bad param. user is supposed to allocate memory for input\n");
        return;
    }
    input->idx = 0;
    input->name = NULL;
    input->value = NULL;
    input->len_ind = NULL;
}

/**
 * Frees the memory used by a SF_QUERY_RESULT_CAPTURE struct.
 * Note that this only frees the struct itself, and *not* the underlying
 * capture buffer! The caller is responsible for managing that.
 *
 * @param capture SF_QUERY_RESULT_CAPTURE pointer whose memory to clear.
 *
 */
void STDCALL snowflake_query_result_capture_term(SF_QUERY_RESULT_CAPTURE *capture) {
    if (capture) {
        SF_FREE(capture->capture_buffer);
        SF_FREE(capture);
    }
}

void STDCALL snowflake_stmt_term(SF_STMT *sfstmt) {
    if (sfstmt) {
        _snowflake_stmt_reset(sfstmt);
        SF_FREE(sfstmt);
    }
}

SF_STATUS STDCALL snowflake_bind_param(
    SF_STMT *sfstmt, SF_BIND_INPUT *sfbind) {

    SF_INT_RET_CODE retcode;
    if (!sfstmt) {
        return SF_STATUS_ERROR_STATEMENT_NOT_EXIST;
    }
    clear_snowflake_error(&sfstmt->error);

    if (sfstmt->params == NULL)
    {
        sf_param_store_init(_snowflake_get_param_style(sfbind),
                            &sfstmt->params);

        if (_snowflake_get_current_param_style(sfstmt) == NAMED)
        {
            _snowflake_allocate_named_param_list(&sfstmt->name_list);
        }
    }

    retcode = sf_param_store_set(sfstmt->params, sfbind, sfbind->idx, sfbind->name);
    if (retcode == SF_INT_RET_CODE_DUPLICATES)
    {
        return SF_STATUS_SUCCESS;
    }
    else if (retcode == SF_INT_RET_CODE_ERROR)
    {
        return SF_STATUS_ERROR_OTHER;
    }
    /*
     * For named parameters, populate the name list in the
     * sfstmt to help extract the params from the treemap
     * during execute
     */
    if (_snowflake_get_current_param_style(sfstmt) == NAMED)
    {
        _snowflake_add_to_named_param_list(sfstmt->name_list,
                sfbind->name, sfstmt->params_len);
    }

    sfstmt->params_len += 1;

    return SF_STATUS_SUCCESS;
}

SF_STATUS snowflake_bind_param_array(
    SF_STMT *sfstmt, SF_BIND_INPUT *sfbind_array, size_t size) {
    size_t i;
    SF_INT_RET_CODE retcode = SF_INT_RET_CODE_ERROR;

    if (!sfstmt) {
        return SF_STATUS_ERROR_STATEMENT_NOT_EXIST;
    }
    clear_snowflake_error(&sfstmt->error);
    if (sfstmt->params == NULL)
    {
        sf_param_store_init(_snowflake_get_param_style(&sfbind_array[0]),
                            &sfstmt->params);

        if (_snowflake_get_current_param_style(sfstmt) == NAMED)
        {
            _snowflake_allocate_named_param_list(&sfstmt->name_list);
        }
    }

    for (i = 0; i < size; i++)
    {
        retcode = sf_param_store_set(sfstmt->params, &sfbind_array[i],
                sfbind_array[i].idx, sfbind_array[i].name);

        if (retcode == SF_INT_RET_CODE_DUPLICATES)
        {
            continue;
        }
        else if (retcode == SF_INT_RET_CODE_ERROR)
        {
            return SF_STATUS_ERROR_OTHER;
        }

        /*
         * For named parameters, populate the name list in the
         * sfstmt to help extract the params from the treemap
         * during execute
         */
        if (_snowflake_get_current_param_style(sfstmt) == NAMED)
        {
            _snowflake_add_to_named_param_list(sfstmt->name_list,
                    sfbind_array[i].name, sfstmt->params_len);
        }

        sfstmt->params_len += 1;
    }

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL snowflake_query(
    SF_STMT *sfstmt, const char *command, size_t command_size) {
    if (!sfstmt) {
        return SF_STATUS_ERROR_STATEMENT_NOT_EXIST;
    }
    clear_snowflake_error(&sfstmt->error);
    SF_STATUS ret = snowflake_prepare(sfstmt, command, command_size);
    if (ret != SF_STATUS_SUCCESS) {
        return ret;
    }
    ret = snowflake_execute(sfstmt);
    if (ret != SF_STATUS_SUCCESS) {
        return ret;
    }
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL _snowflake_query_put_get_legacy(
    SF_STMT *sfstmt, const char *command, size_t command_size) {
    if (!sfstmt) {
        return SF_STATUS_ERROR_STATEMENT_NOT_EXIST;
    }
    clear_snowflake_error(&sfstmt->error);
    SF_STATUS ret = snowflake_prepare(sfstmt, command, command_size);
    if (ret != SF_STATUS_SUCCESS) {
        return ret;
    }
    if (!_is_put_get_command(sfstmt->sql_text))
    {
        // this should never happen as this function should only be
        // called internally for put/get command.
        SET_SNOWFLAKE_ERROR(&sfstmt->error, SF_STATUS_ERROR_GENERAL,
                            "Invalid query type, can be used for put get only",
                            SF_SQLSTATE_GENERAL_ERROR);
        return SF_STATUS_ERROR_GENERAL;
    }

    return _snowflake_execute_ex(sfstmt, SF_BOOLEAN_TRUE, SF_BOOLEAN_FALSE, NULL, SF_BOOLEAN_FALSE, SF_BOOLEAN_FALSE);
}

SF_STATUS STDCALL snowflake_fetch(SF_STMT *sfstmt) {
    if (!sfstmt) {
        return SF_STATUS_ERROR_STATEMENT_NOT_EXIST;
    }

    if (sfstmt->is_async && !sfstmt->is_async_results_fetched) {
      if (!get_real_results(sfstmt)) {
        return SF_STATUS_ERROR_GENERAL;
      }
    }

    clear_snowflake_error(&sfstmt->error);
    SF_STATUS ret = SF_STATUS_ERROR_GENERAL;
    sf_bool get_chunk_success = SF_BOOLEAN_TRUE;
    uint64 index;

    // Check for chunk_downloader error
    if (sfstmt->chunk_downloader && get_error(sfstmt->chunk_downloader)) {
        goto cleanup;
    }

    // If no more results, set return to SF_STATUS_EOF
    if (sfstmt->chunk_rowcount == 0) {
        if (sfstmt->chunk_downloader) {
            log_debug("Fetching next chunk from chunk downloader.");
            _critical_section_lock(&sfstmt->chunk_downloader->queue_lock);
            do {
                if (sfstmt->chunk_downloader->consumer_head >=
                    sfstmt->chunk_downloader->queue_size) {
                    // No more chunks, set EOL and break
                    log_debug("Out of chunks, setting EOL.");
                    ret = SF_STATUS_EOF;
                    break;
                } else {
                    // Get index and increment
                    index = sfstmt->chunk_downloader->consumer_head;
                    while (
                        sfstmt->chunk_downloader->queue[index].chunk == NULL &&
                        !get_shutdown_or_error(
                            sfstmt->chunk_downloader)) {
                        _cond_wait(
                            &sfstmt->chunk_downloader->consumer_cond,
                            &sfstmt->chunk_downloader->queue_lock);
                    }

                    if (get_error(sfstmt->chunk_downloader)) {
                        get_chunk_success = SF_BOOLEAN_FALSE;
                        break;
                    } else if (get_shutdown(sfstmt->chunk_downloader)) {
                        get_chunk_success = SF_BOOLEAN_FALSE;
                        break;
                    }

                    sfstmt->chunk_downloader->consumer_head++;

                    // Set the new chunk, which will internally free the previous chunk appended.
                    // If result set object doesn't exist yet, then create it.
                    if (sfstmt->result_set == NULL) {
                        sfstmt->result_set = rs_create_with_chunk(
                            sfstmt->chunk_downloader->queue[index].chunk,
                            sfstmt->desc,
                            sfstmt->qrf,
                            sfstmt->connection->timezone);
                    } else {
                        rs_append_chunk(
                            sfstmt->result_set,
                            sfstmt->chunk_downloader->queue[index].chunk);
                    }

                    // Remove chunk reference from locked array
                    sfstmt->chunk_downloader->queue[index].chunk = NULL;
                    sfstmt->chunk_rowcount = sfstmt->chunk_downloader->queue[index].row_count;
                    log_debug("Acquired chunk %llu from chunk downloader",
                              index);
                    if (_cond_signal(
                        &sfstmt->chunk_downloader->producer_cond)) {
                        SET_SNOWFLAKE_ERROR(&sfstmt->error,
                                            SF_STATUS_ERROR_PTHREAD,
                                            "Unable to send signal using produce_cond",
                                            "");
                        get_chunk_success = SF_BOOLEAN_FALSE;
                        break;
                    }
                }
            }
            while (0);
            _critical_section_unlock(&sfstmt->chunk_downloader->queue_lock);
        } else {
            // If there is no chunk downloader set, then we've truly reached the end of the results and should set EOL
            log_debug("No chunk downloader set, end of results.");
            ret = SF_STATUS_EOF;
        }

        // If we've reached the end, or we have an error getting the next chunk, goto cleanup and return status
        if (ret == SF_STATUS_EOF || !get_chunk_success) {
            goto cleanup;
        }
    }

    // Get next result row
    _snowflake_next(sfstmt);
    sfstmt->chunk_rowcount--;
    sfstmt->total_row_index++;
    ret = SF_STATUS_SUCCESS;

cleanup:
    return ret;
}

static SF_STATUS STDCALL
_snowflake_internal_query(SF_CONNECT *sf, const char *sql) {
    if (!sf) {
        return SF_STATUS_ERROR_CONNECTION_NOT_EXIST;
    }
    SF_STATUS ret;
    SF_STMT *sfstmt = snowflake_stmt(sf);
    if (sfstmt == NULL) {
        ret = SF_STATUS_ERROR_OUT_OF_MEMORY;
        SET_SNOWFLAKE_ERROR(
            &sf->error,
            SF_STATUS_ERROR_OUT_OF_MEMORY,
            "Out of memory in creating SF_STMT. ",
            SF_SQLSTATE_UNABLE_TO_CONNECT);
        goto error;
    }
    ret = snowflake_query(sfstmt, sql, 0);
    if (ret != SF_STATUS_SUCCESS) {
        snowflake_propagate_error(sf, sfstmt);
        goto error;
    }

error:
    snowflake_stmt_term(sfstmt);
    return ret;
}

SF_STATUS STDCALL snowflake_trans_begin(SF_CONNECT *sf) {
    return _snowflake_internal_query(sf, _SF_STMT_SQL_BEGIN);
}

SF_STATUS STDCALL snowflake_trans_commit(SF_CONNECT *sf) {
    return _snowflake_internal_query(sf, _SF_STMT_SQL_COMMIT);
}

SF_STATUS STDCALL snowflake_trans_rollback(SF_CONNECT *sf) {
    return _snowflake_internal_query(sf, _SF_STMT_SQL_ROLLBACK);
}

int64 STDCALL snowflake_affected_rows(SF_STMT *sfstmt) {
    size_t i;
    int64 ret = -1;
    cJSON *row = NULL;
    cJSON *raw_row_result;
    clear_snowflake_error(&sfstmt->error);
    if (!sfstmt) {
        /* no way to set the error other than return value */
        return ret;
    }

    if (sfstmt->is_dml == SF_BOOLEAN_TRUE) {
        if (sfstmt->affected_rows >= 0)
        {
            // use the value initialized previously from batch execution
            return sfstmt->affected_rows;
        }
        if (SF_STATUS_SUCCESS != snowflake_fetch(sfstmt))
        {
            return -1;
        }
        int64 total = 0;
        for (int i = 1; i <= sfstmt->total_fieldcount; ++i) {
            int64 field = 0;
            if (SF_STATUS_SUCCESS != snowflake_column_as_int64(sfstmt, i, &field))
            {
                return -1;
            }
            total += field;
        }
        return total;
    }
    else {
        return sfstmt->total_rowcount;
    }
}

SF_STATUS STDCALL
snowflake_prepare(SF_STMT *sfstmt, const char *command, size_t command_size) {
    if (!sfstmt) {
        return SF_STATUS_ERROR_STATEMENT_NOT_EXIST;
    }
    clear_snowflake_error(&sfstmt->error);
    SF_STATUS ret = SF_STATUS_ERROR_GENERAL;
    size_t sql_text_size = 1; // Don't forget about null terminator
    if (!command) {
        goto cleanup;
    }
    _snowflake_stmt_reset(sfstmt);
    // Set sql_text to command
    if (command_size == 0) {
        log_debug("Command size is 0, using to strlen to find query length.");
        sql_text_size += strlen(command);
    } else {
        log_debug("Command size non-zero, setting as sql text size.");
        sql_text_size += command_size;
    }
    sfstmt->sql_text = (char *) SF_CALLOC(1, sql_text_size);
    sf_memcpy(sfstmt->sql_text, sql_text_size, command, sql_text_size - 1);
    // Null terminate
    sfstmt->sql_text[sql_text_size - 1] = '\0';

    ret = SF_STATUS_SUCCESS;

cleanup:
    return ret;
}

SF_STATUS STDCALL snowflake_describe_with_capture(SF_STMT *sfstmt,
                                                  SF_QUERY_RESULT_CAPTURE *result_capture) {
    return _snowflake_execute_ex(sfstmt, _is_put_get_command(sfstmt->sql_text), SF_BOOLEAN_FALSE, result_capture, SF_BOOLEAN_TRUE, SF_BOOLEAN_FALSE);
}

SF_STATUS STDCALL snowflake_execute(SF_STMT *sfstmt) {
    return _snowflake_execute_ex(sfstmt, _is_put_get_command(sfstmt->sql_text), SF_BOOLEAN_TRUE, NULL, SF_BOOLEAN_FALSE, SF_BOOLEAN_FALSE);
}

SF_STATUS STDCALL snowflake_async_execute(SF_STMT *sfstmt) {
  if (!sfstmt->is_async) {
    sfstmt->is_async = SF_BOOLEAN_TRUE;
  }
  return _snowflake_execute_ex(sfstmt, _is_put_get_command(sfstmt->sql_text), SF_BOOLEAN_TRUE, NULL, SF_BOOLEAN_FALSE, SF_BOOLEAN_TRUE);
}

SF_STATUS STDCALL snowflake_execute_with_capture(SF_STMT *sfstmt, SF_QUERY_RESULT_CAPTURE *result_capture) {
    return _snowflake_execute_ex(sfstmt, _is_put_get_command(sfstmt->sql_text), SF_BOOLEAN_TRUE, result_capture, SF_BOOLEAN_FALSE, SF_BOOLEAN_FALSE);
}

static SF_STATUS _snowflake_execute_with_binds_ex(SF_STMT* sfstmt,
                                                  sf_bool is_put_get_command,
                                                  SF_QUERY_RESULT_CAPTURE* result_capture,
                                                  sf_bool is_describe_only,
                                                  char* bind_stage,
                                                  cJSON* bindings,
                                                  sf_bool is_async_exec)
{
    SF_STATUS ret = SF_STATUS_ERROR_GENERAL;
    SF_JSON_ERROR json_error;
    const char *error_msg;
    cJSON *body = NULL;
    cJSON *data = NULL;
    cJSON *resp = NULL;
    char *s_body = NULL;
    char *s_resp = NULL;
    sf_bool success = SF_BOOLEAN_FALSE;
    uuid4_generate(sfstmt->request_id);
    URL_KEY_VALUE url_params[] = {
            {.key="requestId=", .value=sfstmt->request_id, .formatted_key=NULL, .formatted_value=NULL, .key_size=0, .value_size=0}
    };

    if (is_string_empty(sfstmt->connection->directURL) &&
        (is_string_empty(sfstmt->connection->master_token) ||
         is_string_empty(sfstmt->connection->token))) {
        log_error(
            "Missing session token or Master token. Are you sure that snowflake_connect was successful?");
        SET_SNOWFLAKE_ERROR(&sfstmt->error,
                            SF_STATUS_ERROR_BAD_CONNECTION_PARAMS,
                            "Missing session or master token. Try running snowflake_connect.",
                            SF_SQLSTATE_UNABLE_TO_CONNECT);
        goto cleanup;
    }

    // Create Body
    body = create_query_json_body(sfstmt->sql_text, sfstmt->sequence_counter,
                                  is_string_empty(sfstmt->connection->directURL) ?
                                  NULL : sfstmt->request_id, is_describe_only,
                                  sfstmt->multi_stmt_count);

    snowflake_cJSON_AddBoolToObject(body, "asyncExec", is_async_exec);

    if (bind_stage)
    {
      snowflake_cJSON_AddStringToObject(body, "bindStage", bind_stage);
      SF_FREE(bind_stage);
    }
    else if (bindings != NULL) {
        /* binding parameters if exists */
      snowflake_cJSON_AddItemToObject(body, "bindings", bindings);
    }

    // send query context cache in query request
    cJSON * qcc = qcc_serialize(sfstmt->connection);
    if (qcc != NULL)
    {
      snowflake_cJSON_AddItemToObject(body, SF_QCC_REQ_KEY, qcc);
    }

    s_body = snowflake_cJSON_Print(body);
    log_debug("Created body");
    log_trace("Here is constructed body:\n%s", s_body);

    char* queryURL = is_string_empty(sfstmt->connection->directURL) ?
                     QUERY_URL : sfstmt->connection->directURL;
    int url_paramSize = is_string_empty(sfstmt->connection->directURL) ?
                        sizeof(url_params) / sizeof(URL_KEY_VALUE) : 0;
    if (request(sfstmt->connection, &resp, queryURL, url_params,
                url_paramSize , s_body, NULL,
                POST_REQUEST_TYPE, &sfstmt->error, is_put_get_command,
                0, sfstmt->connection->retry_count, get_retry_timeout(sfstmt->connection),
                NULL, NULL, NULL, SF_BOOLEAN_FALSE)) {
        // s_resp will be freed by snowflake_query_result_capture_term
        s_resp = snowflake_cJSON_Print(resp);
        log_trace("Here is JSON response:\n%s", s_resp);

        // Store the full query-response text in the capture buffer, if defined.
        if (result_capture != NULL) {
            result_capture->capture_buffer = s_resp;
            result_capture->actual_response_size = strlen(s_resp) + 1;
        }

        data = snowflake_cJSON_GetObjectItem(resp, "data");

        if (json_copy_string_no_alloc(sfstmt->sfqid, data, "queryId",
                                      SF_UUID4_LEN) && !is_put_get_command) {
            log_debug("No valid sfqid found in response");
        }
        if ((json_error = json_copy_bool(&success, resp, "success")) ==
            SF_JSON_ERROR_NONE && success) {
            if (is_put_get_command) {
                sfstmt->put_get_response = sf_put_get_response_allocate();

                json_detach_array_from_object(
                    (cJSON **) (&sfstmt->put_get_response->src_list),
                    data, "src_locations");
                json_copy_string_no_alloc(sfstmt->put_get_response->command,
                                          data, "command", SF_COMMAND_LEN);
                json_copy_int(&sfstmt->put_get_response->parallel, data,
                              "parallel");
                if (sf_strncasecmp(sfstmt->put_get_response->command, "UPLOAD", 6) == 0)
                {
                  json_copy_int(&sfstmt->put_get_response->threshold, data,
                                "threshold");
                }
                json_copy_bool(&sfstmt->put_get_response->auto_compress, data,
                               "autoCompress");
                json_copy_bool(&sfstmt->put_get_response->overwrite, data,
                               "overwrite");
                json_copy_string_no_alloc(
                    sfstmt->put_get_response->source_compression,
                    data, "sourceCompression",
                    SF_SOURCE_COMPRESSION_TYPE_LEN);
                json_copy_bool(
                    &sfstmt->put_get_response->client_show_encryption_param,
                    data, "clientShowEncryptionParameter");

                cJSON *stage_info = snowflake_cJSON_GetObjectItem(data,
                                                                  "stageInfo");
                sf_bool isClientSideEncrypted = SF_BOOLEAN_TRUE;
                json_copy_bool(&isClientSideEncrypted,
                               stage_info, "isClientSideEncrypted");

                cJSON *enc_mat = snowflake_cJSON_GetObjectItem(data,
                                                               "encryptionMaterial");
                if (SF_BOOLEAN_TRUE == isClientSideEncrypted) {
                    // In put command response, value of encryptionMaterial is an
                    // object, which in get command response, value is an array of
                    // object since different remote files might have different
                    // encryption material
                    if (snowflake_cJSON_IsArray(enc_mat)) {
                        json_detach_array_from_object(
                          (cJSON **) (&sfstmt->put_get_response->enc_mat_get),
                          data, "encryptionMaterial");
                    } else if (snowflake_cJSON_IsObject(enc_mat)) {
                        json_copy_string(
                          &sfstmt->put_get_response->enc_mat_put->query_stage_master_key,
                          enc_mat, "queryStageMasterKey");
                        json_copy_string_no_alloc(
                          sfstmt->put_get_response->enc_mat_put->query_id,
                          enc_mat, "queryId", SF_UUID4_LEN);
                        json_copy_int(&sfstmt->put_get_response->enc_mat_put->smk_id,
                                      enc_mat, "smkId");
                    }
                }

                cJSON *stage_cred = snowflake_cJSON_GetObjectItem(stage_info,
                                                                  "creds");
                json_copy_string(
                    &sfstmt->put_get_response->stage_info->location_type,
                    stage_info, "locationType");
                json_copy_string(
                    &sfstmt->put_get_response->stage_info->location,
                    stage_info, "location");
                json_copy_string(&sfstmt->put_get_response->stage_info->path,
                                 stage_info, "path");
                json_copy_string(&sfstmt->put_get_response->stage_info->region,
                                 stage_info, "region");
                json_copy_string(&sfstmt->put_get_response->stage_info->storageAccount,
                                 stage_info, "storageAccount");
                json_copy_string(&sfstmt->put_get_response->stage_info->endPoint,
                                 stage_info, "endPoint");
                sf_bool useS3RegionalUrl = SF_BOOLEAN_FALSE;
                json_copy_bool(&useS3RegionalUrl, stage_info, "useS3RegionalUrl");
                sfstmt->put_get_response->stage_info->useS3RegionalUrl = useS3RegionalUrl;
                sf_bool useRegionalURL = SF_BOOLEAN_FALSE;
                json_copy_bool(&useRegionalURL, stage_info, "useRegionalUrl");
                sfstmt->put_get_response->stage_info->useRegionalUrl = useRegionalURL;
                json_copy_string(
                    &sfstmt->put_get_response->stage_info->stage_cred->aws_secret_key,
                    stage_cred, "AWS_SECRET_KEY");
                json_copy_string(
                    &sfstmt->put_get_response->stage_info->stage_cred->aws_key_id,
                    stage_cred, "AWS_KEY_ID");
                json_copy_string(
                    &sfstmt->put_get_response->stage_info->stage_cred->aws_token,
                    stage_cred, "AWS_TOKEN");
                json_copy_string(
                        &sfstmt->put_get_response->stage_info->stage_cred->azure_sas_token,
                        stage_cred, "AZURE_SAS_TOKEN");
                json_copy_string(
                        &sfstmt->put_get_response->stage_info->stage_cred->gcs_access_token,
                        stage_cred, "GCS_ACCESS_TOKEN");
                json_copy_string(
                    &sfstmt->put_get_response->localLocation, data,
                    "localLocation");
            } else {
                int64 stmt_type_id;
                if (json_copy_int(&stmt_type_id, data, "statementTypeId")) {
                  /* failed to get statement type id */
                  sfstmt->is_multi_stmt = SF_BOOLEAN_FALSE;
                }
                else {
                  sfstmt->is_multi_stmt = (_SF_STMT_TYPE_MULTI_STMT == stmt_type_id);
                }

                if ((sfstmt->is_multi_stmt) && (!is_describe_only))
                {
                    if (!setup_multi_stmt_result(sfstmt, data))
                    {
                        goto cleanup;
                    }
                }
                else
                {
                    if (!setup_result_with_json_resp(sfstmt, data))
                    {
                        goto cleanup;
                    }
                }
            }
        } else if (json_error != SF_JSON_ERROR_NONE) {
            JSON_ERROR_MSG(json_error, error_msg, "Success code");
            SET_SNOWFLAKE_STMT_ERROR(
                &sfstmt->error, SF_STATUS_ERROR_BAD_JSON,
                error_msg, SF_SQLSTATE_APP_REJECT_CONNECTION, sfstmt->sfqid);
            goto cleanup;
        } else if (!success) {
            cJSON *messageJson = NULL;
            char *message = NULL;
            cJSON *codeJson = NULL;
            int64 code = -1;
            if (json_copy_string_no_alloc(sfstmt->error.sqlstate, data,
                                          "sqlState", SF_SQLSTATE_LEN)) {
                log_debug("No valid sqlstate found in response");
            }
            messageJson = snowflake_cJSON_GetObjectItem(resp, "message");
            if (messageJson) {
                message = messageJson->valuestring;
            }
            codeJson = snowflake_cJSON_GetObjectItem(resp, "code");
            if (codeJson) {
                code = (int64) strtol(codeJson->valuestring, NULL, 10);
            } else {
                log_debug("no code element.");
            }
            SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, code,
                                     message ? message
                                             : "Query was not successful",
                                     NULL, sfstmt->sfqid);
            goto cleanup;
        }
    } else {
        log_trace("Connection failed");
        // Set the return status to the error code
        // that we got from the connection layer
        ret = sfstmt->error.error_code;
        goto cleanup;
    }

    // Everything went well if we got to this point
    ret = SF_STATUS_SUCCESS;

cleanup:
    snowflake_cJSON_Delete(body);
    snowflake_cJSON_Delete(resp);
    SF_FREE(s_body);
    if (result_capture == NULL) {
        // If no result capture, we always free s_resp
        SF_FREE(s_resp);
    }
    // Caller should always call result_capture_term to free s_resp,
    // if result_capture is not NULL

    return ret;
}

static SF_STATUS _batch_dml_execute(SF_STMT* sfstmt,
                                    SF_QUERY_RESULT_CAPTURE* result_capture)
{
    SF_STATUS ret = SF_STATUS_ERROR_GENERAL;
    cJSON* bindings = NULL;
    int64 affected_rows = 0;

    // impossible but just in case
    if (!sfstmt->is_dml)
    {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error,
            SF_STATUS_ERROR_GENERAL,
            "Internal error. _batch_dml_execute is called for non-dml query.",
            SF_SQLSTATE_GENERAL_ERROR,
            sfstmt->sfqid);
        return SF_STATUS_ERROR_GENERAL;
    }
    sfstmt->affected_rows = -1;
    for (int64 i = 0; i < sfstmt->paramset_size; i++)
    {
        bindings = _snowflake_get_binding_json(sfstmt, i);
        ret = _snowflake_execute_with_binds_ex(sfstmt,
                                               SF_BOOLEAN_FALSE,
                                               result_capture,
                                               SF_BOOLEAN_FALSE,
                                               NULL,
                                               bindings,
                                               SF_BOOLEAN_FALSE);
        if (ret != SF_STATUS_SUCCESS)
        {
            return ret;
        }
        affected_rows += snowflake_affected_rows(sfstmt);
    }

    sfstmt->affected_rows = affected_rows;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL _snowflake_execute_ex(SF_STMT *sfstmt,
                                        sf_bool is_put_get_command,
                                        sf_bool is_native_put_get,
                                        SF_QUERY_RESULT_CAPTURE* result_capture,
                                        sf_bool is_describe_only,
                                        sf_bool is_async_exec) {
    SF_STATUS ret = SF_STATUS_ERROR_GENERAL;
    cJSON* bindings = NULL;
    char* bind_stage = NULL;

    if (!sfstmt) {
        return SF_STATUS_ERROR_STATEMENT_NOT_EXIST;
    }

    if (is_put_get_command && is_native_put_get && !is_describe_only)
    {
        _snowflake_stmt_desc_reset(sfstmt);
        return _snowflake_execute_put_get_native(sfstmt, NULL, 0, 0, result_capture);
    }

    clear_snowflake_error(&sfstmt->error);

    _mutex_lock(&sfstmt->connection->mutex_sequence_counter);
    sfstmt->sequence_counter = ++sfstmt->connection->sequence_counter;
    _mutex_unlock(&sfstmt->connection->mutex_sequence_counter);

    // batch execution needed when application using array binding for DML
    // queries and in some cases that's not supported by server:
    // paramset_size > 1 && arrayBindSupported = false
    // fallback to execute the query with each parmeter set in such case
    sf_bool need_batch_exec = SF_BOOLEAN_FALSE;

    // describe request only returns metadata and doesn't need bindings
    if (!is_describe_only)
    {
        // for parameter array bindings, send describe request first to
        // check whether that's supported by server (by checking arrayBindSupported)
        if (sfstmt->paramset_size > 1)
        {
            ret = _snowflake_execute_with_binds_ex(sfstmt,
                                                   is_put_get_command,
                                                   result_capture,
                                                   SF_BOOLEAN_TRUE,
                                                   NULL, NULL,
                                                   is_async_exec);
            if (ret != SF_STATUS_SUCCESS)
            {
                return ret;
            }
            if ((sfstmt->is_dml) && (!sfstmt->array_bind_supported))
            {
                log_debug("Array bind is not supported - each parameter set entry "
                          "will be executed as a single request for query: %s",
                          sfstmt->sql_text);
                need_batch_exec = SF_BOOLEAN_TRUE;
            }
        }
        if (!need_batch_exec)
        {
            if (_snowflake_needs_stage_binding(sfstmt))
            {
                bind_stage = _snowflake_stage_bind_upload(sfstmt);
            }
            if (!bind_stage)
            {
                bindings = _snowflake_get_binding_json(sfstmt, SF_BIND_ALL);
            }
        }
    }

    if (need_batch_exec)
    {
        return _batch_dml_execute(sfstmt, result_capture);
    }

    return _snowflake_execute_with_binds_ex(sfstmt,
                                            is_put_get_command,
                                            result_capture,
                                            is_describe_only,
                                            bind_stage,
                                            bindings,
                                            is_async_exec);
}

SF_ERROR_STRUCT *STDCALL snowflake_error(SF_CONNECT *sf) {
    if (!sf) {
        return NULL;
    }
    return &sf->error;
}

SF_ERROR_STRUCT *STDCALL snowflake_stmt_error(SF_STMT *sfstmt) {
    if (!sfstmt) {
        return NULL;
    }
    return &sfstmt->error;
}

int64 STDCALL snowflake_num_rows(SF_STMT *sfstmt) {
    if (!sfstmt) {
        return -1;
    }

    if (sfstmt->is_async && !sfstmt->is_async_results_fetched) {
      if (!get_real_results(sfstmt)) {
        return -1;
      }
    }

    return sfstmt->total_rowcount;
}

int64 STDCALL snowflake_num_fields(SF_STMT *sfstmt) {
    if (!sfstmt) {
        return -1;
    }

    if (sfstmt->is_async && !sfstmt->is_async_results_fetched) {
      if (!get_real_results(sfstmt)) {
        return -1;
      }
    }

    return sfstmt->total_fieldcount;
}

uint64 STDCALL snowflake_num_params(SF_STMT *sfstmt) {
    if (!sfstmt) {
        // TODO change to -1?
        return 0;
    }

    if (sfstmt->is_async && !sfstmt->is_async_results_fetched) {
      if (!get_real_results(sfstmt)) {
        return 0;
      }
    }

    ARRAY_LIST *p = (ARRAY_LIST *) sfstmt->params;
    return p->used;
}

const char *STDCALL snowflake_sfqid(SF_STMT *sfstmt) {
    if (!sfstmt) {
        return NULL;
    }
    return sfstmt->sfqid;
}

const char *STDCALL snowflake_sqlstate(SF_STMT *sfstmt) {
    if (!sfstmt) {
        return NULL;
    }
    return sfstmt->error.sqlstate;
}

SF_COLUMN_DESC *STDCALL snowflake_desc(SF_STMT *sfstmt) {
    if (!sfstmt) {
        return NULL;
    }
    return sfstmt->desc;
}

SF_STATUS STDCALL snowflake_stmt_get_attr(
    SF_STMT *sfstmt, SF_STMT_ATTRIBUTE type, void **value) {
    if (!sfstmt) {
        return SF_STATUS_ERROR_STATEMENT_NOT_EXIST;
    }
    clear_snowflake_error(&sfstmt->error);
    switch(type) {
        case SF_STMT_USER_REALLOC_FUNC:
            *value = sfstmt->user_realloc_func;
            break;
        case SF_STMT_MULTI_STMT_COUNT:
            *value = &sfstmt->multi_stmt_count;
            break;
        case SF_STMT_PARAMSET_SIZE:
            *value = &sfstmt->paramset_size;
            break;
        default:
            SET_SNOWFLAKE_ERROR(
                &sfstmt->error, SF_STATUS_ERROR_BAD_ATTRIBUTE_TYPE,
                "Invalid attribute type",
                SF_SQLSTATE_UNABLE_TO_CONNECT);
            return SF_STATUS_ERROR_APPLICATION_ERROR;
    }
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL snowflake_stmt_set_attr(
    SF_STMT *sfstmt, SF_STMT_ATTRIBUTE type, const void *value) {
    if (!sfstmt) {
        return SF_STATUS_ERROR_STATEMENT_NOT_EXIST;
    }
    clear_snowflake_error(&sfstmt->error);
    switch(type) {
        case SF_STMT_USER_REALLOC_FUNC:
            sfstmt->user_realloc_func = (void*(*)(void*, size_t))value;
            break;
        case SF_STMT_MULTI_STMT_COUNT:
            sfstmt->multi_stmt_count = value ? *((int64*)value) : SF_MULTI_STMT_COUNT_UNSET;
            break;
        case SF_STMT_PARAMSET_SIZE:
            sfstmt->paramset_size = value ? *((int64*)value) : 1;
            break;
        default:
            SET_SNOWFLAKE_ERROR(
                &sfstmt->error, SF_STATUS_ERROR_BAD_ATTRIBUTE_TYPE,
                "Invalid attribute type",
                SF_SQLSTATE_UNABLE_TO_CONNECT);
            return SF_STATUS_ERROR_APPLICATION_ERROR;
    }
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL snowflake_propagate_error(SF_CONNECT *sf, SF_STMT *sfstmt) {
    if (!sf) {
        return SF_STATUS_ERROR_CONNECTION_NOT_EXIST;
    }
    if (!sfstmt) {
        return SF_STATUS_ERROR_STATEMENT_NOT_EXIST;
    }
    if (sf->error.error_code) {
        /* if already error is set */
        SF_FREE(sf->error.msg);
    }
    sf_memcpy(&sf->error, sizeof(SF_ERROR_STRUCT), &sfstmt->error, sizeof(SF_ERROR_STRUCT));
    if (sfstmt->error.error_code) {
        /* any error */
        size_t len = strlen(sfstmt->error.msg);
        sf->error.msg = SF_CALLOC(len + 1, sizeof(char));
        if (sf->error.msg == NULL) {
            SET_SNOWFLAKE_ERROR(
                &sf->error,
                SF_STATUS_ERROR_OUT_OF_MEMORY,
                "Out of memory in creating a buffer for the error message.",
                SF_SQLSTATE_APP_REJECT_CONNECTION);
        }
        sf_strncpy(sf->error.msg, len + 1, sfstmt->error.msg, len);
    }
    return SF_STATUS_SUCCESS;
}

// Does NULL checking and clears the SF_STMT error struct
SF_STATUS STDCALL _snowflake_column_null_checks(SF_STMT *sfstmt, void *value_ptr) {
    if (!sfstmt) {
        return SF_STATUS_ERROR_STATEMENT_NOT_EXIST;
    }
    clear_snowflake_error(&sfstmt->error);
    if (value_ptr == NULL) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, SF_STATUS_ERROR_NULL_POINTER,
                                 "value_ptr must not be NULL", "", sfstmt->sfqid);
        return SF_STATUS_ERROR_NULL_POINTER;
    }

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL _snowflake_next(SF_STMT *sfstmt) {
    return rs_next(sfstmt->result_set);
}

SF_STATUS STDCALL snowflake_column_as_boolean(SF_STMT *sfstmt, int idx, sf_bool *value_ptr) {
    SF_STATUS status;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    if ((status = rs_get_cell_as_bool(
            sfstmt->result_set, idx, value_ptr)) != SF_STATUS_SUCCESS) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, status,
            rs_get_error_message(sfstmt->result_set), "", sfstmt->sfqid);
    }
    return status;
}

SF_STATUS STDCALL snowflake_column_as_uint8(SF_STMT *sfstmt, int idx, uint8 *value_ptr) {
    SF_STATUS status;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    if ((status = rs_get_cell_as_uint8(
            sfstmt->result_set, idx, value_ptr)) != SF_STATUS_SUCCESS) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, status,
            rs_get_error_message(sfstmt->result_set), "", sfstmt->sfqid);
    }
    return status;
}

SF_STATUS STDCALL snowflake_column_as_uint32(SF_STMT *sfstmt, int idx, uint32 *value_ptr) {
    SF_STATUS status;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    if ((status = rs_get_cell_as_uint32(
            sfstmt->result_set, idx, value_ptr)) != SF_STATUS_SUCCESS) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, status,
            rs_get_error_message(sfstmt->result_set), "", sfstmt->sfqid);
    }
    return status;
}

SF_STATUS STDCALL snowflake_column_as_uint64(SF_STMT *sfstmt, int idx, uint64 *value_ptr) {
    SF_STATUS status;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    if ((status = rs_get_cell_as_uint64(
            sfstmt->result_set, idx, value_ptr)) != SF_STATUS_SUCCESS) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, status,
            rs_get_error_message(sfstmt->result_set), "", sfstmt->sfqid);
    }
    return status;
}

SF_STATUS STDCALL snowflake_column_as_int8(SF_STMT *sfstmt, int idx, int8 *value_ptr) {
    SF_STATUS status;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    if ((status = rs_get_cell_as_int8(
            sfstmt->result_set, idx, value_ptr)) != SF_STATUS_SUCCESS) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, status,
            rs_get_error_message(sfstmt->result_set), "", sfstmt->sfqid);
    }
    return status;
}

SF_STATUS STDCALL snowflake_column_as_int32(SF_STMT *sfstmt, int idx, int32 *value_ptr) {
    SF_STATUS status;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    if ((status = rs_get_cell_as_int32(
            sfstmt->result_set, idx, value_ptr)) != SF_STATUS_SUCCESS) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, status,
            rs_get_error_message(sfstmt->result_set), "", sfstmt->sfqid);
    }
    return status;
}

SF_STATUS STDCALL snowflake_column_as_int64(SF_STMT *sfstmt, int idx, int64 *value_ptr) {
    SF_STATUS status;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    if ((status = rs_get_cell_as_int64(
            sfstmt->result_set, idx, value_ptr)) != SF_STATUS_SUCCESS) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, status,
            rs_get_error_message(sfstmt->result_set), "", sfstmt->sfqid);
    }
    return status;
}

SF_STATUS STDCALL snowflake_column_as_float32(SF_STMT *sfstmt, int idx, float32 *value_ptr) {
    SF_STATUS status;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    if ((status = rs_get_cell_as_float32(
        sfstmt->result_set, idx, value_ptr)) != SF_STATUS_SUCCESS) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, status,
            rs_get_error_message(sfstmt->result_set), "", sfstmt->sfqid);
    }
    return status;
}

SF_STATUS STDCALL snowflake_column_as_float64(SF_STMT *sfstmt, int idx, float64 *value_ptr) {
    SF_STATUS status;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    if ((status = rs_get_cell_as_float64(
        sfstmt->result_set, idx, value_ptr)) != SF_STATUS_SUCCESS) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, status,
            rs_get_error_message(sfstmt->result_set), "", sfstmt->sfqid);
    }
    return status;
}

SF_STATUS STDCALL snowflake_column_as_timestamp(SF_STMT *sfstmt, int idx, SF_TIMESTAMP *value_ptr) {
    SF_STATUS status;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    if ((status = rs_get_cell_as_timestamp(
        sfstmt->result_set, idx, value_ptr)) != SF_STATUS_SUCCESS) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, status,
            rs_get_error_message(sfstmt->result_set), "", sfstmt->sfqid);
    }
    return status;
}

SF_STATUS STDCALL snowflake_column_as_const_str(SF_STMT *sfstmt, int idx, const char **value_ptr) {
    SF_STATUS status;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    if ((status = rs_get_cell_as_const_string(
        sfstmt->result_set, idx, value_ptr)) != SF_STATUS_SUCCESS) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, status,
            rs_get_error_message(sfstmt->result_set), "", sfstmt->sfqid);
    }
    return status;
}

SF_STATUS STDCALL snowflake_raw_value_to_str_rep(SF_STMT *sfstmt, const char* const_str_val, const SF_DB_TYPE type, const char *connection_timezone,
                                                 int32 scale, sf_bool isNull, char **value_ptr, size_t *value_len_ptr, size_t *max_value_size_ptr){

    if (value_ptr == NULL) {
      return SF_STATUS_ERROR_NULL_POINTER;
    }

    SF_STATUS status;

    char *value = NULL;
    size_t max_value_size = 0;
    size_t init_value_len = 0;
    size_t value_len = 0;
    sf_bool preallocated = SF_BOOLEAN_FALSE;

    // If value_ptr isn't null and max_value_size exists and is greater than 0,
    // then the user passed in a buffer and we should reallocate if needed
    if (*value_ptr != NULL && max_value_size_ptr != NULL && *max_value_size_ptr != 0) {
        value = *value_ptr;
        init_value_len = *max_value_size_ptr;
        preallocated = SF_BOOLEAN_TRUE;
    }


    if (isNull == SF_BOOLEAN_TRUE) {
      // If value is NULL, allocate buffer for empty string
      if (init_value_len == 0) {
        value = global_hooks.calloc(1, 1);
        max_value_size = 1;
      } else {
        // If we don't need to allocate a buffer, set value len to the initial value len
        max_value_size = init_value_len;
      }
      sf_strncpy(value, max_value_size, "", 1);
      value_len = 0;
      status = SF_STATUS_SUCCESS;
      goto cleanup;
    }

    time_t sec = 0L;
    struct tm tm_obj;
    struct tm *tm_ptr;
    memset(&tm_obj, 0, sizeof(tm_obj));

    SF_ERROR_STRUCT *error = (sfstmt != NULL ? &sfstmt->error : NULL);
    char *sfqid = sfstmt != NULL ? sfstmt->sfqid : NULL;

    switch (type) {
      case SF_DB_TYPE_BOOLEAN: ;
        const char *bool_value;
        if (strcmp(const_str_val, "0") == 0) {
          /* False */
          bool_value = SF_BOOLEAN_FALSE_STR;
        } else {
          /* True */
          bool_value = SF_BOOLEAN_TRUE_STR;
        }
        value_len = strlen(bool_value);
        if (value_len + 1 > init_value_len) {
          if (preallocated) {
            value = global_hooks.realloc(value, value_len + 1);
          } else {
            value = global_hooks.calloc(1, value_len + 1);
          }
          // If we have to allocate memory, then we need to set max_value_size
          // otherwise we leave max_value_size as is
          max_value_size = value_len + 1;
        } else {
          max_value_size = init_value_len;
        }
        sf_strncpy(value, max_value_size, bool_value, value_len + 1);
        break;
      case SF_DB_TYPE_DATE:
        sec =
                (time_t) strtol(const_str_val, NULL, 10) *
                86400L;
        _mutex_lock(&gmlocaltime_lock);
        tm_ptr = sf_gmtime(&sec, &tm_obj);
        _mutex_unlock(&gmlocaltime_lock);
        if (tm_ptr == NULL) {
          SET_SNOWFLAKE_STMT_ERROR(error,
                                   SF_STATUS_ERROR_CONVERSION_FAILURE,
                                   "Failed to convert a date value to a string.",
                                   SF_SQLSTATE_GENERAL_ERROR,
                                   sfqid);
          value = NULL;
          max_value_size = 0;
          goto cleanup;
        }
        // Max size of date string
        value_len = DATE_STRING_MAX_SIZE;
        if (value_len + 1 > init_value_len) {
          if (preallocated) {
            value = global_hooks.realloc(value, value_len + 1);
          } else {
            value = global_hooks.calloc(1, value_len + 1);
          }
          // If we have to allocate memory, then we need to set max_value_size
          // otherwise we leave max_value_size as is
          max_value_size = value_len + 1;
        } else {
          max_value_size = init_value_len;
        }
        value_len = strftime(value, value_len + 1, "%Y-%m-%d", &tm_obj);
        break;
      case SF_DB_TYPE_TIME:
      case SF_DB_TYPE_TIMESTAMP_NTZ:
      case SF_DB_TYPE_TIMESTAMP_LTZ:
      case SF_DB_TYPE_TIMESTAMP_TZ: ;
        SF_TIMESTAMP ts;
        if (scale < 0)
        {
          // If scale is not provided, try to calculate it.
          // E.g., for raw value "1234567890 2040" scale is 0
          // E.g., for raw value "1234567890.1234 2040" scale is from "." to " "
          // before TZ offset
          // E.g., for raw value "1234567890.1234" scale is calculated as length
          // of input minus length of "1234567890." part

          const char *scalePtr = strchr(const_str_val, (int) '.');
          if (scalePtr == NULL)
          {
            scale = 0;
          } else {
            const char *tzOffsetPtr = strchr(scalePtr+1, (int) ' ');
            if (tzOffsetPtr == NULL)
            {
              scale = strlen(const_str_val) - (scalePtr - const_str_val) - 1;
            } else {
              scale = tzOffsetPtr - scalePtr - 1;
            }
          }
          log_info("scale is calculated as %d", scale);
        }

        if (snowflake_timestamp_from_epoch_seconds(&ts,
                                                   const_str_val,
                                                   connection_timezone,
                                                   scale,
                                                   type)) {
          SET_SNOWFLAKE_STMT_ERROR(error,
                                   SF_STATUS_ERROR_CONVERSION_FAILURE,
                                   "Failed to convert the response from the server into a SF_TIMESTAMP.",
                                   SF_SQLSTATE_GENERAL_ERROR,
                                   sfqid);
          value = NULL;
          max_value_size = 0;
          goto cleanup;
        }
        // TODO add format when format is no longer a fixed string
        if (snowflake_timestamp_to_string(&ts, "", &value, max_value_size, &value_len, SF_BOOLEAN_TRUE)) {
          SET_SNOWFLAKE_STMT_ERROR(error,
                                   SF_STATUS_ERROR_CONVERSION_FAILURE,
                                   "Failed to convert a SF_TIMESTAMP value to a string.",
                                   SF_SQLSTATE_GENERAL_ERROR,
                                   sfqid);
          // If the memory wasn't preallocated, then free it
          if (!preallocated && value) {
            global_hooks.dealloc(value);
            value = NULL;
            max_value_size = 0;
          }
          value_len = 0;
          goto cleanup;
        } else {
          // If true, then we reallocated when writing the timestamp to a string
          if (value_len + 1 > init_value_len) {
            max_value_size = value_len + 1;
          } else {
            max_value_size = init_value_len;
          }
        }

        break;
      default:
        value_len = strlen(const_str_val);
        if (value_len + 1 > init_value_len) {
          if (preallocated) {
            value = global_hooks.realloc(value, value_len + 1);
          } else {
            value = global_hooks.calloc(1, value_len + 1);
          }
          // If we have to allocate memory, then we need to set max_value_size
          // otherwise we leave max_value_size as is
          max_value_size = value_len + 1;
        } else {
          max_value_size = init_value_len;
        }
        sf_strncpy(value, max_value_size, const_str_val, value_len + 1);
        break;
    }

    // Everything went okay
    status = SF_STATUS_SUCCESS;

cleanup:
    *value_ptr = value;
    if (max_value_size_ptr) {
        *max_value_size_ptr = max_value_size;
    }
    if (value_len_ptr) {
        *value_len_ptr = value_len;
    }
    return status;
}

SF_STATUS STDCALL snowflake_column_as_str(SF_STMT *sfstmt, int idx, char **value_ptr, size_t *value_len_ptr, size_t *max_value_size_ptr) {
    SF_STATUS status;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    const char* str_val = NULL;
    if ((status = rs_get_cell_as_const_string(
            sfstmt->result_set,
            idx,
            &str_val)) != SF_STATUS_SUCCESS)
    {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, status,
            rs_get_error_message(sfstmt->result_set), "", sfstmt->sfqid);
        return status;
    }

    if (SF_ARROW_FORMAT == sfstmt->qrf)
    {
        // For Arrow the const string is formatted already
        return snowflake_raw_value_to_str_rep(sfstmt, str_val,
                                             SF_DB_TYPE_TEXT,
                                             sfstmt->connection->timezone,
                                             0,
                                             str_val ? SF_BOOLEAN_FALSE : SF_BOOLEAN_TRUE,
                                             value_ptr, value_len_ptr,
                                             max_value_size_ptr);
    }
    else
    {
        return snowflake_raw_value_to_str_rep(sfstmt, str_val,
                                              sfstmt->desc[idx - 1].type,
                                              sfstmt->connection->timezone,
                                              (int32) sfstmt->desc[idx - 1].scale,
                                              str_val ? SF_BOOLEAN_FALSE : SF_BOOLEAN_TRUE,
                                              value_ptr, value_len_ptr,
                                              max_value_size_ptr);
    }
}

SF_STATUS STDCALL snowflake_column_strlen(SF_STMT *sfstmt, int idx, size_t *value_ptr) {
    SF_STATUS status;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    if ((status = rs_get_cell_strlen(
        sfstmt->result_set, idx, value_ptr)) != SF_STATUS_SUCCESS) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, status,
            rs_get_error_message(sfstmt->result_set), "", sfstmt->sfqid);
    }
    return status;
}

SF_STATUS STDCALL snowflake_column_is_null(SF_STMT *sfstmt, int idx, sf_bool *value_ptr) {
    SF_STATUS status;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    if ((status = rs_is_cell_null(
        sfstmt->result_set, idx, value_ptr)) != SF_STATUS_SUCCESS) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, status,
            rs_get_error_message(sfstmt->result_set), "", sfstmt->sfqid);
    }
    return status;
}

SF_STATUS STDCALL snowflake_timestamp_from_parts(SF_TIMESTAMP *ts, int32 nanoseconds, int32 seconds,
                                                 int32 minutes, int32 hours, int32 mday, int32 months,
                                                 int32 year, int32 tzoffset, int32 scale, SF_DB_TYPE ts_type) {
    if (!ts) {
        return SF_STATUS_ERROR_NULL_POINTER;
    }
    memset(ts, 0, sizeof(*ts));

    if (nanoseconds < 0 || nanoseconds > 999999999) {
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }
    ts->nsec = nanoseconds;

    if (seconds < 0 || seconds > 59) {
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }
    ts->tm_obj.tm_sec = seconds;

    if (minutes < 0 || minutes > 59) {
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }
    ts->tm_obj.tm_min = minutes;

    if (hours < 0 || hours > 23) {
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }
    ts->tm_obj.tm_hour = hours;

    if (mday < 1 || mday > 31) {
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }
    ts->tm_obj.tm_mday = mday;

    if (months < 1 || months > 12) {
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }
    ts->tm_obj.tm_mon = months - 1;

    if (year < -99999 || year > 99999) {
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }
    ts->tm_obj.tm_year = year - 1900;

    if (tzoffset < 0 || tzoffset > 1439) {
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }
    ts->tzoffset = tzoffset;

    if (scale < 0 || scale > 9) {
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }
    ts->scale = scale;

    if (ts_type != SF_DB_TYPE_DATE &&
        ts_type != SF_DB_TYPE_TIME &&
        ts_type != SF_DB_TYPE_TIMESTAMP_LTZ &&
        ts_type != SF_DB_TYPE_TIMESTAMP_NTZ &&
        ts_type != SF_DB_TYPE_TIMESTAMP_TZ) {
        return SF_STATUS_ERROR_OUT_OF_RANGE;
    }
    ts->ts_type = ts_type;

    // A call to mktime will automatically set wday and yday in the tm struct
    // We don't care about the output
    mktime(&ts->tm_obj);

    // Everything went okay
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL snowflake_timestamp_from_epoch_seconds(SF_TIMESTAMP *ts, const char *str, const char *timezone,
                                                         int32 scale, SF_DB_TYPE ts_type) {
    if (!ts) {
        return SF_STATUS_ERROR_NULL_POINTER;
    }

    SF_STATUS ret = SF_STATUS_ERROR_GENERAL;
    time_t nsec = 0L;
    time_t sec = 0L;
    int64 tzoffset = 0;
    struct tm *tm_ptr = NULL;
    char tzname[64];
    char *tzptr = (char *) timezone;

    memset(&ts->tm_obj, 0, sizeof(ts->tm_obj));
    ts->nsec = 0;
    ts->scale = scale;
    ts->ts_type = ts_type;
    ts->tzoffset = 0;

    /* Search for a decimal point if scale > 0
     * When scale == 0, str will look like e.g. "1593586800 2040"*/
    const char *ptr = scale > 0 ? strchr(str, (int) '.') : str;
    // No decimal point exists for date types
    if (ptr == NULL && ts->ts_type != SF_DB_TYPE_DATE) {
        ret = SF_STATUS_ERROR_GENERAL;
        goto cleanup;
    }

    sec = strtoll(str, NULL, 10);

    if (ts->ts_type == SF_DB_TYPE_DATE) {
        sec = sec * SECONDS_IN_AN_HOUR;
    } else {
        if (scale > 0) {
          nsec = strtoll(ptr + 1, NULL, 10);
        }
        /* Search for a space for TIMESTAMP_TZ */
        char *sptr = strchr(ptr + 1, (int) ' ');
        if (sptr != NULL) {
            /* TIMESTAMP_TZ */
            tzoffset = strtoll(sptr + 1, NULL, 10) - TIMEZONE_OFFSET_RANGE;
        }
    }

    // If timestamp is before the epoch, we have to do some
    // math to make things work just right
    if (sec < 0 && nsec > 0) {
        nsec = pow10_int64[ts->scale] - nsec;
        sec--;
    }
    // Transform nsec to a 9 digit number to store in the timestamp struct
    ts->nsec = (int32) (nsec * pow10_int64[9-ts->scale]);

    if (ts->ts_type == SF_DB_TYPE_TIMESTAMP_TZ) {
        /* make up Timezone name from the tzoffset */
        ldiv_t dm = ldiv((long) tzoffset, 60L);
        sf_sprintf(tzname, sizeof(tzname), "UTC%c%02ld:%02ld",
                dm.quot > 0 ? '+' : '-', labs(dm.quot), labs(dm.rem));
        tzptr = tzname;
        ts->tzoffset = (int32) tzoffset;
    }

    /* replace a dot character with NULL */
    if (ts->ts_type == SF_DB_TYPE_TIMESTAMP_NTZ ||
        ts->ts_type == SF_DB_TYPE_TIME ||
        ts->ts_type == SF_DB_TYPE_DATE) {
        tm_ptr = sf_gmtime(&sec, &ts->tm_obj);
    } else if (ts->ts_type == SF_DB_TYPE_TIMESTAMP_LTZ ||
      ts->ts_type == SF_DB_TYPE_TIMESTAMP_TZ) {
        /* set the environment variable TZ to the session timezone
         * so that localtime_tz honors it.
         */
        _mutex_lock(&gmlocaltime_lock);
        char tzbuf[128];
        const char *prev_tz_ptr = sf_getenv_s("TZ", tzbuf, sizeof(tzbuf));
        sf_setenv("TZ", tzptr);
        sf_tzset();
        sec += tzoffset * 60 * 2; /* adjust for TIMESTAMP_TZ */
        tm_ptr = sf_localtime(&sec, &ts->tm_obj);
#if defined(__linux__) || defined(__APPLE__)
        if (ts->ts_type == SF_DB_TYPE_TIMESTAMP_LTZ) {
            ts->tzoffset = (int32) (ts->tm_obj.tm_gmtoff / 60);
        }
#endif
        if (prev_tz_ptr != NULL) {
            sf_setenv("TZ", prev_tz_ptr); /* cannot set to NULL */
        } else {
            sf_unsetenv("TZ");
        }
        sf_tzset();
        _mutex_unlock(&gmlocaltime_lock);
    }
    if (tm_ptr == NULL) {
        ret = SF_STATUS_ERROR_GENERAL;
        goto cleanup;
    }


    ret = SF_STATUS_SUCCESS;
cleanup:
    return ret;
}


SF_STATUS STDCALL snowflake_timestamp_to_string(SF_TIMESTAMP *ts, const char *fmt, char **buffer_ptr,
                                                size_t buf_size, size_t *bytes_written,
                                                sf_bool reallocate) {
    SF_STATUS ret = SF_STATUS_ERROR_GENERAL;
    size_t len = 0;
    if (!ts || !buffer_ptr) {
        return SF_STATUS_ERROR_NULL_POINTER;
    }
    char *buffer = *buffer_ptr;

    // Using a fixed format for now.
    // TODO update to translate sql format to C date format instead of using fixed format.
    const char *fmt0;
    size_t max_len = 1;
    if (ts->ts_type != SF_DB_TYPE_TIME) {
        max_len += 21;
        fmt0 = "%Y-%m-%d %H:%M:%S";
    } else {
        max_len += 8;
        fmt0 = "%H:%M:%S";
    }
    /* adjust scale */
    char fmt_static[20];
    sf_sprintf(fmt_static, sizeof(fmt_static), ".%%0%dld", ts->scale);

    // Add space for scale if scale is greater than 0
    max_len += (ts->scale > 0) ? 1 + ts->scale : 0;
    // Add space for timezone if SF_DB_TYPE_TIMESTAMP_TZ is set
    max_len += (ts->ts_type == SF_DB_TYPE_TIMESTAMP_TZ) ? 7 : 0;
    // Allocate string buffer to store date using our calculated max length
    if (max_len > buf_size || buffer == NULL) {
        if (reallocate || buffer == NULL) {
            buffer = global_hooks.realloc(buffer, max_len);
            buf_size = max_len;
        } else {
            ret = SF_STATUS_ERROR_BUFFER_TOO_SMALL;
            goto cleanup;
        }
    }
    len = strftime(buffer, buf_size, fmt0, &ts->tm_obj);
    if (ts->scale > 0) {
        int64 nsec = ts->nsec / pow10_int64[9-ts->scale];
        len += sf_sprintf(
          &(buffer)[len],
          max_len - len, fmt_static,
          nsec);
    }
    if (ts->ts_type == SF_DB_TYPE_TIMESTAMP_TZ) {
        /* Timezone info */
        ldiv_t dm = ldiv((long) ts->tzoffset, 60L);
        len += sf_sprintf(
          &((char *) buffer)[len],
          max_len - len,
          " %c%02ld:%02ld",
          dm.quot > 0 ? '+' : '-', labs(dm.quot), labs(dm.rem));
    }

    ret = SF_STATUS_SUCCESS;

cleanup:
    if (bytes_written) {
        *bytes_written = len;
    }
    *buffer_ptr = buffer;
    return ret;

}



SF_STATUS STDCALL snowflake_timestamp_get_epoch_seconds(SF_TIMESTAMP *ts,
                                                        time_t *epoch_time_ptr) {
    if (!ts) {
        return SF_STATUS_ERROR_NULL_POINTER;
    }

    time_t epoch_time;

    ts->tm_obj.tm_isdst = -1;
    // mktime takes into account tm_gmtoff which is a Linux and OS X ONLY field
#if defined(__linux__) || defined(__APPLE__)
    epoch_time = (time_t)mktime(&ts->tm_obj);
    epoch_time += ts->tm_obj.tm_gmtoff;
#else
    epoch_time = _mkgmtime64(&ts->tm_obj);
#endif
    *epoch_time_ptr = epoch_time - (ts->tzoffset * 60);

    return SF_STATUS_SUCCESS;
}

int32 STDCALL snowflake_timestamp_get_nanoseconds(SF_TIMESTAMP *ts) {
    if (!ts) {
        return -1;
    }
    return ts->nsec;
}

int32 STDCALL snowflake_timestamp_get_seconds(SF_TIMESTAMP *ts) {
    if (!ts) {
        return  -1;
    }
    return ts->tm_obj.tm_sec;
}

int32 STDCALL snowflake_timestamp_get_minutes(SF_TIMESTAMP *ts) {
    if (!ts) {
        return  -1;
    }
    return ts->tm_obj.tm_min;
}

int32 STDCALL snowflake_timestamp_get_hours(SF_TIMESTAMP *ts) {
    if (!ts) {
        return  -1;
    }
    return ts->tm_obj.tm_hour;
}

int32 STDCALL snowflake_timestamp_get_wday(SF_TIMESTAMP *ts) {
    if (!ts) {
        return  -1;
    }
    return ts->tm_obj.tm_wday;
}

int32 STDCALL snowflake_timestamp_get_mday(SF_TIMESTAMP *ts) {
    if (!ts) {
        return  -1;
    }
    return ts->tm_obj.tm_mday;
}

int32 STDCALL snowflake_timestamp_get_yday(SF_TIMESTAMP *ts) {
    if (!ts) {
        return  -1;
    }
    return ts->tm_obj.tm_yday;
}

int32 STDCALL snowflake_timestamp_get_month(SF_TIMESTAMP *ts) {
    if (!ts) {
        return  -1;
    }
    return ts->tm_obj.tm_mon + 1;
}

int32 STDCALL snowflake_timestamp_get_year(SF_TIMESTAMP *ts) {
    if (!ts) {
        return  -100000;
    }
    return ts->tm_obj.tm_year + 1900;
}

int32 STDCALL snowflake_timestamp_get_tzoffset(SF_TIMESTAMP *ts) {
    if (!ts) {
        return -1;
    }
    return ts->tzoffset;
}

int32 STDCALL snowflake_timestamp_get_scale(SF_TIMESTAMP *ts) {
    if (!ts) {
        return -1;
    }
    return ts->scale;
}
