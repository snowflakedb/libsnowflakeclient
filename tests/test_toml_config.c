/*
 * Copyright (c) 2018-2025 Snowflake Computing, Inc. All rights reserved.
 */

#include "utils/test_setup.h"
#include <snowflake/toml_config_parser.h>
#include "memory.h"

void test_valid_toml_file() {
  // Create toml file
  char tomlConfig[] = "[default]\nkey1 = \"value1\"\nkey2 = \"value2\"";
  char tomlFilePath[] = "./connections.toml";
  FILE* file;
  file = fopen(tomlFilePath, "w");
  fprintf(file, "%s", tomlConfig);
  fclose(file);

  char envbuf[MAX_PATH + 1];
  char* snowflakeHome = sf_getenv_s("SNOWFLAKE_HOME", envbuf, sizeof(envbuf));
  sf_setenv("SNOWFLAKE_HOME", "./");

  char* connectionParams = NULL;
  load_toml_config(&connectionParams);
  assert_string_equal(connectionParams, "key1=value1;key2=value2;");

  // Cleanup
  if (snowflakeHome) {
    sf_setenv("SNOWFLAKE_HOME", snowflakeHome);
  } else {
    sf_unsetenv("SNOWFLAKE_HOME");
  }
  SF_FREE(connectionParams);
  remove(tomlFilePath);
}

void test_missing_toml_file() {
  char envbuf[MAX_PATH + 1];
  char* snowflakeHome = sf_getenv_s("SNOWFLAKE_HOME", envbuf, sizeof(envbuf));
  sf_setenv("SNOWFLAKE_HOME", "./");

  char* connectionParams = NULL;
  load_toml_config(&connectionParams);
  assert_null(connectionParams);

  // Cleanup
  if (snowflakeHome) {
    sf_setenv("SNOWFLAKE_HOME", snowflakeHome);
  } else {
    sf_unsetenv("SNOWFLAKE_HOME");
  }
  SF_FREE(connectionParams);
}

void test_invalid_toml_file() {
  // Create toml file
  char tomlConfig[] = "Some fake toml data";
  char tomlFilePath[] = "./connections.toml";
  FILE* file;
  file = fopen(tomlFilePath, "w");
  fprintf(file, "%s", tomlConfig);
  fclose(file);

  char envbuf[MAX_PATH + 1];
  char* snowflakeHome = sf_getenv_s("SNOWFLAKE_HOME", envbuf, sizeof(envbuf));
  sf_setenv("SNOWFLAKE_HOME", "./");

  char* connectionParams = NULL;
  load_toml_config(&connectionParams);
  assert_null(connectionParams);

  // Cleanup
  if (snowflakeHome) {
    sf_setenv("SNOWFLAKE_HOME", snowflakeHome);
  } else {
    sf_unsetenv("SNOWFLAKE_HOME");
  }
  SF_FREE(connectionParams);
  remove(tomlFilePath);
}

void test_use_default_location_env() {
  char tomlConfig[] = "[default]\nkey1 = \"value1\"\nkey2 = \"value2\"";
  char envbuf[MAX_PATH + 1];
#ifdef _WIN32
  char* homeDir = sf_getenv_s("USERPROFILE", envbuf, sizeof(envbuf));
#else
  char* homeDir = sf_getenv_s("HOME", envbuf, sizeof(envbuf));
#endif
  char snowflakeDir[] = "/.snowflake";
  char tomlFile[] = "/.snowflake/connections.toml";
  // Create .snowflake dir
  size_t tomlDirSize = strlen(homeDir) + strlen(tomlFile) + 1;
  char* tomlDir = (char*)SF_CALLOC(1, tomlDirSize);
  sf_strcat(tomlDir, tomlDirSize, homeDir);
  sf_strcat(tomlDir, tomlDirSize, snowflakeDir);
  if (!sf_is_directory_exist(tomlDir)) {
    sf_mkdir(tomlDir);
  }
  // Create toml file
  size_t tomlPathSize = strlen(homeDir) + strlen(tomlFile) + 1;
  char* tomlFilePath = (char*)SF_CALLOC(1, tomlPathSize);
  sf_strcat(tomlFilePath, tomlPathSize, homeDir);
  sf_strcat(tomlFilePath, tomlPathSize, tomlFile);
  FILE* file;
  file = fopen(tomlFilePath, "w");
  fprintf(file, "%s", tomlConfig);
  fclose(file);

  char shbuf[MAX_PATH + 1];
  char* snowflakeHome = sf_getenv_s("SNOWFLAKE_HOME", shbuf, sizeof(shbuf));
  sf_unsetenv("SNOWFLAKE_HOME");

  char* connectionParams = NULL;
  load_toml_config(&connectionParams);
  assert_string_equal(connectionParams, "key1=value1;key2=value2;");

  // Cleanup
  if (snowflakeHome) {
    sf_setenv("SNOWFLAKE_HOME", snowflakeHome);
  }
  SF_FREE(connectionParams);
  remove(tomlFilePath);
}

