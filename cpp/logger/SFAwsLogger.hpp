/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_SFAWSLOGGER_HPP
#define SNOWFLAKECLIENT_SFAWSLOGGER_HPP

#include <aws/core/utils/logging/LogSystemInterface.h>
#include "snowflake/platform.h"

using Aws::Utils::Logging::LogLevel;

namespace Snowflake
{
namespace Client
{
  struct AwsMutex
  {
    AwsMutex()
    {
      _critical_section_init(&m_mutex);
    }

    ~AwsMutex()
    {
      _critical_section_term(&m_mutex);
    }

    void lock()
    {
      _critical_section_lock(&m_mutex);
    }

    void unlock()
    {
      _critical_section_unlock(&m_mutex);
    }

    SF_CRITICAL_SECTION_HANDLE m_mutex;
  };

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
  /**
   * Writes any buffered messages to the underlying device if the logger supports buffering.
   */
  virtual void Flush();

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
