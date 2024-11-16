/**
 * Copyright (c) 2024 Snowflake Computing
 */

#include "snowflake/ClientConfigParser.hpp"
#include "snowflake/Exceptions.hpp"
#include "snowflake/client_config_parser.h"
#include "../logger/SFLogger.hpp"
#include "memory.h"

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
  std::set<std::string> KnownCommonEntries = {"log_level", "log_path"};
}

////////////////////////////////////////////////////////////////////////////////
sf_bool load_client_config(
  const char* in_configFilePath,
  client_config* out_clientConfig)
{
  try {
    EasyLoggingConfigParser configParser;
    configParser.loadClientConfig(in_configFilePath, *out_clientConfig);
  } catch (std::exception e) {
    CXX_LOG_ERROR("Using client configuration path from a connection string: %s", e.what());
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
void free_client_config(client_config* clientConfig)
{
  if (clientConfig->logLevel != NULL)
  {
    SF_FREE(clientConfig->logLevel);
  }
  if (clientConfig->logPath != NULL)
  {
    SF_FREE(clientConfig->logPath);
  }
}

// Public ======================================================================
////////////////////////////////////////////////////////////////////////////////
EasyLoggingConfigParser::EasyLoggingConfigParser()
{
  ; // Do nothing.
}

////////////////////////////////////////////////////////////////////////////////
EasyLoggingConfigParser::~EasyLoggingConfigParser()
{
  ; // Do nothing.
}

////////////////////////////////////////////////////////////////////////////////
void EasyLoggingConfigParser::loadClientConfig(
  const std::string& in_configFilePath,
  client_config& out_clientConfig)
{
  std::string derivedConfigPath = resolveClientConfigPath(in_configFilePath);

  if (!derivedConfigPath.empty())
  {
    parseConfigFile(derivedConfigPath, out_clientConfig);
  }
}

////////////////////////////////////////////////////////////////////////////////
std::string EasyLoggingConfigParser::resolveClientConfigPath(
  const std::string& in_configFilePath)
{
  char envbuf[MAX_PATH + 1];
  if (!in_configFilePath.empty())
  {
    // 1. Try config file if it was passed in
    CXX_LOG_INFO("Using client configuration path from a connection string: %s", in_configFilePath.c_str());
    return in_configFilePath;
  }
  else if (const char* clientConfigEnv = sf_getenv_s(SF_CLIENT_CONFIG_ENV_NAME.c_str(), envbuf, sizeof(envbuf)))
  {
    // 2. Try environment variable SF_CLIENT_CONFIG_ENV_NAME
    CXX_LOG_INFO("Using client configuration path from an environment variable: %s", clientConfigEnv);
    return clientConfigEnv;
  }
  else
  {
    // 3. Try DLL binary dir
    std::string binaryDir = getBinaryPath();
    std::string binaryDirFilePath = binaryDir + SF_CLIENT_CONFIG_FILE_NAME;
    if (boost::filesystem::is_regular_file(binaryDirFilePath))
    {
      CXX_LOG_INFO("Using client configuration path from binary directory: %s", binaryDirFilePath.c_str());
      return binaryDirFilePath;
    }
    else
    {
      // 4. Try user home dir
      return resolveHomeDirConfigPath();
    }
  }
}

// Private =====================================================================
////////////////////////////////////////////////////////////////////////////////
std::string EasyLoggingConfigParser::resolveHomeDirConfigPath()
{
  char envbuf[MAX_PATH + 1];
#if defined(WIN32) || defined(_WIN64)
  std::string homeDirFilePath;
  char* homeDir;
  if ((homeDir = sf_getenv_s("USERPROFILE", envbuf, sizeof(envbuf))) && strlen(homeDir) != 0)
  {
    homeDirFilePath = homeDir + PATH_SEP + SF_CLIENT_CONFIG_FILE_NAME;
  } else {
    // USERPROFILE is empty, try HOMEDRIVE and HOMEPATH
    char* homeDriveEnv = sf_getenv_s("HOMEDRIVE", envbuf, sizeof(envbuf));
    char* homePathEnv = sf_getenv_s("HOMEPATH", envbuf, sizeof(envbuf));
    if (homeDriveEnv && strlen(homeDriveEnv) != 0 && homePathEnv && strlen(homePathEnv) != 0)
    {
      homeDirFilePath = std::string(homeDriveEnv) + homePathEnv + PATH_SEP + SF_CLIENT_CONFIG_FILE_NAME;
    }
  }
  if (boost::filesystem::is_regular_file(homeDirFilePath))
  {
    CXX_LOG_INFO("Using client configuration path from home directory: %s", homeDirFilePath.c_str());
    return homeDirFilePath;
  }
#else
  char* homeDir;
  if ((homeDir = sf_getenv_s("HOME", envbuf, sizeof(envbuf))) && (strlen(homeDir) != 0))
  {
    std::string homeDirFilePath = std::string(homeDir) + PATH_SEP + SF_CLIENT_CONFIG_FILE_NAME;
    if (boost::filesystem::is_regular_file(homeDirFilePath))
    {
      CXX_LOG_INFO("Using client configuration path from home directory: %s", homeDirFilePath.c_str());
      return homeDirFilePath;
    }
  }
#endif
  return "";
}

////////////////////////////////////////////////////////////////////////////////
void EasyLoggingConfigParser::parseConfigFile(
  const std::string& in_filePath,
  client_config& out_clientConfig)
{
  value jsonConfig;
  std::string err;
  std::ifstream configFile;
  try
  {
    configFile.open(in_filePath, std::fstream::in | std::ios::binary);
    if (!configFile)
    {
      CXX_LOG_INFO("Could not open a file. The file may not exist: %s",
        in_filePath.c_str());
      std::string errMsg = "Error finding client configuration file: " + in_filePath;
      throw ClientConfigException(errMsg.c_str());
    }
#if !defined(WIN32) && !defined(_WIN64)
    checkIfValidPermissions(in_filePath);
#endif
    err = parse(jsonConfig, configFile);
    if (!err.empty())
    {
      CXX_LOG_ERROR("Error in parsing JSON: %s, err: %s", in_filePath.c_str(), err.c_str());
      std::string errMsg = "Error parsing client configuration file: " + in_filePath;
      throw ClientConfigException(errMsg.c_str());
    }
  }
  catch (std::exception& e)
  {
    configFile.close();
    throw;
  }

  value commonProps = jsonConfig.get("common");
  checkUnknownEntries(commonProps);
  if (commonProps.is<object>())
  {
    if (commonProps.contains("log_level") && commonProps.get("log_level").is<std::string>())
    {
      const char* logLevel = commonProps.get("log_level").get<std::string>().c_str();
      size_t logLevelSize = strlen(logLevel) + 1;
      out_clientConfig.logLevel = (char*)SF_CALLOC(1, logLevelSize);
      sf_strcpy(out_clientConfig.logLevel, logLevelSize, logLevel);
    }
    if (commonProps.contains("log_path") && commonProps.get("log_path").is<std::string>())
    {
      const char* logPath = commonProps.get("log_path").get<std::string>().c_str();
      size_t logPathSize = strlen(logPath) + 1;
      out_clientConfig.logPath = (char*)SF_CALLOC(1, logPathSize);
      sf_strcpy(out_clientConfig.logPath, logPathSize, logPath);
    }
  }
  configFile.close();
}

////////////////////////////////////////////////////////////////////////////////
void EasyLoggingConfigParser::checkIfValidPermissions(const std::string& in_filePath)
{
  boost::filesystem::file_status fileStatus = boost::filesystem::status(in_filePath);
  boost::filesystem::perms permissions = fileStatus.permissions();
  if (permissions & boost::filesystem::group_write ||
      permissions & boost::filesystem::others_write)
  {
    CXX_LOG_ERROR("Error due to other users having permission to modify the config file: %s",
      in_filePath.c_str());
    std::string errMsg = "Error due to other users having permission to modify the config file: " + in_filePath;
    throw ClientConfigException(errMsg.c_str());
  }
}

////////////////////////////////////////////////////////////////////////////////
void EasyLoggingConfigParser::checkUnknownEntries(value& in_config)
{
  if (in_config.is<object>())
  {
    const value::object& configObj = in_config.get<object>();
    for (value::object::const_iterator i = configObj.begin(); i != configObj.end(); ++i)
    {
      std::string key = i->first;
      bool found = false;
      for (std::string knownEntry : KnownCommonEntries)
      {
        if (sf_strncasecmp(key.c_str(), knownEntry.c_str(), knownEntry.length()) == 0)
        {
          found = true;
        }
      }
      if (!found)
      {
        std::string warnMsg =
          "Unknown configuration entry: " + key + " with value:" + i->second.to_str().c_str();
        CXX_LOG_WARN(warnMsg.c_str());
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
std::string EasyLoggingConfigParser::getBinaryPath()
{
  std::string binaryFullPath;
#if defined(WIN32) || defined(_WIN64)
  std::wstring path;
  HMODULE hm = NULL;
  wchar_t appName[256];
  GetModuleFileNameW(hm, appName, 256);
  path = appName;
  binaryFullPath = std::string(path.begin(), path.end());
#else
  Dl_info info;
  int result = dladdr((void*)load_client_config, &info);
  if (result)
  {
    binaryFullPath = std::string(info.dli_fname);
  }
#endif
  size_t pos = binaryFullPath.find_last_of(PATH_SEP);
  if (pos == std::string::npos)
  {
    return "";
  }
  std::string binaryPath = binaryFullPath.substr(0, pos + 1);
  return binaryPath;
}
