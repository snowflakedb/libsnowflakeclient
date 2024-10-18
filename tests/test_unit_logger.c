/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */


#include "utils/test_setup.h"
#include <snowflake/client_config_parser.h>
#include "memory.h"

#ifdef _WIN32
inline int access(const char* pathname, int mode) {
  return _access(pathname, mode);
}
#else
#include <unistd.h>
#endif


/**
 * Tests converting a string representation of log level to the log level enum
 */
void test_log_str_to_level(void **unused) {
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
void test_invalid_client_config_path(void** unused) {
  char configFilePath[] = "fakePath.json";

  // Parse client config for log details
  client_config clientConfig = { .logLevel = "", .logPath = "" };
  sf_bool result = load_client_config(configFilePath, &clientConfig);
  assert_false(result);
}

/**
 * Tests log settings from client config file with invalid json
 */
void test_client_config_log_invalid_json(void** unused) {
  char clientConfigJSON[] = "{{{\"invalid json\"}";
  char configFilePath[] = "sf_client_config.json";
  FILE* file;
  file = fopen(configFilePath, "w");
  fprintf(file, "%s", clientConfigJSON);
  fclose(file);

  // Parse client config for log details
  client_config clientConfig = { .logLevel = "", .logPath = "" };
  sf_bool result = load_client_config(configFilePath, &clientConfig);
  assert_false(result);

  // Cleanup
  remove(configFilePath);
}

/**
 * Tests log settings from client config file
 */
void test_client_config_log(void **unused) {
    char clientConfigJSON[] = "{\"common\":{\"log_level\":\"warn\",\"log_path\":\"./test/\"}}";
    char configFilePath[] = "sf_client_config.json";
    FILE *file;
    file = fopen(configFilePath,"w");
    fprintf(file, "%s", clientConfigJSON);
    fclose(file);

    // Parse client config for log details
    client_config clientConfig = { .logLevel = "", .logPath = "" };
    load_client_config(configFilePath, &clientConfig);

    // Set log name and level
    char logname[] = "%s/dummy.log";
    size_t log_path_size = 1 + strlen(logname);
    log_path_size += strlen(clientConfig.logPath);
    char* LOG_PATH = (char*)SF_CALLOC(1, log_path_size);
    sf_sprintf(LOG_PATH, log_path_size, logname, clientConfig.logPath);
    log_set_level(log_from_str_to_level(clientConfig.logLevel));
    log_set_path(LOG_PATH);

    // Ensure the log file doesn't exist at the beginning
    remove(LOG_PATH);

    // Info log won't trigger the log file creation since log level is set to warn in config
    log_info("dummy info log");
    assert_int_not_equal(access(LOG_PATH, 0), 0);

    // Warning log will trigger the log file creation
    log_warn("dummy warning log");
    assert_int_equal(access(LOG_PATH, 0), 0);
    log_close();

    // Cleanup
    remove(configFilePath);
    remove(LOG_PATH);
}

/**
 * Tests log settings from client config file via global init
 */
void test_client_config_log_init(void** unused) {
  char LOG_PATH[MAX_PATH] = { 0 };
  char clientConfigJSON[] = "{\"common\":{\"log_level\":\"warn\",\"log_path\":\"./test/\"}}";
  char configFilePath[] = "sf_client_config.json";
  FILE* file;
  file = fopen(configFilePath, "w");
  fprintf(file, "%s", clientConfigJSON);
  fclose(file);

  snowflake_global_set_attribute(SF_GLOBAL_CLIENT_CONFIG_FILE, configFilePath);
  snowflake_global_init("./logs", SF_LOG_TRACE, NULL);

  // Get the log path determined by libsnowflakeclient
  snowflake_global_get_attribute(SF_GLOBAL_LOG_PATH, LOG_PATH, MAX_PATH);
  // Ensure the log file doesn't exist at the beginning
  remove(LOG_PATH);

  // Info log won't trigger the log file creation since log level is set to warn in config
  log_info("dummy info log");
  assert_int_not_equal(access(LOG_PATH, 0), 0);

  // Warning log will trigger the log file creation
  log_warn("dummy warning log");
  assert_int_equal(access(LOG_PATH, 0), 0);
  log_close();

  // Cleanup
  remove(configFilePath);
  remove(LOG_PATH);
}

/**
 * Tests timing of log file creation
 */
void test_log_creation(void **unused) {
    char logname[] = "dummy.log";

    // ensure the log file doesn't exist at the beginning
    remove(logname);
    assert_int_not_equal(access(logname, 0), 0);

    log_set_lock(NULL);
    log_set_level(SF_LOG_WARN);
    log_set_quiet(1);
    log_set_path(logname);

    // info log won't trigger the log file creation since log level is set to warning
    log_info("dummy info log");
    assert_int_not_equal(access(logname, 0), 0);

    // warning log will trigger the log file creation
    log_warn("dummy warning log");
    assert_int_equal(access(logname, 0), 0);
    log_close();

    remove(logname);
}

#ifndef _WIN32
/**
 * Tests masking secret information in log
 */
void test_mask_secret_log(void **unused) {
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
        getline(&line, &len, fp);
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
        cmocka_unit_test(test_client_config_log),
        cmocka_unit_test(test_client_config_log_init),
        cmocka_unit_test(test_log_creation),
#ifndef _WIN32
        cmocka_unit_test(test_mask_secret_log),
#endif
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
