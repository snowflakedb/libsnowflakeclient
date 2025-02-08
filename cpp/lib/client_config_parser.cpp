/**
 * Copyright (c) 2025 Snowflake Computing
 */


#include "snowflake/Exceptions.hpp"
#include "snowflake/client_config_parser.h"
#include "../logger/SFLogger.hpp"
#include "memory.h"
#include "picojson.h"

#include <fstream>
#include <sstream>
#include <set>

#undef snprintf
#include <boost/filesystem.hpp>

#ifndef _WIN32 
#include <dlfcn.h>
#endif

using namespace Snowflake::Client;
using namespace picojson;

namespace
{
  // constants
  const std::string SF_CLIENT_CONFIG_FILE_NAME("sf_client_config.json");
  const std::string SF_CLIENT_CONFIG_ENV_NAME("SF_CLIENT_CONFIG_FILE");
  const std::initializer_list<const std::string> KnownCommonEntries{ "log_level", "log_path" };

  // helpers
  ////////////////////////////////////////////////////////////////////////////////
#if defined(WIN32) || defined(_WIN64)
  boost::filesystem::path getBinaryPath()
  {
    boost::filesystem::path binaryFullPath;
    std::wstring path;
    HMODULE hm = NULL;
    wchar_t appName[256];
    GetModuleFileNameW(hm, appName, 256);
    path = appName;
    binaryFullPath = std::string(path.begin(), path.end());
    return binaryFullPath.parent_path();
  }
#else
  boost::filesystem::path getBinaryPath()
  {
    boost::filesystem::path binaryFullPath;
    Dl_info info;
    int result = dladdr((void*)load_client_config, &info);
    if (result)
    {
      binaryFullPath = std::string(info.dli_fname);
    }
    return binaryFullPath.parent_path();
  }
#endif

  ////////////////////////////////////////////////////////////////////////////////
  std::string getEnvironmentVariableValue(const std::string& envVarName)
  {
    char envbuf[MAX_PATH + 1];
    if (char* value = sf_getenv_s(envVarName.c_str(), envbuf, sizeof(envbuf)))
    {
      return std::string(value);
    }
    return "";
  }

  ////////////////////////////////////////////////////////////////////////////////
#if defined(WIN32) || defined(_WIN64)
  std::string resolveHomeDirConfigPath()
  {
    std::string homeDirFilePath;
    std::string homeDir = getEnvironmentVariableValue("USERPROFILE");
    if (!homeDir.empty())
    {
      homeDirFilePath = std::string(homeDir) + PATH_SEP + SF_CLIENT_CONFIG_FILE_NAME;
    }
    else {
      // USERPROFILE is empty, try HOMEDRIVE and HOMEPATH
      std::string homeDriveEnv = getEnvironmentVariableValue("HOMEDRIVE");
      std::string homePathEnv = getEnvironmentVariableValue("HOMEPATH");
      if (!homeDriveEnv.empty() && !homePathEnv.empty())
      {
        homeDirFilePath = homeDriveEnv + homePathEnv + PATH_SEP + SF_CLIENT_CONFIG_FILE_NAME;
      }
    }
    if (boost::filesystem::is_regular_file(homeDirFilePath))
    {
      CXX_LOG_INFO("Using client configuration path from home directory: %s", homeDirFilePath.c_str());
      return homeDirFilePath;
    }
    return "";
  }
#else
  std::string resolveHomeDirConfigPath()
  {
    char envbuf[MAX_PATH + 1];
    std::string homeDir = getEnvironmentVariableValue("HOME");
    if (!homeDir.empty())
    {
      std::string homeDirFilePath = std::string(homeDir) + PATH_SEP + SF_CLIENT_CONFIG_FILE_NAME;
      if (boost::filesystem::is_regular_file(homeDirFilePath))
      {
        CXX_LOG_INFO("Using client configuration path from home directory: %s", homeDirFilePath.c_str());
        return homeDirFilePath;
      }
    }
    return "";
  }
  #endif