void test_use_snowflake_default_connection_var() {
  // Create toml file
  char tomlConfig[] = "[default]\nkey1 = \"value1\"\nkey2 = \"value2\"\n\n[test]\nkey3 = \"value3\"\nkey4 = \"value4\"";
  char tomlFilePath[] = "./connections.toml";
  FILE* file;
  file = fopen(tomlFilePath, "w");
  fprintf(file, "%s", tomlConfig);
  fclose(file);

  char shbuf[MAX_PATH + 1];
  char* snowflakeHome = sf_getenv_s("SNOWFLAKE_HOME", shbuf, sizeof(shbuf));
  sf_setenv("SNOWFLAKE_HOME", "./");
  char sdcnbuf[MAX_PATH + 1];
  char* snowflakeDefaultConnName = sf_getenv_s("SNOWFLAKE_DEFAULT_CONNECTION_NAME", sdcnbuf, sizeof(sdcnbuf));
  sf_setenv("SNOWFLAKE_DEFAULT_CONNECTION_NAME", "test");

  char* connectionParams = NULL;
  load_toml_config(&connectionParams);
  assert_string_equal(connectionParams, "key3=value3;key4=value4;");

  // Cleanup
  if (snowflakeHome) {
    sf_setenv("SNOWFLAKE_HOME", snowflakeHome);
  } else {
    sf_unsetenv("SNOWFLAKE_HOME");
  }
  if (snowflakeDefaultConnName) {
    sf_setenv("SNOWFLAKE_DEFAULT_CONNECTION_NAME", snowflakeDefaultConnName);
  } else {
    sf_unsetenv("SNOWFLAKE_DEFAULT_CONNECTION_NAME");
  }
  SF_FREE(connectionParams);
  remove(tomlFilePath);
}

void test_client_config_log_invalid_config_name() {
  // Create toml file
  char tomlConfig[] = "[default]\nkey1 = \"value1\"\nkey2 = \"value2\"";
  char tomlFilePath[] = "./connections.toml";
  FILE* file;
  file = fopen(tomlFilePath, "w");
  fprintf(file, "%s", tomlConfig);
  fclose(file);

  char shbuf[MAX_PATH + 1];
  char* snowflakeHome = sf_getenv_s("SNOWFLAKE_HOME", shbuf, sizeof(shbuf));
  sf_setenv("SNOWFLAKE_HOME", "./");
  char sdcnbuf[MAX_PATH + 1];
  char* snowflakeDefaultConnName = sf_getenv_s("SNOWFLAKE_DEFAULT_CONNECTION_NAME", sdcnbuf, sizeof(sdcnbuf));
  sf_setenv("SNOWFLAKE_DEFAULT_CONNECTION_NAME", "test");

  char* connectionParams = NULL;
  load_toml_config(&connectionParams);
  assert_null(connectionParams);

  // Cleanup
  if (snowflakeHome) {
    sf_setenv("SNOWFLAKE_HOME", snowflakeHome);
  } else {
    sf_unsetenv("SNOWFLAKE_HOME");
  }
  if (snowflakeDefaultConnName) {
    sf_setenv("SNOWFLAKE_DEFAULT_CONNECTION_NAME", snowflakeDefaultConnName);
  } else {
    sf_unsetenv("SNOWFLAKE_DEFAULT_CONNECTION_NAME");
  }
  SF_FREE(connectionParams);
  remove(tomlFilePath);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_valid_toml_file),
        cmocka_unit_test(test_missing_toml_file),
        cmocka_unit_test(test_invalid_toml_file),
        cmocka_unit_test(test_use_default_location_env),
        cmocka_unit_test(test_use_snowflake_default_connection_var),
        cmocka_unit_test(test_client_config_log_invalid_config_name),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
