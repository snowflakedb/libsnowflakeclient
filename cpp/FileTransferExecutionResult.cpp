/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#include <snowflake/SnowflakeTransferException.hpp>
#include "FileTransferExecutionResult.hpp"
#include "snowflake/platform.h"

#define STATUS_SKIPPED "SKIPPED"
#define STATUS_UPLOADED "UPLOADED"
#define STATUS_DOWNLOADED "DOWNLOADED"
#define STATUS_ERROR "ERROR"
#define ENCRYPTION_ENCRYPTED "ENCRYPTED"
#define ENCRYPTION_DECRYPTED "DECRYPTED"
#define MESSAGE_SKIPPED "File with same name already exists. SKIPPED"

namespace Snowflake
{
namespace Client
{

std::vector<const char *> PUT_COLUMN_NAMES={"source",
                                            "target",
                                            "source_size",
                                            "target_size",
                                            "source_compression",
                                            "target_compression",
                                            "status",
                                            "encryption",
                                            "message"};

enum PUT_COLUMN
{
  SOURCE, TARGET, SOURCE_SIZE, TARGET_SIZE, SOURCE_COMPRESSION,
  TARGET_COMPRESSION, PUT_STATUS, PUT_ENCRYPTION, PUT_MESSAGE
};

std::vector<const char *> GET_COLUMN_NAMES={"file",
                                            "size",
                                            "status",
                                            "encryption",
                                            "message"};

enum GET_COLUMN
{
  FILE, SIZE, GET_STATUS, GET_ENCRYPTION, GET_MESSAGE
};


FileTransferExecutionResult::FileTransferExecutionResult(
  CommandType commandType,
  size_t resultEntryNum) :
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
  return (unsigned int)m_currentIndex < m_resultEntryNum;
}

const char* FileTransferExecutionResult::fromOutcomeToStr(
  RemoteStorageRequestOutcome outcome)
{
  switch(outcome)
  {
    case SUCCESS:
      return m_commandType == CommandType::UPLOAD ?
             STATUS_UPLOADED :
             STATUS_DOWNLOADED;
    case SKIP_UPLOAD_FILE:
      return STATUS_SKIPPED;
    default:
      return STATUS_ERROR;
  }
}

CommandType FileTransferExecutionResult::getCommandType()
{
  return m_commandType;
}

unsigned int FileTransferExecutionResult::getColumnSize()
{
  switch(m_commandType)
  {
    case UPLOAD:
      return (unsigned int)PUT_COLUMN_NAMES.size();
    case DOWNLOAD:
      return (unsigned int)GET_COLUMN_NAMES.size();
    default:
      return 0;
  }
}

const char * FileTransferExecutionResult::getColumnName(unsigned int columnIndex)
{
  if (columnIndex < getColumnSize())
  {
    switch(m_commandType)
    {
      case UPLOAD:
        return PUT_COLUMN_NAMES.at(columnIndex);
      case DOWNLOAD:
        return GET_COLUMN_NAMES.at(columnIndex);
      default:
        return "";
    }
  }
  else
  {
    throw SnowflakeTransferException(TransferError::COLUMN_INDEX_OUT_OF_RANGE,
      columnIndex, getColumnSize());
  }
}

size_t FileTransferExecutionResult::getResultSize()
{
  return m_resultEntryNum;
}

void FileTransferExecutionResult::getColumnAsString(unsigned int columnIndex,
                                                    std::string & value)
{
  if (columnIndex < getColumnSize())
  {
    switch (m_commandType)
    {
      case UPLOAD:
        switch (columnIndex)
        {
          case PUT_COLUMN::SOURCE:
            {
              std::string &srcFileFull = m_fileMetadatas[m_currentIndex]->srcFileName;
              value = srcFileFull.substr(srcFileFull.find_last_of(PATH_SEP) + 1);
            }
            break;

          case PUT_COLUMN::TARGET:
            value = m_fileMetadatas[m_currentIndex]->destFileName;
            break;

          case PUT_COLUMN::SOURCE_SIZE:
            value = std::to_string(m_fileMetadatas[m_currentIndex]->srcFileSize);
            break;

          case PUT_COLUMN::TARGET_SIZE:
            value = std::to_string(m_fileMetadatas[m_currentIndex]->destFileSize);
            break;

          case PUT_COLUMN::SOURCE_COMPRESSION:
            value = std::string(m_fileMetadatas[m_currentIndex]->
              sourceCompression->getName());
            break;

          case PUT_COLUMN::TARGET_COMPRESSION:
            value = std::string(m_fileMetadatas[m_currentIndex]->
              targetCompression->getName());
            break;

          case PUT_COLUMN::PUT_STATUS:
            value = std::string(fromOutcomeToStr(m_outcomes[m_currentIndex]));
            break;

          case PUT_COLUMN::PUT_ENCRYPTION:
            value = std::string(ENCRYPTION_ENCRYPTED);
            break;

          case PUT_COLUMN::PUT_MESSAGE:
            value = std::string(
              m_outcomes[m_currentIndex] == RemoteStorageRequestOutcome::
                   SKIP_UPLOAD_FILE ? MESSAGE_SKIPPED : "");
            break;
        }
        break;
      case DOWNLOAD:
        switch(columnIndex)
        {
          case GET_COLUMN::FILE:
            value = m_fileMetadatas[m_currentIndex]->destFileName;
            break;

          case GET_COLUMN::SIZE:
            value = std::to_string(m_fileMetadatas[m_currentIndex]->srcFileSize);
            break;

          case GET_COLUMN::GET_STATUS:
            value = std::string(fromOutcomeToStr(m_outcomes[m_currentIndex]));
            break;

          case GET_COLUMN::GET_ENCRYPTION:
            value = std::string(ENCRYPTION_DECRYPTED);
            break;

          case GET_COLUMN::GET_MESSAGE:
            value ="";
            break;
        }
        break;
      case UNKNOWN:
        break;  
    }
  }
}

int FileTransferExecutionResult::findColumnByName(
  const char *columnName, int columnNameSize)
{
  std::vector<const char *> * arraysToFind = m_commandType == UPLOAD ?
                                             &PUT_COLUMN_NAMES :
                                             &GET_COLUMN_NAMES;
  for (int i=0; i<arraysToFind->size(); i++)
  {
    if (sf_strncasecmp(arraysToFind->at(i), columnName, columnNameSize) == 0)
    {
      return i;
    }
  }
  return -1;

}

}
}
