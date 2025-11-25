#include <fstream>
#include <iostream>

#include "snowflake/TomlConfigParser.hpp"
#include "utils/test_setup.h"
#include "utils/TestSetup.hpp"
#include "utils/EnvOverride.hpp"
#include "utils/FileCleanup.hpp"
#include <boost/filesystem.hpp>

using namespace boost::filesystem;

void test_valid_toml_file(void** unused) {
  SF_UNUSED(unused);
  FileCleanup tomlCleanup("./connections.toml");
  std::string tomlConfig = "[default]\nkey1 = \"value1\"\nkey2 = \"value2\"";
  std::ofstream file;
  file.open(tomlCleanup.path(), std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride override("SNOWFLAKE_HOME", "./");

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 2);
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
  FileCleanup tomlCleanup("./connections.toml");
  std::string tomlConfig = "Some fake toml data";
  std::ofstream file;
  file.open(tomlCleanup.path(), std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride override("SNOWFLAKE_HOME", "./");

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  assert_true(connectionParams.empty());
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
  std::string tomlDir = homeDir + snowflakeDir;
  if (!sf_is_directory_exist(tomlDir.c_str())) {
    sf_mkdir(tomlDir.c_str());
  }
  std::string tomlFilePath = homeDir + tomlFile;
  FileCleanup tomlCleanup(tomlFilePath);
  std::ofstream file;
  file.open(tomlFilePath, std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride override("SNOWFLAKE_HOME", "");

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 2);
  assert_string_equal(boost::get<std::string>(connectionParams["key1"]).c_str(), "value1");
  assert_string_equal(boost::get<std::string>(connectionParams["key2"]).c_str(), "value2");
}

void test_use_snowflake_default_connection_var(void** unused) {
  SF_UNUSED(unused);
  FileCleanup tomlCleanup("./connections.toml");
  std::string tomlConfig = "[default]\nkey1 = \"value1\"\nkey2 = \"value2\"\n\n[test]\nkey3 = \"value3\"\nkey4 = \"value4\"";
  std::ofstream file;
  file.open(tomlCleanup.path(), std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride shOverride("SNOWFLAKE_HOME", "./");
  EnvOverride sdcnOverride("SNOWFLAKE_DEFAULT_CONNECTION_NAME", "test");

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 2);
  assert_string_equal(boost::get<std::string>(connectionParams["key3"]).c_str(), "value3");
  assert_string_equal(boost::get<std::string>(connectionParams["key4"]).c_str(), "value4");
}

void test_client_config_log_invalid_config_name(void** unused) {
  SF_UNUSED(unused);
  FileCleanup tomlCleanup("./connections.toml");
  std::string tomlConfig = "[default]\nkey1 = \"value1\"\nkey2 = \"value2\"";
  std::ofstream file;
  file.open(tomlCleanup.path(), std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride shOverride("SNOWFLAKE_HOME", "./");
  EnvOverride sdcnOverride("SNOWFLAKE_DEFAULT_CONNECTION_NAME", "test");

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  assert_true(connectionParams.empty());
}

void test_data_types(void **unused) {
  SF_UNUSED(unused);
  FileCleanup tomlCleanup("./connections.toml");
  std::string tomlConfig = "[default]\nkey1 = \"value1\"\nkey2 = true\nkey3 = 3\nkey4 = 4.4\nkey5 = []";
  std::ofstream file;
  file.open(tomlCleanup.path(), std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride override("SNOWFLAKE_HOME", "./");

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 4);
  assert_string_equal(boost::get<std::string>(connectionParams["key1"]).c_str(), "value1");
  assert_true(boost::get<bool>(connectionParams["key2"]));
  assert_int_equal(boost::get<int>(connectionParams["key3"]), 3);
  assert_int_equal(boost::get<double>(connectionParams["key4"]), 4.4);
}

void test_token_file_path(void** unused) {
  SF_UNUSED(unused);
  
  FileCleanup tokenCleanup("./test_token.txt");
  FileCleanup tomlCleanup("./connections.toml");
  std::ofstream tokenFile;
  tokenFile.open(tokenCleanup.path(), std::fstream::out);
  tokenFile << "test_token_from_file\n";
  tokenFile.close();

#ifndef _WIN32
  boost::filesystem::permissions(tokenCleanup.path(),
    boost::filesystem::owner_read | boost::filesystem::owner_write);
#endif

  std::string tomlConfig = "[default]\nauthenticator = \"oauth\"\ntoken_file_path = \"" + tokenCleanup.path() + "\"";
  std::ofstream file;
  file.open(tomlCleanup.path(), std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride override("SNOWFLAKE_HOME", "./");

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  
  assert_int_equal(connectionParams.size(), 2);
  assert_string_equal(boost::get<std::string>(connectionParams["token"]).c_str(), "test_token_from_file");
  assert_string_equal(boost::get<std::string>(connectionParams["authenticator"]).c_str(), "oauth");
  assert_true(connectionParams.find("token_file_path") == connectionParams.end());
}

void test_token_file_path_missing_file(void** unused) {
  SF_UNUSED(unused);
  
  FileCleanup tomlCleanup("./connections.toml");
  std::string tomlConfig = "[default]\nauthenticator = \"oauth\"\ntoken_file_path = \"/nonexistent/path/to/token.txt\"";
  std::ofstream file;
  file.open(tomlCleanup.path(), std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride override("SNOWFLAKE_HOME", "./");

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  
  assert_int_equal(connectionParams.size(), 1);
  assert_true(connectionParams.find("token") == connectionParams.end());
  assert_true(connectionParams.find("token_file_path") == connectionParams.end());
}

void test_token_file_path_empty_file(void** unused) {
  SF_UNUSED(unused);
  
  FileCleanup tokenCleanup("./empty_token.txt");
  FileCleanup tomlCleanup("./connections.toml");
  std::ofstream tokenFile;
  tokenFile.open(tokenCleanup.path(), std::fstream::out);
  tokenFile.close();

#ifndef _WIN32
  boost::filesystem::permissions(tokenCleanup.path(),
    boost::filesystem::owner_read | boost::filesystem::owner_write);
#endif

  std::string tomlConfig = "[default]\nauthenticator = \"oauth\"\ntoken_file_path = \"" + tokenCleanup.path() + "\"";
  std::ofstream file;
  file.open(tomlCleanup.path(), std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride override("SNOWFLAKE_HOME", "./");

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  
  assert_int_equal(connectionParams.size(), 1);
  assert_true(connectionParams.find("token") == connectionParams.end());
  assert_true(connectionParams.find("token_file_path") == connectionParams.end());
}

#ifndef _WIN32
void test_toml_config_permissions(void **unused) {
  SF_UNUSED(unused);
  
  FileCleanup tomlCleanup("./connections.toml");
  std::string tomlConfig = "[default]\nkey1 = \"value1\"";
  std::ofstream file;
  file.open(tomlCleanup.path(), std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride override("SNOWFLAKE_HOME", "./");
  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams;

  boost::filesystem::permissions(tomlCleanup.path(), owner_read | owner_write);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 1);
  assert_string_equal(boost::get<std::string>(connectionParams["key1"]).c_str(), "value1");

  boost::filesystem::permissions(tomlCleanup.path(), owner_all | group_write);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 0);

  boost::filesystem::permissions(tomlCleanup.path(), owner_all | others_write);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 0);

  boost::filesystem::permissions(tomlCleanup.path(), owner_all | group_write | others_write);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 0);

  boost::filesystem::permissions(tomlCleanup.path(), owner_read | owner_write | group_read);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 1);
  assert_string_equal(boost::get<std::string>(connectionParams["key1"]).c_str(), "value1");

  boost::filesystem::permissions(tomlCleanup.path(), owner_read | owner_write | others_read);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 1);
  assert_string_equal(boost::get<std::string>(connectionParams["key1"]).c_str(), "value1");

  boost::filesystem::permissions(tomlCleanup.path(), owner_read | owner_write | group_read | group_write);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 0);
}

