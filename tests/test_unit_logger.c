#include "utils/test_setup.h"
#include <snowflake/client_config_parser.h>
#include "memory.h"
#include <stdio.h>
#include <sys/stat.h>
// #include <io.h>
#include <fcntl.h>

#include "../lib/http_perform.c"
#include <snowflake/logger.h>

#ifndef _WIN32
#include <unistd.h>
#include <pwd.h>
#define SF_TMP_FOLDER "/tmp/sf_client_config_folder"
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

void test_null_log_path() {
  char LOG_PATH[MAX_PATH] = { 0 };
  char LOG_LEVEL[64] = { 0 };

  // Pass in empty log path
  snowflake_global_init(NULL, SF_LOG_WARN, NULL);

  // Get the log path determined by libsnowflakeclient
  snowflake_global_get_attribute(SF_GLOBAL_LOG_PATH, LOG_PATH, MAX_PATH);
  char log_path_dir[6];
  strncpy(log_path_dir, LOG_PATH, 5);
  log_path_dir[5] = '\0';
  assert_string_equal(log_path_dir, "logs/");

  // Get the log level determined by libsnowflakeclient and ensure that it's correctly set
  snowflake_global_get_attribute(SF_GLOBAL_LOG_LEVEL, LOG_LEVEL, 64);
  assert_string_equal(LOG_LEVEL, "WARN");

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
  remove(LOG_PATH);
  remove(LOG_LEVEL);
  remove(log_path_dir);
}

void test_default_log_path() {
  char LOG_PATH[MAX_PATH] = { 0 };
  char LOG_LEVEL[64] = { 0 };

  // Pass in empty log path
  snowflake_global_init("", SF_LOG_WARN, NULL);

  // Get the log path determined by libsnowflakeclient
  snowflake_global_get_attribute(SF_GLOBAL_LOG_PATH, LOG_PATH, MAX_PATH);
  char log_path_dir[6];
  strncpy(log_path_dir, LOG_PATH, 5);
  log_path_dir[5] = '\0';
  assert_string_equal(log_path_dir, "logs/");

  // Get the log level determined by libsnowflakeclient and ensure that it's correctly set
  snowflake_global_get_attribute(SF_GLOBAL_LOG_LEVEL, LOG_LEVEL, 64);
  assert_string_equal(LOG_LEVEL, "WARN");

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
  remove(LOG_PATH);
  remove(LOG_LEVEL);
  remove(log_path_dir);
}

void test_invalid_client_config_path() {
  char configFilePath[] = "fakePath.json";

  // Parse client config for log details
  client_config clientConfig;
  sf_bool result = load_client_config(configFilePath, &clientConfig);
  assert_false(result);
}

void test_client_config_log_invalid_json() {
  char clientConfigJSON[] = "{{{\"invalid json\"}";
  char configFilePath[] = "sf_client_config.json";
  FILE* file;
  file = fopen(configFilePath, "w");
  fprintf(file, "%s", clientConfigJSON);
  fclose(file);

  // Parse client config for log details
  client_config clientConfig;
  sf_bool result = load_client_config(configFilePath, &clientConfig);
  assert_false(result);

  // Cleanup
  remove(configFilePath);
}

void test_client_config_log_malformed_json() {
  char clientConfigJSON[] = "[]";
  char configFilePath[] = "sf_client_config.json";
  FILE* file;
  file = fopen(configFilePath, "w");
  fprintf(file, "%s", clientConfigJSON);
  fclose(file);

  // Parse client config for log details
  client_config clientConfig;
  sf_bool result = load_client_config(configFilePath, &clientConfig);
  assert_false(result);

  // Cleanup
  remove(configFilePath);
}

