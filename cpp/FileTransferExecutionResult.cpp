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
  CommandType commandType,
  unsigned int resultEntryNum) :
  m_commandType(commandType),
  m_resultEntryNum(resultEntryNum),
  m_currentIndex(-1)
{
  m_fileMetadatas = new FileMetadata*[resultEntryNum];
  m_outcomes = new RemoteStorageRequestOutcome[resultEntryNum];
}

FileTransferExecutionResult::~FileTransferExecutionResult()
{
  delete[] m_fileMetadatas;
  delete[] m_outcomes;
}

bool FileTransferExecutionResult::next()
{
  m_currentIndex ++;
  return m_currentIndex < m_resultEntryNum;
}

const char* FileTransferExecutionResult::getStatus()
{
  switch(m_outcomes[m_currentIndex])
  {
    case SUCCESS:
      return STATUS_SUCCEED;
    case SKIP_UPLOAD_FILE:
      return STATUS_SKIPPED;
    default:
      return "ERROR";
  }
}

unsigned int FileTransferExecutionResult::getResultSize()
{
  return m_resultEntryNum;
}

}
}