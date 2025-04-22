/*
 * Copyright (c) 2018-2025 Snowflake Computing, Inc. All rights reserved.
 */
#include <fstream>
#include <iostream>

#include "snowflake/TomlConfigParser.hpp"
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"
#include "utils/EnvOverride.hpp"

void test_valid_toml_file(void** unused) {
  SF_UNUSED(unused);
  // Create toml file
  std::string tomlConfig = "[default]\nkey1 = \"value1\"\nkey2 = \"value2\"";
  std::string tomlFilePath = "./connections.toml";
  std::ofstream file;
  file.open(tomlFilePath, std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride override("SNOWFLAKE_HOME", "./");

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 2);

  // Cleanup
  remove(tomlFilePath.c_str());
}

void test_missing_toml_file(void** unused) {
  SF_UNUSED(unused);
  EnvOverride override("SNOWFLAKE_HOME", "./");

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  assert_true(connectionParams.empty());
}

void test_invalid_toml_file(void** unused) {
  SF_UNUSED(unused);
  // Create toml file
  std::string tomlConfig = "Some fake toml data";
  std::string tomlFilePath = "./connections.toml";
  std::ofstream file;
  file.open(tomlFilePath, std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride override("SNOWFLAKE_HOME", "./");

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  assert_true(connectionParams.empty());

  // Cleanup
  remove(tomlFilePath.c_str());
}

void test_use_default_location_env(void** unused) {
  SF_UNUSED(unused);
  std::string tomlConfig = "[default]\nkey1 = \"value1\"\nkey2 = \"value2\"";
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
  std::ofstream file;
  file.open(tomlFilePath, std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride override("SNOWFLAKE_HOME", "");

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 2);
  assert_string_equal(boost::get<std::string>(connectionParams["key1"]).c_str(), "value1");
  assert_string_equal(boost::get<std::string>(connectionParams["key2"]).c_str(), "value2");

  // Cleanup
  remove(tomlFilePath.c_str());
}

void test_use_snowflake_default_connection_var(void** unused) {
  SF_UNUSED(unused);
  // Create toml file
  std::string tomlConfig = "[default]\nkey1 = \"value1\"\nkey2 = \"value2\"\n\n[test]\nkey3 = \"value3\"\nkey4 = \"value4\"";
  std::string tomlFilePath = "./connections.toml";
  std::ofstream file;
  file.open(tomlFilePath, std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride shOverride("SNOWFLAKE_HOME", "./");
  EnvOverride sdcnOverride("SNOWFLAKE_DEFAULT_CONNECTION_NAME", "test");

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 2);
  assert_string_equal(boost::get<std::string>(connectionParams["key3"]).c_str(), "value3");
  assert_string_equal(boost::get<std::string>(connectionParams["key4"]).c_str(), "value4");

  // Cleanup
  remove(tomlFilePath.c_str());
}

void test_client_config_log_invalid_config_name(void** unused) {
  SF_UNUSED(unused);
  // Create toml file
  std::string tomlConfig = "[default]\nkey1 = \"value1\"\nkey2 = \"value2\"";
  std::string tomlFilePath = "./connections.toml";
  std::ofstream file;
  file.open(tomlFilePath, std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride shOverride("SNOWFLAKE_HOME", "./");
  EnvOverride sdcnOverride("SNOWFLAKE_DEFAULT_CONNECTION_NAME", "test");

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  assert_true(connectionParams.empty());

  // Cleanup
  remove(tomlFilePath.c_str());
}

void test_data_types(void **unused) {
  SF_UNUSED(unused);
  // Create toml file
  std::string tomlConfig = "[default]\nkey1 = \"value1\"\nkey2 = true\nkey3 = 3\nkey4 = 4.4\nkey5 = []";
  std::string tomlFilePath = "./connections.toml";
  std::ofstream file;
  file.open(tomlFilePath, std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride override("SNOWFLAKE_HOME", "./");

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 4);
  assert_string_equal(boost::get<std::string>(connectionParams["key1"]).c_str(), "value1");
  assert_true(boost::get<bool>(connectionParams["key2"]));
  assert_int_equal(boost::get<int>(connectionParams["key3"]), 3);
  assert_int_equal(boost::get<double>(connectionParams["key4"]), 4.4);

  // Cleanup
  remove(tomlFilePath.c_str());
}

int main(void) {
  initialize_test(SF_BOOLEAN_FALSE);
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_missing_toml_file),
      cmocka_unit_test(test_invalid_toml_file),
      cmocka_unit_test(test_client_config_log_invalid_config_name),
      cmocka_unit_test(test_valid_toml_file),
      cmocka_unit_test(test_use_default_location_env),
      cmocka_unit_test(test_use_snowflake_default_connection_var),
      cmocka_unit_test(test_data_types),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
