/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <openssl/crypto.h>
#include <snowflake/client.h>
#include "constants.h"
#include "client_int.h"
#include "connection.h"
#include "memory.h"
#include "results.h"
#include "error.h"
#include "chunk_downloader.h"

#define curl_easier_escape(curl, string) curl_easy_escape(curl, string, 0)

// Define internal constants
sf_bool DISABLE_VERIFY_PEER;
char *CA_BUNDLE_FILE;
int32 SSL_VERSION;
sf_bool DEBUG;
sf_bool SF_OCSP_CHECK;
char *SF_HEADER_USER_AGENT = NULL;

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

#define _SF_STMT_TYPE_DML 0x3000
#define _SF_STMT_TYPE_INSERT (_SF_STMT_TYPE_DML + 0x100)
#define _SF_STMT_TYPE_UPDATE (_SF_STMT_TYPE_DML + 0x200)
#define _SF_STMT_TYPE_DELETE (_SF_STMT_TYPE_DML + 0x300)
#define _SF_STMT_TYPE_MERGE (_SF_STMT_TYPE_DML + 0x400)
#define _SF_STMT_TYPE_MULTI_TABLE_INSERT (_SF_STMT_TYPE_DML + 0x500)

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
        sb_strncpy(*var, str_size, str, str_size);
    } else {
        *var = NULL;
    }
}

/**
 * Make a directory with the mode
 * @param file_path directory name
 * @param mode mode
 * @return 0 if success otherwise -1
 */
