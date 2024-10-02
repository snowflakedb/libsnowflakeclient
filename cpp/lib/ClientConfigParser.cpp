/**
 * Copyright (c) 2024 Snowflake Computing
 */

#include "snowflake/ClientConfigParser.hpp"
#include "snowflake/Exceptions.hpp"
#include "../logger/SFLogger.hpp"
#include "client_config_parser.h"
#include "memory.h"

#include <fstream>
#include <sstream>
#include <set>

#undef snprintf
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

#ifndef _WIN32 
#include <dlfcn.h>
#endif

using namespace Snowflake::Client;

namespace
{
  // constants
  const std::string SF_CLIENT_CONFIG_FILE_NAME("sf_client_config.json");
  const std::string SF_CLIENT_CONFIG_ENV_NAME("SF_CLIENT_CONFIG_FILE");
  std::set<std::string> KnownCommonEntries = {"log_level", "log_path"};
}

////////////////////////////////////////////////////////////////////////////////
void load_client_config(
  const char* in_configFilePath,
  client_config* out_clientConfig)
{
  ClientConfigParser configParser;
  ClientConfig clientConfig;
  configParser.loadClientConfig(in_configFilePath, clientConfig);
  if (!clientConfig.logLevel.empty())
  {
    out_clientConfig->logLevel = (char*)SF_CALLOC(1, sizeof(clientConfig.logLevel));
    strcpy(out_clientConfig->logLevel, clientConfig.logLevel.data());
  }
  if (!clientConfig.logPath.empty())
  {
    out_clientConfig->logPath = (char*)SF_CALLOC(1, sizeof(clientConfig.logPath));
    strcpy(out_clientConfig->logPath, clientConfig.logPath.data());
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
  ClientConfig& out_clientConfig)
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
    if (boost::filesystem::exists(binaryDirFilePath))
    {
      derivedConfigPath = binaryDirFilePath;
      CXX_LOG_INFO("sf", "ClientConfigParser", "loadClientConfig",
        "Using client configuration path from binary directory: %s", binaryDirFilePath.c_str());
    }
    else
    {
#if defined(WIN32) || defined(_WIN64)
      // 4. Try user home dir
      std::string homeDir = sf_getenv_s("USERPROFILE", envbuf, sizeof(envbuf));
      if (homeDir.empty())
      {
        // USERPROFILE is empty, try HOMEDRIVE and HOMEPATH
        std::string homeDriveEnv = sf_getenv_s("HOMEDRIVE", envbuf, sizeof(envbuf));
        std::string homePathEnv = sf_getenv_s("HOMEPATH", envbuf, sizeof(envbuf));
        if (!homeDriveEnv.empty() && !homePathEnv.empty())
        {
          homeDir = homeDriveEnv + homePathEnv;
        }
      }
      std::string homeDirFilePath = homeDir + PATH_SEP + SF_CLIENT_CONFIG_FILE_NAME;
      if (boost::filesystem::exists(homeDirFilePath))
      {
        derivedConfigPath = homeDirFilePath;
        CXX_LOG_INFO("sf", "ClientConfigParser", "loadClientConfig",
          "Using client configuration path from home directory: %s", homeDirFilePath.c_str());
      }
#else
      // 4. Try user home dir
      if (const char* homeDir = sf_getenv_s("HOME", envbuf, sizeof(envbuf)))
      {
        std::string homeDirFilePath = homeDir + PATH_SEP + SF_CLIENT_CONFIG_FILE_NAME;
        if (boost::filesystem::exists(homeDirFilePath))
        {
          derivedConfigPath = homeDirFilePath;
          CXX_LOG_INFO("sf", "ClientConfigParser", "loadClientConfig",
            "Using client configuration path from home directory: %s", homeDirFilePath.c_str());
        }
      }
#endif
    }
  }
  if (!derivedConfigPath.empty())
  {
    parseConfigFile(derivedConfigPath, out_clientConfig);
  }
}

// Private =====================================================================
////////////////////////////////////////////////////////////////////////////////
bool ClientConfigParser::checkFileExists(const std::string& in_filePath)
{
  FILE* file = sf_fopen(&file, in_filePath.c_str(), "r");
  if (file != nullptr)
  {
    fclose(file);
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
void ClientConfigParser::parseConfigFile(
  const std::string& in_filePath,
  ClientConfig& out_clientConfig)
{
  std::ifstream configFile;
  cJSON* jsonConfig;
  try 
  {
    configFile.open(in_filePath);
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
    std::stringstream configJSON;
    configJSON << configFile.rdbuf();
    jsonConfig = snowflake_cJSON_Parse(configJSON.str().c_str());
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
    configFile.close();
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
  configFile.close();
}

////////////////////////////////////////////////////////////////////////////////
void ClientConfigParser::checkIfValidPermissions(const std::string& in_filePath)
{
  boost::filesystem::file_status fileStatus = boost::filesystem::status(in_filePath);
  boost::filesystem::perms permissions = fileStatus.permissions();
  if (permissions & boost::filesystem::group_write ||
      permissions & boost::filesystem::others_write)
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
      boost::algorithm::to_lower(key);
      if (KnownCommonEntries.find(key) == KnownCommonEntries.end())
      {
        std::string warnMsg =
          "Unknown configuration entry: " + key + " with value:" + entry->valuestring;
        CXX_LOG_WARN(
          "sf",
          "ClientConfigParser",
          "checkUnknownEntries",
          warnMsg);
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
