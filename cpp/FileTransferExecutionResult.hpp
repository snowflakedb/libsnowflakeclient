/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_IRESULTSET_HPP
#define SNOWFLAKECLIENT_IRESULTSET_HPP

#include "RemoteStorageRequestOutcome.hpp"
#include "FileMetadata.hpp"
#include "PutGetParseResponse.hpp"

namespace Snowflake
{
namespace Client
{
enum RemoteStorageRequestOutcome;

/**
 * Class wrapped up the information about file transfer information
 * If succeed, includes the information about dest file name/size etc.
 *
 * External component needs build a result set wrapper on top of this class
 */
class FileTransferExecutionResult
{
public:
  FileTransferExecutionResult(CommandType commandType,
                              unsigned int resultEntryNum);

  ~FileTransferExecutionResult();

  void SetTransferOutCome(RemoteStorageRequestOutcome outcome, unsigned int index)
  {
    m_outcomes[index] = outcome;
  }

  void SetFileMetadata(FileMetadata *fileMetadata, unsigned int index)
  {
    m_fileMetadatas[index] = fileMetadata;
  }

  bool next();

  const char * getStatus();

  unsigned int getResultSize();

private:
  /// enum to indicate command type
  CommandType m_commandType;

  FileMetadata ** m_fileMetadatas;

  RemoteStorageRequestOutcome * m_outcomes;

  unsigned int m_resultEntryNum;

  unsigned int m_currentIndex;
};
}
}

#endif //SNOWFLAKECLIENT_IRESULTSET_HPP
