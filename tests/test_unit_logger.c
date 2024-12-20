/*
 * Copyright (c) 2018-2025 Snowflake Computing, Inc. All rights reserved.
 */


#include "utils/test_setup.h"
#include <snowflake/client_config_parser.h>
#include "memory.h"

#ifndef _WIN32
#include <unistd.h>
#else
#define F_OK 0
inline int access(const char* pathname, int mode) {
  return _access(pathname, mode);
}
#endif

/**
 * Tests converting a string representation of log level to the log level enum
 */
void test_log_str_to_level() {
    assert_int_equal(log_from_str_to_level("TRACE"), SF_LOG_TRACE);
    assert_int_equal(log_from_str_to_level("DEBUG"), SF_LOG_DEBUG);
    assert_int_equal(log_from_str_to_level("INFO"), SF_LOG_INFO);
    assert_int_equal(log_from_str_to_level("wArN"), SF_LOG_WARN);
    assert_int_equal(log_from_str_to_level("erroR"), SF_LOG_ERROR);
    assert_int_equal(log_from_str_to_level("fatal"), SF_LOG_FATAL);

    /* negative */
    assert_int_equal(log_from_str_to_level("hahahaha"), SF_LOG_FATAL);
    assert_int_equal(log_from_str_to_level(NULL), SF_LOG_FATAL);
}

/**
 * Tests log settings with invalid client config filepath
 */
void test_invalid_client_config_path() {
  char configFilePath[] = "fakePath.json";

  // Parse client config for log details
  client_config clientConfig = { 0 };
  sf_bool result = load_client_config(configFilePath, &clientConfig);
#if (!defined(_WIN32) && !defined(_DEBUG)) || defined(_WIN64)
  assert_false(result);
#else
  assert_true(result);
#endif
}

/**
 * Tests log settings from client config file with invalid json
 */
void test_client_config_log_invalid_json() {
  char clientConfigJSON[] = "{{{\"invalid json\"}";
  char configFilePath[] = "sf_client_config.json";
  FILE* file;
  file = fopen(configFilePath, "w");
  fprintf(file, "%s", clientConfigJSON);
  fclose(file);

  // Parse client config for log details
  client_config clientConfig = { 0 };
  sf_bool result = load_client_config(configFilePath, &clientConfig);
#if (!defined(_WIN32) && !defined(_DEBUG)) || defined(_WIN64)
  assert_false(result);
#else
  assert_true(result);
#endif

  // Cleanup
  remove(configFilePath);
}

/**
 * Tests log settings from client config file with malformed json
 */
void test_client_config_log_malformed_json() {
  char clientConfigJSON[] = "[]";
  char configFilePath[] = "sf_client_config.json";
  FILE* file;
  file = fopen(configFilePath, "w");
  fprintf(file, "%s", clientConfigJSON);
  fclose(file);

  // Parse client config for log details
  client_config clientConfig = { 0 };
  sf_bool result = load_client_config(configFilePath, &clientConfig);
#if (!defined(_WIN32) && !defined(_DEBUG)) || defined(_WIN64)
  assert_false(result);
#else
  assert_true(result);
#endif

  // Cleanup
  remove(configFilePath);
}

/**
 * Tests log settings from client config file
 */
void test_client_config_log() {
    char clientConfigJSON[] = "{\"common\":{\"log_level\":\"warn\",\"log_path\":\"./test/\"}}";
    char configFilePath[] = "sf_client_config.json";
    FILE *file;
    file = fopen(configFilePath,"w");
    fprintf(file, "%s", clientConfigJSON);
    fclose(file);

    // Parse client config for log details
    client_config clientConfig = { 0 };
    load_client_config(configFilePath, &clientConfig);

    // Set log name and level
    char logname[] = "%s/dummy.log";
    size_t log_path_size = 1 + strlen(logname);
    assert_non_null(clientConfig.logPath);
    log_path_size += strlen(clientConfig.logPath);
    char* LOG_PATH = (char*)SF_CALLOC(1, log_path_size);
    sf_sprintf(LOG_PATH, log_path_size, logname, clientConfig.logPath);
    log_set_level(log_from_str_to_level(clientConfig.logLevel));
    log_set_path(LOG_PATH);

    // Ensure the log file doesn't exist at the beginning
    remove(LOG_PATH);

    // Info log won't trigger the log file creation since log level is set to warn in config
    log_info("dummy info log");
    assert_int_not_equal(access(LOG_PATH, F_OK), 0);

    // Warning log will trigger the log file creation
    log_warn("dummy warning log");
    assert_int_equal(access(LOG_PATH, F_OK), 0);
    log_close();

    // Cleanup
    remove(configFilePath);
    remove(LOG_PATH);
    SF_FREE(LOG_PATH);
}

