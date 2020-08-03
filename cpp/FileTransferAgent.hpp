/*
 * Copyright (c) 2018-2019 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_FILETRANSFERAGENT_HPP
#define SNOWFLAKECLIENT_FILETRANSFERAGENT_HPP

#include <map>
#include <string>
#include <snowflake/IFileTransferAgent.hpp>
#include "snowflake/IStatementPutGet.hpp"
#include "snowflake/IFileTransferAgent.hpp"
#include "snowflake/ITransferResult.hpp"
#include "FileTransferExecutionResult.hpp"
#include "FileMetadata.hpp"
#include "FileMetadataInitializer.hpp"
#include "snowflake/platform.h"

#define FILE_ENCRYPTION_BLOCK_SIZE 128

namespace Snowflake
{
namespace Client
{

class IStorageClient;

class FileTransferExecutionResult;

/**
 * This is the main class to external component (c api or ODBC)
 * External component should implement IStatement interface to submit put
 * or get command to server to do parsing.
 */
class FileTransferAgent : public IFileTransferAgent
{
public:
  FileTransferAgent(IStatementPutGet *statement,
                    TransferConfig * transferConfig = nullptr);

  ~FileTransferAgent();

  /**
   * Called by external component to execute put/get command
   * @param command put/get command
   * @return a fixed view result set representing upload/download result
   */
  virtual ITransferResult *execute(std::string *command);

  /**
  * Set upload stream to enable upload file from stream in memory.
  * @param uploadStream The stream to be uploaded.
  * @param dataSize The data size of the stream.
  */
  virtual void setUploadStream(std::basic_iostream<char>* uploadStream,
                               size_t dataSize)
  {
    m_uploadStream = uploadStream;
    m_uploadStreamSize = dataSize;
  }

  /**
   * Set useUrand to true to use /dev/urandom device
   * Set it to false to use /dev/random device
   * @param useUrand
   */
  virtual void setRandomDeviceAsUrand(bool useUrand)
  {
    m_useDevUrand = useUrand;
  }

private:
  /**
   * Populate file metadata, (Get source file name)
   * Process compression metadata
   * Divide files to large and small ones
   */
  void initFileMetadata(std::string* command);

  /**
   * Upload large files in sequence, upload small files in parallel in
   * a thread pool object
   */
  void upload(std::string *command);

  /**
   * Upload single file.
   */
  RemoteStorageRequestOutcome uploadSingleFile(IStorageClient *client,
    FileMetadata *fileMetadata, size_t resultIndex);

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

  /**
   * Download large files in sequence, download small files in parallel in
   * a thread pool object
   */
  void download(std::string *command);

  void downloadFilesInParallel(std::string *command);

  /**
   * Download single file.
   */
  RemoteStorageRequestOutcome downloadSingleFile(IStorageClient *client,
                                                 FileMetadata *fileMetadata, size_t resultIndex);

  /**
   * compress source file into a temporary file if required by
   * user.
   * @param fileMetadata
   */
  void compressSourceFile(FileMetadata *fileMetadata);

  /**
   * Reset private members between two consecutive put/get command
   */
  void reset();

  void getPresignedUrlForUploading(FileMetadata& io_fileMetadata,
                                   const std::string& command);

  /**
  * Parses out the local file path from the command. We need this to get the
  * file paths to expand wildcards and make sure the paths GS returns are
  * correct
  *
  * @param command  The GET/PUT command we send to GS
  * @param unescape True to unescape backslashes coming from GS
  * @return Path to the local file
  */
  std::string getLocalFilePathFromCommand(std::string const& command,
                                          bool unescape);

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

  /// mutex to prevent from multiple thread renewing token same time
  SF_MUTEX_HANDLE m_parallelTokRenewMutex;

  /// mutex to prevent from multiple thread appending failed transfer file at the same time
  SF_MUTEX_HANDLE m_parallelFailedMsgMutex;

  /// seconds in unix time for last time token is refreshed
  long m_lastRefreshTokenSec;

  /// config struct that is passed in.
  TransferConfig * m_transferConfig;

  // The stream for uploading data from memory. (NOT OWN)
  std::basic_iostream<char>* m_uploadStream;

  // The data size of upload stream.
  size_t m_uploadStreamSize;

  /// Whether to use /dev/urandom or /dev/random;
  bool m_useDevUrand;
};
}
}

#endif //SNOWFLAKECLIENT_FILETRANSFERAGENT_HPP
