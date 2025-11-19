#include <fstream>
#include <iostream>

#include "snowflake/TomlConfigParser.hpp"
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"
#include "utils/EnvOverride.hpp"
#include <boost/filesystem.hpp>

using namespace boost::filesystem;

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
  remove("./connections.toml");
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

void test_permissions(void **unused) {
  SF_UNUSED(unused);
  // Create toml file
  std::string tomlConfig = "[default]\nkey1 = \"value1\"\nkey2 = \"value2\"";
  std::string tomlFilePath = "./connections.toml";
  std::ofstream file;
  file.open(tomlFilePath, std::fstream::out);
  file << tomlConfig;
  file.close();

  // Set logging
  std::string logPath = "logs/snowflake_toml.txt";
  log_set_level(SF_LOG_INFO);
  log_set_path(logPath.c_str());

  EnvOverride override("SNOWFLAKE_HOME", "./");

  std::vector<std::pair<perms, bool>> configPermissions =
  {
    { owner_all, true },
    { owner_read | owner_write, true },
    { owner_read | owner_exe, true },
    { owner_read, true },
    { owner_all | group_all, false },
    { owner_all | group_read | group_write, false },
    { owner_all | group_read | group_exe, true },
    { owner_all | group_read, true },
    { owner_all | group_write | group_exe, false },
    { owner_all | group_write, false },
    { owner_all | group_exe, true },
    { owner_all | others_all, false },
    { owner_all | others_read | others_write, false },
    { owner_all | others_read | others_exe, true },
    { owner_all | others_read, true },
    { owner_all | others_write | others_exe, false },
    { owner_all | others_write, false },
    { owner_all | others_exe, true }
  };

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams;
  for (auto permission : configPermissions)
  {
    boost::filesystem::permissions(tomlFilePath, permission.first);
    connectionParams = load_toml_config();
    if (permission.second)
    {
      assert_int_equal(connectionParams.size(), 2);
      if (permission.first & boost::filesystem::group_read ||
        permission.first & boost::filesystem::others_read)
      {
        std::string line;
        std::fstream logfile;
        logfile.open(logPath);
        if (logfile.is_open())
        {
          bool isFound = false;
          while (getline(logfile, line))
          {
            if (line.find("Warning due to other users having permission to read the config file") != std::string::npos)
            {
              isFound = true;
              break;
            }
          }
          logfile.close();
          remove(logPath.c_str());
          assert_true(isFound);
        }
      }
    }
    else
    {
      assert_true(connectionParams.empty());
    }
    connectionParams.clear();
  }

  // Cleanup
  log_close();
  remove(tomlFilePath.c_str());
}

void test_skip_warn(void **unused) {
  SF_UNUSED(unused);
  // Create toml file
  std::string tomlConfig = "[default]\nkey1 = \"value1\"\nkey2 = \"value2\"";
  std::string tomlFilePath = "./connections.toml";
  std::ofstream file;
  file.open(tomlFilePath, std::fstream::out);
  file << tomlConfig;
  file.close();

  // Set logging
  std::string logPath = "logs/snowflake_tomlwarn.txt";
  remove(logPath.c_str());
  log_set_level(SF_LOG_INFO);
  log_set_path(logPath.c_str());

  EnvOverride permOverride("SF_SKIP_WARNING_FOR_READ_PERMISSIONS_ON_CONFIG_FILE", "true");
  EnvOverride homeOverride("SNOWFLAKE_HOME", "./");

  // Test warning is skipped
  boost::filesystem::permissions(tomlFilePath, owner_all | group_read | others_read);
  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 2);

  // Test error is not skipped
  boost::filesystem::permissions(tomlFilePath, owner_all | group_all | others_all);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 0);

  bool isWarnFound = false;
  bool isErrorFound = false;
  std::string line;
  std::fstream logfile;
  logfile.open(logPath);
  if (logfile.is_open())
  {
    while (getline(logfile, line))
    {
      if (line.find("Warning due to other users having permission to read the config file") != std::string::npos)
      {
        isWarnFound = true;
      }
      if (line.find("Error due to other users having permission to modify the config file") != std::string::npos)
      {
        isErrorFound = true;
      }
    }
    logfile.close();
  }
  log_close();
  assert_false(isWarnFound);
  assert_true(isErrorFound);

  remove(tomlFilePath.c_str());
  remove(logPath.c_str());
}

void test_skip_permissions_verification(void **unused) {
  SF_UNUSED(unused);
  // Create toml file
  std::string tomlConfig = "[default]\nkey1 = \"value1\"\nkey2 = \"value2\"";
  std::string tomlFilePath = "./connections.toml";
  std::ofstream file;
  file.open(tomlFilePath, std::fstream::out);
  file << tomlConfig;
  file.close();

  // Set logging
  std::string logPath = "logs/snowflake_toml_skip_verification.txt";
  remove(logPath.c_str());
  log_set_level(SF_LOG_INFO);
  log_set_path(logPath.c_str());

  EnvOverride permOverride("SKIP_TOKEN_FILE_PERMISSIONS_VERIFICATION", "true");
  EnvOverride homeOverride("SNOWFLAKE_HOME", "./");

  // Set bad permissions that would normally fail (group and others can write)
  boost::filesystem::permissions(tomlFilePath, owner_all | group_all | others_all);
  
  // Should succeed despite bad permissions due to skip flag
  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 2);
  assert_string_equal(boost::get<std::string>(connectionParams["key1"]).c_str(), "value1");
  assert_string_equal(boost::get<std::string>(connectionParams["key2"]).c_str(), "value2");

  // Verify the skip message appears in the log
  bool isSkipMessageFound = false;
  std::string line;
  std::fstream logfile;
  logfile.open(logPath);
  if (logfile.is_open())
  {
    while (getline(logfile, line))
    {
      if (line.find("Skipping token file permissions verification") != std::string::npos)
      {
        isSkipMessageFound = true;
        break;
      }
    }
    logfile.close();
  }
  log_close();
  assert_true(isSkipMessageFound);

  remove(tomlFilePath.c_str());
  remove(logPath.c_str());
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
#ifndef _WIN32
      cmocka_unit_test(test_permissions),
      cmocka_unit_test(test_skip_warn),
      cmocka_unit_test(test_skip_permissions_verification)
#endif
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
