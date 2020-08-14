/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <aws/core/utils/logging/LogLevel.h>
#include "SFAwsLogger.hpp"
#include "snowflake/logger.h"
#include "SFLogger.hpp"

#define AWS_NS "AwsSdk"
#define AWS_LINE 0

namespace
{
  size_t findCaseInsensitive(std::string data, std::string toSearch, size_t pos = 0)
  {
    // Convert complete given String to lower case
    std::transform(data.begin(), data.end(), data.begin(), ::tolower);
    // Convert complete given Sub String to lower case
    std::transform(toSearch.begin(), toSearch.end(), toSearch.begin(), ::tolower);
    // Find sub string in given string
    return data.find(toSearch, pos);
  }
}

Snowflake::Client::SFAwsLogger::SFAwsLogger()
{}

LogLevel Snowflake::Client::SFAwsLogger::GetLogLevel() const
{
  return (LogLevel)(6 - log_get_level());
}

void Snowflake::Client::SFAwsLogger::Log(LogLevel logLevel,
                                         const char *tag,
                                         const char *formatStr, ...)
{
  if (logLevel != LogLevel::Off)
  {
    va_list args;
    va_start(args, formatStr);
    if (SFLogger::getExternalLogger() != NULL)
    {
      SFLogger::getExternalLogger()->logLineVA((SF_LOG_LEVEL)toSFLogeLevel(logLevel),
        AWS_NS, tag, formatStr, args);
    }
    else
    {
      log_log_va_list(toSFLogeLevel(logLevel), tag, AWS_LINE, AWS_NS, formatStr,
                      args);
    }
    va_end(args);
  }
}

void Snowflake::Client::SFAwsLogger::LogStream(LogLevel logLevel,
                                               const char *tag,
                                               const Aws::OStringStream &messageStream)
{
  std::string logStr = messageStream.rdbuf()->str();
  if ((std::string::npos != findCaseInsensitive(logStr, "token")) ||
      (std::string::npos != findCaseInsensitive(logStr, "Credential")) ||
      (std::string::npos != findCaseInsensitive(logStr, "Signature")) ||
      (std::string::npos != findCaseInsensitive(logStr, "authorization")) ||
      (std::string::npos != findCaseInsensitive(logStr, "amz-key")) ||
      (std::string::npos != findCaseInsensitive(logStr, "amz-iv")) ||
      (std::string::npos != findCaseInsensitive(logStr, "smkId")))
  {
    return;
  }

  this->Log(logLevel, tag, "%s", logStr.c_str());
}

int Snowflake::Client::SFAwsLogger::toSFLogeLevel(LogLevel logLevel)
{
  return 6 - (int)logLevel;
}
