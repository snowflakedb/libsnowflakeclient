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

void test_token_file_path(void** unused) {
  SF_UNUSED(unused);
  
  std::string tokenFilePath = "./test_token.txt";
  std::ofstream tokenFile;
  tokenFile.open(tokenFilePath, std::fstream::out);
  tokenFile << "test_token_from_file\n";
  tokenFile.close();

#ifndef _WIN32
  boost::filesystem::permissions(tokenFilePath,
    boost::filesystem::owner_read | boost::filesystem::owner_write);
#endif

  std::string tomlConfig = "[default]\nauthenticator = \"oauth\"\ntoken_file_path = \"" + tokenFilePath + "\"";
  std::string tomlFilePath = "./connections.toml";
  std::ofstream file;
  file.open(tomlFilePath, std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride override("SNOWFLAKE_HOME", "./");

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  
  assert_int_equal(connectionParams.size(), 2);
  assert_string_equal(boost::get<std::string>(connectionParams["token"]).c_str(), "test_token_from_file");
  assert_string_equal(boost::get<std::string>(connectionParams["authenticator"]).c_str(), "oauth");
  assert_true(connectionParams.find("token_file_path") == connectionParams.end());

  // Cleanup
  remove(tokenFilePath.c_str());
  remove(tomlFilePath.c_str());
}

void test_token_file_path_missing_file(void** unused) {
  SF_UNUSED(unused);
  
  std::string tomlConfig = "[default]\nauthenticator = \"oauth\"\ntoken_file_path = \"/nonexistent/path/to/token.txt\"";
  std::string tomlFilePath = "./connections.toml";
  std::ofstream file;
  file.open(tomlFilePath, std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride override("SNOWFLAKE_HOME", "./");

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  
  assert_int_equal(connectionParams.size(), 1);
  assert_true(connectionParams.find("token") == connectionParams.end());
  assert_true(connectionParams.find("token_file_path") == connectionParams.end());

  // Cleanup
  remove(tomlFilePath.c_str());
}

void test_token_file_path_empty_file(void** unused) {
  SF_UNUSED(unused);
  
  std::string tokenFilePath = "./empty_token.txt";
  std::ofstream tokenFile;
  tokenFile.open(tokenFilePath, std::fstream::out);
  tokenFile.close();

#ifndef _WIN32
  boost::filesystem::permissions(tokenFilePath,
    boost::filesystem::owner_read | boost::filesystem::owner_write);
#endif

  std::string tomlConfig = "[default]\nauthenticator = \"oauth\"\ntoken_file_path = \"" + tokenFilePath + "\"";
  std::string tomlFilePath = "./connections.toml";
  std::ofstream file;
  file.open(tomlFilePath, std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride override("SNOWFLAKE_HOME", "./");

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  
  assert_int_equal(connectionParams.size(), 1);
  assert_true(connectionParams.find("token") == connectionParams.end());
  assert_true(connectionParams.find("token_file_path") == connectionParams.end());

  // Cleanup
  remove(tokenFilePath.c_str());
  remove(tomlFilePath.c_str());
}

#ifndef _WIN32
void test_toml_config_permissions(void **unused) {
  SF_UNUSED(unused);
  
  std::string tomlConfig = "[default]\nkey1 = \"value1\"";
  std::string tomlFilePath = "./connections.toml";
  std::ofstream file;
  file.open(tomlFilePath, std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride override("SNOWFLAKE_HOME", "./");
  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams;

  boost::filesystem::permissions(tomlFilePath, owner_read | owner_write);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 1);
  assert_string_equal(boost::get<std::string>(connectionParams["key1"]).c_str(), "value1");

  boost::filesystem::permissions(tomlFilePath, owner_all | group_write);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 0);

  boost::filesystem::permissions(tomlFilePath, owner_all | others_write);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 0);

  boost::filesystem::permissions(tomlFilePath, owner_all | group_write | others_write);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 0);

  boost::filesystem::permissions(tomlFilePath, owner_read | owner_write | group_read);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 1);
  assert_string_equal(boost::get<std::string>(connectionParams["key1"]).c_str(), "value1");

  boost::filesystem::permissions(tomlFilePath, owner_read | owner_write | others_read);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 1);
  assert_string_equal(boost::get<std::string>(connectionParams["key1"]).c_str(), "value1");

  boost::filesystem::permissions(tomlFilePath, owner_read | owner_write | group_read | group_write);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 0);

  remove(tomlFilePath.c_str());
}

void test_toml_skip_read_warning(void **unused) {
  SF_UNUSED(unused);
  
  std::string tomlConfig = "[default]\nkey1 = \"value1\"";
  std::string tomlFilePath = "./connections.toml";
  std::ofstream file;
  file.open(tomlFilePath, std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride permOverride("SF_SKIP_WARNING_FOR_READ_PERMISSIONS_ON_CONFIG_FILE", "true");
  EnvOverride homeOverride("SNOWFLAKE_HOME", "./");

  boost::filesystem::permissions(tomlFilePath, owner_all | group_read | others_read);
  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 1);
  assert_string_equal(boost::get<std::string>(connectionParams["key1"]).c_str(), "value1");

  boost::filesystem::permissions(tomlFilePath, owner_all | group_write);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 0);

  boost::filesystem::permissions(tomlFilePath, owner_all | others_write);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 0);

  remove(tomlFilePath.c_str());
}