static int mkpath(char *file_path) {
    assert(file_path && *file_path);
    char *p;
    for (p = strchr(file_path + 1, '/'); p; p = strchr(p + 1, '/')) {
        *p = '\0';
        if (sf_mkdir(file_path) == -1) {
            if (errno != EEXIST) {
                *p = '/';
                return -1;
            }
        }
        *p = '/';
    }
    return 0;
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
            sb_sprintf(msg, sizeof(msg), "Specified database doesn't exists: [%s]",
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
            sb_sprintf(msg, sizeof(msg), "Specified schema doesn't exists: [%s]",
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
            sb_sprintf(msg, sizeof(msg), "Specified warehouse doesn't exists: [%s]",
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
    time_info = localtime(&current_time);
    strftime(time_str, sizeof(time_str), "%Y%m%d%H%M%S", time_info);
    const char *sf_log_path;
    const char *sf_log_level_str;
    SF_LOG_LEVEL sf_log_level = log_level;

    size_t log_path_size = 1; //Start with 1 to include null terminator
    log_path_size += strlen(time_str);

    /* The environment variables takes precedence over the specified parameters */
    sf_log_path = sf_getenv("SNOWFLAKE_LOG_PATH");
    if (sf_log_path == NULL && log_path) {
        sf_log_path = log_path;
    }

    sf_log_level_str = sf_getenv("SNOWFLAKE_LOG_LEVEL");
    if (sf_log_level_str != NULL) {
        sf_log_level = log_from_str_to_level(sf_log_level_str);
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
    if (sf_log_path) {
        log_path_size += strlen(sf_log_path);
        LOG_PATH = (char *) SF_CALLOC(1, log_path_size);
        sb_sprintf(LOG_PATH, log_path_size, "%s/snowflake_%s.txt", sf_log_path,
                 (char *) time_str);
    } else {
        LOG_PATH = (char *) SF_CALLOC(1, log_path_size);
        sb_sprintf(LOG_PATH, log_path_size, "logs/snowflake_%s.txt",
                 (char *) time_str);
    }
    if (LOG_PATH != NULL) {
        // Create log file path (if it already doesn't exist)
        if (mkpath(LOG_PATH) == -1) {
            fprintf(stderr, "Error creating log directory. Error code: %s\n",
                    strerror(errno));
            goto cleanup;
        }
        // Open log file
        LOG_FP = fopen(LOG_PATH, "w+");
        if (LOG_FP) {
            // Set log file
            log_set_fp(LOG_FP);
        } else {
            fprintf(stderr,
                    "Error opening file from file path: %s\nError code: %s\n",
                    LOG_PATH, strerror(errno));
            goto cleanup;
        }

    } else {
        fprintf(stderr,
                "Log path is NULL. Was there an error during path construction?\n");
        goto cleanup;
    }

    ret = SF_BOOLEAN_TRUE;

cleanup:
    return ret;
}

/**
 * Cleans up memory allocated for log init and closes log file.
 */
static void STDCALL log_term() {
    SF_FREE(LOG_PATH);
    if (LOG_FP) {
        fclose(LOG_FP);
        log_set_fp(NULL);
    }
    _mutex_term(&gmlocaltime_lock);
    _mutex_term(&log_lock);
}

/**
 * Process connection parameters
 * @param sf SF_CONNECT
 */
SF_STATUS STDCALL
_snowflake_check_connection_parameters(SF_CONNECT *sf) {
    if (is_string_empty(sf->account)) {
        // Invalid connection
        log_error(ERR_MSG_ACCOUNT_PARAMETER_IS_MISSING);
        SET_SNOWFLAKE_ERROR(
            &sf->error,
            SF_STATUS_ERROR_BAD_CONNECTION_PARAMS,
            ERR_MSG_ACCOUNT_PARAMETER_IS_MISSING,
            SF_SQLSTATE_UNABLE_TO_CONNECT);
        return SF_STATUS_ERROR_GENERAL;
    }

    if (is_string_empty(sf->user)) {
        // Invalid connection
        log_error(ERR_MSG_USER_PARAMETER_IS_MISSING);
        SET_SNOWFLAKE_ERROR(
            &sf->error,
            SF_STATUS_ERROR_BAD_CONNECTION_PARAMS,
            ERR_MSG_USER_PARAMETER_IS_MISSING,
            SF_SQLSTATE_UNABLE_TO_CONNECT);
        return SF_STATUS_ERROR_GENERAL;
    }

    if (is_string_empty(sf->password)) {
        // Invalid connection
        log_error(ERR_MSG_PASSWORD_PARAMETER_IS_MISSING);
        SET_SNOWFLAKE_ERROR(
            &sf->error,
            SF_STATUS_ERROR_BAD_CONNECTION_PARAMS,
            ERR_MSG_PASSWORD_PARAMETER_IS_MISSING,
            SF_SQLSTATE_UNABLE_TO_CONNECT);
        return SF_STATUS_ERROR_GENERAL;
    }

    if (!sf->host) {
        // construct a host parameter if not specified,
        char buf[1024];
        if (sf->region) {
            sb_sprintf(buf, sizeof(buf), "%s.%s.snowflakecomputing.com",
                     sf->account, sf->region);
        } else {
            sb_sprintf(buf, sizeof(buf), "%s.snowflakecomputing.com",
                     sf->account);
        }
        alloc_buffer_and_copy(&sf->host, buf);
    }
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
    log_debug("timezone: %s", sf->timezone);
    log_debug("login_timeout: %d", sf->login_timeout);
    log_debug("network_timeout: %d", sf->network_timeout);

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
        fprintf(stderr, "Error during log initialization");
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
        sb_sprintf(SF_HEADER_USER_AGENT, HEADER_C_API_USER_AGENT_MAX_LEN, HEADER_C_API_USER_AGENT_FORMAT, SF_API_NAME, SF_API_VERSION, platform, platform_version, "STDC", c_version);
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
                sb_strncpy(value, size, CA_BUNDLE_FILE, strlen(CA_BUNDLE_FILE) + 1);
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
        default:
            break;
    }
    return SF_STATUS_SUCCESS;
}

SF_CONNECT *STDCALL snowflake_init() {
    SF_CONNECT *sf = (SF_CONNECT *) SF_CALLOC(1, sizeof(SF_CONNECT));

    // Make sure memory was actually allocated
    if (sf) {
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
        sf->autocommit = SF_BOOLEAN_TRUE;
        sf->timezone = NULL;
        sf->service_name = NULL;
        alloc_buffer_and_copy(&sf->authenticator, SF_AUTHENTICATOR_DEFAULT);
        alloc_buffer_and_copy(&sf->application_name, SF_API_NAME);
        alloc_buffer_and_copy(&sf->application_version, SF_API_VERSION);

        _mutex_init(&sf->mutex_parameters);

        sf->token = NULL;
        sf->master_token = NULL;
        sf->login_timeout = SF_LOGIN_TIMEOUT;
        sf->network_timeout = 0;
        sf->sequence_counter = 0;
        _mutex_init(&sf->mutex_sequence_counter);
        sf->request_id[0] = '\0';
        clear_snowflake_error(&sf->error);

        sf->directURL_param = NULL;
        sf->directURL = NULL;
        sf->direct_query_token = NULL;
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

    if (sf->token && sf->master_token) {
        /* delete the session */
        URL_KEY_VALUE url_params[] = {
            {.key="delete=", .value="true", .formatted_key=NULL, .formatted_value=NULL, .key_size=0, .value_size=0}
        };
        if (request(sf, &resp, DELETE_SESSION_URL, url_params,
                    sizeof(url_params) / sizeof(URL_KEY_VALUE), NULL, NULL,
                    POST_REQUEST_TYPE, &sf->error, SF_BOOLEAN_FALSE)) {
            s_resp = snowflake_cJSON_Print(resp);
            log_trace("JSON response:\n%s", s_resp);
            /* Even if the session deletion fails, it will be cleaned after 7 days.
             * Catching error here won't help
             */
        }
        snowflake_cJSON_Delete(resp);
        SF_FREE(s_resp);
    }

    _mutex_term(&sf->mutex_sequence_counter);
    _mutex_term(&sf->mutex_parameters);
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
    SF_FREE(sf->timezone);
    SF_FREE(sf->service_name);
    SF_FREE(sf->master_token);
    SF_FREE(sf->token);
    SF_FREE(sf->directURL);
    SF_FREE(sf->directURL_param);
    SF_FREE(sf->direct_query_token);
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
    ret = SF_STATUS_ERROR_GENERAL; // reset to the error

    uuid4_generate(sf->request_id);// request id

    // Create body
    body = create_auth_json_body(
        sf,
        sf->application_name,
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
    if (request(sf, &resp, SESSION_URL, url_params,
                sizeof(url_params) / sizeof(URL_KEY_VALUE), s_body, NULL,
                POST_REQUEST_TYPE, &sf->error, SF_BOOLEAN_FALSE)) {
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

        _mutex_lock(&sf->mutex_parameters);
        ret = _set_parameters_session_info(sf, data);
        _mutex_unlock(&sf->mutex_parameters);
        if (ret > 0) {
            goto cleanup;
        }
    } else {
        log_error("No response");
        if (sf->error.error_code == SF_STATUS_SUCCESS) {
            SET_SNOWFLAKE_ERROR(&sf->error, SF_STATUS_ERROR_BAD_JSON,
                                "No valid JSON response",
                                SF_SQLSTATE_UNABLE_TO_CONNECT);
        }
        goto cleanup;
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
        case SF_CON_AUTHENTICATOR:
            alloc_buffer_and_copy(&sf->authenticator, value);
            break;
        case SF_CON_INSECURE_MODE:
            sf->insecure_mode = value ? *((sf_bool *) value) : SF_BOOLEAN_FALSE;
            break;
        case SF_CON_LOGIN_TIMEOUT:
            sf->login_timeout = value ? *((int64 *) value) : SF_LOGIN_TIMEOUT;
            break;
        case SF_CON_NETWORK_TIMEOUT:
            sf->network_timeout = value ? *((int64 *) value) : SF_LOGIN_TIMEOUT;
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
        case SF_CON_AUTHENTICATOR:
            *value = sf->authenticator;
            break;
        case SF_CON_INSECURE_MODE:
            *value = &sf->insecure_mode;
            break;
        case SF_CON_LOGIN_TIMEOUT:
            *value = &sf->login_timeout;
            break;
        case SF_CON_NETWORK_TIMEOUT:
            *value = &sf->network_timeout;
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
        nparams->name_list = SF_REALLOC(nparams->name_list,(2*cur_size));
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

/**
 * Resets SNOWFLAKE_STMT parameters.
 *
 * @param sfstmt
 */
static void STDCALL _snowflake_stmt_reset(SF_STMT *sfstmt) {

    clear_snowflake_error(&sfstmt->error);

    sb_strncpy(sfstmt->sfqid, SF_UUID4_LEN, "", sizeof(""));
    sfstmt->request_id[0] = '\0';

    if (sfstmt->sql_text) {
        SF_FREE(sfstmt->sql_text); /* SQL */
    }
    sfstmt->sql_text = NULL;

    if (sfstmt->cur_row) {
        snowflake_cJSON_Delete(sfstmt->cur_row);
        sfstmt->cur_row = NULL;
    }
    sfstmt->cur_row = NULL;

    if (sfstmt->raw_results) {
        snowflake_cJSON_Delete(sfstmt->raw_results);
        sfstmt->raw_results = NULL;
    }
    sfstmt->raw_results = NULL;


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
    }
    return sfstmt;
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

SF_STATUS STDCALL snowflake_fetch(SF_STMT *sfstmt) {
    if (!sfstmt) {
        return SF_STATUS_ERROR_STATEMENT_NOT_EXIST;
    }
    clear_snowflake_error(&sfstmt->error);
    SF_STATUS ret = SF_STATUS_ERROR_GENERAL;
    sf_bool get_chunk_success = SF_BOOLEAN_TRUE;
    uint64 index;
    if (sfstmt->cur_row != NULL) {
        snowflake_cJSON_Delete(sfstmt->cur_row);
        sfstmt->cur_row = NULL;
    }

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
                    snowflake_cJSON_Delete(sfstmt->raw_results);
                    sfstmt->raw_results = NULL;
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

                    // Delete old cJSON results struct
                    snowflake_cJSON_Delete((cJSON *) sfstmt->raw_results);
                    // Set new chunk and remove chunk reference from locked array
                    sfstmt->raw_results = sfstmt->chunk_downloader->queue[index].chunk;
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
    sfstmt->cur_row = snowflake_cJSON_DetachItemFromArray(sfstmt->raw_results, 0);
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
    if (snowflake_cJSON_GetArraySize(sfstmt->raw_results) == 0) {
        /* no affected rows is determined. The potential cause is
         * the query is not DML or no stmt was executed at all . */
        SET_SNOWFLAKE_STMT_ERROR(
            &sfstmt->error,
            SF_STATUS_ERROR_APPLICATION_ERROR, "No results found.",
            SF_SQLSTATE_NO_DATA, sfstmt->sfqid);
        sfstmt->error.error_code = SF_STATUS_ERROR_APPLICATION_ERROR;
        return ret;
    }

    if (sfstmt->is_dml) {
        row = snowflake_cJSON_DetachItemFromArray(sfstmt->raw_results, 0);
        ret = 0;
        for (i = 0; i < (size_t) sfstmt->total_fieldcount; ++i) {
            raw_row_result = snowflake_cJSON_GetArrayItem(row, (int) i);
            ret += (int64) strtoll(raw_row_result->valuestring, NULL, 10);
        }
        snowflake_cJSON_Delete(row);
    } else {
        ret = sfstmt->total_rowcount;
    }
    return ret;
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
    sb_memcpy(sfstmt->sql_text, sql_text_size, command, sql_text_size - 1);
    // Null terminate
    sfstmt->sql_text[sql_text_size - 1] = '\0';

    ret = SF_STATUS_SUCCESS;

cleanup:
    return ret;
}

SF_STATUS STDCALL snowflake_execute(SF_STMT *sfstmt) {
    return _snowflake_execute_ex(sfstmt, _is_put_get_command(sfstmt->sql_text));
}

SF_STATUS STDCALL _snowflake_execute_ex(SF_STMT *sfstmt,
                                        sf_bool is_put_get_command) {
    if (!sfstmt) {
        return SF_STATUS_ERROR_STATEMENT_NOT_EXIST;
    }
    clear_snowflake_error(&sfstmt->error);
    SF_STATUS ret = SF_STATUS_ERROR_GENERAL;
    SF_JSON_ERROR json_error;
    const char *error_msg;
    cJSON *body = NULL;
    cJSON *data = NULL;
    cJSON *rowtype = NULL;
    cJSON *stats = NULL;
    cJSON *resp = NULL;
    cJSON *chunks = NULL;
    cJSON *chunk_headers = NULL;
    char *qrmk = NULL;
    char *s_body = NULL;
    char *s_resp = NULL;
    sf_bool success = SF_BOOLEAN_FALSE;
    uuid4_generate(sfstmt->request_id);
    URL_KEY_VALUE url_params[] = {
            {.key="requestId=", .value=sfstmt->request_id, .formatted_key=NULL, .formatted_value=NULL, .key_size=0, .value_size=0}
    };
    size_t i;
    cJSON *bindings = NULL;
    SF_BIND_INPUT *input;
    const char *type;
    char *value;

    _mutex_lock(&sfstmt->connection->mutex_sequence_counter);
    sfstmt->sequence_counter = ++sfstmt->connection->sequence_counter;
    _mutex_unlock(&sfstmt->connection->mutex_sequence_counter);

    if (_snowflake_get_current_param_style(sfstmt) == POSITIONAL)
    {
        bindings = snowflake_cJSON_CreateObject();
        for (i = 0; i < sfstmt->params_len; i++)
        {
            cJSON *binding;
            input = (SF_BIND_INPUT *) sf_param_store_get(sfstmt->params,
                    i+1,NULL);
            if (input == NULL) {
                continue;
            }
            // TODO check if input is null and either set error or write msg to log
            type = snowflake_type_to_string(
                    c_type_to_snowflake(input->c_type, SF_DB_TYPE_TIMESTAMP_NTZ));
            value = value_to_string(input->value, input->len, input->c_type);
            binding = snowflake_cJSON_CreateObject();
            char idxbuf[20];
            sb_sprintf(idxbuf, sizeof(idxbuf), "%lu", (unsigned long) (i + 1));
            snowflake_cJSON_AddStringToObject(binding, "type", type);
            snowflake_cJSON_AddStringToObject(binding, "value", value);
            snowflake_cJSON_AddItemToObject(bindings, idxbuf, binding);
            if (value) {
                SF_FREE(value);
            }
        }
    }
    else if (_snowflake_get_current_param_style(sfstmt) == NAMED)
    {
        bindings = snowflake_cJSON_CreateObject();
        char *named_param = NULL;
        for(i = 0; i < sfstmt->params_len; i++)
        {
            cJSON *binding;
            named_param = (char *)(((NamedParams *)sfstmt->name_list)->name_list[i]);
            input = (SF_BIND_INPUT *) sf_param_store_get(sfstmt->params,
                    0,named_param);
            if (input == NULL)
            {
                log_error("_snowflake_execute_ex: No parameter by this name %s",named_param);
                continue;
            }
            type = snowflake_type_to_string(
                    c_type_to_snowflake(input->c_type, SF_DB_TYPE_TIMESTAMP_NTZ));
            value = value_to_string(input->value, input->len, input->c_type);
            binding = snowflake_cJSON_CreateObject();

            snowflake_cJSON_AddStringToObject(binding, "type", type);
            snowflake_cJSON_AddStringToObject(binding, "value", value);
            snowflake_cJSON_AddItemToObject(bindings, named_param, binding);
            if (value)
            {
                SF_FREE(value);
            }
        }
    }

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
                                  NULL : sfstmt->request_id);
    if (bindings != NULL) {
        /* binding parameters if exists */
      snowflake_cJSON_AddItemToObject(body, "bindings", bindings);
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
                POST_REQUEST_TYPE, &sfstmt->error, is_put_get_command)) {
        s_resp = snowflake_cJSON_Print(resp);
        log_trace("Here is JSON response:\n%s", s_resp);
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

                cJSON *enc_mat = snowflake_cJSON_GetObjectItem(data,
                                                               "encryptionMaterial");

                // In put command response, value of encryptionMaterial is an
                // object, which in get command response, value is an array of
                // object since different remote files might have different
                // encryption material
                if (snowflake_cJSON_IsArray(enc_mat))
                {
                    json_detach_array_from_object(
                      (cJSON **) (&sfstmt->put_get_response->enc_mat_get),
                      data, "encryptionMaterial");
                }
                else
                {
                    json_copy_string(
                      &sfstmt->put_get_response->enc_mat_put->query_stage_master_key,
                      enc_mat, "queryStageMasterKey");
                    json_copy_string_no_alloc(
                      sfstmt->put_get_response->enc_mat_put->query_id,
                      enc_mat, "queryId", SF_UUID4_LEN);
                    json_copy_int(&sfstmt->put_get_response->enc_mat_put->smk_id,
                                  enc_mat, "smkId");
                }

                cJSON *stage_info = snowflake_cJSON_GetObjectItem(data,
                                                                  "stageInfo");
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
                    &sfstmt->put_get_response->localLocation, data,
                    "localLocation");

            } else {
                // Set Database info
                _mutex_lock(&sfstmt->connection->mutex_parameters);
                /* Set other parameters. Ignore the status */
                _set_current_objects(sfstmt, data);
                _set_parameters_session_info(sfstmt->connection, data);
                _mutex_unlock(&sfstmt->connection->mutex_parameters);
                int64 stmt_type_id;
                if (json_copy_int(&stmt_type_id, data, "statementTypeId")) {
                    /* failed to get statement type id */
                    sfstmt->is_dml = SF_BOOLEAN_FALSE;
                } else {
                    sfstmt->is_dml = detect_stmt_type(stmt_type_id);
                }
                rowtype = snowflake_cJSON_GetObjectItem(data, "rowtype");
                if (snowflake_cJSON_IsArray(rowtype)) {
                    sfstmt->total_fieldcount = snowflake_cJSON_GetArraySize(
                      rowtype);
                    _snowflake_stmt_desc_reset(sfstmt);
                    sfstmt->desc = set_description(rowtype);
                }
                stats = snowflake_cJSON_GetObjectItem(data, "stats");
                if (snowflake_cJSON_IsObject(stats)) {
                    _snowflake_stmt_row_metadata_reset(sfstmt);
                    sfstmt->stats = set_stats(stats);
                } else {
                    sfstmt->stats = NULL;
                }
                // Set results array
                if (json_detach_array_from_object(
                    (cJSON **) (&sfstmt->raw_results),
                    data, "rowset")) {
                    log_error("No valid rowset found in response");
                    SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error,
                                             SF_STATUS_ERROR_BAD_JSON,
                                             "Missing rowset from response. No results found.",
                                             SF_SQLSTATE_APP_REJECT_CONNECTION,
                                             sfstmt->sfqid);
                    goto cleanup;
                }
                if (json_copy_int(&sfstmt->total_rowcount, data, "total")) {
                    log_warn(
                        "No total count found in response. Reverting to using array size of results");
                    sfstmt->total_rowcount = snowflake_cJSON_GetArraySize(
                      sfstmt->raw_results);
                }
                // Get number of rows in this chunk
                sfstmt->chunk_rowcount = snowflake_cJSON_GetArraySize(
                  sfstmt->raw_results);

                // Index starts at 0 and incremented each fetch
                sfstmt->total_row_index = 0;

                // Set large result set if one exists
                if ((chunks = snowflake_cJSON_GetObjectItem(data, "chunks")) != NULL) {
                    // We don't care if there is no qrmk, so ignore return code
                    json_copy_string(&qrmk, data, "qrmk");
                    chunk_headers = snowflake_cJSON_GetObjectItem(data,
                                                                  "chunkHeaders");
                    sfstmt->chunk_downloader = chunk_downloader_init(
                        qrmk,
                        chunk_headers,
                        chunks,
                        2, // thread count
                        4, // fetch slot
                        &sfstmt->error,
                        sfstmt->connection->insecure_mode);
                    if (!sfstmt->chunk_downloader) {
                        // Unable to create chunk downloader. Error is set in chunk_downloader_init function.
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
                code = (int64) atol(codeJson->valuestring);
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
    SF_FREE(s_resp);
    SF_FREE(qrmk);

    return ret;
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
    return sfstmt->total_rowcount;
}

int64 STDCALL snowflake_num_fields(SF_STMT *sfstmt) {
    if (!sfstmt) {
        return -1;
    }
    return sfstmt->total_fieldcount;
}

uint64 STDCALL snowflake_num_params(SF_STMT *sfstmt) {
    if (!sfstmt) {
        // TODO change to -1?
        return 0;
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
    sb_memcpy(&sf->error, sizeof(SF_ERROR_STRUCT), &sfstmt->error, sizeof(SF_ERROR_STRUCT));
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
        sb_strncpy(sf->error.msg, len + 1, sfstmt->error.msg, len);
    }
    return SF_STATUS_SUCCESS;
}

// Make sure that idx is in bounds and that column exists
SF_STATUS STDCALL _snowflake_get_cJSON_column(SF_STMT *sfstmt, int idx, cJSON **column_ptr) {
    if (idx > snowflake_num_fields(sfstmt) || idx <= 0) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, SF_STATUS_ERROR_OUT_OF_BOUNDS,
                                 "Column index must be between 1 and snowflake_num_fields()", "", sfstmt->sfqid);
        return SF_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    cJSON *column = snowflake_cJSON_GetArrayItem(sfstmt->cur_row, idx - 1);
    if (!column) {
        *column_ptr = NULL;
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, SF_STATUS_ERROR_MISSING_COLUMN_IN_ROW,
                                 "Column is missing from row.", "", sfstmt->sfqid);
        return SF_STATUS_ERROR_MISSING_COLUMN_IN_ROW;
    }

    *column_ptr = column;
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

SF_STATUS STDCALL snowflake_column_as_boolean(SF_STMT *sfstmt, int idx, sf_bool *value_ptr) {
    SF_STATUS status;
    cJSON *column = NULL;
    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    // Get column
    if ((status = _snowflake_get_cJSON_column(sfstmt, idx, &column)) != SF_STATUS_SUCCESS) {
        return status;
    }

    sf_bool value = SF_BOOLEAN_FALSE;
    if (snowflake_cJSON_IsNull(column)) {
        status = SF_STATUS_SUCCESS;
        goto cleanup;
    }

    char *endptr;
    errno = 0;
    switch (sfstmt->desc[idx - 1].c_type) {
        case SF_C_TYPE_BOOLEAN:
            value = strcmp("1", column->valuestring) == 0 ? SF_BOOLEAN_TRUE: SF_BOOLEAN_FALSE;
            break;
        case SF_C_TYPE_FLOAT64: ;
            float64 float_val = strtod(column->valuestring, &endptr);
            // Check for errors
            if (endptr == column->valuestring) {
                SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, SF_STATUS_ERROR_CONVERSION_FAILURE,
                                         "Cannot convert value into boolean from float64", "", sfstmt->sfqid);
                status = SF_STATUS_ERROR_CONVERSION_FAILURE;
                goto cleanup;
            }
            if (errno == ERANGE || float_val == INFINITY || float_val == -INFINITY) {
                SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, SF_STATUS_ERROR_OUT_OF_RANGE,
                                         "Value out of range for float64. Cannot convert value into boolean", "", sfstmt->sfqid);
                status = SF_STATUS_ERROR_OUT_OF_RANGE;
                goto cleanup;
            }
            // Determine if true or false
            value = (float_val == 0.0) ? SF_BOOLEAN_FALSE : SF_BOOLEAN_TRUE;
            break;
        case SF_C_TYPE_INT64: ;
            int64 int_val = strtoll(column->valuestring, &endptr, 10);
            // Check for errors
            if (endptr == column->valuestring) {
                SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, SF_STATUS_ERROR_CONVERSION_FAILURE,
                                         "Cannot convert value into boolean from int64", "", sfstmt->sfqid);
                status = SF_STATUS_ERROR_CONVERSION_FAILURE;
                goto cleanup;
            }
            if ((int_val == SF_INT64_MAX || int_val == SF_INT64_MIN) && errno == ERANGE) {
                SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, SF_STATUS_ERROR_OUT_OF_RANGE,
                                         "Value out of range for int64. Cannot convert value into boolean", "", sfstmt->sfqid);
                status = SF_STATUS_ERROR_OUT_OF_RANGE;
                goto cleanup;
            }
            // Determine if true or false
            value = (int_val == 0) ? SF_BOOLEAN_FALSE : SF_BOOLEAN_TRUE;
            break;
        case SF_C_TYPE_STRING:
            if (strlen(column->valuestring) == 0) {
                value = SF_BOOLEAN_FALSE;
            } else {
                value = SF_BOOLEAN_TRUE;
            }
            break;
        default:
            SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, SF_STATUS_ERROR_CONVERSION_FAILURE,
                                     "No valid conversion to boolean from data type", "", sfstmt->sfqid);
            status = SF_STATUS_ERROR_CONVERSION_FAILURE;
            goto cleanup;
    }

cleanup:
    *value_ptr = value;
    return status;
}