/**
 * Tests log settings from client config file via global init
 */
void test_client_config_log_init() {
  char LOG_PATH[MAX_PATH] = { 0 };
  char clientConfigJSON[] = "{\"common\":{\"log_level\":\"warn\",\"log_path\":\"./test/\"}}";
  char configFilePath[] = "sf_client_config.json";
  FILE* file;
  file = fopen(configFilePath, "w");
  fprintf(file, "%s", clientConfigJSON);
  fclose(file);

  snowflake_global_set_attribute(SF_GLOBAL_CLIENT_CONFIG_FILE, configFilePath);
  snowflake_global_init("./logs", SF_LOG_DEFAULT, NULL);

  // Get the log path determined by libsnowflakeclient
  snowflake_global_get_attribute(SF_GLOBAL_LOG_PATH, LOG_PATH, MAX_PATH);
  // Ensure the log file doesn't exist at the beginning
  remove(LOG_PATH);

  // Info log won't trigger the log file creation since log level is set to warn in config
  log_info("dummy info log");
  assert_int_not_equal(access(LOG_PATH, F_OK), 0);

  // Warning log will trigger the log file creation
  log_warn("dummy warning log");
  assert_int_equal(access(LOG_PATH, F_OK), 0);
  log_close();

  // Cleanup
  remove(configFilePath);
  remove(LOG_PATH);
}

/**
 * Tests log settings from client config file via global init in home dir
 */
void test_client_config_log_init_home_config() {
  char LOG_PATH[MAX_PATH] = { 0 };

  char clientConfigJSON[] = "{\"common\":{\"log_level\":\"warn\",\"log_path\":\"./test/\"}}";

  char envbuf[MAX_PATH + 1];
#ifdef _WIN32
  char* homeDir = sf_getenv_s("USERPROFILE", envbuf, sizeof(envbuf));
#else
  char* homeDir = sf_getenv_s("HOME", envbuf, sizeof(envbuf));
#endif
  char configFile[] = "/sf_client_config.json";
  size_t log_path_size = strlen(homeDir) + strlen(configFile) + 1;
  char *configFilePath = (char*)SF_CALLOC(1, log_path_size);
  sf_strcat(configFilePath, log_path_size, homeDir);
  sf_strcat(configFilePath, log_path_size, configFile);
  FILE* file;
  file = fopen(configFilePath, "w");
  fprintf(file, "%s", clientConfigJSON);
  fclose(file);

  snowflake_global_set_attribute(SF_GLOBAL_CLIENT_CONFIG_FILE, "");
  snowflake_global_init("./logs", SF_LOG_DEFAULT, NULL);

  // Get the log path determined by libsnowflakeclient
  snowflake_global_get_attribute(SF_GLOBAL_LOG_PATH, LOG_PATH, MAX_PATH);
  // Ensure the log file doesn't exist at the beginning
  remove(LOG_PATH);

  // Info log won't trigger the log file creation since log level is set to warn in config
  log_info("dummy info log");
  assert_int_not_equal(access(LOG_PATH, F_OK), 0);

  // Warning log will trigger the log file creation
  log_warn("dummy warning log");
  assert_int_equal(access(LOG_PATH, F_OK), 0);
  log_close();

  // Cleanup
  remove(configFilePath);
  remove(LOG_PATH);
  SF_FREE(configFilePath);
}

/**
 * Tests log settings from client config file without log_path
 */
void test_client_config_log_no_level() {
  char LOG_PATH[MAX_PATH] = { 0 };
  char clientConfigJSON[] = "{\"common\":{\"log_path\":\"./test/\"}}";
  char configFilePath[] = "sf_client_config.json";
  FILE* file;
  file = fopen(configFilePath, "w");
  fprintf(file, "%s", clientConfigJSON);
  fclose(file);

  snowflake_global_set_attribute(SF_GLOBAL_CLIENT_CONFIG_FILE, configFilePath);
  snowflake_global_init("./logs", SF_LOG_DEFAULT, NULL);

  // Get the log path determined by libsnowflakeclient
  snowflake_global_get_attribute(SF_GLOBAL_LOG_PATH, LOG_PATH, MAX_PATH);
  // Ensure the log file doesn't exist at the beginning
  remove(LOG_PATH);

  // Info log won't trigger the log file creation since log level default is fatal
  log_info("dummy info log");
  assert_int_not_equal(access(LOG_PATH, F_OK), 0);

  // Warning log won't trigger the log file creation since log level default is fatal
  log_warn("dummy warning log");
  assert_int_not_equal(access(LOG_PATH, F_OK), 0);

  // Fatal log will trigger the log file creation
  log_fatal("dummy fatal log");
  assert_int_equal(access(LOG_PATH, F_OK), 0);
  log_close();

  // Cleanup
  remove(configFilePath);
  remove(LOG_PATH);
}

