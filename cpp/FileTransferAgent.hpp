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
  void upload(std::string *command);

  /**
   * Upload single file.
   */
  TransferOutcome uploadSingleFile(IStorageClient *client,
    FileMetadata *fileMetadata, unsigned int resultIndex);

  /**
   * Given file name, calculate sha256 message digest. The digest is
   * calculated after file is compressed (if needed)
   * @param fileName
   * @param digest
   */
  void updateFileDigest(FileMetadata *fileMetadata);

  /**
   * Renew aws expired token by re-submitting put/get command to server
   * @param client the new client object using the new token
   */
  void renewToken(std::string * command);

  void uploadFilesInParallel(std::string *command);

  void download();

  /**
   * compress source file into a temporary file if required by
   * user.
   * @param fileMetadata
   */
  void compressSourceFile(FileMetadata *fileMetadata);

  /// interface to communicate with server
  IStatementPutGet *m_stmtPutGet;

  /// interface to communicate with remote storage
  IStorageClient * m_storageClient;

  /// Files that will be uploaded in sequence
  std::vector<FileMetadata> m_largeFilesMeta;

  /// Files that will be uploaded in parallel
  std::vector<FileMetadata> m_smallFilesMeta;

  /// vectors to store newly created execution result
  FileTransferExecutionResult *m_executionResults;

  /// parallel thread for upload/download small files
  PutGetParseResponse response;

  /// used to initialize file metadata
  FileMetadataInitializer m_FileMetadataInitializer;
};
}
}

#endif //SNOWFLAKECLIENT_FILETRANSFERAGENT_HPP
