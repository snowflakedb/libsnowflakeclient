/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_IRESULTSET_HPP
#define SNOWFLAKECLIENT_IRESULTSET_HPP

#include "RemoteStorageRequestOutcome.hpp"
#include "FileMetadata.hpp"
#include "snowflake/PutGetParseResponse.hpp"
#include "snowflake/ITransferResult.hpp"

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
class FileTransferExecutionResult : public ITransferResult
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

  int getResultSize();

  unsigned int getColumnSize();

  const char * getColumnName(int columnIndex);

  const char * getColumnAsString(int columnIndex);

  CommandType getCommandType();

  int findColumnByName(const char * columnName, int columnNameSize);

private:
  /// enum to indicate command type
  CommandType m_commandType;

  /// arrays of result file metadata
  FileMetadata ** m_fileMetadatas;

  /// arrays of upload outcome
  RemoteStorageRequestOutcome * m_outcomes;

  /// result size
  unsigned int m_resultEntryNum;

  /// current index already consumed
  int m_currentIndex;

  const char * fromOutcomeToStr(RemoteStorageRequestOutcome outcome);
};
}
}

#endif //SNOWFLAKECLIENT_IRESULTSET_HPP