void test_token_file_permissions(void **unused) {
  SF_UNUSED(unused);
  
  std::string tokenFilePath = "./test_token_perms.txt";
  std::ofstream tokenFile;
  tokenFile.open(tokenFilePath, std::fstream::out);
  tokenFile << "test_token_value";
  tokenFile.close();

  std::string tomlConfig = "[default]\nauthenticator = \"oauth\"\ntoken_file_path = \"" + tokenFilePath + "\"";
  std::string tomlFilePath = "./connections.toml";
  std::ofstream file;
  file.open(tomlFilePath, std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride override("SNOWFLAKE_HOME", "./");

  boost::filesystem::permissions(tomlFilePath, owner_read | owner_write);
  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams;

  boost::filesystem::permissions(tokenFilePath, owner_read | owner_write);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 2);
  assert_string_equal(boost::get<std::string>(connectionParams["token"]).c_str(), "test_token_value");

  boost::filesystem::permissions(tokenFilePath, owner_read | owner_write | group_read);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 1);
  assert_true(connectionParams.find("token") == connectionParams.end());

  boost::filesystem::permissions(tokenFilePath, owner_read | owner_write | group_write);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 1);
  assert_true(connectionParams.find("token") == connectionParams.end());

  boost::filesystem::permissions(tokenFilePath, owner_read | owner_write | group_exe);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 1);
  assert_true(connectionParams.find("token") == connectionParams.end());

  boost::filesystem::permissions(tokenFilePath, owner_read | owner_write | others_read);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 1);
  assert_true(connectionParams.find("token") == connectionParams.end());

  boost::filesystem::permissions(tokenFilePath, owner_read | owner_write | others_write);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 1);
  assert_true(connectionParams.find("token") == connectionParams.end());

  boost::filesystem::permissions(tokenFilePath, owner_read | owner_write | others_exe);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 1);
  assert_true(connectionParams.find("token") == connectionParams.end());

  boost::filesystem::permissions(tokenFilePath, owner_read);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 2);
  assert_string_equal(boost::get<std::string>(connectionParams["token"]).c_str(), "test_token_value");

  remove(tokenFilePath.c_str());
  remove(tomlFilePath.c_str());
}

void test_token_priority(void **unused) {
  SF_UNUSED(unused);
  
  std::string tokenFilePath = "./test_token_priority.txt";
  std::ofstream tokenFile;
  tokenFile.open(tokenFilePath, std::fstream::out);
  tokenFile << "token_from_file";
  tokenFile.close();

#ifndef _WIN32
  boost::filesystem::permissions(tokenFilePath, owner_read | owner_write);
#endif

  std::string tomlConfig = "[default]\nauthenticator = \"oauth\"\ntoken = \"inline_token\"\ntoken_file_path = \"" + tokenFilePath + "\"";
  std::string tomlFilePath = "./connections.toml";
  std::ofstream file;
  file.open(tomlFilePath, std::fstream::out);
  file << tomlConfig;
  file.close();

#ifndef _WIN32
  boost::filesystem::permissions(tomlFilePath, owner_read | owner_write);
#endif

  EnvOverride override("SNOWFLAKE_HOME", "./");

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 2);
  assert_string_equal(boost::get<std::string>(connectionParams["token"]).c_str(), "inline_token");
  assert_true(connectionParams.find("token_file_path") == connectionParams.end());

  tomlConfig = "[default]\nauthenticator = \"oauth\"\ntoken_file_path = \"" + tokenFilePath + "\"";
  file.open(tomlFilePath, std::fstream::out | std::fstream::trunc);
  file << tomlConfig;
  file.close();

  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 2);
  assert_string_equal(boost::get<std::string>(connectionParams["token"]).c_str(), "token_from_file");
  assert_true(connectionParams.find("token_file_path") == connectionParams.end());

  remove(tokenFilePath.c_str());
  remove(tomlFilePath.c_str());
}

void test_skip_token_file_verification(void **unused) {
  SF_UNUSED(unused);
  
  std::string tokenFilePath = "./test_token_skip.txt";
  std::ofstream tokenFile;
  tokenFile.open(tokenFilePath, std::fstream::out);
  tokenFile << "test_token_value";
  tokenFile.close();

  std::string tomlConfig = "[default]\nauthenticator = \"oauth\"\ntoken_file_path = \"" + tokenFilePath + "\"";
  std::string tomlFilePath = "./connections.toml";
  std::ofstream file;
  file.open(tomlFilePath, std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride permOverride("SKIP_TOKEN_FILE_PERMISSIONS_VERIFICATION", "true");
  EnvOverride homeOverride("SNOWFLAKE_HOME", "./");

  boost::filesystem::permissions(tomlFilePath, owner_read | owner_write);
  
  boost::filesystem::permissions(tokenFilePath, owner_all | group_all | others_all);

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 2);
  assert_string_equal(boost::get<std::string>(connectionParams["token"]).c_str(), "test_token_value");
  
  assert_true(connectionParams.find("token") != connectionParams.end());
  assert_true(connectionParams.find("token_file_path") == connectionParams.end());

  remove(tokenFilePath.c_str());
  remove(tomlFilePath.c_str());
}
#endif

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
      cmocka_unit_test(test_token_file_path),
      cmocka_unit_test(test_token_file_path_missing_file),
      cmocka_unit_test(test_token_file_path_empty_file),
#ifndef _WIN32
      cmocka_unit_test(test_toml_config_permissions),
      cmocka_unit_test(test_toml_skip_read_warning),
      cmocka_unit_test(test_token_file_permissions),
      cmocka_unit_test(test_token_priority),
      cmocka_unit_test(test_skip_token_file_verification)
#endif
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
