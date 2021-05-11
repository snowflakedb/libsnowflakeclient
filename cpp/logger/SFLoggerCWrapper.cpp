//
// Created by smirzaei on 5/5/21.
//

#include "SFLoggerCWrapper.h"
#include "SFLogger.hpp"


void externalLogger_logLineVA(SF_LOG_LEVEL logLevel,
                              const char *fileName,
                              const char *msgFmt, va_list args)
{
  va_list args_copy;
  va_copy(args_copy, args);
  Snowflake::Client::SFLogger::getExternalLogger()->logLineVA(logLevel,
                                                              "Snowflake::Client",
                                                              fileName, msgFmt,
                                                              args_copy);
  va_end(args_copy);
}


void *getExternalLogger()
{
  return Snowflake::Client::SFLogger::getExternalLogger();
}


void setExternalLogger(void *logger)
{
  Snowflake::Client::SFLogger::init(
          static_cast<Snowflake::Client::ISFLogger *>(logger));
}
