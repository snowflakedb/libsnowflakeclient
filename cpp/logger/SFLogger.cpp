/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include "SFLogger.hpp"
#include "SecretDetector.hpp"

Snowflake::Client::ISFLogger *
  Snowflake::Client::SFLogger::m_externalLogger = nullptr;

void Snowflake::Client::SFLogger::init(ISFLogger *logger)
{
  m_externalLogger = logger;
}

Snowflake::Client::ISFLogger * Snowflake::Client::SFLogger::getExternalLogger()
{
  return m_externalLogger;
}

void log_masked_va_list(FILE* fp, const char *fmt, va_list args)
{
  std::string maskedMsg = Snowflake::Client::SFLogger::getMaskedMsgVA(fmt, args);
  fprintf(fp, maskedMsg.c_str());
}

std::string Snowflake::Client::SFLogger::getMaskedMsg(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  std::string maskedMsg = getMaskedMsgVA(fmt, args);
  va_end(args);

  return maskedMsg;
}

std::string Snowflake::Client::SFLogger::getMaskedMsgVA(const char* fmt, va_list args)
{
  size_t bufLen = 4096;
  // Just in case not to fall into dead loop. 8MB would be large enough for
  // single log.
  const int MAX_LOG_LEN = 8 * 1024 * 1024;
  std::vector<char> buf(bufLen);

  int ret = -1;
  while ((ret < 0) && (bufLen <= MAX_LOG_LEN))
  {
    // va_list can only be consumed once. Make a copy here in case need to retry
    // with larger buffer size.
    va_list copy;
    va_copy(copy, args);
    ret = sb_vsnprintf(buf.data(), bufLen, bufLen - 1, fmt, copy);
    va_end(copy);
    if (ret < 0)
    {
      bufLen *= 2;
      buf.resize(bufLen);
    }
  }

  return SecretDetector::maskSecrets(std::string(buf.data()));
}
