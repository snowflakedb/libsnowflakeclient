/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include <stdio.h>
#include <stdarg.h>
#include "snowflake/SnowflakeTransferException.hpp"

// keep same order as entries in enum TransferError, so that error message can
// be matched correspondingly
static const char * errorMsgFmts[] = {
  "Internal error: %s", // INTERNAL_ERROR
  "Auto GZIP compression failed, code %d", // COMPRESSION_ERROR
  "Failed to create directory %s, code %d", // MKDIR_ERROR
  "Feature not supported yet: %s", // UNSUPPORTED_FEATURE
  "Column index %d out of range. Total column count: %d", // COLUMN_INDEX_OUT_OF_RANGE
  "Failed to read directory structure on disk, directory %s, code %d", // FILE_OPEN_ERROR
  "Compression type %s is either not supported." //COMPRESSION_NOT_SUPPORTED
};

Snowflake::Client::SnowflakeTransferException::SnowflakeTransferException(
  TransferError transferError, ...) :
  m_code(transferError)
{
  const char * msgFmt = errorMsgFmts[(int)transferError];
  va_list args;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvarargs"
  va_start (args, transferError);
#pragma clang diagnostic pop
  vsprintf(m_msg, msgFmt, args);
  va_end(args);
}

const char * Snowflake::Client::SnowflakeTransferException::what() const noexcept
{
  return m_msg;
}

int Snowflake::Client::SnowflakeTransferException::getCode()
{
  return m_code;
}