SF_STATUS STDCALL snowflake_column_as_uint8(SF_STMT *sfstmt, int idx, uint8 *value_ptr) {
    SF_STATUS status;
    cJSON *column = NULL;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    // Get column
    if ((status = _snowflake_get_cJSON_column(sfstmt, idx, &column)) != SF_STATUS_SUCCESS) {
        return status;
    }

    *value_ptr = (!snowflake_cJSON_IsNull(column)) ? (uint8) column->valuestring[0] : (uint8) 0;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL snowflake_column_as_uint32(SF_STMT *sfstmt, int idx, uint32 *value_ptr) {
    SF_STATUS status;
    cJSON *column = NULL;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    // Get column
    if ((status = _snowflake_get_cJSON_column(sfstmt, idx, &column)) != SF_STATUS_SUCCESS) {
        return status;
    }

    uint64 value = 0;
    if (snowflake_cJSON_IsNull(column)) {
        status = SF_STATUS_SUCCESS;
        goto cleanup;
    }

    char *endptr;
    errno = 0;
    value = strtoull(column->valuestring, &endptr, 10);
    // Check for errors
    if (endptr == column->valuestring) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, SF_STATUS_ERROR_CONVERSION_FAILURE,
                                 "Cannot convert value into uint32", "", sfstmt->sfqid);
        status = SF_STATUS_ERROR_CONVERSION_FAILURE;
        goto cleanup;
    }
    sf_bool neg = (strchr(column->valuestring, '-') != NULL) ? SF_BOOLEAN_TRUE: SF_BOOLEAN_FALSE;
    // Check for out of range
    if (((value == ULONG_MAX || value == 0) && errno == ERANGE) ||
            (!neg && value > SF_UINT32_MAX) ||
            (neg && value < (SF_UINT64_MAX - SF_UINT32_MAX))) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, SF_STATUS_ERROR_OUT_OF_RANGE,
                                 "Value out of range for uint32", "", sfstmt->sfqid);
        status = SF_STATUS_ERROR_OUT_OF_RANGE;
        goto cleanup;
    }
    // If the input was negative, we have to do a little trickery to get it into uint32 form
    if (neg && value > SF_UINT32_MAX) {
        value = value - (SF_UINT64_MAX - SF_UINT32_MAX);
    }
    // Everything checks out, set value and return success
    status = SF_STATUS_SUCCESS;

