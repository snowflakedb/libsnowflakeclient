/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include "snowflake/IFileTransferAgent.hpp"
#include "FileTransferAgent.hpp"
#include "logger/SFLogger.hpp"
#include "snowflake/version.h"

Snowflake::Client::IFileTransferAgent *
Snowflake::Client::IFileTransferAgent::getTransferAgent(
  IStatementPutGet *statementPutGet,
  TransferConfig * transferConfig)
{
  return new FileTransferAgent(statementPutGet, transferConfig);
}

void Snowflake::Client::IFileTransferAgent::injectExternalLogger(
  ISFLogger *logger)
{
  Snowflake::Client::SFLogger::init(logger);
  CXX_LOG_INFO("External logger injected. libsnowflakeclient version: %s",
    SF_API_VERSION);
}
