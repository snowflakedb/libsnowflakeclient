#include "utils/test_setup.h"
#include <snowflake/client_config_parser.h>
#include "memory.h"
#include <stdio.h>
#include <string.h>  
#include <sys/stat.h>

#ifndef _WIN32
#include <unistd.h>
#include <pwd.h>
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

void test_log_creation_no_permission_to_home_folder(){
  // check if current user is root. If so, exit test
  char *name;
  struct passwd *pwd;
  pwd = getpwuid(getuid());
  name = pwd->pw_name;
  if(strcmp(name, "root") != 0){

    // FILE *mfptr;
    // mfptr = fopen("testEasyLogging.txt", "w");

   // scenario #2 using tmp/SF_client_config folder to set up for exception throw
    // fprintf(mfptr, "----- Entering Scenario #2 -----\n");

    // ensuring /tmp/SF_client_config_folder does not exist
    struct stat fileBuffer;
    int rc;
    if(stat("/tmp/SF_client_config_folder/sf_client_config.json", &fileBuffer) != 0){
      // /tmp/SF_Client_config folder exists
      chmod("/tmp/SF_client_config_folder", 777);
      rc = remove("/tmp/SF_client_config_folder/sf_client_config.json");

      if(rc == 0){
        // successful in removing sf_client_config.json
        rc = remove("/tmp/SF_client_config_folder/");
      } else {
        log_debug("Not successful in ensuring /tmp/sf_client_config_folder/sf_client_config.json does not exist");
      }

      if(rc != 0){
        // not successful in removing sf_client_config_folder
        log_debug("Not successful in ensuring /tmp/sf_client_config_folder does not exist");
      }
    }
    
    
    // if(rc == 0){
    //   fprintf(mfptr, "Successfully in cleaning up sf_client_config.json\n");
    // } else {
    //   fprintf(mfptr, "Not succesfful in cleaning up sf_client_config.json\n");
    // }

    // rc = remove("/tmp/SF_client_config_folder/");
    // if(rc == 0){
    //   fprintf(mfptr, "Successful in removing /tmp/SF_client_config folder\n");
    // } else {
    //   fprintf(mfptr, "Not Successful in removing /tmp/SF_client_config folder\n");
    // }

    // if(rc == 0){
    //   fprintf(mfptr, "There are no SF Client Config folder present\n");
    // } else {
    //   fprintf(mfptr, "Not successful in removing /tmp/SF_client_config\n");
    // }

    // creating SF Client Config folder under tmp folder
    // fprintf(mfptr, "Creating SF client config folder in tmp folder\n");

    rc = mkdir("/tmp/SF_client_config_folder", S_IRWXU | S_IRGRP | S_IROTH | S_IXOTH);
    if(rc != 0){
      log_debug("NOT successful in creating sf client config folder inside of tmp folder");
    }
    
    // if (rc == 0){
    //   fprintf(mfptr,"Successful in creating sf client config folder in tmp folder\n");
    // } else {
    //   fprintf(mfptr, "NOT successful in creating sf client config folder inside of tmp folder\n");
    // }
       
    // system("ls -l $HOME");

    // obtaining a record of tmp folder permission
    struct stat file_stat;
    mode_t file_stat_originalStat;
    if(stat("/tmp/SF_client_config_folder", &file_stat) == 0){
      // fprintf(mfptr, "File: /tmp/SF_client_config_folder\n");
      file_stat_originalStat = file_stat.st_mode & 0777;
      // fprintf(mfptr, "Permission: %o\n", file_stat_originalStat);
    } else {
      // fprintf(mfptr, "Error in getting sf client config folder stat\n");
      log_debug("Error: unable to get /tmp/sf_client_config folder permission");
    }

    // fprintf(mfptr, "Creating sf client config,json inside tmp/ SF_Config_folder\n");
    
    char configFilePath[] = "/tmp/SF_client_config_folder/sf_client_config.json";
    char clientConfigJSON[] = "{\"common\":{\"log_level\":\"warn\",\"log_path\":\"./test/\"}}";

    FILE *file;
    file = fopen(configFilePath, "w");
    // if(file == NULL){
    //   fprintf(mfptr, "Error opening sf client config.json file\n");
    // } else {
    //   fprintf(mfptr, "Successful in opening sf client config.json file to write\n");
    // }
    
    fprintf(file, "%s", clientConfigJSON);
    fclose(file);

    // system("ls -l /tmp/SF_client_config_folder");

    // setting $HOME to point at /tmp/sf_client_config_folder
    setenv("HOME", "/tmp/SF_client_config_folder/", 1);
    // setenv("HOME", "/tmp/SF_client_config_folder/sf_client_config.json", 1);
    // system("echo $HOME");
    
    // setting SF_CLIENT_CONFIG dir to be inaccessible to all
    mode_t newPermission = 0000; // no read, write, execute permission for anyone
    chmod("/tmp/SF_client_config_folder", newPermission);
    // system("ls -l /tmp");
    
    // parse client config for log details - exception should be thrown here and caught
    client_config clientConfig;
    sf_bool result = load_client_config("", &clientConfig);
    assert_false(result);

    // changing permission of tmp folder back to what it was 
    chmod("/tmp/SF_client_config_folder", file_stat_originalStat);
    
    // clean up /tmp/SF_client_config_folder
    rc = remove("/tmp/SF_client_config_folder/sf_client_config.json");

    if(rc == 0){
      // fprintf(mfptr, "Successfully in cleaning up sf_client_config.json\n");
      rc = remove("/tmp/SF_client_config_folder/");
    } else {
      // fprintf(mfptr, "Not succesfful in cleaning up sf_client_config.json\n");
      log_debug("Not succesfful in cleaning up sf_client_config.json");
    }

    // rc = remove("/tmp/SF_client_config_folder/");
    if(rc != 0){
      log_debug("Not succesfful in removing /tmp/sf_client_config_folder");
    }
    // if(rc == 0){
    //   fprintf(mfptr, "Successful in removing /tmp/SF_client_config folder\n");
    // } else {
    //   fprintf(mfptr, "Not Successful in removing /tmp/SF_client_config folder\n");
    // }
    // fclose(mfptr);

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
#endif
        cmocka_unit_test(test_log_creation),
#ifndef _WIN32
        cmocka_unit_test(test_log_creation_no_permission_to_home_folder),
        cmocka_unit_test(test_mask_secret_log),
#endif
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
