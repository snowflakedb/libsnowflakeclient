/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "snowflake/IFileTransferAgent.hpp"
#include "FileTransferAgent.hpp"
#include "logger/SFLogger.hpp"

Snowflake::Client::IFileTransferAgent *
Snowflake::Client::IFileTransferAgent::getTransferAgent(
  IStatementPutGet *statementPutGet)
{
  return new FileTransferAgent(statementPutGet);
}

void Snowflake::Client::IFileTransferAgent::injectExternalLogger(
  ISFLogger *logger)
{
  Snowflake::Client::SFLogger::init(logger);
}
