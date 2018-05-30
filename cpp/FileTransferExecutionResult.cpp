/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#include "FileTransferExecutionResult.hpp"

#define STATUS_SKIPPED "SKIPPED"
#define STATUS_UPLOADED "UPLOADED"
#define STATUS_DOWNLOADED "DOWNLOADED"
#define STATUS_ERROR "ERROR"
#define ENCRYPTION_ENCRYPTED "ENCRYPTED"
#define ENCRYPTION_DECRYPTED "DECRYPTED"
#define MESSAGE_SKIPPED "File with same name and checksum already exists. SKIPPED"

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

const char * FileTransferExecutionResult::getColumnName(int columnIndex)
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
    throw;
  }
}

int FileTransferExecutionResult::getResultSize()
{
  return m_resultEntryNum;
}

const char * FileTransferExecutionResult::getColumnAsString(int columnIndex)
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
              return srcFileFull.substr(
                srcFileFull.find_last_of('/') + 1).c_str();
            }

          case PUT_COLUMN::TARGET:
            return m_fileMetadatas[m_currentIndex]->destFileName.c_str();

          case PUT_COLUMN::SOURCE_SIZE:
            return std::to_string(
              m_fileMetadatas[m_currentIndex]->srcFileSize).c_str();

          case PUT_COLUMN::TARGET_SIZE:
            return std::to_string(
              m_fileMetadatas[m_currentIndex]->destFileSize).c_str();

          case PUT_COLUMN::SOURCE_COMPRESSION:
            return m_fileMetadatas[m_currentIndex]->sourceCompression->getName();

          case PUT_COLUMN::TARGET_COMPRESSION:
            return m_fileMetadatas[m_currentIndex]->targetCompression->getName();

          case PUT_COLUMN::PUT_STATUS:
            return fromOutcomeToStr(m_outcomes[m_currentIndex]);

          case PUT_COLUMN::PUT_ENCRYPTION:
            return ENCRYPTION_ENCRYPTED;

          case PUT_COLUMN::PUT_MESSAGE:
            return m_outcomes[m_currentIndex] == RemoteStorageRequestOutcome::
                   SKIP_UPLOAD_FILE ? MESSAGE_SKIPPED : "";
        }
      case DOWNLOAD:
        switch(columnIndex)
        {
          case GET_COLUMN::FILE:
            return m_fileMetadatas[m_currentIndex]->destFileName.c_str();
          case GET_COLUMN::SIZE:
            return std::to_string(m_fileMetadatas[m_currentIndex]->srcFileSize).c_str();
          case GET_COLUMN::GET_STATUS:
            return fromOutcomeToStr(m_outcomes[m_currentIndex]);
          case GET_COLUMN::GET_ENCRYPTION:
            return ENCRYPTION_DECRYPTED;
          case GET_COLUMN::GET_MESSAGE:
            return "";
        }
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