cleanup:
    *value_ptr = (uint32) value;
    return status;
}

SF_STATUS STDCALL snowflake_column_as_uint64(SF_STMT *sfstmt, int idx, uint64 *value_ptr) {
    SF_STATUS status;
    cJSON *column = NULL;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    // Get column
    if ((status = _snowflake_get_cJSON_column(sfstmt, idx, &column)) != SF_STATUS_SUCCESS) {
        return status;
    }

    uint64 value = 0;
    if (snowflake_cJSON_IsNull(column)) {
        status = SF_STATUS_SUCCESS;
        goto cleanup;
    }

    char *endptr;
    errno = 0;
    value = strtoull(column->valuestring, &endptr, 10);
    // Check for errors
    if (endptr == column->valuestring) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, SF_STATUS_ERROR_CONVERSION_FAILURE,
                                 "Cannot convert value into uint64", "", sfstmt->sfqid);
        status = SF_STATUS_ERROR_CONVERSION_FAILURE;
        goto cleanup;
    }
    if ((value == SF_UINT64_MAX || value == 0) && errno == ERANGE) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, SF_STATUS_ERROR_OUT_OF_RANGE,
                                 "Value out of range for uint64", "", sfstmt->sfqid);
        status = SF_STATUS_ERROR_OUT_OF_RANGE;
        goto cleanup;
    }
    // Everything checks out, set value and return success
    status = SF_STATUS_SUCCESS;

