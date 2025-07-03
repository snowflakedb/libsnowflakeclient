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

namespace
{
  using namespace Snowflake::Client;

  // constants
  const std::string SF_CLIENT_CONFIG_FILE_NAME("sf_client_config.json");
  const std::string SF_CLIENT_CONFIG_ENV_NAME("SF_CLIENT_CONFIG_FILE");
  const std::initializer_list<const std::string> KnownCommonEntries{ "log_level", "log_path" };

  // helpers
#if defined(_WIN32) || defined(_WIN64)
  boost::filesystem::path getBinaryPath()
  {
    boost::filesystem::path binaryFullPath;
#ifdef UNICODE
    wchar_t path[MAX_PATH];
#else
    char path[MAX_PATH];
#endif
    if (auto pathLen = GetModuleFileName(NULL, path, MAX_PATH); pathLen < MAX_PATH + 1) {
      binaryFullPath = std::string(path, path + pathLen);
    } else {
      std::string message = std::system_category().message(GetLastError());
      CXX_LOG_ERROR("Error getting binary path: %s", message.c_str());
    }
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
      binaryFullPath = info.dli_fname;
    }
    return binaryFullPath.parent_path();
  }
#endif

  std::string getEnvironmentVariableValue(const std::string& envVarName)
  {
    // Environment variables being checked point to file paths, hence MAX_PATH is used
    char envbuf[MAX_PATH + 1];
    if (char* value = sf_getenv_s(envVarName.c_str(), envbuf, sizeof(envbuf)))
    {
      return std::string(value);
    }
    return "";
  }

#if defined(_WIN32) || defined(_WIN64)
  boost::filesystem::path resolveHomeDirConfigPath()
  {
    boost::filesystem::path homeDirFilePath;
    std::string homeDir = getEnvironmentVariableValue("USERPROFILE");
    if (!homeDir.empty())
    {
      homeDirFilePath = homeDir;
      homeDirFilePath.append(SF_CLIENT_CONFIG_FILE_NAME);
    } else {
      // USERPROFILE is empty, try HOMEDRIVE and HOMEPATH
      std::string homeDriveEnv = getEnvironmentVariableValue("HOMEDRIVE");
      std::string homePathEnv = getEnvironmentVariableValue("HOMEPATH");
      if (!homeDriveEnv.empty() && !homePathEnv.empty())
      {
        homeDirFilePath = homeDriveEnv + homePathEnv;
        homeDirFilePath.append(SF_CLIENT_CONFIG_FILE_NAME);
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
  boost::filesystem::path resolveHomeDirConfigPath()
  {
    std::string homeDir = getEnvironmentVariableValue("HOME");
    if (!homeDir.empty())
    {
      boost::filesystem::path homeDirFilePath = homeDir;
      homeDirFilePath.append(SF_CLIENT_CONFIG_FILE_NAME);
      if (boost::filesystem::is_regular_file(homeDirFilePath))
      {
        CXX_LOG_INFO("Using client configuration path from home directory: %s", homeDirFilePath.c_str());
        return homeDirFilePath;
      }
    }
    return "";
  }
#endif

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
    std::string clientConfigEnv = getEnvironmentVariableValue(SF_CLIENT_CONFIG_ENV_NAME);
    if (!clientConfigEnv.empty())
    {
      CXX_LOG_INFO("Using client configuration path from an environment variable: %s", clientConfigEnv.c_str());
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

  bool isKnownCommonEntry(const std::string& entry) {
    return std::any_of(KnownCommonEntries.begin(), KnownCommonEntries.end(),
      [&entry](auto& knownEntry) {
        return sf_strncasecmp(entry.c_str(), knownEntry.c_str(), knownEntry.length()) == 0;
    });
  }

  void checkUnknownEntries(picojson::value& config) {
    if (config.is<picojson::object>()) {
      for (auto& kv : config.get<picojson::object>()) {
        if (!isKnownCommonEntry(kv.first)) {
          CXX_LOG_WARN("Unknown configuration entry: %s with value: %s",
            kv.first.c_str(), kv.second.to_str().c_str());
        }
      }
    }
  }

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

  sf_bool parseConfigFile(
    const boost::filesystem::path& filePath,
    client_config& clientConfig)
  {
    picojson::value jsonConfig;
    std::string err;
    std::ifstream configFile;
    configFile.open(filePath.string(), std::fstream::in | std::ios::binary);
    if (!configFile)
    {
      CXX_LOG_INFO("Could not open a file. The file may not exist: %s",
        filePath.c_str());
      return false;
    }
#if !defined(_WIN32) && !defined(_WIN64)
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

    if (jsonConfig.is<picojson::object>())
    {
      picojson::value commonProps = jsonConfig.get("common");
      if (commonProps.is<picojson::object>())
      {
        checkUnknownEntries(commonProps);
        if (commonProps.contains("log_level") && commonProps.get("log_level").is<std::string>())
        {
          const char* logLevel = commonProps.get("log_level").get<std::string>().c_str();
          if (strlen(logLevel) < 64) {
            sf_strcpy(clientConfig.logLevel, strlen(logLevel) + 1, logLevel);
          } else {
            CXX_LOG_ERROR("Error: The maximum length for log level is 64.");
            return false;
          }
        }
        if (commonProps.contains("log_path") && commonProps.get("log_path").is<std::string>())
        {
          const char* logPath = commonProps.get("log_path").get<std::string>().c_str();
          if (strlen(logPath) < MAX_PATH) {
            sf_strcpy(clientConfig.logPath, strlen(logPath) + 1, logPath);
          } else {
            CXX_LOG_ERROR("Error: The maximum length for log path is %d", MAX_PATH);
            return false;
          }
        }
        return true;
      }
    }
    CXX_LOG_ERROR("Malformed client config file: %s", filePath.c_str());
    return false;
  }

  sf_bool loadClientConfig(
    const boost::filesystem::path& configFilePath,
    client_config& clientConfig)
  {
    try {
      boost::filesystem::path derivedConfigPath = resolveClientConfigPath(configFilePath);
      
      if (!derivedConfigPath.empty())
      {
        return parseConfigFile(derivedConfigPath, clientConfig);
      }
    } catch (boost::filesystem::filesystem_error &e) {
      CXX_LOG_ERROR("boost filesystem error caught in loadClientConfig(): %s", e.what());
    } catch (const std::ios_base::failure &e) {
      // catch exception from fstream
      CXX_LOG_ERROR("file operation exception caught in loadClientConfig(): %s", e.what());
    } catch (const std::exception &e) {
      CXX_LOG_ERROR("Caught a general excpetion in loadClientConfig(): %s", e.what());
    } catch (...) {
      CXX_LOG_ERROR("Caught unknown exception in loadClientConfig()");
    }
    return false;
  }
}

sf_bool load_client_config(
  const char* configFilePath,
  client_config* clientConfig)
{
// Disable easy logging for 32-bit windows debug build due to linking issues
// with _osfile causing hanging/assertions until dynamic linking is available
#if (!defined(_WIN32) && !defined(_DEBUG)) || defined(_WIN64)
  return loadClientConfig(boost::filesystem::path(configFilePath), *clientConfig);
#else
  return false;
#endif
}
