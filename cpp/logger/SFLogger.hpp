/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_SFLOGGER_HPP
#define SNOWFLAKECLIENT_SFLOGGER_HPP

#include "snowflake/ISFLogger.hpp"
#include "snowflake/logger.h"
#include <string>

namespace Snowflake
{
namespace Client
{

class SFLogger
{
public:
  static void init(ISFLogger *logger);

  static ISFLogger * getExternalLogger();

  static std::string getMaskedMsg(const char* fmt, ...);

  static std::string getMaskedMsgVA(const char* fmt, va_list args);

private:
  static ISFLogger * m_externalLogger;

#define CXX_LOG_FATAL(...)  \
  if (SFLogger::getExternalLogger() != NULL) \
  { \
    SFLogger::getExternalLogger()->logLine(SF_LOG_LEVEL::SF_LOG_FATAL, __FILE__, \
                                           SFLogger::getMaskedMsg(__VA_ARGS__).c_str()); \
  } else { \
    sf_log_fatal(CXX_LOG_NS, __VA_ARGS__); \
  } \

#define CXX_LOG_ERROR(...)  \
  if (SFLogger::getExternalLogger() != NULL) \
  { \
    SFLogger::getExternalLogger()->logLine(SF_LOG_LEVEL::SF_LOG_ERROR, __FILE__, \
                                           SFLogger::getMaskedMsg(__VA_ARGS__).c_str()); \
  } else { \
    sf_log_error(CXX_LOG_NS, __VA_ARGS__); \
  } \

#define CXX_LOG_WARN(...)  \
  if (SFLogger::getExternalLogger() != NULL) \
  { \
    SFLogger::getExternalLogger()->logLine(SF_LOG_LEVEL::SF_LOG_WARN, __FILE__, \
                                           SFLogger::getMaskedMsg(__VA_ARGS__).c_str()); \
  } else { \
    sf_log_warn(CXX_LOG_NS, __VA_ARGS__); \
  } \

#define CXX_LOG_INFO(...)  \
  if (SFLogger::getExternalLogger() != NULL) \
  { \
    SFLogger::getExternalLogger()->logLine(SF_LOG_LEVEL::SF_LOG_INFO, __FILE__, \
                                           SFLogger::getMaskedMsg(__VA_ARGS__).c_str()); \
  } else { \
    sf_log_info(CXX_LOG_NS, __VA_ARGS__); \
  } \

#define CXX_LOG_DEBUG(...)  \
  if (SFLogger::getExternalLogger() != NULL) \
  { \
    SFLogger::getExternalLogger()->logLine(SF_LOG_LEVEL::SF_LOG_DEBUG, __FILE__, \
                                           SFLogger::getMaskedMsg(__VA_ARGS__).c_str()); \
  } else { \
    sf_log_debug(CXX_LOG_NS, __VA_ARGS__); \
  } \

#define CXX_LOG_TRACE(...)  \
  if (SFLogger::getExternalLogger() != NULL) \
  { \
    SFLogger::getExternalLogger()->logLine(SF_LOG_LEVEL::SF_LOG_TRACE, __FILE__, \
                                           SFLogger::getMaskedMsg(__VA_ARGS__).c_str()); \
  } else { \
    sf_log_trace(CXX_LOG_NS, __VA_ARGS__); \
  } \

};

}
}
#endif //SNOWFLAKECLIENT_SFLOGGER_HPP
