/*
 * Copyright (c) 2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_FILETRANSFERAGENT_HPP
#define SNOWFLAKECLIENT_FILETRANSFERAGENT_HPP

#include <map>
#include <string>
#include "IStatementPutGet.hpp"
#include "FileTransferExecutionResult.hpp"
#include "FileMetadata.hpp"
#include "FileMetadataInitializer.hpp"
#include "snowflake/platform.h"

namespace Snowflake
{
namespace Client
{
class StorageObjectMetadata;

class IStorageClient;

struct FileTransferExecutionResult;


/**
 * This is the main class to external component (c api or ODBC)
 * External component should implement IStatement interface to submit put
 * or get command to server to do parsing.
 */
class FileTransferAgent
{
public:
  FileTransferAgent(IStatementPutGet *statement);

  ~FileTransferAgent();

  /**
   * Called by external component to execute put/get command
   * @param command put/get command
   * @return a fixed view result set representing upload/download result
   */
  FileTransferExecutionResult *execute(std::string *command);

  std::vector<FileTransferExecutionResult> *getResult()
  {
    return &executionResults;
  }

private:
  /**
   * Populate file metadata, (Get source file name)
   * Process compression metadata
   * Divide files to large and small ones
   */
  void initFileMetadata();

  /**
   * Upload large files in sequence, upload small files in parallel in
   * a thread pool object
   * @param stageInfo
   */
  void upload(StageInfo *stageInfo);

  /**
   * Upload single file.
   */
  void uploadSingleFile(IStorageClient *client,
    FileMetadata *fileMetadata,
    FileTransferExecutionResult *result);

  /**
   * Given file name, calculate sha256 message digest. The digest is
   * calculated after file is compressed (if needed)
   * @param fileName
   * @param digest
   */
  void updateFileDigest(FileMetadata *fileMetadata);

  void download();

  /**
   * compress source file into a temporary file if required by
   * user.
   * @param fileMetadata
   */
  void compressSourceFile(FileMetadata *fileMetadata);

  IStatementPutGet *m_stmtPutGet;

  /// Files that will be uploaded in sequence
  std::vector<FileMetadata> m_largeFilesMeta;

  /// Files that will be uploaded in parallel
  std::vector<FileMetadata> m_smallFilesMeta;

  /// vectors to store newly created execution result
  std::vector<FileTransferExecutionResult> executionResults;

  /// used in small file upload
  SF_MUTEX_HANDLE m_resultMutex;

  /// parallel thread for upload/download small files
  PutGetParseResponse response;

  /// used to initialize file metadata
  FileMetadataInitializer m_FileMetadataInitializer;
};
}
}

#endif //SNOWFLAKECLIENT_FILETRANSFERAGENT_HPP