void test_toml_skip_read_warning(void **unused) {
  SF_UNUSED(unused);
  
  FileCleanup tomlCleanup("./connections.toml");
  std::string tomlConfig = "[default]\nkey1 = \"value1\"";
  std::ofstream file;
  file.open(tomlCleanup.path(), std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride permOverride("SF_SKIP_WARNING_FOR_READ_PERMISSIONS_ON_CONFIG_FILE", "true");
  EnvOverride homeOverride("SNOWFLAKE_HOME", "./");

  boost::filesystem::permissions(tomlCleanup.path(), owner_all | group_read | others_read);
  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 1);
  assert_string_equal(boost::get<std::string>(connectionParams["key1"]).c_str(), "value1");

  boost::filesystem::permissions(tomlCleanup.path(), owner_all | group_write);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 0);

  boost::filesystem::permissions(tomlCleanup.path(), owner_all | others_write);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 0);
}

void test_token_file_permissions(void **unused) {
  SF_UNUSED(unused);
  
  FileCleanup tokenCleanup("./test_token_perms.txt");
  FileCleanup tomlCleanup("./connections.toml");
  std::ofstream tokenFile;
  tokenFile.open(tokenCleanup.path(), std::fstream::out);
  tokenFile << "test_token_value";
  tokenFile.close();

  std::string tomlConfig = "[default]\nauthenticator = \"oauth\"\ntoken_file_path = \"" + tokenCleanup.path() + "\"";
  std::ofstream file;
  file.open(tomlCleanup.path(), std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride override("SNOWFLAKE_HOME", "./");

  boost::filesystem::permissions(tomlCleanup.path(), owner_read | owner_write);
  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams;

  boost::filesystem::permissions(tokenCleanup.path(), owner_read | owner_write);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 2);
  assert_string_equal(boost::get<std::string>(connectionParams["token"]).c_str(), "test_token_value");

  boost::filesystem::permissions(tokenCleanup.path(), owner_read | owner_write | group_read);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 1);
  assert_true(connectionParams.find("token") == connectionParams.end());

  boost::filesystem::permissions(tokenCleanup.path(), owner_read | owner_write | group_write);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 1);
  assert_true(connectionParams.find("token") == connectionParams.end());

  boost::filesystem::permissions(tokenCleanup.path(), owner_read | owner_write | group_exe);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 1);
  assert_true(connectionParams.find("token") == connectionParams.end());

  boost::filesystem::permissions(tokenCleanup.path(), owner_read | owner_write | others_read);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 1);
  assert_true(connectionParams.find("token") == connectionParams.end());

  boost::filesystem::permissions(tokenCleanup.path(), owner_read | owner_write | others_write);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 1);
  assert_true(connectionParams.find("token") == connectionParams.end());

  boost::filesystem::permissions(tokenCleanup.path(), owner_read | owner_write | others_exe);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 1);
  assert_true(connectionParams.find("token") == connectionParams.end());

  boost::filesystem::permissions(tokenCleanup.path(), owner_read);
  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 2);
  assert_string_equal(boost::get<std::string>(connectionParams["token"]).c_str(), "test_token_value");
}

