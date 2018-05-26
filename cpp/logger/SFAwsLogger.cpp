/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include <aws/core/utils/logging/LogLevel.h>
#include "SFAwsLogger.hpp"
#include "snowflake/logger.h"

#define AWS_NS "AwsSdk"
#define AWS_LINE 0

Snowflake::Client::Logger::SFAwsLogger::SFAwsLogger()
{}

LogLevel Snowflake::Client::Logger::SFAwsLogger::GetLogLevel() const
{
  return (LogLevel)(6 - log_get_level());
}

void Snowflake::Client::Logger::SFAwsLogger::Log(LogLevel logLevel,
                                                 const char *tag,
                                                 const char *formatStr, ...)
{
  if (logLevel != LogLevel::Off)
  {
    va_list args;
    va_start(args, formatStr);
    log_log_va_list(toSFLogeLevel(logLevel), tag, AWS_LINE, AWS_NS, formatStr, args);
    va_end(args);
  }
}

void Snowflake::Client::Logger::SFAwsLogger::LogStream(LogLevel logLevel,
                                                       const char *tag,
                                                       const Aws::OStringStream &messageStream)
{
  if (logLevel != LogLevel::Off)
  {
    log_log(toSFLogeLevel(logLevel), tag, AWS_LINE, AWS_NS,
            messageStream.rdbuf()->str().c_str());
  }
}

int Snowflake::Client::Logger::SFAwsLogger::toSFLogeLevel(LogLevel logLevel)
{
  return 6 - (int)logLevel;
}
