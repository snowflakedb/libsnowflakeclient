/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include "SFLogger.hpp"

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