cleanup:
    *value_ptr = value;
    return status;
}

SF_STATUS STDCALL snowflake_column_as_int8(SF_STMT *sfstmt, int idx, int8 *value_ptr) {
    SF_STATUS status;
    cJSON *column = NULL;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    // Get column
    if ((status = _snowflake_get_cJSON_column(sfstmt, idx, &column)) != SF_STATUS_SUCCESS) {
        return status;
    }

    *value_ptr = (!snowflake_cJSON_IsNull(column)) ? (int8) column->valuestring[0] : (int8) 0;
    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL snowflake_column_as_int32(SF_STMT *sfstmt, int idx, int32 *value_ptr) {
    SF_STATUS status;
    cJSON *column = NULL;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    // Get column
    if ((status = _snowflake_get_cJSON_column(sfstmt, idx, &column)) != SF_STATUS_SUCCESS) {
        return status;
    }

    int64 value = 0;
    if (snowflake_cJSON_IsNull(column)) {
        status = SF_STATUS_SUCCESS;
        goto cleanup;
    }

    char *endptr;
    errno = 0;
    value = strtoll(column->valuestring, &endptr, 10);
    // Check for errors
    if (endptr == column->valuestring) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, SF_STATUS_ERROR_CONVERSION_FAILURE,
                                 "Cannot convert value into int32", "", sfstmt->sfqid);
        status = SF_STATUS_ERROR_CONVERSION_FAILURE;
        goto cleanup;
    }
    if (((value == LONG_MAX || value == LONG_MIN) && errno == ERANGE) || (value > SF_INT32_MAX || value < SF_INT32_MIN)) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, SF_STATUS_ERROR_OUT_OF_RANGE,
                                 "Value out of range for int32", "", sfstmt->sfqid);
        status = SF_STATUS_ERROR_OUT_OF_RANGE;
        goto cleanup;
    }
    // Everything checks out, set value and return success
    status = SF_STATUS_SUCCESS;

