/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "snowflake/IFileTransferAgent.hpp"
#include "FileTransferAgent.hpp"

Snowflake::Client::IFileTransferAgent *
Snowflake::Client::IFileTransferAgent::getTransferAgent(
  IStatementPutGet *statementPutGet)
{
  return new FileTransferAgent(statementPutGet);
}