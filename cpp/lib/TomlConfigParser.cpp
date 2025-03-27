#include "snowflake/TomlConfigParser.hpp"
#include "../logger/SFLogger.hpp"
#include "memory.h"

#define TOML_EXCEPTIONS 0
#include <toml++/toml.hpp>

#include <boost/filesystem.hpp>

namespace
{
  using namespace Snowflake::Client;

  // constants
  const std::string ENV_SNOWFLAKE_HOME = "SNOWFLAKE_HOME";
  const std::string ENV_SNOWFLAKE_DEF_CONN_NAME = "SNOWFLAKE_DEFAULT_CONNECTION_NAME";
  const std::string SNOWFLAKE_HOME_DIR = ".snowflake";
  const std::string TOML_FILENAME = "connections.toml";

  // helpers
  std::string getEnvironmentVariableValue(const std::string& envVarName) {
    // Environment variables being checked point to file paths, hence MAX_PATH is used
    char envbuf[MAX_PATH + 1];
    if (char* value = sf_getenv_s(envVarName.c_str(), envbuf, sizeof(envbuf))) {
      return std::string(value);
    }
    return "";
  }

  boost::filesystem::path resolveTomlPath() {
    boost::filesystem::path tomlFilePath;
    // Check in SNOWFLAKE_HOME
    std::string snowflakeHomeEnv = getEnvironmentVariableValue(ENV_SNOWFLAKE_HOME);
    if (!snowflakeHomeEnv.empty()) {
      tomlFilePath = snowflakeHomeEnv;
      tomlFilePath.append(TOML_FILENAME);
    } else {
      // Check in ~/.snowflake
#if defined(_WIN32) || defined(_WIN64)
      std::string homeDir = getEnvironmentVariableValue("USERPROFILE");
      if (!homeDir.empty()) {
        tomlFilePath = homeDir;
        tomlFilePath.append(SNOWFLAKE_HOME_DIR);
        tomlFilePath.append(TOML_FILENAME);
      } else {
        // USERPROFILE is empty, try HOMEDRIVE and HOMEPATH
        std::string homeDriveEnv = getEnvironmentVariableValue("HOMEDRIVE");
        std::string homePathEnv = getEnvironmentVariableValue("HOMEPATH");
        if (!homeDriveEnv.empty() && !homePathEnv.empty()) {
          tomlFilePath = std::string(homeDriveEnv) + homePathEnv;
          tomlFilePath.append(SNOWFLAKE_HOME_DIR);
          tomlFilePath.append(TOML_FILENAME);
        }
      }
#else
      std::string homeDir = getEnvironmentVariableValue("HOME");
      if (!homeDir.empty()) {
        tomlFilePath = homeDir;
        tomlFilePath.append(SNOWFLAKE_HOME_DIR);
        tomlFilePath.append(TOML_FILENAME);
      }
#endif
    }
    return tomlFilePath;
  }

  std::map<std::string, boost::variant<std::string, int, bool, double>> parseTomlFile(const boost::filesystem::path& filePath) {
    std::map<std::string, boost::variant<std::string, int, bool, double>> connectionParams;
    toml::parse_result result = toml::parse_file(filePath.c_str());
    if (!result)
    {
      CXX_LOG_ERROR("Failed to parse toml file: %s. Error: %s", filePath.c_str(), result.error().description().data());
      return connectionParams;
    }
    toml::table table = result.table();
    std::string configurationName = getEnvironmentVariableValue(ENV_SNOWFLAKE_DEF_CONN_NAME);
    if (configurationName.empty()) {
      configurationName = "default";
    }
    toml::node_view config = table[configurationName];
    if (!config) {
      CXX_LOG_ERROR("Could not find connection configuration name %s in toml file.", configurationName.c_str());
      return connectionParams;
    }
    for (auto [key, val] : *config.as_table()) {
      if (val.is_string()) {
        connectionParams[key.data()] = val.as_string()->get();
      } else if (val.is_boolean()) {
        connectionParams[key.data()] = val.as_boolean()->get();
      } else if (val.is_integer()) {
        connectionParams[key.data()] = (int)val.as_integer()->get();
      } else if (val.is_floating_point()) {
        connectionParams[key.data()] = val.as_floating_point()->get();
      } else {
        CXX_LOG_TRACE("Ignoring key in toml file due to unsupported data type: %s", key.data());
      }
    }
    return connectionParams;
  }
}

std::map<std::string, boost::variant<std::string, int, bool, double>> load_toml_config()
{
  std::map<std::string, boost::variant<std::string, int, bool, double>> params;
  boost::filesystem::path derivedTomlFilePath = resolveTomlPath();

  if (!derivedTomlFilePath.empty()) {
    CXX_LOG_INFO("Using toml file path: %s", derivedTomlFilePath.c_str());
    params = parseTomlFile(derivedTomlFilePath);
  }
  return params;
}
