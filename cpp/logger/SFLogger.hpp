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
    if ((SFLogger::getExternalLogger()->needSecretMask() == SF_BOOLEAN_TRUE) && \
        (SFLogger::getExternalLogger()->getLogLevel() <= SF_LOG_LEVEL::SF_LOG_FATAL)) \
    { \
      SFLogger::getExternalLogger()->logLine(SF_LOG_LEVEL::SF_LOG_FATAL, __FILE__, "%s", \
                                             SFLogger::getMaskedMsg(__VA_ARGS__).c_str()); \
    } else { \
      SFLogger::getExternalLogger()->logLine(SF_LOG_LEVEL::SF_LOG_FATAL, __FILE__, __VA_ARGS__); \
    } \
  } else { \
    sf_log_fatal(CXX_LOG_NS, __VA_ARGS__); \
  } \

#define CXX_LOG_ERROR(...)  \
  if (SFLogger::getExternalLogger() != NULL) \
  { \
    if ((SFLogger::getExternalLogger()->needSecretMask() == SF_BOOLEAN_TRUE) && \
        (SFLogger::getExternalLogger()->getLogLevel() <= SF_LOG_LEVEL::SF_LOG_ERROR)) \
    { \
      SFLogger::getExternalLogger()->logLine(SF_LOG_LEVEL::SF_LOG_ERROR, __FILE__, "%s", \
                                             SFLogger::getMaskedMsg(__VA_ARGS__).c_str()); \
    } else { \
      SFLogger::getExternalLogger()->logLine(SF_LOG_LEVEL::SF_LOG_ERROR, __FILE__, __VA_ARGS__); \
    } \
  } else { \
    sf_log_error(CXX_LOG_NS, __VA_ARGS__); \
  } \

#define CXX_LOG_WARN(...)  \
  if (SFLogger::getExternalLogger() != NULL) \
  { \
    if ((SFLogger::getExternalLogger()->needSecretMask() == SF_BOOLEAN_TRUE) && \
        (SFLogger::getExternalLogger()->getLogLevel() <= SF_LOG_LEVEL::SF_LOG_WARN)) \
    { \
      SFLogger::getExternalLogger()->logLine(SF_LOG_LEVEL::SF_LOG_WARN, __FILE__, "%s", \
                                             SFLogger::getMaskedMsg(__VA_ARGS__).c_str()); \
    } else { \
      SFLogger::getExternalLogger()->logLine(SF_LOG_LEVEL::SF_LOG_WARN, __FILE__, __VA_ARGS__); \
    } \
  } else { \
    sf_log_warn(CXX_LOG_NS, __VA_ARGS__); \
  } \

#define CXX_LOG_INFO(...)  \
  if (SFLogger::getExternalLogger() != NULL) \
  { \
    if ((SFLogger::getExternalLogger()->needSecretMask() == SF_BOOLEAN_TRUE) && \
        (SFLogger::getExternalLogger()->getLogLevel() <= SF_LOG_LEVEL::SF_LOG_INFO)) \
    { \
      SFLogger::getExternalLogger()->logLine(SF_LOG_LEVEL::SF_LOG_INFO, __FILE__, "%s", \
                                             SFLogger::getMaskedMsg(__VA_ARGS__).c_str()); \
    } else { \
      SFLogger::getExternalLogger()->logLine(SF_LOG_LEVEL::SF_LOG_INFO, __FILE__, __VA_ARGS__); \
    } \
  } else { \
    sf_log_info(CXX_LOG_NS, __VA_ARGS__); \
  } \

#define CXX_LOG_DEBUG(...)  \
  if (SFLogger::getExternalLogger() != NULL) \
  { \
    if ((SFLogger::getExternalLogger()->needSecretMask() == SF_BOOLEAN_TRUE) && \
        (SFLogger::getExternalLogger()->getLogLevel() <= SF_LOG_LEVEL::SF_LOG_DEBUG)) \
    { \
      SFLogger::getExternalLogger()->logLine(SF_LOG_LEVEL::SF_LOG_DEBUG, __FILE__, "%s", \
                                             SFLogger::getMaskedMsg(__VA_ARGS__).c_str()); \
    } else { \
      SFLogger::getExternalLogger()->logLine(SF_LOG_LEVEL::SF_LOG_DEBUG, __FILE__, __VA_ARGS__); \
    } \
  } else { \
    sf_log_debug(CXX_LOG_NS, __VA_ARGS__); \
  } \

#define CXX_LOG_TRACE(...)  \
  if (SFLogger::getExternalLogger() != NULL) \
  { \
    if ((SFLogger::getExternalLogger()->needSecretMask() == SF_BOOLEAN_TRUE) && \
        (SFLogger::getExternalLogger()->getLogLevel() <= SF_LOG_LEVEL::SF_LOG_TRACE)) \
    { \
      SFLogger::getExternalLogger()->logLine(SF_LOG_LEVEL::SF_LOG_TRACE, __FILE__, "%s", \
                                             SFLogger::getMaskedMsg(__VA_ARGS__).c_str()); \
    } else { \
      SFLogger::getExternalLogger()->logLine(SF_LOG_LEVEL::SF_LOG_TRACE, __FILE__, __VA_ARGS__); \
    } \
  } else { \
    sf_log_trace(CXX_LOG_NS, __VA_ARGS__); \
  } \

};

}
}
#endif //SNOWFLAKECLIENT_SFLOGGER_HPP
