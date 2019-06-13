/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include "snowflake/ISFLogger.hpp"

void Snowflake::Client::ISFLogger::logLine(SF_LOG_LEVEL logLevel,
                                           const char * fileName,
                                           const char * msgFmt,
                                           ...)
{
  va_list args;
  va_start(args, msgFmt);
  this->logLineVA(logLevel, "Snowflake::Client", sf_filename_from_path(fileName),
                  msgFmt, args);
  va_end(args);
}