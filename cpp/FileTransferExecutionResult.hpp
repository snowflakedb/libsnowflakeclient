/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_IRESULTSET_HPP
#define SNOWFLAKECLIENT_IRESULTSET_HPP

#include "TransferOutcome.hpp"
#include "FileMetadata.hpp"
#include "PutGetParseResponse.hpp"

namespace Snowflake
{
namespace Client
{
enum TransferOutcome;

/**
 * Class wrapped up the information about file transfer information
 * If succeed, includes the information about dest file name/size etc.
 *
 * External component needs build a result set wrapper on top of this class
 */
class FileTransferExecutionResult
{
public:
  FileTransferExecutionResult(FileMetadata *fileMetadata,
                              CommandType commandType);

  void SetTransferOutCome(TransferOutcome outcome)
  {
    this->outcome = outcome;
  }

  const char * getStatus();

  std::string & getSource();

private:
  /// enum to indicate command type
  CommandType commandType;

  TransferOutcome outcome;

  FileMetadata *fileMetadata;

  /************************
   *  PUT specific field  *
   ************************/
  /// source file name
  std::string source;

  /// target file name
  std::string target;

  /// source file size
  long sourceSize;

  /// target file size
  long targetSize;

  /// source compression
  const FileCompressionType * souceCompression;

  /// target compression
  const FileCompressionType * targetCompression;

  /************************
   *  GET specific field  *
   ************************/
  /// file name
  std::string file;

  /// file size
  long size;

  /************************
   *    common field      *
   ************************/

  /// status (succeed or failed or skipped)
  std::string status;

  /// additional message related to status
  std::string message;

};
}
}

#endif //SNOWFLAKECLIENT_IRESULTSET_HPP