cleanup:
    *value_ptr = (int32) value;
    return status;
}

SF_STATUS STDCALL snowflake_column_as_int64(SF_STMT *sfstmt, int idx, int64 *value_ptr) {
    SF_STATUS status;
    cJSON *column = NULL;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    // Get column
    if ((status = _snowflake_get_cJSON_column(sfstmt, idx, &column)) != SF_STATUS_SUCCESS) {
        return status;
    }

    int64 value = 0;
    if (snowflake_cJSON_IsNull(column)) {
        status = SF_STATUS_SUCCESS;
        goto cleanup;
    }

    char *endptr;
    errno = 0;
    value = strtoll(column->valuestring, &endptr, 10);
    // Check for errors
    if (endptr == column->valuestring) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, SF_STATUS_ERROR_CONVERSION_FAILURE,
                                 "Cannot convert value into int64", "", sfstmt->sfqid);
        status = SF_STATUS_ERROR_CONVERSION_FAILURE;
        goto cleanup;
    }
    if ((value == SF_INT64_MAX || value == SF_INT64_MIN) && errno == ERANGE) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, SF_STATUS_ERROR_OUT_OF_RANGE,
                                 "Value out of range for int64", "", sfstmt->sfqid);
        status = SF_STATUS_ERROR_OUT_OF_RANGE;
        goto cleanup;
    }
    // Everything checks out, set value and return success
    status = SF_STATUS_SUCCESS;

