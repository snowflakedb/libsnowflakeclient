/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "FileTransferExecutionResult.hpp"

#define STATUS_SKIPPED "SKIPPED"
#define STATUS_SUCCEED "SUCCEED"
#define MESSAGE_SKIPPED "File with same name and checksum already exists. SKIPPED"

namespace Snowflake
{
namespace Client
{

FileTransferExecutionResult::FileTransferExecutionResult(
  FileMetadata *fileMetadata,
  CommandType commandType) :
  fileMetadata(fileMetadata),
  commandType(commandType)
{
  outcome = TransferOutcome::FAILED;
}

const char* FileTransferExecutionResult::getStatus()
{
  switch(outcome)
  {
    case SUCCESS:
      return STATUS_SUCCEED;
    case SKIPPED:
      return STATUS_SKIPPED;
    default:
      return "ERROR";
  }
}

std::string& FileTransferExecutionResult::getSource()
{
  return fileMetadata->srcFileName;
}


}
}