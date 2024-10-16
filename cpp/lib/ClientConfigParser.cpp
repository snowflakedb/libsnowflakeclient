/**
 * Copyright (c) 2024 Snowflake Computing
 */

#include "snowflake/ClientConfigParser.hpp"
#include "snowflake/Exceptions.hpp"
#include "snowflake/client_config_parser.h"
#include "../logger/SFLogger.hpp"
#include "memory.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <set>

#ifndef _WIN32 
#include <dlfcn.h>
#endif

using namespace Snowflake::Client;
using namespace std::filesystem;

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
    ClientConfigParser configParser;
    configParser.loadClientConfig(in_configFilePath, *out_clientConfig);
  } catch (std::exception e) {
    sf_fprintf(stderr, e.what());
    return false;
  }
}

// Public ======================================================================
////////////////////////////////////////////////////////////////////////////////
ClientConfigParser::ClientConfigParser()
{
  ; // Do nothing.
}

////////////////////////////////////////////////////////////////////////////////
ClientConfigParser::~ClientConfigParser()
{
  ; // Do nothing.
}

////////////////////////////////////////////////////////////////////////////////
void ClientConfigParser::loadClientConfig(
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
std::string ClientConfigParser::resolveClientConfigPath(
  const std::string& in_configFilePath)
{
  char envbuf[MAX_PATH + 1];
  std::string derivedConfigPath = "";
  if (!in_configFilePath.empty())
  {
    // 1. Try config file if it was passed in
    derivedConfigPath = in_configFilePath;
    CXX_LOG_INFO("sf", "ClientConfigParser", "loadClientConfig",
      "Using client configuration path from a connection string: %s", in_configFilePath.c_str());
  }
  else if (const char* clientConfigEnv = sf_getenv_s(SF_CLIENT_CONFIG_ENV_NAME.c_str(), envbuf, sizeof(envbuf)))
  {
    // 2. Try environment variable SF_CLIENT_CONFIG_ENV_NAME
    derivedConfigPath = clientConfigEnv;
    CXX_LOG_INFO("sf", "ClientConfigParser", "loadClientConfig",
      "Using client configuration path from an environment variable: %s", clientConfigEnv);
  }
  else
  {
    // 3. Try DLL binary dir
    std::string binaryDir = getBinaryPath();
    std::string binaryDirFilePath = binaryDir + SF_CLIENT_CONFIG_FILE_NAME;
    if (is_regular_file(binaryDirFilePath))
    {
      derivedConfigPath = binaryDirFilePath;
      CXX_LOG_INFO("sf", "ClientConfigParser", "loadClientConfig",
        "Using client configuration path from binary directory: %s", binaryDirFilePath.c_str());
    }
    else
    {
#if defined(WIN32) || defined(_WIN64)
      // 4. Try user home dir
      std::string homeDirFilePath;
      char* homeDir;
      if ((homeDir = sf_getenv_s("USERPROFILE", envbuf, sizeof(envbuf))) && strlen(homeDir) != 0)
      {
        homeDirFilePath = homeDir + PATH_SEP + SF_CLIENT_CONFIG_FILE_NAME;
      } else {
        // USERPROFILE is empty, try HOMEDRIVE and HOMEPATH
        char* homeDriveEnv;
        char* homePathEnv;
        if ((homeDriveEnv = sf_getenv_s("HOMEDRIVE", envbuf, sizeof(envbuf))) && (strlen(homeDriveEnv) != 0) &&
          (homePathEnv = sf_getenv_s("HOMEPATH", envbuf, sizeof(envbuf))) && (strlen(homePathEnv) != 0))
        {
          homeDirFilePath = std::string(homeDriveEnv) + homePathEnv + PATH_SEP + SF_CLIENT_CONFIG_FILE_NAME;
        }
      }
      if (is_regular_file(homeDirFilePath))
      {
        derivedConfigPath = homeDirFilePath;
        CXX_LOG_INFO("sf", "ClientConfigParser", "loadClientConfig",
          "Using client configuration path from home directory: %s", homeDirFilePath.c_str());
      }
#else
      // 4. Try user home dir
      char* homeDir;
      if ((homeDir = sf_getenv_s("HOME", envbuf, sizeof(envbuf))) && (strlen(homeDir) != 0)
      {
        std::string homeDirFilePath = std::string(homeDir) + PATH_SEP + SF_CLIENT_CONFIG_FILE_NAME;
        if (is_regular_file(homeDirFilePath))
        {
          derivedConfigPath = homeDirFilePath;
          CXX_LOG_INFO("sf", "ClientConfigParser", "loadClientConfig",
            "Using client configuration path from home directory: %s", homeDirFilePath.c_str());
        }
      }
#endif
    }
  }
  return derivedConfigPath;
}

// Private =====================================================================
////////////////////////////////////////////////////////////////////////////////
void ClientConfigParser::parseConfigFile(
  const std::string& in_filePath,
  client_config& out_clientConfig)
{
  cJSON* jsonConfig;
  FILE* configFile;
  try 
  {
    configFile = sf_fopen(&configFile, in_filePath.c_str(), "r");
    if (!configFile)
    {
      CXX_LOG_INFO("sf", "ClientConfigParser", "parseConfigFile",
        "Could not open a file. The file may not exist: %s",
        in_filePath.c_str());
      std::string errMsg = "Error finding client configuration file: " + in_filePath;
      throw ClientConfigException(errMsg.c_str());
    }
#if !defined(WIN32) && !defined(_WIN64)
    checkIfValidPermissions(in_filePath);
#endif
    const int fileSize = file_size(in_filePath);
    char* buffer = (char*)malloc(fileSize);
    if (buffer)
    {
      size_t result = fread(buffer, 1, fileSize, configFile);
      if (result != fileSize)
      {
        CXX_LOG_ERROR(
          "sf",
          "ClientConfigParser",
          "parseConfigFile",
          "Error in reading file: %s", in_filePath.c_str());
      }
    }
    jsonConfig = snowflake_cJSON_Parse(buffer);
    const char* error_ptr = snowflake_cJSON_GetErrorPtr();
    if (error_ptr)
    {
      CXX_LOG_ERROR(
        "sf",
        "ClientConfigParser",
        "parseConfigFile",
        "Error in parsing JSON: %s, err: %s", in_filePath.c_str(), error_ptr);
      std::string errMsg = "Error parsing client configuration file: " + in_filePath;
      throw ClientConfigException(errMsg.c_str());
    }
  }
  catch (std::exception& ex)
  {
    if (configFile)
    {
      fclose(configFile);
    }
    throw;
  }

  const cJSON* commonProps = snowflake_cJSON_GetObjectItem(jsonConfig, "common");
  checkUnknownEntries(snowflake_cJSON_Print(commonProps));
  const cJSON* logLevel = snowflake_cJSON_GetObjectItem(commonProps, "log_level");
  if (snowflake_cJSON_IsString(logLevel))
  {
    out_clientConfig.logLevel = snowflake_cJSON_GetStringValue(logLevel);
  }
  const cJSON* logPath = snowflake_cJSON_GetObjectItem(commonProps, "log_path");
  if (snowflake_cJSON_IsString(logPath))
  {
    out_clientConfig.logPath = snowflake_cJSON_GetStringValue(logPath);
  }
  if (configFile)
  {
    fclose(configFile);
  }
}

////////////////////////////////////////////////////////////////////////////////
void ClientConfigParser::checkIfValidPermissions(const std::string& in_filePath)
{
  file_status fileStatus = status(in_filePath);
  perms permissions = fileStatus.permissions();
  if ((perms::group_write == (permissions & perms::group_write)) ||
      (perms::others_write == (permissions & perms::others_write)))
  {
    CXX_LOG_ERROR(
      "sf",
      "ClientConfigParser",
      "checkIfValidPermissions",
      "Error due to other users having permission to modify the config file: %s",
      in_filePath.c_str());
    std::string errMsg = "Error due to other users having permission to modify the config file: " + in_filePath;
    throw ClientConfigException(errMsg.c_str());
  }
}

////////////////////////////////////////////////////////////////////////////////
void ClientConfigParser::checkUnknownEntries(const std::string& in_jsonString)
{
  cJSON* jsonConfig = snowflake_cJSON_Parse(in_jsonString.c_str());
  const char* error_ptr = snowflake_cJSON_GetErrorPtr();
  if (error_ptr)
  {
    CXX_LOG_ERROR(
      "sf",
      "ClientConfigParser",
      "checkUnknownEntries",
      "Error in parsing JSON: %s, err: %s", in_jsonString.c_str(), error_ptr);
    std::string errMsg = "Error parsing json: " + in_jsonString;
    throw ClientConfigException(errMsg.c_str());
  }

  if (snowflake_cJSON_IsObject(jsonConfig))
  {
    cJSON* entry = NULL;
    snowflake_cJSON_ArrayForEach(entry, jsonConfig)
    {
      std::string key = entry->string;
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
          "Unknown configuration entry: " + key + " with value:" + entry->valuestring;
        CXX_LOG_WARN(
          "sf",
          "ClientConfigParser",
          "checkUnknownEntries",
          warnMsg.c_str());
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
std::string ClientConfigParser::getBinaryPath()
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