cleanup:
    *value_ptr = value;
    return status;
}

SF_STATUS STDCALL snowflake_column_as_float32(SF_STMT *sfstmt, int idx, float32 *value_ptr) {
    SF_STATUS status;
    cJSON *column = NULL;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    // Get column
    if ((status = _snowflake_get_cJSON_column(sfstmt, idx, &column)) != SF_STATUS_SUCCESS) {
        return status;
    }

    float32 value = 0.0;
    if (snowflake_cJSON_IsNull(column)) {
        status = SF_STATUS_SUCCESS;
        goto cleanup;
    }

    char *endptr;
    errno = 0;
    value = strtof(column->valuestring, &endptr);
    // Check for errors
    if (endptr == column->valuestring) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, SF_STATUS_ERROR_CONVERSION_FAILURE,
                                 "Cannot convert value into float32", "", sfstmt->sfqid);
        status = SF_STATUS_ERROR_CONVERSION_FAILURE;
        goto cleanup;
    }
    if (errno == ERANGE || value == INFINITY || value == -INFINITY) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, SF_STATUS_ERROR_OUT_OF_RANGE,
                                 "Value out of range for float32", "", sfstmt->sfqid);
        status = SF_STATUS_ERROR_OUT_OF_RANGE;
        goto cleanup;
    }
    // Everything checks out, set value and return success
    status = SF_STATUS_SUCCESS;

cleanup:
    *value_ptr = value;
    return status;
}

SF_STATUS STDCALL snowflake_column_as_float64(SF_STMT *sfstmt, int idx, float64 *value_ptr) {
    SF_STATUS status;
    cJSON *column = NULL;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    // Get column
    if ((status = _snowflake_get_cJSON_column(sfstmt, idx, &column)) != SF_STATUS_SUCCESS) {
        return status;
    }

    float64 value = 0.0;
    if (snowflake_cJSON_IsNull(column)) {
        status = SF_STATUS_SUCCESS;
        goto cleanup;
    }

    char *endptr;
    errno = 0;
    value = strtod(column->valuestring, &endptr);
    // Check for errors
    if (endptr == column->valuestring) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, SF_STATUS_ERROR_CONVERSION_FAILURE,
                                 "Cannot convert value into float64", "", sfstmt->sfqid);
        status = SF_STATUS_ERROR_CONVERSION_FAILURE;
        goto cleanup;
    }
    if (errno == ERANGE || value == INFINITY || value == -INFINITY) {
        SET_SNOWFLAKE_STMT_ERROR(&sfstmt->error, SF_STATUS_ERROR_OUT_OF_RANGE,
                                 "Value out of range for float64", "", sfstmt->sfqid);
        status = SF_STATUS_ERROR_OUT_OF_RANGE;
        goto cleanup;
    }
    // Everything checks out, set value and return success
    status = SF_STATUS_SUCCESS;

cleanup:
    *value_ptr = value;
    return status;
}

