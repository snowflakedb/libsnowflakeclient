/*
 * Copyright (c) 2018-2024 Snowflake Computing, Inc. All rights reserved.
 */

#include "snowflake/Exceptions.hpp"
#include "../logger/SFLogger.hpp"

// helper functions
namespace
{
  std::string setupErrorMessage(const std::string& message,
                                const std::string& file,
                                int line,
                                const std::string& queryId,
                                const std::string& sqlState,
                                int code)
  {
    std::string errmsg = "Snowflake exception: ";
    if (!file.empty())
    {
      errmsg += file + ":" + std::to_string(line) + ", ";
    }
    if (!queryId.empty())
    {
      errmsg += std::string("query ID: ") + queryId + ", ";
    }
    if (!sqlState.empty())
    {
      errmsg += std::string("SQLState: ") + sqlState + ", ";
    }
    errmsg += std::string("error code :") + std::to_string(code) + ", ";

    errmsg += std::string("error message: ") + message;

    return errmsg;
  }
}

namespace Snowflake
{
namespace Client
{

void SnowflakeException::setErrorMessage(const std::string& errmsg)
{
  m_errmsg = SFLogger::getMaskedMsg("%s", errmsg.c_str());
}

void SnowflakeException::setErrorMessage(const char* fmt, va_list args)
{
  m_errmsg = SFLogger::getMaskedMsgVA(fmt, args);
}

SnowflakeGeneralException::SnowflakeGeneralException(SF_ERROR_STRUCT *error) :
  m_message(error->msg ? error->msg : ""),
  m_file(error->file ? error->file : ""),
  m_line(error->line),
  m_queryId(error->sfqid),
  m_sqlState(error->sqlstate),
  m_code((int)error->error_code)
{
  std::string errmsg = setupErrorMessage(m_message, m_file, m_line, m_queryId, m_sqlState, m_code);
  setErrorMessage(errmsg);
}

SnowflakeGeneralException::SnowflakeGeneralException(const std::string& message,
                                                     const char* file, int line,
                                                     int code,
                                                     const std::string queryId,
                                                     const std::string sqlState) :
  m_message(message),
  m_file(file ? file : ""),
  m_line(line),
  m_queryId(queryId),
  m_sqlState(sqlState),
  m_code(code)
{
  std::string errmsg = setupErrorMessage(m_message, m_file, m_line, m_queryId, m_sqlState, m_code);
  setErrorMessage(errmsg);
}

SnowflakeGeneralException::SnowflakeGeneralException(const char* file, int line,
                                                     int code,
                                                     const std::string queryId,
                                                     const std::string sqlState,
                                                     const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  m_message = SFLogger::getMaskedMsgVA(fmt, args);
  va_end(args);
  m_errmsg = setupErrorMessage(m_message, m_file, m_line, m_queryId, m_sqlState, m_code);
}

int SnowflakeGeneralException::code()
{
  return m_code;
}

const char* SnowflakeGeneralException::sqlstate()
{
  return m_sqlState.c_str();
}

const char* SnowflakeGeneralException::msg()
{
  return m_message.c_str();
}

const char* SnowflakeGeneralException::sfqid()
{
  return m_queryId.c_str();
}

const char* SnowflakeGeneralException::file()
{
  return m_file.c_str();
}

int SnowflakeGeneralException::line()
{
  return m_line;
}

} // namespace Client
} // namespace Snowflake
