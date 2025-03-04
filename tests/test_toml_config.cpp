/*
 * Copyright (c) 2018-2025 Snowflake Computing, Inc. All rights reserved.
 */

#include <snowflake/TomlConfigParser.hpp>
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"

void test_valid_toml_file(void** unused) {
  SF_UNUSED(unused);
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

  std::map<std::string, std::string> connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 2);

  // Cleanup
  if (snowflakeHome) {
    sf_setenv("SNOWFLAKE_HOME", snowflakeHome);
  } else {
    sf_unsetenv("SNOWFLAKE_HOME");
  }
  remove(tomlFilePath);
}

void test_missing_toml_file(void** unused) {
  SF_UNUSED(unused);
  char envbuf[MAX_PATH + 1];
  char* snowflakeHome = sf_getenv_s("SNOWFLAKE_HOME", envbuf, sizeof(envbuf));
  sf_setenv("SNOWFLAKE_HOME", "./");

  std::map<std::string, std::string> connectionParams = load_toml_config();
  assert_true(connectionParams.empty());

  // Cleanup
  if (snowflakeHome) {
    sf_setenv("SNOWFLAKE_HOME", snowflakeHome);
  } else {
    sf_unsetenv("SNOWFLAKE_HOME");
  }
}

void test_invalid_toml_file(void** unused) {
  SF_UNUSED(unused);
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

  std::map<std::string, std::string> connectionParams = load_toml_config();
  assert_true(connectionParams.empty());

  // Cleanup
  if (snowflakeHome) {
    sf_setenv("SNOWFLAKE_HOME", snowflakeHome);
  } else {
    sf_unsetenv("SNOWFLAKE_HOME");
  }
  remove(tomlFilePath);
}

void test_use_default_location_env(void** unused) {
  SF_UNUSED(unused);
  char tomlConfig[] = "[default]\nkey1 = \"value1\"\nkey2 = \"value2\"";
  char envbuf[MAX_PATH + 1];
#ifdef _WIN32
  char* homeDir = sf_getenv_s("USERPROFILE", envbuf, sizeof(envbuf));
#else
  char* homeDir = sf_getenv_s("HOME", envbuf, sizeof(envbuf));
#endif
  std::string snowflakeDir = "/.snowflake";
  std::string tomlFile = "/.snowflake/connections.toml";
  // Create .snowflake dir
  std::string tomlDir = homeDir + snowflakeDir;
  if (!sf_is_directory_exist(tomlDir.c_str())) {
    sf_mkdir(tomlDir.c_str());
  }
  // Create toml file
  std::string tomlFilePath = homeDir + tomlFile;
  FILE* file;
  file = fopen(tomlFilePath.c_str(), "w");
  fprintf(file, "%s", tomlConfig);
  fclose(file);

  char shbuf[MAX_PATH + 1];
  char* snowflakeHome = sf_getenv_s("SNOWFLAKE_HOME", shbuf, sizeof(shbuf));
  sf_unsetenv("SNOWFLAKE_HOME");

  std::map<std::string, std::string> connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 2);
  assert_string_equal(connectionParams["key1"].c_str(), "value1");
  assert_string_equal(connectionParams["key2"].c_str(), "value2");

  // Cleanup
  if (snowflakeHome) {
    sf_setenv("SNOWFLAKE_HOME", snowflakeHome);
  }
  remove(tomlFilePath.c_str());
}

void test_use_snowflake_default_connection_var(void** unused) {
  SF_UNUSED(unused);
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

  std::map<std::string, std::string> connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 2);
  assert_string_equal(connectionParams["key3"].c_str(), "value3");
  assert_string_equal(connectionParams["key4"].c_str(), "value4");

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
  remove(tomlFilePath);
}

void test_client_config_log_invalid_config_name(void** unused) {
  SF_UNUSED(unused);
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

  std::map<std::string, std::string> connectionParams = load_toml_config();
  assert_true(connectionParams.empty());

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
  remove(tomlFilePath);
}

int main(void) {
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_missing_toml_file),
      cmocka_unit_test(test_invalid_toml_file),
      cmocka_unit_test(test_client_config_log_invalid_config_name),
#if (!defined(_WIN32) && !defined(_DEBUG)) || defined(_WIN64)
      cmocka_unit_test(test_valid_toml_file),
      cmocka_unit_test(test_use_default_location_env),
      cmocka_unit_test(test_use_snowflake_default_connection_var),
#endif
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
