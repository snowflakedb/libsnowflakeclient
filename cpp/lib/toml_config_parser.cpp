/**
 * Copyright (c) 2025 Snowflake Computing
 */

#include "snowflake/toml_config_parser.h"
#include "../logger/SFLogger.hpp"
#include "memory.h"
#include <toml++/toml.hpp>
#include <filesystem>

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

  std::filesystem::path resolveTomlPath() {
    std::filesystem::path tomlFilePath;
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

  void parseTomlFile(
    const std::filesystem::path& filePath,
    char **connectionParams) {
    if (connectionParams == NULL) {
      CXX_LOG_ERROR("NULL pointer passed into parse toml file.");
      return;
    }
    try {
      toml::table table;
      table = toml::parse_file(filePath.c_str());
      std::string configurationName = getEnvironmentVariableValue(ENV_SNOWFLAKE_DEF_CONN_NAME);
      if (configurationName.empty()) {
        configurationName = "default";
      }
      toml::node_view config = table[configurationName];
      if (!config) {
        CXX_LOG_ERROR("Could not find connection configuration name %s in toml file.", configurationName.c_str());
        return;
      }
      size_t paramSize = config.as_table()->size();
      int i = 0;
      std::string connStr;
      for (auto [key, val] : *config.as_table()) {
        std::string keystr = key.data();
        connStr += keystr + "=" + val.as_string()->get() + ";";
      }
      *connectionParams = (char*)SF_MALLOC(connStr.size() + 1);
      sf_strcpy(*connectionParams, connStr.size() + 1, connStr.c_str());
    }
    catch (const toml::parse_error& err) {
      CXX_LOG_ERROR("Failed to parse toml file: %s. Error: %s", filePath.c_str(), err.what());
    }
  }
}

void load_toml_config(char** connectionParams)
{
// Disable toml config parsing for 32-bit windows debug build due to linking issues
// with _osfile causing hanging/assertions until dynamic linking is available
#if (!defined(_WIN32) && !defined(_DEBUG)) || defined(_WIN64)
  std::filesystem::path derivedTomlFilePath = resolveTomlPath();

  if (!derivedTomlFilePath.empty()) {
    parseTomlFile(derivedTomlFilePath, connectionParams);
  }
#endif
}