  ////////////////////////////////////////////////////////////////////////////////
  boost::filesystem::path resolveClientConfigPath(
    const boost::filesystem::path& configFilePath)
  {
    // 1. Try config file if it was passed in
    if (!configFilePath.empty())
    {
      CXX_LOG_INFO("Using client configuration path from a connection string: %s", configFilePath.c_str());
      return configFilePath;
    }

    // 2. Try environment variable SF_CLIENT_CONFIG_ENV_NAME
    char envbuf[MAX_PATH + 1];
    if (const char* clientConfigEnv = sf_getenv_s(SF_CLIENT_CONFIG_ENV_NAME.c_str(), envbuf, sizeof(envbuf)))
    {
      CXX_LOG_INFO("Using client configuration path from an environment variable: %s", clientConfigEnv);
      return clientConfigEnv;
    }

    // 3. Try library dir
    boost::filesystem::path binaryDirFilePath = getBinaryPath().append(SF_CLIENT_CONFIG_FILE_NAME);
    if (boost::filesystem::is_regular_file(binaryDirFilePath))
    {
      CXX_LOG_INFO("Using client configuration path from library directory: %s", binaryDirFilePath.c_str());
      return binaryDirFilePath;
    }

    // 4. Try user home dir
    return resolveHomeDirConfigPath();
  }

  ////////////////////////////////////////////////////////////////////////////////
  bool isKnownCommonEntry(const std::string& entry) {
    return std::any_of(KnownCommonEntries.begin(), KnownCommonEntries.end(),
      [&entry](auto& knownEntry) {
      return sf_strncasecmp(entry.c_str(), knownEntry.c_str(), knownEntry.length()) == 0;
    });
  }

  ////////////////////////////////////////////////////////////////////////////////
  void checkUnknownEntries(value& config) {
    if (config.is<object>()) {
      for (auto& kv : config.get<object>()) {
        if (!isKnownCommonEntry(kv.first)) {
          CXX_LOG_WARN("Unknown configuration entry: %s with value: %s",
            kv.first.c_str(), kv.second.to_str().c_str());
        }
      }
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
  sf_bool checkIfValidPermissions(const boost::filesystem::path& filePath)
  {
    boost::filesystem::file_status fileStatus = boost::filesystem::status(filePath);
    boost::filesystem::perms permissions = fileStatus.permissions();
    if (permissions & boost::filesystem::group_write ||
      permissions & boost::filesystem::others_write)
    {
      CXX_LOG_ERROR("Error due to other users having permission to modify the config file: %s",
        filePath.c_str());
      return false;
    }
    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////
  sf_bool parseConfigFile(
    const boost::filesystem::path& filePath,
    client_config& clientConfig)
  {
    value jsonConfig;
    std::string err;
    std::ifstream configFile;
    configFile.open(filePath.string(), std::fstream::in | std::ios::binary);
    if (!configFile)
    {
      CXX_LOG_INFO("Could not open a file. The file may not exist: %s",
        filePath.c_str());
      return false;
    }
#if !defined(WIN32) && !defined(_WIN64)
    if (!checkIfValidPermissions(filePath))
    {
      return false;
    }
#endif
    err = parse(jsonConfig, configFile);

    if (!err.empty())
    {
      CXX_LOG_ERROR("Error in parsing JSON: %s, err: %s", filePath.c_str(), err.c_str());
      return false;
    }

    if (jsonConfig.is<object>())
    {
      value commonProps = jsonConfig.get("common");
      if (commonProps.is<object>())
      {
        checkUnknownEntries(commonProps);
        if (commonProps.contains("log_level") && commonProps.get("log_level").is<std::string>())
        {
          const char* logLevel = commonProps.get("log_level").get<std::string>().c_str();
          sf_strcpy(clientConfig.logLevel, strlen(logLevel) + 1, logLevel);
        }
        if (commonProps.contains("log_path") && commonProps.get("log_path").is<std::string>())
        {
          const char* logPath = commonProps.get("log_path").get<std::string>().c_str();
          sf_strcpy(clientConfig.logPath, strlen(logPath) + 1, logPath);
        }
        return true;
      }
    }
    CXX_LOG_ERROR("Malformed client config file: %s", filePath.c_str());
    return false;
  }

  ////////////////////////////////////////////////////////////////////////////////
  sf_bool loadClientConfig(
    const boost::filesystem::path& configFilePath,
    client_config& clientConfig)
  {
    boost::filesystem::path derivedConfigPath = resolveClientConfigPath(configFilePath);

    if (!derivedConfigPath.empty())
    {
      return parseConfigFile(derivedConfigPath, clientConfig);
    }
    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
sf_bool load_client_config(
  const char* configFilePath,
  client_config* clientConfig)
{
// Disable easy logging for 32-bit windows debug build due to linking issues
// with _osfile causing hanging/assertions until dynamic linking is available
#if (!defined(_WIN32) && !defined(_DEBUG)) || defined(_WIN64)
  return loadClientConfig(boost::filesystem::path(configFilePath), *clientConfig);
#else
  return true;
#endif
}