SF_STATUS STDCALL snowflake_column_as_timestamp(SF_STMT *sfstmt, int idx, SF_TIMESTAMP *value_ptr) {
    SF_STATUS status;
    cJSON *column = NULL;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    // Get column
    if ((status = _snowflake_get_cJSON_column(sfstmt, idx, &column)) != SF_STATUS_SUCCESS) {
        return status;
    }

    SF_DB_TYPE db_type = sfstmt->desc[idx - 1].type;
    if (snowflake_cJSON_IsNull(column)) {
        snowflake_timestamp_from_parts(value_ptr, 0, 0, 0, 0, 1, 1, 1970, 0, 9, SF_DB_TYPE_TIMESTAMP_NTZ);
        return SF_STATUS_SUCCESS;
    }

    if (db_type == SF_DB_TYPE_DATE ||
        db_type == SF_DB_TYPE_TIME ||
        db_type == SF_DB_TYPE_TIMESTAMP_LTZ ||
        db_type == SF_DB_TYPE_TIMESTAMP_NTZ ||
        db_type == SF_DB_TYPE_TIMESTAMP_TZ) {
        return snowflake_timestamp_from_epoch_seconds(value_ptr,
                                                      column->valuestring,
                                                      sfstmt->connection->timezone,
                                                      (int32) sfstmt->desc[idx - 1].scale,
                                                      db_type);
    } else {
        return SF_STATUS_ERROR_CONVERSION_FAILURE;
    }
}

SF_STATUS STDCALL snowflake_column_as_const_str(SF_STMT *sfstmt, int idx, const char **value_ptr) {
    SF_STATUS status;
    cJSON *column = NULL;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    // Get column
    if ((status = _snowflake_get_cJSON_column(sfstmt, idx, &column)) != SF_STATUS_SUCCESS) {
        return status;
    }

    *value_ptr = (!snowflake_cJSON_IsNull(column)) ? column->valuestring : NULL;

    return SF_STATUS_SUCCESS;
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

    if (isNull != SF_BOOLEAN_TRUE && isNull != SF_BOOLEAN_FALSE)
    {
      // If nullablity is not provided try to extract if from the input raw value
      if (strncmp(const_str_val, "null", 4) == 0)
      {
        isNull = SF_BOOLEAN_TRUE;
      }
      else{
        isNull = SF_BOOLEAN_FALSE;
      }
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
      sb_strncpy(value, max_value_size, "", 1);
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
        sb_strncpy(value, max_value_size, bool_value, value_len + 1);
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
        sb_strncpy(value, max_value_size, const_str_val, value_len + 1);
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
    cJSON *column = NULL;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    // Get column
    if ((status = _snowflake_get_cJSON_column(sfstmt, idx, &column)) != SF_STATUS_SUCCESS) {
        return status;
    }

  return snowflake_raw_value_to_str_rep(sfstmt, column->valuestring,
                                        sfstmt->desc[idx - 1].type,
                                        sfstmt->connection->timezone,
                                        (int32) sfstmt->desc[idx - 1].scale,
                                        snowflake_cJSON_IsNull(column) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE,
                                        value_ptr, value_len_ptr,
                                        max_value_size_ptr);
}

SF_STATUS STDCALL snowflake_column_strlen(SF_STMT *sfstmt, int idx, size_t *value_ptr) {
    SF_STATUS status;
    cJSON *column = NULL;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    // Get column
    if ((status = _snowflake_get_cJSON_column(sfstmt, idx, &column)) != SF_STATUS_SUCCESS) {
        return status;
    }

    if (snowflake_cJSON_IsNull(column)) {
        *value_ptr = 0;
    } else {
        *value_ptr = strlen(snowflake_cJSON_GetArrayItem(sfstmt->cur_row, idx - 1)->valuestring);
    }

    return SF_STATUS_SUCCESS;
}

SF_STATUS STDCALL snowflake_column_is_null(SF_STMT *sfstmt, int idx, sf_bool *value_ptr) {
    SF_STATUS status;
    cJSON *column = NULL;

    if ((status = _snowflake_column_null_checks(sfstmt, (void *) value_ptr)) != SF_STATUS_SUCCESS) {
        return status;
    }

    // Get column
    if ((status = _snowflake_get_cJSON_column(sfstmt, idx, &column)) != SF_STATUS_SUCCESS) {
        return status;
    }

    *value_ptr = snowflake_cJSON_IsNull(column) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;

    return SF_STATUS_SUCCESS;
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
    log_info("sec: %lld, nsec: %lld", sec, nsec);
    // Transform nsec to a 9 digit number to store in the timestamp struct
    ts->nsec = (int32) (nsec * pow10_int64[9-ts->scale]);

    if (ts->ts_type == SF_DB_TYPE_TIMESTAMP_TZ) {
        /* make up Timezone name from the tzoffset */
        ldiv_t dm = ldiv((long) tzoffset, 60L);
        sb_sprintf(tzname, sizeof(tzname), "UTC%c%02ld:%02ld",
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
        const char *prev_tz_ptr = sf_getenv("TZ");
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
    sb_sprintf(fmt_static, sizeof(fmt_static), ".%%0%dld", ts->scale);

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
        len += sb_sprintf(
          &(buffer)[len],
          max_len - len, fmt_static,
          nsec);
    }
    if (ts->ts_type == SF_DB_TYPE_TIMESTAMP_TZ) {
        /* Timezone info */
        ldiv_t dm = ldiv((long) ts->tzoffset, 60L);
        len += sb_sprintf(
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

    time_t epoch_time_local;

    ts->tm_obj.tm_isdst = -1;
    epoch_time_local = (time_t) mktime(&ts->tm_obj);
    // mktime takes into account tm_gmtoff which is a Linux and OS X ONLY field
#if defined(__linux__) || defined(__APPLE__)
    epoch_time_local += ts->tm_obj.tm_gmtoff;
#endif
    *epoch_time_ptr = epoch_time_local - (ts->tzoffset * 60);

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

