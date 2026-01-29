#define CURL_STATICLIB
#include <string.h>
#include <iostream>
#include <sstream>
#include <curl/curl.h>
#include "boost/regex.hpp"
#include "boost/filesystem.hpp"
#include "snowflake/basic_types.h"
#include "snowflake/platform.h"
#include "snowflake/Proxy.hpp"
#include "../logger/SFLogger.hpp"
#include <chrono>
#include "SnowflakeCommon.hpp"
#include "log_file_util.h"
#include "cJSON.h"
#include "snowflake/PlatformDetection.hpp"

using namespace Snowflake;
using namespace Snowflake::Client;

static bool exception_on_memory_error = false;

/**
 * Validate partner application name.
 * No cross-platform regex lib in C so we use C++ one.
 * @param application partner application name
 */
extern "C" {

sf_bool validate_application(const char* application)
{
  boost::regex APPLICATION_REGEX = boost::regex("^[A-Za-z][A-Za-z0-9\\.\\-_]{1,50}$", boost::regex::icase);

  // It's OK to leave application unset.
  if (!application || (strlen(application) == 0))
  {
    return SF_BOOLEAN_TRUE;
  }

  if (regex_match(application, APPLICATION_REGEX))
  {
    return SF_BOOLEAN_TRUE;
  }

  return SF_BOOLEAN_FALSE;
}

int STDCALL sf_delete_directory_if_exists(const char * directoryName)
{
  log_file_usage(directoryName, "Deleting directory", false);
  if (!sf_is_directory_exist(directoryName))
  {
    return 0;
  }

  boost::system::error_code err;
  try
  {
    boost::filesystem::remove_all(boost::filesystem::path(directoryName), err);
  }
  catch (...)
  {
    // should not happen as we use the function with output parameter of error code
    CXX_LOG_ERROR("Removing folder %s failed with unknown exception",
                  directoryName);
    return -1;
  }

  if (err.value() != boost::system::errc::success)
  {
    CXX_LOG_ERROR("Removing folder %s failed with error code: %d",
                  directoryName, err.value());
  }
  else if (sf_is_directory_exist(directoryName))
  {
    CXX_LOG_ERROR("Removing folder %s failed. Function call succeeded but the folder is still there.",
                  directoryName);
    return -1;
  }

  CXX_LOG_TRACE("Removing folder %s succeeded.", directoryName);

  return err.value();
}

CURLcode set_curl_proxy(CURL *curl, const char* proxy, const char* no_proxy)
{
  if (!proxy)
  {
    return CURLE_OK;
  }

  if (!curl)
  {
    return CURLE_BAD_FUNCTION_ARGUMENT;
  }

  CURLcode res;
  std::string sProxy(proxy);

  Util::Proxy proxySettings(sProxy);
	if (no_proxy)
	{
		proxySettings.setNoProxy(std::string(no_proxy));
	}

  if (proxySettings.getMachine().empty())
  {
    // explicitly disable proxy and ignore settings in environment variables
    return curl_easy_setopt(curl, CURLOPT_PROXY, "");
  }
  else
  {
    res = curl_easy_setopt(curl, CURLOPT_PROXY, proxySettings.getHost().c_str());
    if (res != CURLE_OK) return res;
    res = curl_easy_setopt(curl, CURLOPT_PROXYPORT, (long)proxySettings.getPort());
    if (res != CURLE_OK) return res;
    if (!proxySettings.getUser().empty() || !proxySettings.getPwd().empty())
    {
      res = curl_easy_setopt(curl, CURLOPT_PROXYUSERNAME, proxySettings.getUser().c_str());
      if (res != CURLE_OK) return res;
      res = curl_easy_setopt(curl, CURLOPT_PROXYPASSWORD, proxySettings.getPwd().c_str());
      if (res != CURLE_OK) return res;
    }
    return curl_easy_setopt(curl, CURLOPT_NOPROXY, proxySettings.getNoProxy().c_str());
  }
}

void STDCALL sf_exception_on_memory_failure()
{
  exception_on_memory_error = true;
}

void STDCALL sf_memory_error_handler()
{
  if (exception_on_memory_error)
  {
    throw std::bad_alloc();
  }

  exit(EXIT_FAILURE);
}

uint64 sf_get_current_time_millis()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

// have to put the implementation here to avoid conflict type definition with AWS of cJSON
cJSON * get_detected_platforms()
{
  // to ensure no exception thrown from C interface
  try
  {
    std::vector<std::string> detectedPlatforms;
    Snowflake::Client::PlatformDetection::getDetectedPlatforms(detectedPlatforms);
    cJSON* platformsJson = snowflake_cJSON_CreateArray();
    for (auto platform : detectedPlatforms)
    {
      cJSON* val = snowflake_cJSON_CreateString(platform.c_str());
      snowflake_cJSON_AddItemToArray(platformsJson, val);
    }
    return platformsJson;
  }
  catch (...)
  {
    // TODO: log error
  }
  return NULL;
}

}

void Snowflake::Client::Util::replaceStrAll(std::string& stringToReplace,
  std::string const& oldValue,
  std::string const& newValue)
{
  size_t oldValueLen = oldValue.length();
  size_t newValueLen = newValue.length();
  if (0 == oldValueLen)
  {
    return;
  }

  size_t index = 0;
  while (true) {
    /* Locate the substring to replace. */
    index = stringToReplace.find(oldValue, index);
    if (index == std::string::npos) break;

    /* Make the replacement. */
    stringToReplace.replace(index, oldValueLen, newValue);

    /* Advance index forward so the next iteration doesn't pick it up as well. */
    index += newValueLen;
  }
}

void Snowflake::Client::Util::parseHttpRespHeaders(std::string const& headerString,
                                                   std::map<std::string, std::string>& headers)
{
  std::string header;
  std::stringstream headerStream(headerString);
  size_t index;
  while (std::getline(headerStream, header) && header != "\r")
  {
    index = header.find(':', 0);
    if (index != std::string::npos)
    {
      std::string key, value;
      key = header.substr(0, index);
      value = header.substr(index + 1);
      headers[key] = value;
    }
  }
}
