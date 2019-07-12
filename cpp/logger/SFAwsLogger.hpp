/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_SFAWSLOGGER_HPP
#define SNOWFLAKECLIENT_SFAWSLOGGER_HPP

#include <aws/core/utils/logging/LogSystemInterface.h>

using Aws::Utils::Logging::LogLevel;

namespace Snowflake
{
namespace Client
{
class SFAwsLogger : public Aws::Utils::Logging::LogSystemInterface
{
public:
  SFAwsLogger();

  /**
   * Gets the currently configured log level for this logger.
   */
  virtual LogLevel GetLogLevel(void) const;
  /**
   * Does a printf style output to the output stream. Don't use this, it's unsafe. See LogStream
   */
  virtual void Log(LogLevel logLevel, const char* tag, const char* formatStr, ...);
  /**
  * Writes the stream to the output stream.
  */
  virtual void LogStream(LogLevel logLevel, const char* tag, const Aws::OStringStream &messageStream);

private:

  // AWS enum of log level
  LogLevel m_logLevel;

  /**
   * Convert aws enum of log level to sf log level
   */
  int toSFLogeLevel(LogLevel logLevel);

};
}
}

#endif //SNOWFLAKECLIENT_SFAWSLOGGER_HPP