void test_client_config_log() {
    char clientConfigJSON[] = "{\"common\":{\"log_level\":\"warn\",\"log_path\":\"./test/\"}}";
    char configFilePath[] = "sf_client_config.json";
    FILE *file;
    file = fopen(configFilePath,"w");
    fprintf(file, "%s", clientConfigJSON);
    fclose(file);

    // Parse client config for log details
    client_config clientConfig;
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

void test_client_config_log_unknown_entries() {
  char clientConfigJSON[] = "{\"common\":{\"log_level\":\"warn\",\"log_path\":\"./test/\",\"unknownEntry\":\"fakeValue\"}}";
  char configFilePath[] = "sf_client_config.json";
  char logPath[] = "./test/";
  char logLevel[] = "warn";
  FILE* file;
  file = fopen(configFilePath, "w");
  fprintf(file, "%s", clientConfigJSON);
  fclose(file);

  // Set log name and level
  char logname[] = "%s/dummy.log";
  size_t log_path_size = 1 + strlen(logname);
  log_path_size += strlen(logPath);
  char* LOG_PATH = (char*)SF_CALLOC(1, log_path_size);
  sf_sprintf(LOG_PATH, log_path_size, logname, logPath);
  log_set_level(log_from_str_to_level(logLevel));
  log_set_path(LOG_PATH);

  // Ensure the log file doesn't exist at the beginning
  remove(LOG_PATH);

  // Load client config and log unknown entries
  client_config clientConfig;
  load_client_config(configFilePath, &clientConfig);
  log_close();

  // Ensure that log file is created
  assert_int_equal(access(LOG_PATH, F_OK), 0);

  // Check unknown entries is logged
  char line[1024];
  sf_bool unknown_found = 0;
  file = fopen(LOG_PATH, "r");
  while (fgets(line, sizeof(line), file)) {
    if (strstr(line, "Unknown configuration entry:") != NULL) {
      unknown_found = 1;
    }
  }
  fclose(file);

  assert_int_equal(unknown_found, 1);

  // Cleanup
  remove(configFilePath);
  remove(LOG_PATH);
  SF_FREE(LOG_PATH);
}

void test_client_config_log_init() {
  char LOG_PATH[MAX_PATH] = { 0 };
  char LOG_LEVEL[64] = { 0 };
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
  // Get the log level determined by libsnowflakeclient and ensure that it's correctly set
  snowflake_global_get_attribute(SF_GLOBAL_LOG_LEVEL, LOG_LEVEL, 64);
  assert_string_equal(LOG_LEVEL, "WARN");
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

void test_client_config_stdout() {
  char LOG_PATH[MAX_PATH] = { 0 };
  char clientConfigJSON[] = "{\"common\":{\"log_level\":\"warn\",\"log_path\":\"STDOUT\"}}";
  char configFilePath[] = "sf_client_config.json";
  FILE* file;
  file = fopen(configFilePath, "w");
  fprintf(file, "%s", clientConfigJSON);
  fclose(file);

  snowflake_global_set_attribute(SF_GLOBAL_CLIENT_CONFIG_FILE, configFilePath);
  snowflake_global_init("", SF_LOG_DEFAULT, NULL);

  // Get the log path determined by libsnowflakeclient
  snowflake_global_get_attribute(SF_GLOBAL_LOG_PATH, LOG_PATH, MAX_PATH);

  // Ensure the log file doesn't exist at the beginning
  assert_string_equal(LOG_PATH, "");

  // Info log won't trigger the log file creation since log level is set to warn in config
  log_info("dummy info log");

  // Warning log will trigger the log file creation
  log_warn("dummy warning log");
  log_close();

  // Cleanup
  remove(configFilePath);
}

/* Test terminal masking*/
void test_terminal_mask(){
  
  char masked[450] = "\0";
  char *token = "\"oldSessionToken\":.\"ver:3-hint:92019686956010-ETMsDgAAAZnuCZEqABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwEAABAAEFvTRpZh3vTIN0aeQGHgtZUAAACgEe4rGMIhP+9VB6W02vfgNxd7TzjF7V9CFNiobWsPKfRaVm0e+Pgan+NKiWqJGeYPY0kNDKc+iZZArOgYb3bj0JaU2ovmSRTzEKF4/oQdunFrob66HU+x5piBINNQ327tcSglCOBKxAmjHwQxv+C3t7Yzsaa1I10VUA3fRwGcMlluuCC/7ucFnLUeSESYzImlmWBtftQS/giLDli9CyghpgAUblZOu/WGGryesNxqKCr2qHxYUrQ=\"";  
  terminal_mask(token, strlen(token), masked, sizeof(masked));
  assert_string_not_equal(token, masked);
  
  char *expected = "\"oldSessionToken\": ****";
  assert_string_equal(masked, expected);

  char *token1 = "Snowflake Token=\"ver:3-hint:92019686956010-ETMsDgAAAZnuCZDdABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwEAABAAEE8nWQwJCW8y71MmS0MTiQAAADAKKvKBOXVEWiCRMEHtrZlROAljOWTb1wDD6rIgPC8odgqH9ieZZuxfm5GmPkP2DasqFfBMDxk0sw1ZWqE2c7Sos+tUSh09EKraNoANaMSMsL71u7JKMtSIPJ907FVM0xeDw924bYTY1+D3gKvVn93nzdAZto8pOPVs9ag0MlmFrQQH0RLuLAMgAx4ZBkyeoeuTco0A3PNoedb/kvIpfIQWtukVDuXJmCetZQxATxXVuu3cXisGg7I8Mu/VJqd/iABScY0nslPWxaodfF0nwZ4fquJWUaQ==\"";
  masked[0] = '\0';
  terminal_mask(token1, strlen(token1), masked, sizeof(masked));
  expected = "\"Snowflake Token\": ****";
  assert_string_equal(masked, expected);

  char *token2 = "this text is not meant to be masked";
  masked[0] = '\0';
  terminal_mask(token2, strlen(token2), masked, sizeof(masked));
  assert_string_equal(token2, masked);

  char *token3 = "";
  masked[0] = '\0';
  terminal_mask(token3, strlen(token3), masked, sizeof(masked));
  expected = "";
  assert_string_equal(masked, expected);
}

/* Test masking in stderr */
void run_each_test_case(int *stderr_fd, int testcasenum, const char *test_token[], const char *filename[]){
  enum curl_infotype infotype = 0;
  struct data d = {'1'};  // debug struct trace_ascii needed for CURL
  size_t size = 0;
  char output_buff[100] = "\0";
  
  // saving original stderr
  *stderr_fd = dup(_fileno(stderr));
  if(*stderr_fd == -1){
    printf("[Test] unable to save stderr\n");
  }

  // redirecting stderr to a tmp file
  FILE *tmp_stderr = freopen(filename[testcasenum], "w", stderr);
  if(tmp_stderr == NULL){
    printf("Failed to open temp file for stderr redirection\n");
    return 1;
  }

  // file descriptor for reading tmp file
  FILE *fp_out = fopen(filename[testcasenum], "r");

    switch(testcasenum){
      case 0:
      infotype = 2;  // CURLINFO_HEADER_OUT
      size = strlen(test_token[testcasenum]);  
      my_trace(NULL, infotype, test_token[testcasenum], size, &d);

      // read stderr content
      if(fgets(output_buff, sizeof(output_buff), fp_out) == NULL){
        printf(stderr, "[Test] fgets unable to retrieve text\n");
      }
      fflush(stderr);
      output_buff[0] = '\0';
      if(fgets(output_buff, strlen(test_token[testcasenum]), fp_out) == NULL){
        printf(stderr, "[Test] fgets unable to retrieve text\n");
      }
      fflush(stderr);

      //compare output
      char *expected = "0000: Authorization: \"Snowflake Token\": ****..........................\n";
      assert_string_equal(output_buff, expected);
      break;

    case 1:
      infotype = 3;  // CURLINFO_DATA_IN
      size = strlen(test_token[testcasenum]);  
      my_trace(NULL, infotype, test_token[testcasenum], size, &d);

      // read stderr content
      if(fgets(output_buff, sizeof(output_buff), fp_out) == NULL){
        printf(stderr, "[Test] fgets unable to retrieve text\n");
      }
      fflush(stderr);
      output_buff[0] = '\0';
      if(fgets(output_buff, strlen(test_token[testcasenum]), fp_out) == NULL){
        printf(stderr, "[Test] fgets unable to retrieve text\n");
      }
      fflush(stderr);
      
      //compare output
      char *expected1 = "0000: {.  \"data\" : {.  \"sessionToken\": ****,.  \"validityInSecondsST\" :\n";
      assert_string_equal(output_buff, expected1);
      output_buff[0] = '\0';
      if(fgets(output_buff, strlen(test_token[testcasenum]), fp_out) == NULL){
        printf(stderr, "[Test] fgets unable to retrieve text\n");
      }
      fflush(stderr);
      expected1 = "0040:  3599,.  \"masterToken\": ****,.  \"validityInSecondsMT\" : 14399,. \n";
      assert_string_equal(output_buff, expected1);
    break;

    case 2:
      infotype = 2;  // CURLINFO_HEADER_OUT
      size = strlen(test_token[testcasenum]);  
      my_trace(NULL, infotype, test_token[testcasenum], size, &d);

      // read stderr content
      if(fgets(output_buff, sizeof(output_buff), fp_out) == NULL){
        printf(stderr, "[Test] fgets unable to retrieve text\n");
      }
      fflush(stderr);
      output_buff[0] = '\0';
      if(fgets(output_buff, sizeof(output_buff), fp_out) == NULL){
        printf(stderr, "[Test] fgets unable to retrieve text\n");
      }
      fflush(stderr);

      //compare output
      char *expected3 = "0000: Host: simbapartner.snowflakecomputing.com.\n";
      assert_string_equal(output_buff, expected3);
    break;

    case 3:
      infotype = 1;  // CURLINFO_HEADER_OUT
      size = strlen(test_token[testcasenum]);  
      my_trace(NULL, infotype, test_token[testcasenum], size, &d);

      // read stderr content
      if(fgets(output_buff, sizeof(output_buff), fp_out) == NULL){
        printf(stderr, "[Test] fgets unable to retrieve text\n");
      }
      fflush(stderr);
      output_buff[0] = '\0';
      if(fgets(output_buff, sizeof(output_buff), fp_out) == NULL){
        printf(stderr, "[Test] fgets unable to retrieve text\n");
      }
      fflush(stderr);

      //compare output
      char *expected4 = "0000: This message is not meant to be masked.\n";
      assert_string_equal(output_buff, expected4);
    break;
  }

  // restore stderr
  if(dup2(*stderr_fd, _fileno(stderr)) == -1){
    printf("[Test] unable to restore stdeer\n");
  }
  close(*stderr_fd);
  fclose(fp_out);
}
void test_mask_stderr(){
  int original_stderr = 0;
  char *filename[] = {"tmp_stderr1.txt", "tmp_stderr2.txt", "tmp_stderr3.txt", "tmp_stderr4.txt"}; 
  char *test_token[] = {
    "Authorization: Snowflake Token=\"ver:3-hint:92019686956010-ETMsDgAAAZnuCZDdABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwEAABAAEE8nWQwJCW8y71MmS0MTiQAAADAKKvKBOXVEWiCRMEHtrZlROAljOWTb1wDD6rIgPC8odgqH9ieZZuxfm5GmPkP2DasqFfBMDxk0sw1ZWqE2c7Sos+tUSh09EKraNoANaMSMsL71u7JKMtSIPJ907FVM0xeDw924bYTY1+D3gKvVn93nzdAZto8pOPVs9ag0MlmFrQQH0RLuLAMgAx4ZBkyeoeuTco0A3PNoedb/kvIpfIQWtukVDuXJmCetZQxATxXVuu3cXisGg7I8Mu/VJqd/iABScY0nslPWxaodfF0nwZ4fquJWUaQ==\"",
    "{.  \"data\" : {.  \"sessionToken\" : \"ver:3-hint:447445930662-ETMsDgAAAZkSKHQIABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwEAABAAEJkiB9bmtdBmHSIGVLOyEk8AAACQ3KiabpjaByFYbBrstpaGjBPnzWDcWgOZmuzWVLjOpSftdeIdgg1Wf8wu8LWekfmp50Pskw12nQYDAn0DsfcCQDNHiLYoUX5OfJZrqRqxcFq+aMjTG9IFwiGxzGnb1g6/4cEvquM8p70XUuUworwugM3GSqOppOEWKx7skYjlXtW4vh/qToBNfXnCq+7kHgfABRUm+YOYoxpqzutzNptN5UVBHZqVw==\",.  \"validityInSecondsST\" : 3599,.  \"masterToken\" : \"ver:3-hint:447445930662-ETMsDgAAAZkSKHQIABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwEAABAAEAVbB7SPRmK0GbhTF+XNE2YAAACwShsU1R77NNyduqOK06Cvps4JXU+sQ9LR4yDlyUjLUJporv4mi+aH2Ha7uYmkcccbwc2EurFrd6EUi0skCvctkEmCLzdbQMeaPbfoJmnq82oIx7SBgCnhc9Q9qOszwR7Co06BHW+KFrRj77ZGVS7C7fuRZHfhpKds8tvaS99OVKcGqtV5V9M84kNcI9+Bl8TO0lIxuwMM4gF/ExLrynzGiL6k+Ga6b7OhbAXFNjZmoAAFFoyzuuNAAxM66zT1egsBOvQe0x\",.  \"validityInSecondsMT\" : 14399,.  \"sessionId\" : 114369304442287.},.  \"code\" : null,.  \"message\" : null,.  \"success\" : true.}",
    "Host: simbapartner.snowflakecomputing.com\n",
    "This message is not meant to be masked\n"
  };
  
  for(int i = 0; i < 4; i++){
    run_each_test_case(&original_stderr, i, test_token, filename);
  }

  // clean up
  for(int i = 0; i < 4; i++){
    if(remove(filename[i]) != 0){
      fprintf(stderr, "[Test] unable to remove tmp_stderr%d\n", i);
    }
  }

}

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
 * Test that generate exception
 */
void test_log_creation_no_permission_to_home_folder() {

  // check if current user is root. If so, exit test
  char *name;
  struct passwd *pwd;
  pwd = getpwuid(getuid());
  name = pwd->pw_name;

  if(strcmp(name, "root") == 0){
    return;
  }
  
  // clean up tmp folder if necessary
  sf_delete_directory_if_exists(SF_TMP_FOLDER);    

  // creating SF_client_config_folder
  sf_create_directory_if_not_exists(SF_TMP_FOLDER);

  // getting $HOME dir
  char *homedirOrig = getenv("HOME");

  // setting $HOME to point at /tmp/sf_client_config_folder
  setenv("HOME", SF_TMP_FOLDER, 1);
    
  // setting SF_CLIENT_CONFIG dir to be inaccessible to all
  mode_t newPermission = 0x0000;
  chmod(SF_TMP_FOLDER, newPermission);
    
  // parse client config for log details - exception should be thrown here and caught
  client_config clientConfig;
  sf_bool result = load_client_config("", &clientConfig);
  assert_false(result);

  // changing permission of tmp folder to ensure cleanup is possible
  chmod(SF_TMP_FOLDER, 0750);
    
  // clean up /tmp/SF_client_config_folder
  sf_delete_directory_if_exists(SF_TMP_FOLDER);

  // resetting HOME 
  setenv("HOME", homedirOrig, 1);   
}


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
        {//13
            "queryStageMasterKey: 123asdfasdfASDFasdf456asdfasdfASDFasdf==",
            "queryStageMasterKey: ****"
        },
        {//14
            "AWS_KEY_ID: AKIAIOSFODNN7EXAMPLE",
            "AWS_KEY_ID: ****"
        },
        {//15
            "AWS_SECRET_KEY: 123asdfasdfASDFasdf/456asdfasdfASDF/asdf",
            "AWS_SECRET_KEY: ****"
        },
        {//16
            "AWS_TOKEN: ETMsDgAAAXI0IS9NABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwCAABAAEEb/xAQlmT+mwIx9G32E+ikAAACA/CPlEkq//+jWZnQkOj5VhjayruDsCVRGS/B6GzHUugXLc94EfEwuto94gS/oKSVrUg/JRPekypLAx4Afa1KW8n1RqXRF9Hzy1VVLmVEBMtei3yFJPNSHtfbeFHSr9eVB/OL8dOGbxQluGCh6XmaqTjyrh3fqUTWz7+n74+gu2ugAFFZ18iT+DStK0TTdmy4vBC6xUcHQ==",
            "AWS_TOKEN: ****"
        },
        {//17
            "\"encryptionMaterial\":\t{\n\t\t\t\"queryStageMasterKey\":\t\"123asdfasdfASDFasdf==\",\n\t\t\t\"queryId\":\t\"01b6f5ba-0002-0181-0000-11111111da\",\n\t\t\t\"smkId\":\t1111\n\t\t}",
            "\"encryptionMaterial\": ****"
        },
        {//18
            "\"creds\":\t{\n\t\t\t\t\"AWS_KEY_ID\":\t\"AKIAIOSFODNN7EXAMPLE\",\n\t\t\t\t\"AWS_SECRET_KEY\":\t\"123asdfasdfASDFasdf456asdfasdfASDFasdf\",\n\t\t\t\t\"AWS_TOKEN\":\t\"abc\",\n\t\t\t\t\"AWS_ID\":\t\"AKIAIOSFODNN7EXAMPLE\",\n\t\t\t\t\"AWS_KEY\":\t\"123asdfasdfASDFasdf456asdfasdfASDFasdf\"\n\t\t\t}",
            "\"creds\": ****"
        },
        {//19
            "\"token\":\t\"ETM:sDgAAA-XI0IS9NABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwCAABAAEEb/xAQlmT+mwIx9G32E+ikAAACA/CPlEkq//+jWZnQkOj5VhjayruDsCVRGS/B6GzHUugXLc94EfEwuto94gS/oKSVrUg/JRPekypLAx4Afa1KW8n1RqXRF9Hzy1VVLmVEBMtei3yFJPNSHtfbeFHSr9eVB/OL8dOGbxQluGCh6XmaqTjyrh3fqUTWz7+n74+gu2ugAFFZ18iT+DStK0TTdmy4vBC6xUcHQ==\"",
            "\"token\": ****"
        },
        {//20
            "\"masterToken\":\t\"ETM:sDgAAA-XI0IS9NABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwCAABAAEEb/xAQlmT+mwIx9G32E+ikAAACA/CPlEkq//+jWZnQkOj5VhjayruDsCVRGS/B6GzHUugXLc94EfEwuto94gS/oKSVrUg/JRPekypLAx4Afa1KW8n1RqXRF9Hzy1VVLmVEBMtei3yFJPNSHtfbeFHSr9eVB/OL8dOGbxQluGCh6XmaqTjyrh3fqUTWz7+n74+gu2ugAFFZ18iT+DStK0TTdmy4vBC6xUcHQ==\"",
            "\"masterToken\": ****"
        },
        {//21
             "\"Snowflake Token\"=\"ver:3-hint:92019686956010-ETMsDgAAAZnuCZDdABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwEAABAAEE8nWQwJCW8+y71MmS0MTiQAAADAKKvKBOXVEWiCRMEHtrZlROAljOWTb1wDD6rIgPC8odgqH9ieZZuxfm5GmPkP2DasqFfBMDxk0sw1ZWqE2c7Sos+tUSh09EKraNoANaMSMsL71u7JKMtSIPJ907FVM0xeDw924bYTY1+D3gKvVn93nzdAZto8pOPVs9ag0Mlm+FrQQH0RLuLAMgAx4ZBkyeoeuTco0A3PNoedb/HkvIpfIQWtukVDuXJmCetZQxATxXVuu3cXisGg7I8Mu/VJqd/iABScY0nslPWxaodfF0nwZ4fquJWUaQ==\"",
            "\"Snowflake Token\": ****"
        },
        {//22
            "\"oldSessionToken\":.\"ver:3-hint:92019686956010-ETMsDgAAAZnuCZEqABRBRVMvQ0JDL1BLQ1M1UGFkZGluZwEAABAAEFvTRpZh3vTIN0aeQGHgtZUAAACgEe4rGMIhP+9VB6W02vfgNxd7TzjF7V9CFNiobWsPKfRaVm0e+Pgan+NKiWqJGeYPY0kNDKc+iZZArOgYb3bj0JaU2ovmSRTzEKF4/oQdunFrob66HU+x5piBINNQ327tcSglCOBKxAmjHwQxv+C3t7Yzsaa1I10VUA3fRwGcMlluuCC/7ucFnLUeSESYzImlmWBtftQS/giLDli9CyghpgAUblZOu/WGGryesNxqKCr2qHxYUrQ=\"",
            "\"masterToken\": ****"
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
        cmocka_unit_test(test_null_log_path),
        cmocka_unit_test(test_default_log_path),
        cmocka_unit_test(test_log_str_to_level),
        cmocka_unit_test(test_invalid_client_config_path),
        cmocka_unit_test(test_client_config_log_invalid_json),
        cmocka_unit_test(test_client_config_log_malformed_json),
#if (!defined(_WIN32) && !defined(_DEBUG)) || defined(_WIN64)
        cmocka_unit_test(test_client_config_log),
        cmocka_unit_test(test_client_config_log_unknown_entries),
        cmocka_unit_test(test_client_config_log_init),
        cmocka_unit_test(test_client_config_log_init_home_config),
        cmocka_unit_test(test_client_config_log_no_level),
        cmocka_unit_test(test_client_config_log_no_path),
        cmocka_unit_test(test_client_config_stdout),
        cmocka_unit_test(test_terminal_mask),
        cmocka_unit_test(test_mask_stderr),
#endif
        cmocka_unit_test(test_log_creation),
#ifndef _WIN32
        cmocka_unit_test(test_log_creation_no_permission_to_home_folder),
        cmocka_unit_test(test_mask_secret_log),
#endif
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