/**
 * Tests log settings from client config file without log_level
 */
void test_client_config_log_no_path() {
  char LOG_PATH[MAX_PATH] = { 0 };
  char LOG_SUBPATH[MAX_PATH] = { 0 };
  char clientConfigJSON[] = "{\"common\":{\"log_level\":\"warn\"}}";
  char configFilePath[] = "sf_client_config.json";
  FILE* file;
  file = fopen(configFilePath, "w");
  fprintf(file, "%s", clientConfigJSON);
  fclose(file);

  snowflake_global_set_attribute(SF_GLOBAL_CLIENT_CONFIG_FILE, configFilePath);
  snowflake_global_init("./logs", SF_LOG_DEFAULT, NULL);

  // Get the log path determined by libsnowflakeclient
  snowflake_global_get_attribute(SF_GLOBAL_LOG_PATH, LOG_PATH, MAX_PATH);
  memcpy(LOG_SUBPATH, &LOG_PATH[0], 6);
  assert_string_equal(LOG_SUBPATH, "./logs");
  // Ensure the log file doesn't exist at the beginning
  remove(LOG_PATH);

  // Info log won't trigger the log file creation since log level is set to warn in config
  log_info("dummy info log");
  assert_int_not_equal(access(LOG_PATH, F_OK), 0);

  // Warning log will trigger the log file creation
  log_warn("dummy warning log");
  assert_int_equal(access(LOG_PATH, F_OK), 0);
  log_close();

  // Cleanup
  remove(configFilePath);
  remove(LOG_PATH);
}

/**
 * Tests timing of log file creation
 */
void test_log_creation() {
    char logname[] = "dummy.log";

    // ensure the log file doesn't exist at the beginning
    remove(logname);
    assert_int_not_equal(access(logname, F_OK), 0);

    log_set_lock(NULL);
    log_set_level(SF_LOG_WARN);
    log_set_quiet(1);
    log_set_path(logname);

    // info log won't trigger the log file creation since log level is set to warning
    log_info("dummy info log");
    assert_int_not_equal(access(logname, F_OK), 0);

    // warning log will trigger the log file creation
    log_warn("dummy warning log");
    assert_int_equal(access(logname, F_OK), 0);
    log_close();

    remove(logname);
}

#ifndef _WIN32
/**
 * Tests masking secret information in log
 */
