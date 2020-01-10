/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
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
                              size_t resultEntryNum);

  ~FileTransferExecutionResult();

  void SetTransferOutCome(RemoteStorageRequestOutcome outcome, size_t index)
  {
    m_outcomes[index] = outcome;
  }

  void SetFileMetadata(FileMetadata *fileMetadata, size_t index)
  {
    m_fileMetadatas[index] = fileMetadata;
  }

  bool next();

  size_t getResultSize();

  unsigned int getColumnSize();

  const char * getColumnName(unsigned int columnIndex);

  void getColumnAsString(unsigned int columnIndex, std::string &value);

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
  size_t m_resultEntryNum;

  /// current index already consumed
  int m_currentIndex;

  const char * fromOutcomeToStr(RemoteStorageRequestOutcome outcome);
};
}
}

#endif //SNOWFLAKECLIENT_IRESULTSET_HPP