void test_token_priority(void **unused) {
  SF_UNUSED(unused);
  
  FileCleanup tokenCleanup("./test_token_priority.txt");
  FileCleanup tomlCleanup("./connections.toml");
  std::ofstream tokenFile;
  tokenFile.open(tokenCleanup.path(), std::fstream::out);
  tokenFile << "token_from_file";
  tokenFile.close();

#ifndef _WIN32
  boost::filesystem::permissions(tokenCleanup.path(), owner_read | owner_write);
#endif

  std::string tomlConfig = "[default]\nauthenticator = \"oauth\"\ntoken = \"inline_token\"\ntoken_file_path = \"" + tokenCleanup.path() + "\"";
  std::ofstream file;
  file.open(tomlCleanup.path(), std::fstream::out);
  file << tomlConfig;
  file.close();

#ifndef _WIN32
  boost::filesystem::permissions(tomlCleanup.path(), owner_read | owner_write);
#endif

  EnvOverride override("SNOWFLAKE_HOME", "./");

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 2);
  assert_string_equal(boost::get<std::string>(connectionParams["token"]).c_str(), "inline_token");
  assert_true(connectionParams.find("token_file_path") == connectionParams.end());

  tomlConfig = "[default]\nauthenticator = \"oauth\"\ntoken_file_path = \"" + tokenCleanup.path() + "\"";
  file.open(tomlCleanup.path(), std::fstream::out | std::fstream::trunc);
  file << tomlConfig;
  file.close();

  connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 2);
  assert_string_equal(boost::get<std::string>(connectionParams["token"]).c_str(), "token_from_file");
  assert_true(connectionParams.find("token_file_path") == connectionParams.end());
}

void test_skip_token_file_verification(void **unused) {
  SF_UNUSED(unused);
  
  FileCleanup tokenCleanup("./test_token_skip.txt");
  FileCleanup tomlCleanup("./connections.toml");
  std::ofstream tokenFile;
  tokenFile.open(tokenCleanup.path(), std::fstream::out);
  tokenFile << "test_token_value";
  tokenFile.close();

  std::string tomlConfig = "[default]\nauthenticator = \"oauth\"\ntoken_file_path = \"" + tokenCleanup.path() + "\"";
  std::ofstream file;
  file.open(tomlCleanup.path(), std::fstream::out);
  file << tomlConfig;
  file.close();

  EnvOverride permOverride("SKIP_TOKEN_FILE_PERMISSIONS_VERIFICATION", "true");
  EnvOverride homeOverride("SNOWFLAKE_HOME", "./");

  boost::filesystem::permissions(tomlCleanup.path(), owner_read | owner_write);
  
  boost::filesystem::permissions(tokenCleanup.path(), owner_all | group_all | others_all);

  std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams = load_toml_config();
  assert_int_equal(connectionParams.size(), 2);
  assert_string_equal(boost::get<std::string>(connectionParams["token"]).c_str(), "test_token_value");
  
  assert_true(connectionParams.find("token") != connectionParams.end());
  assert_true(connectionParams.find("token_file_path") == connectionParams.end());
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