void test_mask_secret_log() {
    FILE* fp = fopen("dummy.log", "w+");
    assert_non_null(fp);
    log_set_lock(NULL);
    log_set_level(SF_LOG_TRACE);
    log_set_quiet(1);
    log_set_fp(fp);

    const char * logtext[][2] = {
        {//0
            "Secure log record!",
            "Secure log record!"
        },
        {//1
            "Token =ETMsDgAAAXI0IS9NABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwCAABAAEEb/xAQlmT+mwIx9G32E+ikAAACA/CPlEkq//+jWZnQkOj5VhjayruDsCVRGS/B6GzHUugXLc94EfEwuto94gS/oKSVrUg/JRPekypLAx4Afa1KW8n1RqXRF9Hzy1VVLmVEBMtei3yFJPNSHtfbeFHSr9eVB/OL8dOGbxQluGCh6XmaqTjyrh3fqUTWz7+n74+gu2ugAFFZ18iT+DStK0TTdmy4vBC6xUcHQ",
            "Token =****"
        },
        {//2
            "idToken : ETMsDgAAAXI0IS9NABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwCAABAAEEb/xAQlmT+mwIx9G32E+ikAAACA/CPlEkq//+jWZnQkOj5VhjayruDsCVRGS/B6GzHUugXLc94EfEwuto94gS/oKSVrUg/JRPekypLAx4Afa1KW8n1RqXRF9Hzy1VVLmVEBMtei3yFJPNSHtfbeFHSr9eVB/OL8dOGbxQluGCh6XmaqTjyrh3fqUTWz7+n74+gu2ugAFFZ18iT+DStK0TTdmy4vBC6xUcHQ",
            "idToken : ****"
        },
        {//3
            "sessionToken:ETMsDgAAAXI0IS9NABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwCAABAAEEb/xAQlmT+mwIx9G32E+ikAAACA/CPlEkq//+jWZnQkOj5VhjayruDsCVRGS/B6GzHUugXLc94EfEwuto94gS/oKSVrUg/JRPekypLAx4Afa1KW8n1RqXRF9Hzy1VVLmVEBMtei3yFJPNSHtfbeFHSr9eVB/OL8dOGbxQluGCh6XmaqTjyrh3fqUTWz7+n74+gu2ugAFFZ18iT+DStK0TTdmy4vBC6xUcHQ",
            "sessionToken:****"
        },
        {//4
            "masterToken : 'ETMsDgAAAXI0IS9NABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwCAABAAEEb/xAQlmT+mwIx9G32E+ikAAACA/CPlEkq//+jWZnQkOj5VhjayruDsCVRGS/B6GzHUugXLc94EfEwuto94gS/oKSVrUg/JRPekypLAx4Afa1KW8n1RqXRF9Hzy1VVLmVEBMtei3yFJPNSHtfbeFHSr9eVB/OL8dOGbxQluGCh6XmaqTjyrh3fqUTWz7+n74+gu2ugAFFZ18iT+DStK0TTdmy4vBC6xUcHQ'",
            "masterToken : '****'"
        },
        {//5
            "assertion content:\"ETMsDgAAAXI0IS9NABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwCAABAAEEb/xAQlmT+mwIx9G32E+ikAAACA/CPlEkq//+jWZnQkOj5VhjayruDsCVRGS/B6GzHUugXLc94EfEwuto94gS/oKSVrUg/JRPekypLAx4Afa1KW8n1RqXRF9Hzy1VVLmVEBMtei3yFJPNSHtfbeFHSr9eVB/OL8dOGbxQluGCh6XmaqTjyrh3fqUTWz7+n74+gu2ugAFFZ18iT+DStK0TTdmy4vBC6xUcHQ\"",
            "assertion content:\"****\""
        },
        {//6
            "password: random!TEST/-pwd=123++#",
            "password: ****"
        },
        {//7
            "pwd =\"random!TEST/-pwd=123++#",
            "pwd =\"****"
        },
        {//8
            "AWSAccessKeyId=ABCD%efg+1234/567",
            "AWSAccessKeyId=****"
        },
        {//9
            "https://sfc-fake.s3.fakeamazon.com/012345xx-012x-012x-0123-1a2b3c4d/fake/data_fake?x-amz-server-side-encryption-customer-algorithm=fakealgo&response-content-encoding=fakezip&AWSAccessKeyId=ABCD%efg+1234/567&Expires=123456789&Signature=ABCD%efg+1234/567ABCD%efg+1234/567",
            "https://sfc-fake.s3.fakeamazon.com/012345xx-012x-012x-0123-1a2b3c4d/fake/data_fake?x-amz-server-side-encryption-customer-algorithm=fakealgo&response-content-encoding=fakezip&AWSAccessKeyId=****&Expires=123456789&Signature=****"
        },
        {//10
            "aws_key_id='afhl124lomsafho0582'",
            "aws_key_id='****'"
        },
        {//11
            "aws_secret_key = 'dfhuwaojm753omsdfh30oi+fj'",
            "aws_secret_key = '****'"
        },
        {//12
            "\"privateKeyData\": \"abcdefghijk\"",
            "\"privateKeyData\": \"XXXX\""
        },
    };

    char * line = NULL;
    size_t len = 0;
    for (int i = 0; i < 13; i++)
    {
        fseek(fp, 0, SEEK_SET);
        log_trace("%s", logtext[i][0]);
        fseek(fp, 0, SEEK_SET);
        len = getline(&line, &len, fp);
        if (i != 0)
        {
            assert_null(strstr(line, logtext[i][0]));
        }
        assert_non_null(strstr(line, logtext[i][1]));
    }

    free(line);
    fclose(fp);
}
#endif

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_log_str_to_level),
        cmocka_unit_test(test_invalid_client_config_path),
        cmocka_unit_test(test_client_config_log_invalid_json),
        cmocka_unit_test(test_client_config_log_malformed_json),
#if (!defined(_WIN32) && !defined(_DEBUG)) || defined(_WIN64)
        cmocka_unit_test(test_client_config_log),
        cmocka_unit_test(test_client_config_log_init),
        cmocka_unit_test(test_client_config_log_init_home_config),
        cmocka_unit_test(test_client_config_log_no_level),
        cmocka_unit_test(test_client_config_log_no_path),
#endif
        cmocka_unit_test(test_log_creation),
#ifndef _WIN32
        cmocka_unit_test(test_mask_secret_log),
#endif
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
